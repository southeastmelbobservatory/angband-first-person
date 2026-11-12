#include "../include/abfp_upstream_adapter.h"

/* Compiled against the peeled Angband 4.2.6 release commit:
 * f3082213b73f3e463e3d0d60bff4b00462beae6e
 */
#include "angband.h"
#include "cave.h"
#include "cmd-core.h"
#include "game-event.h"
#include "monster.h"
#include "object.h"

#define ABFP_SNAPSHOT_RADIUS 10
#define ABFP_SNAPSHOT_SIDE (ABFP_SNAPSHOT_RADIUS * 2 + 1)
#define ABFP_SNAPSHOT_CELLS (ABFP_SNAPSHOT_SIDE * ABFP_SNAPSHOT_SIDE)
#define ABFP_MAX_SNAPSHOT_ENTITIES 1024

#define ABFP_MONSTER_ID_BASE UINT64_C(0x1000000000000000)
#define ABFP_OBJECT_ID_BASE  UINT64_C(0x2000000000000000)

static bool handlers_installed;
static struct loc pending_from;
static bool pending_move;
static bool pending_command;
static bool observed_player_moved;
static abfp_cell snapshot_cells[ABFP_SNAPSHOT_CELLS];
static abfp_entity_state snapshot_entities[ABFP_MAX_SNAPSHOT_ENTITIES];

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

static void abfp_player_moved(game_event_type type,
    game_event_data *data, void *user)
{
    (void)type;
    (void)data;
    (void)user;
    observed_player_moved = true;
}

bool abfp_upstream_install_handlers(void)
{
    if (handlers_installed) return true;
    event_add_handler(EVENT_PLAYERMOVED, abfp_player_moved, NULL);
    handlers_installed = true;
    (void)abfp_upstream_refresh_snapshot();
    return true;
}

void abfp_upstream_remove_handlers(void)
{
    if (!handlers_installed) return;
    event_remove_handler(EVENT_PLAYERMOVED, abfp_player_moved, NULL);
    handlers_installed = false;
    pending_move = false;
    pending_command = false;
    observed_player_moved = false;
}

bool abfp_upstream_queue_command(abfp_command_type command)
{
    int dir = abfp_direction(command);

    if (!player || !cave || pending_command) return false;

    if (dir != DIR_UNKNOWN) {
        if (cmdq_push(CMD_WALK) != 0) return false;
        cmd_set_arg_direction(cmdq_peek(), "direction", dir);
        pending_from = player->grid;
        pending_move = true;
        pending_command = true;
        observed_player_moved = false;
        return true;
    }

    if (command == ABFP_COMMAND_WAIT) {
        if (cmdq_push(CMD_HOLD) != 0) return false;
        pending_command = true;
        return true;
    }

    return false;
}

static abfp_terrain_type abfp_terrain_at(struct loc grid)
{
    if (!square_in_bounds(cave, grid) || !square_isknown(cave, grid))
        return ABFP_TERRAIN_UNKNOWN;

    if (square_isupstairs(cave, grid)) return ABFP_TERRAIN_STAIRS_UP;
    if (square_isdownstairs(cave, grid)) return ABFP_TERRAIN_STAIRS_DOWN;
    if (square_isdoor(cave, grid)) return ABFP_TERRAIN_DOOR;
    if (square_isrubble(cave, grid)) return ABFP_TERRAIN_RUBBLE;
    if (square_isshop(cave, grid)) return ABFP_TERRAIN_SHOP;
    if (square_isfloor(cave, grid)) return ABFP_TERRAIN_FLOOR;
    if (square_isrock(cave, grid)) return ABFP_TERRAIN_WALL;

    return ABFP_TERRAIN_WALL;
}

static void abfp_append_monster(struct monster *mon, int x, int y,
    uint32_t *entity_count)
{
    abfp_entity_state *entity;

    if (!mon || !mon->race ||
        *entity_count >= ABFP_MAX_SNAPSHOT_ENTITIES) return;

    entity = &snapshot_entities[(*entity_count)++];
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

                cell->occupied_by_monster = mon != NULL;
                cell->occupied_by_object = obj != NULL;
                abfp_append_monster(mon, grid.x, grid.y, &entity_count);
                abfp_append_objects(obj, grid.x, grid.y, &entity_count);
            }
        }
    }

    next.turn = (uint64_t)cave->turn;
    next.dungeon_depth = cave->depth;
    next.player_x = player->grid.x;
    next.player_y = player->grid.y;
    next.cells = snapshot_cells;
    next.cell_count = ABFP_SNAPSHOT_CELLS;
    next.entities = snapshot_entities;
    next.entity_count = entity_count;

    abfp_bridge_set_snapshot(&next);
    return true;
}

/*
 * The host calls this after Angband's run_game_loop() returns. That boundary
 * is later than EVENT_PLAYERMOVED: monsters and world processing have had
 * their normal opportunity to act, and blocked moves are also acknowledged.
 */
void abfp_upstream_sync_after_game_loop(void)
{
    if (player && cave) {
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
            event.turn = (uint64_t)cave->turn;
            abfp_bridge_emit(&event);
        }
        (void)abfp_upstream_refresh_snapshot();
    }

    pending_move = false;
    pending_command = false;
    observed_player_moved = false;
}
