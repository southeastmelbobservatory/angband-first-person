#include "../include/abfp_upstream_adapter.h"

/* Compiled against the peeled Angband 4.2.6 release commit:
 * f3082213b73f3e463e3d0d60bff4b00462beae6e
 */
#include "angband.h"
#include "cave.h"
#include "cmd-core.h"
#include "game-event.h"
#include "game-world.h"
#include "init.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "store.h"
#include "target.h"
#include <string.h>

#define ABFP_SNAPSHOT_RADIUS 10
#define ABFP_SNAPSHOT_SIDE (ABFP_SNAPSHOT_RADIUS * 2 + 1)
#define ABFP_SNAPSHOT_CELLS (ABFP_SNAPSHOT_SIDE * ABFP_SNAPSHOT_SIDE)
#define ABFP_MAX_SNAPSHOT_ENTITIES 1024
#define ABFP_MAX_SNAPSHOT_MESSAGES 16

#define ABFP_MONSTER_ID_BASE UINT64_C(0x1000000000000000)
#define ABFP_OBJECT_ID_BASE  UINT64_C(0x2000000000000000)

static bool handlers_installed;
static struct loc pending_from;
static bool pending_move;
static bool pending_attack;
static uint64_t pending_attack_target_id;
static struct loc pending_attack_target;
static bool pending_command;
static abfp_command_type pending_command_type;
static bool observed_player_moved;
static abfp_cell snapshot_cells[ABFP_SNAPSHOT_CELLS];
static abfp_cell previous_cells[ABFP_SNAPSHOT_CELLS];
static uint32_t previous_cell_count;
static abfp_entity_state snapshot_entities[ABFP_MAX_SNAPSHOT_ENTITIES];
static abfp_entity_state previous_entities[ABFP_MAX_SNAPSHOT_ENTITIES];
static uint32_t previous_entity_count;
static abfp_message pending_messages[ABFP_MAX_SNAPSHOT_MESSAGES];
static uint32_t pending_message_count;
static abfp_message snapshot_messages[ABFP_MAX_SNAPSHOT_MESSAGES];
static uint32_t snapshot_message_count;
static abfp_inventory_item snapshot_inventory[ABFP_MAX_INVENTORY_ITEMS];
static struct object *snapshot_inventory_ptrs[ABFP_MAX_INVENTORY_ITEMS];
static uint32_t snapshot_inventory_count;
static abfp_shop_state snapshot_shop;
static struct object *snapshot_shop_ptrs[ABFP_MAX_SHOP_ITEMS];
static abfp_spell_entry snapshot_spells[ABFP_MAX_SPELLS];
static uint32_t snapshot_spell_count;
static bool pending_bolt_active;
static struct loc pending_bolt_from;
static struct loc pending_bolt_to;
static int32_t pending_bolt_proj_type;
static bool pending_missile_active;
static struct loc pending_missile_from;
static struct loc pending_missile_to;
static bool pending_explosion_active;
static struct loc pending_explosion_center;
static int32_t pending_explosion_proj_type;
static abfp_player_state previous_player;
static int32_t previous_depth;
static bool snapshot_history_valid;

static int abfp_direction(abfp_command_type command)
{
    switch (command) {
    case ABFP_COMMAND_MOVE_NORTH: return DIR_N;
    case ABFP_COMMAND_MOVE_SOUTH: return DIR_S;
    case ABFP_COMMAND_MOVE_WEST: return DIR_W;
    case ABFP_COMMAND_MOVE_EAST: return DIR_E;
    case ABFP_COMMAND_MOVE_NORTH_WEST: return DIR_NW;
    case ABFP_COMMAND_MOVE_NORTH_EAST: return DIR_NE;
    case ABFP_COMMAND_MOVE_SOUTH_WEST: return DIR_SW;
    case ABFP_COMMAND_MOVE_SOUTH_EAST: return DIR_SE;
    default: return DIR_UNKNOWN;
    }
}

static int abfp_alter_direction(abfp_command_type command)
{
    switch (command) {
    case ABFP_COMMAND_ALTER_NORTH: return DIR_N;
    case ABFP_COMMAND_ALTER_SOUTH: return DIR_S;
    case ABFP_COMMAND_ALTER_WEST: return DIR_W;
    case ABFP_COMMAND_ALTER_EAST: return DIR_E;
    case ABFP_COMMAND_ALTER_NORTH_WEST: return DIR_NW;
    case ABFP_COMMAND_ALTER_NORTH_EAST: return DIR_NE;
    case ABFP_COMMAND_ALTER_SOUTH_WEST: return DIR_SW;
    case ABFP_COMMAND_ALTER_SOUTH_EAST: return DIR_SE;
    default: return DIR_UNKNOWN;
    }
}

static void abfp_player_moved(game_event_type type,
    game_event_data *data, void *user)
{
    (void)type;
    (void)data;
    (void)user;
    observed_player_moved = true;
}

static void abfp_message_received(game_event_type type,
    game_event_data *data, void *user)
{
    abfp_message *slot;
    (void)type;
    (void)user;
    if (!data || !data->message.msg || !data->message.msg[0]) return;

    if (pending_message_count >= ABFP_MAX_SNAPSHOT_MESSAGES) {
        memmove(&pending_messages[0], &pending_messages[1],
            (ABFP_MAX_SNAPSHOT_MESSAGES - 1) * sizeof(*pending_messages));
        pending_message_count = ABFP_MAX_SNAPSHOT_MESSAGES - 1;
    }
    slot = &pending_messages[pending_message_count++];
    memset(slot, 0, sizeof(*slot));
    slot->type = data->message.type;
    strncpy(slot->text, data->message.msg, sizeof(slot->text) - 1);
}

static void abfp_flush_projectile_events(void)
{
    if (pending_bolt_active) {
        abfp_event event = {0};
        event.type = ABFP_EVENT_SPELL;
        event.turn = (uint64_t)turn;
        event.subject_id = (player && loc_eq(pending_bolt_from, player->grid)) ? 1 : 0;
        event.from_x = pending_bolt_from.x;
        event.from_y = pending_bolt_from.y;
        event.to_x = pending_bolt_to.x;
        event.to_y = pending_bolt_to.y;
        event.value = pending_bolt_proj_type + 1;
        abfp_bridge_emit(&event);
        pending_bolt_active = false;
    }
    if (pending_missile_active) {
        abfp_event event = {0};
        event.type = ABFP_EVENT_SPELL;
        event.turn = (uint64_t)turn;
        event.subject_id = 1;
        event.from_x = pending_missile_from.x;
        event.from_y = pending_missile_from.y;
        event.to_x = pending_missile_to.x;
        event.to_y = pending_missile_to.y;
        event.value = 0; /* Physical arrow/shot/thrown weapon */
        abfp_bridge_emit(&event);
        pending_missile_active = false;
    }
    if (pending_explosion_active) {
        abfp_event event = {0};
        event.type = ABFP_EVENT_SPELL;
        event.turn = (uint64_t)turn;
        event.subject_id = 0;
        event.from_x = event.to_x = pending_explosion_center.x;
        event.from_y = event.to_y = pending_explosion_center.y;
        event.value = 1000 + pending_explosion_proj_type;
        abfp_bridge_emit(&event);
        pending_explosion_active = false;
    }
}

static void abfp_bolt_received(game_event_type type,
    game_event_data *data, void *user)
{
    (void)type;
    (void)user;
    if (!data || !data->bolt.drawing || !data->bolt.seen) return;
    if (pending_bolt_active && pending_bolt_proj_type != data->bolt.proj_type) {
        abfp_flush_projectile_events();
    }
    if (!pending_bolt_active) {
        pending_bolt_active = true;
        pending_bolt_from = loc(data->bolt.ox, data->bolt.oy);
        pending_bolt_proj_type = data->bolt.proj_type;
    }
    pending_bolt_to = loc(data->bolt.x, data->bolt.y);
}

static void abfp_missile_received(game_event_type type,
    game_event_data *data, void *user)
{
    (void)type;
    (void)user;
    if (!data || !data->missile.seen) return;
    if (!pending_missile_active) {
        pending_missile_active = true;
        pending_missile_from = player ? player->grid : loc(data->missile.x, data->missile.y);
    }
    pending_missile_to = loc(data->missile.x, data->missile.y);
}

static void abfp_explosion_received(game_event_type type,
    game_event_data *data, void *user)
{
    (void)type;
    (void)user;
    if (!data || !data->explosion.drawing) return;
    pending_explosion_active = true;
    pending_explosion_center = data->explosion.centre;
    pending_explosion_proj_type = data->explosion.proj_type;
}

bool abfp_upstream_install_handlers(void)
{
    if (handlers_installed) return true;
    event_add_handler(EVENT_PLAYERMOVED, abfp_player_moved, NULL);
    event_add_handler(EVENT_MESSAGE, abfp_message_received, NULL);
    event_add_handler(EVENT_BOLT, abfp_bolt_received, NULL);
    event_add_handler(EVENT_MISSILE, abfp_missile_received, NULL);
    event_add_handler(EVENT_EXPLOSION, abfp_explosion_received, NULL);
    handlers_installed = true;
    (void)abfp_upstream_refresh_snapshot();
    return true;
}

void abfp_upstream_remove_handlers(void)
{
    if (!handlers_installed) return;
    event_remove_handler(EVENT_PLAYERMOVED, abfp_player_moved, NULL);
    event_remove_handler(EVENT_MESSAGE, abfp_message_received, NULL);
    event_remove_handler(EVENT_BOLT, abfp_bolt_received, NULL);
    event_remove_handler(EVENT_MISSILE, abfp_missile_received, NULL);
    event_remove_handler(EVENT_EXPLOSION, abfp_explosion_received, NULL);
    handlers_installed = false;
    snapshot_history_valid = false;
    previous_cell_count = 0;
    previous_entity_count = 0;
    pending_message_count = 0;
    snapshot_message_count = 0;
    snapshot_inventory_count = 0;
    snapshot_spell_count = 0;
    memset(&snapshot_shop, 0, sizeof(snapshot_shop));
    previous_depth = 0;
    pending_move = false;
    pending_attack = false;
    pending_attack_target_id = 0;
    pending_bolt_active = false;
    pending_missile_active = false;
    pending_explosion_active = false;
    pending_command = false;
    pending_command_type = ABFP_COMMAND_WAIT;
    observed_player_moved = false;
}

static bool abfp_queue_noop_with_msg(const char *text, abfp_command_type command)
{
    if (text && *text) msg("%s", text);
    if (cmdq_push(CMD_NULL) != 0) return false;
    pending_command = true;
    pending_command_type = command;
    return true;
}

static struct object *abfp_resolve_inventory_object(int32_t idx)
{
    if (idx < 0 || (uint32_t)idx >= snapshot_inventory_count) return NULL;
    return snapshot_inventory_ptrs[idx];
}

static int abfp_apply_aim_target(const abfp_command *cmd)
{
    if (cmd && (cmd->arg3 > 0 || cmd->arg4 > 0) && cave) {
        struct loc tgrid = loc(cmd->arg3, cmd->arg4);
        if (square_in_bounds(cave, tgrid)) {
            struct monster *mon = square_monster(cave, tgrid);
            if (mon && mon->hp > 0 && target_able(mon)) {
                target_set_monster(mon);
                return DIR_TARGET;
            }
            target_set_location(tgrid.y, tgrid.x);
            return DIR_TARGET;
        }
    }
    if (cmd && cmd->arg2 >= 1 && cmd->arg2 <= 9 && cmd->arg2 != 5) {
        return cmd->arg2;
    }
    return DIR_N;
}

static struct object *abfp_find_book_for_spell(int32_t spell_index)
{
    const struct class_spell *spell;
    int i;
    if (!player || !player->upkeep || !player->upkeep->inven) return NULL;
    spell = spell_by_index(player, spell_index);
    if (!spell) return NULL;
    for (i = 0; i < z_info->pack_size; ++i) {
        struct object *obj = player->upkeep->inven[i];
        const struct class_book *book;
        if (!obj) continue;
        book = player_object_to_book(player, obj);
        if (book && spell->bidx >= 0 && spell->bidx < player->class->magic.num_books &&
            book == &player->class->magic.books[spell->bidx]) {
            return obj;
        }
    }
    return NULL;
}

static struct object *abfp_find_default_ammo(void)
{
    int i;
    if (!player || !player->upkeep || !player->state.ammo_tval) return NULL;
    if (player->upkeep->quiver) {
        for (i = 0; i < z_info->quiver_size; ++i) {
            struct object *obj = player->upkeep->quiver[i];
            if (obj && obj->tval == player->state.ammo_tval) return obj;
        }
    }
    if (player->upkeep->inven) {
        for (i = 0; i < z_info->pack_size; ++i) {
            struct object *obj = player->upkeep->inven[i];
            if (obj && obj->tval == player->state.ammo_tval) return obj;
        }
    }
    return NULL;
}

static struct object *abfp_resolve_shop_object(int32_t idx)
{
    if (!snapshot_shop.active || idx < 0 || (uint32_t)idx >= snapshot_shop.item_count)
        return NULL;
    return snapshot_shop_ptrs[idx];
}

bool abfp_upstream_queue_command_ex(const abfp_command *cmd)
{
    abfp_command_type command;
    int dir, alter_dir;

    if (!cmd || !player || !cave || pending_command) return false;

    command = cmd->type;
    dir = abfp_direction(command);
    alter_dir = abfp_alter_direction(command);

    pending_message_count = 0;
    pending_attack = false;
    pending_attack_target_id = 0;

    if (dir != DIR_UNKNOWN) {
        struct loc target = loc_sum(player->grid, ddgrid[dir]);
        struct monster *mon = square_in_bounds(cave, target) ?
            square_monster(cave, target) : NULL;
        /*
         * Prevent stepping onto a town shop from blocking the game thread inside
         * Angband's ncurses store menu loop while running under a 3D client.
         * The 3D HUD renders the live shop interface directly from snapshot.shop.
         */
        if (square_in_bounds(cave, target) && square_isshop(cave, target)) {
            event_remove_handler_type(EVENT_ENTER_STORE);
            event_remove_handler_type(EVENT_USE_STORE);
            event_remove_handler_type(EVENT_LEAVE_STORE);
        }
        const bool attacking = mon != NULL;
        if (cmdq_push(attacking ? CMD_ALTER : CMD_WALK) != 0) return false;
        cmd_set_arg_direction(cmdq_peek(), "direction", dir);
        pending_from = player->grid;
        pending_move = true;
        if (attacking) {
            pending_attack = true;
            pending_attack_target_id = ABFP_MONSTER_ID_BASE | (uint64_t)(uint32_t)mon->midx;
            pending_attack_target = target;
        }
        pending_command = true;
        pending_command_type = command;
        observed_player_moved = false;
        return true;
    }

    if (alter_dir != DIR_UNKNOWN) {
        struct loc target = loc_sum(player->grid, ddgrid[alter_dir]);
        struct monster *mon = square_in_bounds(cave, target) ?
            square_monster(cave, target) : NULL;
        if (cmdq_push(CMD_ALTER) != 0) return false;
        cmd_set_arg_direction(cmdq_peek(), "direction", alter_dir);
        pending_from = player->grid;
        if (mon) {
            pending_attack = true;
            pending_attack_target_id = ABFP_MONSTER_ID_BASE | (uint64_t)(uint32_t)mon->midx;
            pending_attack_target = target;
        }
        pending_command = true;
        pending_command_type = command;
        return true;
    }

    if (command == ABFP_COMMAND_WAIT) {
        if (cmdq_push(CMD_HOLD) != 0) return false;
        pending_command = true;
        pending_command_type = command;
        return true;
    }

    if (command == ABFP_COMMAND_REST) {
        if (cmdq_push(CMD_REST) != 0) return false;
        cmd_set_arg_choice(cmdq_peek(), "choice", REST_COMPLETE);
        pending_command = true;
        pending_command_type = command;
        return true;
    }

    if (command == ABFP_COMMAND_AUTOPICKUP) {
        struct object *floor_obj = square_object(cave, player->grid);
        if (floor_obj) {
            struct object *pick = NULL;
            struct object *cur;
            for (cur = floor_obj; cur; cur = cur->next) {
                if (!tval_is_money(cur) && inven_carry_num(player, cur) > 0) {
                    pick = cur;
                    break;
                }
            }
            if (!pick) {
                /* Pick up gold if present, otherwise report full pack cleanly. */
                bool has_money = false;
                for (cur = floor_obj; cur; cur = cur->next) {
                    if (tval_is_money(cur)) {
                        has_money = true;
                        break;
                    }
                }
                if (!has_money) {
                    return abfp_queue_noop_with_msg(
                        "Your pack is too full to pick that up.", command);
                }
                if (cmdq_push(CMD_AUTOPICKUP) != 0) return false;
            } else {
                /* Explicit manual pickup of the top carriable floor item without opening a text UI menu. */
                if (cmdq_push(CMD_PICKUP) != 0) return false;
                cmd_set_arg_item(cmdq_peek(), "item", pick);
            }
        } else {
            if (cmdq_push(CMD_AUTOPICKUP) != 0) return false;
        }
        pending_command = true;
        pending_command_type = command;
        return true;
    }

    if (command == ABFP_COMMAND_GO_UP) {
        if (cmdq_push(CMD_GO_UP) != 0) return false;
        pending_command = true;
        pending_command_type = command;
        return true;
    }

    if (command == ABFP_COMMAND_GO_DOWN) {
        if (cmdq_push(CMD_GO_DOWN) != 0) return false;
        pending_command = true;
        pending_command_type = command;
        return true;
    }

    if (command == ABFP_COMMAND_USE_ITEM ||
        command == ABFP_COMMAND_QUAFF_ITEM ||
        command == ABFP_COMMAND_READ_ITEM ||
        command == ABFP_COMMAND_EAT_ITEM ||
        command == ABFP_COMMAND_WIELD_ITEM ||
        command == ABFP_COMMAND_TAKEOFF_ITEM ||
        command == ABFP_COMMAND_DROP_ITEM ||
        command == ABFP_COMMAND_REFILL_ITEM) {
        struct object *obj = abfp_resolve_inventory_object(cmd->arg1);
        cmd_code code = CMD_USE;
        if (!obj) {
            return abfp_queue_noop_with_msg("No item in that slot.", command);
        }
        if (command == ABFP_COMMAND_QUAFF_ITEM) {
            if (!tval_is_potion(obj))
                return abfp_queue_noop_with_msg("That item is not a potion.", command);
            code = CMD_QUAFF;
        } else if (command == ABFP_COMMAND_READ_ITEM) {
            if (!tval_is_scroll(obj))
                return abfp_queue_noop_with_msg("That item is not a scroll.", command);
            code = CMD_READ_SCROLL;
        } else if (command == ABFP_COMMAND_EAT_ITEM) {
            if (!tval_is_edible(obj))
                return abfp_queue_noop_with_msg("You cannot eat that.", command);
            code = CMD_EAT;
        } else if (command == ABFP_COMMAND_WIELD_ITEM) {
            if (!obj_can_wear(obj))
                return abfp_queue_noop_with_msg("You cannot wear or wield that.", command);
            code = CMD_WIELD;
        } else if (command == ABFP_COMMAND_TAKEOFF_ITEM) {
            if (!object_is_equipped(player->body, obj) || !obj_can_takeoff(obj))
                return abfp_queue_noop_with_msg("You cannot take that off.", command);
            code = CMD_TAKEOFF;
        } else if (command == ABFP_COMMAND_DROP_ITEM) {
            if (object_is_equipped(player->body, obj) && !obj_can_takeoff(obj))
                return abfp_queue_noop_with_msg("That item cannot be removed.", command);
            code = CMD_DROP;
        } else if (command == ABFP_COMMAND_REFILL_ITEM) {
            if (!obj_can_refill(obj))
                return abfp_queue_noop_with_msg("That cannot refuel your light.", command);
            code = CMD_REFILL;
        } else {
            if (!obj_is_useable(obj))
                return abfp_queue_noop_with_msg("You cannot use that item directly.", command);
            code = CMD_USE;
        }

        if (cmdq_push(code) != 0) return false;
        cmd_set_arg_item(cmdq_peek(), "item", obj);
        if (code == CMD_DROP) {
            int qty = cmd->arg2 > 0 ? cmd->arg2 : 1;
            if (qty > obj->number) qty = obj->number;
            cmd_set_arg_number(cmdq_peek(), "quantity", qty);
        } else if (code == CMD_USE && obj_needs_aim(obj)) {
            int aim_dir = abfp_apply_aim_target(cmd);
            cmd_set_arg_target(cmdq_peek(), "target", aim_dir);
        }
        pending_command = true;
        pending_command_type = command;
        return true;
    }

    if (command == ABFP_COMMAND_FIRE_ITEM) {
        struct object *bow = equipped_item_by_slot_name(player, "shooting");
        struct object *ammo = NULL;
        int aim_dir;
        if (!bow || !player->state.ammo_tval) {
            return abfp_queue_noop_with_msg("You have no bow, sling, or crossbow equipped.", command);
        }
        if (cmd->arg1 >= 0) {
            ammo = abfp_resolve_inventory_object(cmd->arg1);
            if (!ammo || ammo->tval != player->state.ammo_tval) {
                ammo = abfp_find_default_ammo();
            }
        } else {
            ammo = abfp_find_default_ammo();
        }
        if (!ammo) {
            return abfp_queue_noop_with_msg("You have no matching ammunition to fire.", command);
        }
        aim_dir = abfp_apply_aim_target(cmd);
        if (cmdq_push(CMD_FIRE) != 0) return false;
        cmd_set_arg_item(cmdq_peek(), "item", ammo);
        cmd_set_arg_target(cmdq_peek(), "target", aim_dir);
        pending_command = true;
        pending_command_type = command;
        return true;
    }

    if (command == ABFP_COMMAND_THROW_ITEM) {
        struct object *obj = abfp_resolve_inventory_object(cmd->arg1);
        int aim_dir;
        if (!obj) {
            return abfp_queue_noop_with_msg("No item in that slot to throw.", command);
        }
        if (!obj_can_throw(obj)) {
            return abfp_queue_noop_with_msg("You cannot throw that item.", command);
        }
        aim_dir = abfp_apply_aim_target(cmd);
        if (cmdq_push(CMD_THROW) != 0) return false;
        cmd_set_arg_item(cmdq_peek(), "item", obj);
        cmd_set_arg_target(cmdq_peek(), "target", aim_dir);
        pending_command = true;
        pending_command_type = command;
        return true;
    }

    if (command == ABFP_COMMAND_CAST_SPELL) {
        const struct class_spell *spell;
        if (!player_can_cast(player, true)) {
            return abfp_queue_noop_with_msg(NULL, command);
        }
        spell = spell_by_index(player, cmd->arg1);
        if (!spell) {
            return abfp_queue_noop_with_msg("Unknown spell.", command);
        }
        if (!spell_okay_to_cast(player, cmd->arg1)) {
            return abfp_queue_noop_with_msg("You have not learned that spell yet.", command);
        }
        if (!abfp_find_book_for_spell(cmd->arg1)) {
            return abfp_queue_noop_with_msg("You do not have the spellbook for that spell.", command);
        }
        /* Prevent do_cmd_cast() from calling interactive get_check() when out of mana. */
        if (spell->smana > player->csp) {
            return abfp_queue_noop_with_msg("You do not have enough mana to cast that spell.", command);
        }
        if (cmdq_push(CMD_CAST) != 0) return false;
        cmd_set_arg_choice(cmdq_peek(), "spell", cmd->arg1);
        if (spell_needs_aim(cmd->arg1)) {
            int aim_dir = abfp_apply_aim_target(cmd);
            cmd_set_arg_target(cmdq_peek(), "target", aim_dir);
        }
        pending_command = true;
        pending_command_type = command;
        return true;
    }

    if (command == ABFP_COMMAND_STUDY_SPELL) {
        if (!player->upkeep || player->upkeep->new_spells <= 0) {
            return abfp_queue_noop_with_msg("You cannot learn any new spells yet.", command);
        }
        if (!player_can_study(player, true)) {
            return abfp_queue_noop_with_msg(NULL, command);
        }
        if (player_has(player, PF_CHOOSE_SPELLS)) {
            if (!spell_okay_to_study(player, cmd->arg1) || !abfp_find_book_for_spell(cmd->arg1)) {
                return abfp_queue_noop_with_msg("You cannot study that spell.", command);
            }
            if (cmdq_push(CMD_STUDY) != 0) return false;
            cmd_set_arg_choice(cmdq_peek(), "spell", cmd->arg1);
        } else {
            struct object *book_obj = abfp_find_book_for_spell(cmd->arg1);
            if (!book_obj && cmd->arg2 >= 0) {
                book_obj = abfp_resolve_inventory_object(cmd->arg2);
            }
            if (!book_obj || !player_object_to_book(player, book_obj)) {
                return abfp_queue_noop_with_msg("You have no book to study from.", command);
            }
            if (cmdq_push(CMD_STUDY) != 0) return false;
            cmd_set_arg_item(cmdq_peek(), "item", book_obj);
        }
        pending_command = true;
        pending_command_type = command;
        return true;
    }

    if (command == ABFP_COMMAND_SHOP_BUY) {
        struct store *store = store_at(cave, player->grid);
        struct object *obj = abfp_resolve_shop_object(cmd->arg1);
        int qty;
        if (!store)
            return abfp_queue_noop_with_msg("You are not standing in a shop.", command);
        if (!obj)
            return abfp_queue_noop_with_msg("That item is no longer in stock.", command);
        qty = cmd->arg2 > 0 ? cmd->arg2 : 1;
        if (qty > obj->number) qty = obj->number;
        if (cmdq_push(store->feat == FEAT_HOME ? CMD_RETRIEVE : CMD_BUY) != 0) return false;
        cmd_set_arg_item(cmdq_peek(), "item", obj);
        cmd_set_arg_number(cmdq_peek(), "quantity", qty);
        pending_command = true;
        pending_command_type = command;
        return true;
    }

    if (command == ABFP_COMMAND_SHOP_SELL) {
        struct store *store = store_at(cave, player->grid);
        struct object *obj = abfp_resolve_inventory_object(cmd->arg1);
        int qty;
        if (!store)
            return abfp_queue_noop_with_msg("You are not standing in a shop.", command);
        if (!obj)
            return abfp_queue_noop_with_msg("No item in that inventory slot.", command);
        if (store->feat != FEAT_HOME && !store_will_buy_tester(obj))
            return abfp_queue_noop_with_msg("The shopkeeper will not buy that item.", command);
        qty = cmd->arg2 > 0 ? cmd->arg2 : 1;
        if (qty > obj->number) qty = obj->number;
        if (cmdq_push(store->feat == FEAT_HOME ? CMD_STASH : CMD_SELL) != 0) return false;
        cmd_set_arg_item(cmdq_peek(), "item", obj);
        cmd_set_arg_number(cmdq_peek(), "quantity", qty);
        pending_command = true;
        pending_command_type = command;
        return true;
    }

    return false;
}

bool abfp_upstream_queue_command(abfp_command_type command)
{
    abfp_command full = { command, 0, 1, 0, 0, {0} };
    return abfp_upstream_queue_command_ex(&full);
}

static abfp_terrain_type abfp_terrain_at(struct loc grid)
{
    if (!square_in_bounds(cave, grid) || !square_isknown(cave, grid))
        return ABFP_TERRAIN_UNKNOWN;

    /* Never leak secret doors: Angband presents them as solid rock walls. */
    if (square_issecretdoor(cave, grid)) return ABFP_TERRAIN_WALL;

    if (square_isupstairs(cave, grid)) return ABFP_TERRAIN_STAIRS_UP;
    if (square_isdownstairs(cave, grid)) return ABFP_TERRAIN_STAIRS_DOWN;
    if (square_iscloseddoor(cave, grid)) return ABFP_TERRAIN_DOOR;
    if (square_isopendoor(cave, grid)) return ABFP_TERRAIN_DOOR_OPEN;
    if (square_isbrokendoor(cave, grid)) return ABFP_TERRAIN_DOOR_BROKEN;
    if (square_isrubble(cave, grid)) return ABFP_TERRAIN_RUBBLE;
    if (square_isshop(cave, grid)) return ABFP_TERRAIN_SHOP;
    if (square_isvisibletrap(cave, grid)) return ABFP_TERRAIN_TRAP;
    if (square_isfloor(cave, grid)) return ABFP_TERRAIN_FLOOR;
    if (square_ismineral(cave, grid)) return ABFP_TERRAIN_VEIN;
    if (square_isrock(cave, grid)) return ABFP_TERRAIN_WALL;

    return ABFP_TERRAIN_WALL;
}

static void abfp_append_monster(struct monster *mon, int x, int y,
    uint32_t *entity_count)
{
    abfp_entity_state *entity;

    if (!mon || !mon->race || mon->hp <= 0 ||
        *entity_count >= ABFP_MAX_SNAPSHOT_ENTITIES) return;

    entity = &snapshot_entities[(*entity_count)++];
    memset(entity, 0, sizeof(*entity));
    if (mon->race->name)
        strncpy(entity->name, mon->race->name, sizeof(entity->name) - 1);
    entity->id = ABFP_MONSTER_ID_BASE | (uint64_t)(uint32_t)mon->midx;
    entity->x = x;
    entity->y = y;
    entity->type = ABFP_ENTITY_MONSTER;
    entity->kind_id = (int32_t)mon->race->ridx;
    entity->category_id = 0;
    entity->variant_id = 0;
    entity->hitpoints = mon->hp;
    entity->max_hitpoints = mon->maxhp;
    entity->quantity = 1;
}

static void abfp_append_objects(struct object *obj, int x, int y,
    uint32_t *entity_count)
{
    for (; obj && *entity_count < ABFP_MAX_SNAPSHOT_ENTITIES; obj = obj->next) {
        const struct object *known = obj->known;
        abfp_entity_state *entity = &snapshot_entities[(*entity_count)++];
        memset(entity, 0, sizeof(*entity));
        /* Use Angband's own spoiler-free description so unidentified scrolls/potions/rings
         * display their flavor/title (e.g. '6 Scrolls titled "sactium pulum"') rather than
         * leaking raw kind->name ('Phase Door'). */
        if (known) {
            object_desc(entity->name, sizeof(entity->name), obj,
                ODESC_PREFIX | ODESC_FULL, player);
        } else {
            strncpy(entity->name, "an unknown item", sizeof(entity->name) - 1);
        }

        entity->id = ABFP_OBJECT_ID_BASE | (uint64_t)obj->oidx;
        entity->x = x;
        entity->y = y;
        entity->type = ABFP_ENTITY_OBJECT;
        entity->kind_id = (known && known->kind) ?
            (int32_t)known->kind->kidx : 0;
        entity->category_id = known ?
            (known->tval ? (int32_t)known->tval : (int32_t)obj->tval) : 0;
        entity->variant_id = (known && known->kind) ? known->sval : 0;
        entity->hitpoints = 0;
        entity->max_hitpoints = 0;
        entity->quantity = known ?
            (known->number > 0 ? (int32_t)known->number : (int32_t)obj->number) : 1;
    }
}

static void abfp_refresh_inventory_and_shop(void)
{
    struct store *store = store_at(cave, player->grid);
    int i;

    snapshot_inventory_count = 0;
    memset(&snapshot_shop, 0, sizeof(snapshot_shop));

    /* 1. Equipped items */
    for (i = 0; i < player->body.count && snapshot_inventory_count < ABFP_MAX_INVENTORY_ITEMS; ++i) {
        struct object *obj = slot_object(player, i);
        abfp_inventory_item *dst;
        const char *slot_desc;
        if (!obj) continue;
        dst = &snapshot_inventory[snapshot_inventory_count];
        snapshot_inventory_ptrs[snapshot_inventory_count] = obj;
        memset(dst, 0, sizeof(*dst));
        dst->index = (int32_t)snapshot_inventory_count;
        dst->label = gear_to_label(player, obj);
        dst->equipped = 1;
        dst->quantity = obj->number;
        dst->category_id = obj->tval;
        dst->variant_id = obj->sval;
        if (obj_is_useable(obj)) dst->flags |= ABFP_ITEM_FLAG_USABLE;
        if (store && (store->feat == FEAT_HOME || store_will_buy_tester(obj))) {
            dst->flags |= ABFP_ITEM_FLAG_SELLABLE;
            dst->sell_price = (store->feat == FEAT_HOME) ? 0 : price_item(store, obj, true, 1);
        }
        slot_desc = equip_mention(player, i);
        if (slot_desc) strncpy(dst->slot_name, slot_desc, sizeof(dst->slot_name) - 1);
        object_desc(dst->name, sizeof(dst->name), obj, ODESC_PREFIX | ODESC_FULL, player);
        ++snapshot_inventory_count;
    }

    /* 2. Backpack items */
    if (player->upkeep && player->upkeep->inven) {
        for (i = 0; i < z_info->pack_size && snapshot_inventory_count < ABFP_MAX_INVENTORY_ITEMS; ++i) {
            struct object *obj = player->upkeep->inven[i];
            abfp_inventory_item *dst;
            if (!obj) continue;
            dst = &snapshot_inventory[snapshot_inventory_count];
            snapshot_inventory_ptrs[snapshot_inventory_count] = obj;
            memset(dst, 0, sizeof(*dst));
            dst->index = (int32_t)snapshot_inventory_count;
            dst->label = gear_to_label(player, obj);
            dst->equipped = 0;
            dst->quantity = obj->number;
            dst->category_id = obj->tval;
            dst->variant_id = obj->sval;
            if (obj_is_useable(obj)) dst->flags |= ABFP_ITEM_FLAG_USABLE;
            if (obj_can_wear(obj)) dst->flags |= ABFP_ITEM_FLAG_WEARABLE;
            if (tval_is_potion(obj)) dst->flags |= ABFP_ITEM_FLAG_POTION;
            if (tval_is_scroll(obj)) dst->flags |= ABFP_ITEM_FLAG_SCROLL;
            if (tval_is_edible(obj)) dst->flags |= ABFP_ITEM_FLAG_FOOD;
            if (obj_can_refill(obj)) dst->flags |= ABFP_ITEM_FLAG_FUEL;
            if (obj_can_fire(obj)) dst->flags |= ABFP_ITEM_FLAG_AMMO;
            if (obj_can_throw(obj)) dst->flags |= ABFP_ITEM_FLAG_THROWABLE;
            if (store && (store->feat == FEAT_HOME || store_will_buy_tester(obj))) {
                dst->flags |= ABFP_ITEM_FLAG_SELLABLE;
                dst->sell_price = (store->feat == FEAT_HOME) ? 0 : price_item(store, obj, true, 1);
            }
            object_desc(dst->name, sizeof(dst->name), obj, ODESC_PREFIX | ODESC_FULL, player);
            ++snapshot_inventory_count;
        }
    }

    /* 2b. Quiver ammunition */
    if (player->upkeep && player->upkeep->quiver) {
        for (i = 0; i < z_info->quiver_size && snapshot_inventory_count < ABFP_MAX_INVENTORY_ITEMS; ++i) {
            struct object *obj = player->upkeep->quiver[i];
            abfp_inventory_item *dst;
            if (!obj) continue;
            dst = &snapshot_inventory[snapshot_inventory_count];
            snapshot_inventory_ptrs[snapshot_inventory_count] = obj;
            memset(dst, 0, sizeof(*dst));
            dst->index = (int32_t)snapshot_inventory_count;
            dst->label = gear_to_label(player, obj);
            dst->equipped = 0;
            dst->quantity = obj->number;
            dst->category_id = obj->tval;
            dst->variant_id = obj->sval;
            dst->flags |= ABFP_ITEM_FLAG_QUIVER;
            if (obj_can_fire(obj)) dst->flags |= ABFP_ITEM_FLAG_AMMO;
            if (obj_can_throw(obj)) dst->flags |= ABFP_ITEM_FLAG_THROWABLE;
            if (store && (store->feat == FEAT_HOME || store_will_buy_tester(obj))) {
                dst->flags |= ABFP_ITEM_FLAG_SELLABLE;
                dst->sell_price = (store->feat == FEAT_HOME) ? 0 : price_item(store, obj, true, 1);
            }
            strncpy(dst->slot_name, "quiver", sizeof(dst->slot_name) - 1);
            object_desc(dst->name, sizeof(dst->name), obj, ODESC_PREFIX | ODESC_FULL, player);
            ++snapshot_inventory_count;
        }
    }

    /* 2c. Spells available from carried spellbooks */
    snapshot_spell_count = 0;
    if (player->class && player->class->magic.total_spells > 0 && player->spell_flags) {
        uint32_t inv_idx;
        for (inv_idx = 0; inv_idx < snapshot_inventory_count && snapshot_spell_count < ABFP_MAX_SPELLS; ++inv_idx) {
            struct object *obj = snapshot_inventory_ptrs[inv_idx];
            const struct class_book *book;
            int s;
            if (!obj || snapshot_inventory[inv_idx].equipped) continue;
            book = player_object_to_book(player, obj);
            if (!book) continue;
            for (s = 0; s < book->num_spells && snapshot_spell_count < ABFP_MAX_SPELLS; ++s) {
                const struct class_spell *sp = &book->spells[s];
                uint32_t k;
                bool dup = false;
                abfp_spell_entry *dst;
                if (sp->slevel >= 99) continue;
                for (k = 0; k < snapshot_spell_count; ++k) {
                    if (snapshot_spells[k].index == sp->sidx) { dup = true; break; }
                }
                if (dup) continue;
                dst = &snapshot_spells[snapshot_spell_count++];
                memset(dst, 0, sizeof(*dst));
                dst->index = sp->sidx;
                dst->book_index = (int32_t)inv_idx;
                dst->level = sp->slevel;
                dst->mana = sp->smana;
                dst->fail = (int32_t)spell_chance(sp->sidx);
                if (player->spell_flags[sp->sidx] & PY_SPELL_LEARNED)
                    dst->flags |= ABFP_SPELL_FLAG_LEARNED;
                if (spell_okay_to_cast(player, sp->sidx) && sp->smana <= player->csp &&
                    player_can_cast(player, false))
                    dst->flags |= ABFP_SPELL_FLAG_CAN_CAST;
                if (player->upkeep && player->upkeep->new_spells > 0 &&
                    spell_okay_to_study(player, sp->sidx) && player_can_study(player, false))
                    dst->flags |= ABFP_SPELL_FLAG_CAN_STUDY;
                if (spell_needs_aim(sp->sidx))
                    dst->flags |= ABFP_SPELL_FLAG_NEEDS_AIM;
                if (sp->name)
                    strncpy(dst->name, sp->name, sizeof(dst->name) - 1);
                get_spell_info(sp->sidx, dst->info, sizeof(dst->info));
            }
        }
    }

    /* 3. Active Shop stock when standing on a shop entrance */
    if (store) {
        struct object *stock_list[ABFP_MAX_SHOP_ITEMS];
        int max_items = z_info->store_inven_max;
        if (max_items > ABFP_MAX_SHOP_ITEMS) max_items = ABFP_MAX_SHOP_ITEMS;
        memset(stock_list, 0, sizeof(stock_list));
        store_stock_list(store, stock_list, max_items);

        snapshot_shop.active = 1;
        snapshot_shop.is_home = (store->feat == FEAT_HOME) ? 1 : 0;
        if (f_info[store->feat].name)
            strncpy(snapshot_shop.name, f_info[store->feat].name, sizeof(snapshot_shop.name) - 1);
        if (store->owner && store->owner->name)
            strncpy(snapshot_shop.owner, store->owner->name, sizeof(snapshot_shop.owner) - 1);

        snapshot_shop.item_count = 0;
        for (i = 0; i < max_items && stock_list[i] != NULL; ++i) {
            struct object *obj = stock_list[i];
            abfp_shop_item *si = &snapshot_shop.items[snapshot_shop.item_count];
            snapshot_shop_ptrs[snapshot_shop.item_count] = obj;
            memset(si, 0, sizeof(*si));
            si->index = (int32_t)snapshot_shop.item_count;
            si->quantity = obj->number;
            si->price = snapshot_shop.is_home ? 0 : price_item(store, obj, false, 1);
            si->category_id = obj->tval;
            si->variant_id = obj->sval;
            object_desc(si->name, sizeof(si->name), obj,
                ODESC_PREFIX | ODESC_FULL | ODESC_STORE, player);
            ++snapshot_shop.item_count;
        }
    }
}

void abfp_upstream_monster_died(const struct monster *mon)
{
    abfp_event event;
    if (!mon || !snapshot_history_valid) return;
    memset(&event, 0, sizeof(event));
    event.type = ABFP_EVENT_DEATH;
    event.turn = turn;
    event.subject_id = ABFP_MONSTER_ID_BASE | (uint64_t)mon->midx;
    event.from_x = event.to_x = mon->grid.x;
    event.from_y = event.to_y = mon->grid.y;
    event.value = mon->race ? mon->race->level : 0;
    abfp_bridge_emit(&event);
}

bool abfp_upstream_refresh_snapshot(void)
{
    abfp_snapshot next = {0};
    int dx, dy, index = 0;
    uint32_t entity_count = 0;

    if (!player || !cave) return false;

    for (dy = -ABFP_SNAPSHOT_RADIUS; dy <= ABFP_SNAPSHOT_RADIUS; ++dy) {
        for (dx = -ABFP_SNAPSHOT_RADIUS; dx <= ABFP_SNAPSHOT_RADIUS; ++dx) {
            struct loc grid = loc(player->grid.x + dx, player->grid.y + dy);
            abfp_cell *cell = &snapshot_cells[index++];
            bool in_bounds = square_in_bounds(cave, grid);

            cell->x = grid.x;
            cell->y = grid.y;
            cell->terrain = abfp_terrain_at(grid);
            cell->visible = in_bounds && square_isseen(cave, grid);
            cell->lit = in_bounds && square_islit(cave, grid);
            cell->occupied_by_monster = 0;
            cell->occupied_by_object = 0;

            if (cell->visible) {
                struct monster *mon = square_monster(cave, grid);
                struct object *obj = square_object(cave, grid);

                /* HP zero is Angband's lethal state, even if cleanup
                 * has not yet removed the monster from its cave square. */
                cell->occupied_by_monster = mon != NULL && mon->hp > 0;
                cell->occupied_by_object = obj != NULL;
                if (mon && mon->hp > 0)
                    abfp_append_monster(mon, grid.x, grid.y, &entity_count);
                abfp_append_objects(obj, grid.x, grid.y, &entity_count);
            }
        }
    }

    abfp_refresh_inventory_and_shop();

    if (pending_message_count > 0) {
        memcpy(snapshot_messages, pending_messages,
            pending_message_count * sizeof(*pending_messages));
        snapshot_message_count = pending_message_count;
        pending_message_count = 0;
    }

    next.turn = (uint64_t)turn;
    next.dungeon_depth = cave->depth;
    next.player_x = player->grid.x;
    next.player_y = player->grid.y;
    next.player.hitpoints = player->chp;
    next.player.max_hitpoints = player->mhp;
    next.player.level = player->lev;
    next.player.gold = player->au;
    next.player.mana = player->csp;
    next.player.max_mana = player->msp;
    next.player.is_dead = player->is_dead ? 1 : 0;
    next.player.exp = player->exp;
    next.player.max_exp = player->max_exp;
    next.player.armor_class = player->known_state.ac + player->known_state.to_a;
    next.player.speed = player->state.speed - 110;
    next.player.food_pct = (PY_FOOD_FULL > 0) ?
        (int32_t)((player->timed[TMD_FOOD] * 100) / PY_FOOD_FULL) : 100;
    next.player.new_spells = (player->upkeep) ? player->upkeep->new_spells : 0;
    next.player.can_choose_spells = player_has(player, PF_CHOOSE_SPELLS) ? 1 : 0;
    next.player.has_launcher = (equipped_item_by_slot_name(player, "shooting") != NULL &&
        player->state.ammo_tval != 0) ? 1 : 0;
    if (player->full_name[0])
        strncpy(next.player.name, player->full_name, sizeof(next.player.name) - 1);
    if (player->race && player->race->name)
        strncpy(next.player.race_name, player->race->name, sizeof(next.player.race_name) - 1);
    if (player->class && player->class->name)
        strncpy(next.player.class_name, player->class->name, sizeof(next.player.class_name) - 1);
    next.cells = snapshot_cells;
    next.cell_count = ABFP_SNAPSHOT_CELLS;
    next.entities = snapshot_entities;
    next.entity_count = entity_count;
    next.messages = snapshot_messages;
    next.message_count = snapshot_message_count;
    next.inventory = snapshot_inventory;
    next.inventory_count = snapshot_inventory_count;
    next.shop = snapshot_shop.active ? &snapshot_shop : NULL;
    next.spells = snapshot_spells;
    next.spell_count = snapshot_spell_count;
    next.birth = NULL;

    if (snapshot_history_valid && next.dungeon_depth == previous_depth) {
        uint32_t i, j;

        if (next.player.hitpoints < previous_player.hitpoints) {
            abfp_event event = {0};
            event.type = ABFP_EVENT_DAMAGE;
            event.turn = next.turn;
            event.target_id = 1;
            event.value = previous_player.hitpoints - next.player.hitpoints;
            abfp_bridge_emit(&event);
        }

        /* Emit terrain changes for previously known cells that changed feature
         * (e.g. closed door opened, door smashed, rubble tunneled, trap disarmed). */
        for (i = 0; i < ABFP_SNAPSHOT_CELLS; ++i) {
            const abfp_cell *current = &snapshot_cells[i];
            if (!current->visible || current->terrain == ABFP_TERRAIN_UNKNOWN) continue;
            for (j = 0; j < previous_cell_count; ++j) {
                const abfp_cell *previous = &previous_cells[j];
                if (previous->x == current->x && previous->y == current->y) {
                    if (previous->terrain != ABFP_TERRAIN_UNKNOWN &&
                        previous->terrain != current->terrain) {
                        abfp_event event = {0};
                        event.type = ABFP_EVENT_TERRAIN_CHANGED;
                        event.turn = next.turn;
                        event.from_x = event.to_x = current->x;
                        event.from_y = event.to_y = current->y;
                        event.value = (int32_t)current->terrain;
                        abfp_bridge_emit(&event);
                    }
                    break;
                }
            }
        }

        /*
         * Angband performs pickup as a side effect of walking onto a square
         * and waiting, as well as for the explicit AUTOPICKUP command.
         */
        if (pending_command_type == ABFP_COMMAND_AUTOPICKUP ||
            pending_command_type == ABFP_COMMAND_WAIT ||
            abfp_direction(pending_command_type) != DIR_UNKNOWN) {
            for (i = 0; i < previous_entity_count; ++i) {
                const abfp_entity_state *previous = &previous_entities[i];
                bool still_present = false;
                if (previous->type != ABFP_ENTITY_OBJECT ||
                    previous->x != next.player_x ||
                    previous->y != next.player_y) continue;
                for (j = 0; j < entity_count; ++j) {
                    if (snapshot_entities[j].id == previous->id) {
                        still_present = true;
                        break;
                    }
                }
                if (!still_present) {
                    abfp_event event = {0};
                    event.type = ABFP_EVENT_PICKUP;
                    event.turn = next.turn;
                    event.subject_id = 1;
                    event.target_id = previous->id;
                    event.from_x = previous->x;
                    event.from_y = previous->y;
                    event.to_x = previous->x;
                    event.to_y = previous->y;
                    event.value = previous->quantity;
                    abfp_bridge_emit(&event);
                }
            }
        }

        for (i = 0; i < entity_count; ++i) {
            const abfp_entity_state *current = &snapshot_entities[i];
            const abfp_entity_state *previous = NULL;
            for (j = 0; j < previous_entity_count; ++j) {
                if (previous_entities[j].id == current->id) {
                    previous = &previous_entities[j];
                    break;
                }
            }
            if (!previous || current->type != ABFP_ENTITY_MONSTER) continue;

            if (current->x != previous->x || current->y != previous->y) {
                abfp_event event = {0};
                event.type = ABFP_EVENT_ENTITY_MOVED;
                event.turn = next.turn;
                event.subject_id = current->id;
                event.from_x = previous->x;
                event.from_y = previous->y;
                event.to_x = current->x;
                event.to_y = current->y;
                abfp_bridge_emit(&event);
            }
            if (current->hitpoints < previous->hitpoints) {
                abfp_event event = {0};
                event.type = ABFP_EVENT_DAMAGE;
                event.turn = next.turn;
                event.target_id = current->id;
                event.from_x = current->x;
                event.from_y = current->y;
                event.to_x = current->x;
                event.to_y = current->y;
                event.value = previous->hitpoints - current->hitpoints;
                abfp_bridge_emit(&event);
            }
        }
    }

    memcpy(previous_cells, snapshot_cells, sizeof(snapshot_cells));
    previous_cell_count = ABFP_SNAPSHOT_CELLS;
    memcpy(previous_entities, snapshot_entities,
        entity_count * sizeof(*snapshot_entities));
    previous_entity_count = entity_count;
    previous_player = next.player;
    previous_depth = next.dungeon_depth;
    snapshot_history_valid = true;

    abfp_bridge_set_snapshot(&next);
    return true;
}

/*
 * The host calls this after Angband's run_game_loop() returns to its input
 * boundary. Any monster/world work scheduled before that boundary has run;
 * blocked moves are also acknowledged here.
 */
void abfp_upstream_sync_after_game_loop(void)
{
    if (player && cave) {
        if (pending_attack) {
            abfp_event event = {0};
            event.type = ABFP_EVENT_ATTACK;
            event.subject_id = 1;
            event.target_id = pending_attack_target_id;
            event.from_x = pending_from.x;
            event.from_y = pending_from.y;
            event.to_x = pending_attack_target.x;
            event.to_y = pending_attack_target.y;
            event.turn = (uint64_t)turn;
            abfp_bridge_emit(&event);
        }
        if (pending_move && observed_player_moved &&
            (player->grid.x != pending_from.x ||
             player->grid.y != pending_from.y)) {
            abfp_event event = {0};
            event.type = ABFP_EVENT_ENTITY_MOVED;
            event.subject_id = 1;
            event.from_x = pending_from.x;
            event.from_y = pending_from.y;
            event.to_x = player->grid.x;
            event.to_y = player->grid.y;
            event.turn = (uint64_t)turn;
            abfp_bridge_emit(&event);
        }
        abfp_flush_projectile_events();
        if (pending_message_count == 0) {
            snapshot_message_count = 0;
        }
        (void)abfp_upstream_refresh_snapshot();
    }

    pending_move = false;
    pending_attack = false;
    pending_attack_target_id = 0;
    pending_bolt_active = false;
    pending_missile_active = false;
    pending_explosion_active = false;
    pending_command = false;
    pending_command_type = ABFP_COMMAND_WAIT;
    observed_player_moved = false;
}
