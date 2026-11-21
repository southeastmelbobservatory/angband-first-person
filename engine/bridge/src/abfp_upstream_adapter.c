#include "../include/abfp_upstream_adapter.h"

/* Compiled against the peeled Angband 4.2.6 release commit:
 * f3082213b73f3e463e3d0d60bff4b00462beae6e
 */
#include "angband.h"
#include "cave.h"
#include "cmd-core.h"
#include "game-event.h"
#include "game-world.h"
#include "monster.h"
#include "object.h"
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

bool abfp_upstream_install_handlers(void)
{
    if (handlers_installed) return true;
    event_add_handler(EVENT_PLAYERMOVED, abfp_player_moved, NULL);
    event_add_handler(EVENT_MESSAGE, abfp_message_received, NULL);
    handlers_installed = true;
    (void)abfp_upstream_refresh_snapshot();
    return true;
}

void abfp_upstream_remove_handlers(void)
{
    if (!handlers_installed) return;
    event_remove_handler(EVENT_PLAYERMOVED, abfp_player_moved, NULL);
    event_remove_handler(EVENT_MESSAGE, abfp_message_received, NULL);
    handlers_installed = false;
    snapshot_history_valid = false;
    previous_cell_count = 0;
    previous_entity_count = 0;
    pending_message_count = 0;
    snapshot_message_count = 0;
    previous_depth = 0;
    pending_move = false;
    pending_attack = false;
    pending_attack_target_id = 0;
    pending_command = false;
    pending_command_type = ABFP_COMMAND_WAIT;
    observed_player_moved = false;
}

bool abfp_upstream_queue_command(abfp_command_type command)
{
    int dir = abfp_direction(command);
    int alter_dir = abfp_alter_direction(command);

    if (!player || !cave || pending_command) return false;

    pending_message_count = 0;
    pending_attack = false;
    pending_attack_target_id = 0;

    if (dir != DIR_UNKNOWN) {
        struct loc target = loc_sum(player->grid, ddgrid[dir]);
        struct monster *mon = square_in_bounds(cave, target) ?
            square_monster(cave, target) : NULL;
        /*
         * Use Angband's explicit alter action for an occupied monster grid.
         * It calls py_attack() directly and avoids the walk-test's visibility
         * gate; ordinary empty-square movement remains CMD_WALK.
         */
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

    if (command == ABFP_COMMAND_AUTOPICKUP) {
        if (cmdq_push(CMD_AUTOPICKUP) != 0) return false;
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

    return false;
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
        if (known && known->kind && known->kind->name)
            strncpy(entity->name, known->kind->name, sizeof(entity->name) - 1);

        entity->id = ABFP_OBJECT_ID_BASE | (uint64_t)obj->oidx;
        entity->x = x;
        entity->y = y;
        entity->type = ABFP_ENTITY_OBJECT;
        entity->kind_id = (known && known->kind) ?
            (int32_t)known->kind->kidx : 0;
        entity->category_id = (known && known->kind) ? known->tval : 0;
        entity->variant_id = (known && known->kind) ? known->sval : 0;
        entity->hitpoints = 0;
        entity->max_hitpoints = 0;
        entity->quantity = (known && known->kind) ? known->number : 1;
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
    next.cells = snapshot_cells;
    next.cell_count = ABFP_SNAPSHOT_CELLS;
    next.entities = snapshot_entities;
    next.entity_count = entity_count;
    next.messages = snapshot_messages;
    next.message_count = snapshot_message_count;

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
        if (pending_message_count == 0) {
            snapshot_message_count = 0;
        }
        (void)abfp_upstream_refresh_snapshot();
    }

    pending_move = false;
    pending_attack = false;
    pending_attack_target_id = 0;
    pending_command = false;
    pending_command_type = ABFP_COMMAND_WAIT;
    observed_player_moved = false;
}
