#include "../include/abfp_upstream_adapter.h"

/* Compiled against the peeled Angband 4.2.6 release commit:
 * f3082213b73f3e463e3d0d60bff4b00462beae6e
 */
#include "angband.h"
#include "cave.h"
#include "cmd-core.h"
#include "game-event.h"
#include "object.h"

#define ABFP_SNAPSHOT_RADIUS 10
#define ABFP_SNAPSHOT_SIDE (ABFP_SNAPSHOT_RADIUS * 2 + 1)
#define ABFP_SNAPSHOT_CELLS (ABFP_SNAPSHOT_SIDE * ABFP_SNAPSHOT_SIDE)

static bool handlers_installed;
static struct loc pending_from;
static bool pending_move;
static abfp_cell snapshot_cells[ABFP_SNAPSHOT_CELLS];

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
    abfp_event event = {0};
    (void)type;
    (void)data;
    (void)user;

    event.type = ABFP_EVENT_ENTITY_MOVED;
    event.subject_id = 1;
    event.from_x = pending_move ? pending_from.x : player->grid.x;
    event.from_y = pending_move ? pending_from.y : player->grid.y;
    event.to_x = player->grid.x;
    event.to_y = player->grid.y;
    event.turn = cave ? (uint64_t)cave->turn : 0;
    abfp_bridge_emit(&event);
    pending_move = false;
    (void)abfp_upstream_refresh_snapshot();
}

bool abfp_upstream_install_handlers(void)
{
    if (handlers_installed) return true;
    event_add_handler(EVENT_PLAYERMOVED, abfp_player_moved, NULL);
    handlers_installed = true;
    /* A frontend may register before character creation or dungeon entry. */
    (void)abfp_upstream_refresh_snapshot();
    return true;
}

void abfp_upstream_remove_handlers(void)
{
    if (!handlers_installed) return;
    event_remove_handler(EVENT_PLAYERMOVED, abfp_player_moved, NULL);
    handlers_installed = false;
}

bool abfp_upstream_queue_command(abfp_command_type command)
{
    int dir = abfp_direction(command);

    if (!player || !cave) return false;

    if (dir != DIR_UNKNOWN) {
        if (cmdq_push(CMD_WALK) != 0) return false;
        cmd_set_arg_direction(cmdq_peek(), "direction", dir);
        pending_from = player->grid;
        pending_move = true;
        return true;
    }

    if (command == ABFP_COMMAND_WAIT)
        return cmdq_push(CMD_HOLD) == 0;

    return false;
}

static abfp_terrain_type abfp_terrain_at(struct loc grid)
{
    if (!square_in_bounds(cave, grid) || !square_isknown(cave, grid))
        return ABFP_TERRAIN_UNKNOWN;

    if (square_isupstairs(cave, grid)) return ABFP_TERRAIN_STAIRS_UP;
    if (square_isdownstairs(cave, grid)) return ABFP_TERRAIN_STAIRS_DOWN;
    if (square_isdoor(cave, grid)) return ABFP_TERRAIN_DOOR;
    if (square_isfloor(cave, grid)) return ABFP_TERRAIN_FLOOR;
    if (square_isrock(cave, grid)) return ABFP_TERRAIN_WALL;

    /* Other known non-floor features (rubble, traps, shops) are solid to
     * the initial renderer. A later terrain-id extension will distinguish them. */
    return ABFP_TERRAIN_WALL;
}

bool abfp_upstream_refresh_snapshot(void)
{
    abfp_snapshot next = {0};
    int dx, dy, index = 0;

    if (!player || !cave) return false;

    for (dy = -ABFP_SNAPSHOT_RADIUS; dy <= ABFP_SNAPSHOT_RADIUS; ++dy) {
        for (dx = -ABFP_SNAPSHOT_RADIUS; dx <= ABFP_SNAPSHOT_RADIUS; ++dx) {
            struct loc grid = loc(player->grid.x + dx, player->grid.y + dy);
            abfp_cell *cell = &snapshot_cells[index++];

            cell->x = grid.x;
            cell->y = grid.y;
            cell->terrain = abfp_terrain_at(grid);
            cell->visible = square_in_bounds(cave, grid) &&
                square_isseen(cave, grid);
            cell->lit = square_in_bounds(cave, grid) &&
                square_islit(cave, grid);

            /* Never leak hidden monster/object occupancy through remembered
             * terrain. Only currently visible squares expose these flags. */
            if (cell->visible) {
                cell->occupied_by_monster =
                    square_monster(cave, grid) != NULL;
                cell->occupied_by_object =
                    square_object(cave, grid) != NULL;
            } else {
                cell->occupied_by_monster = 0;
                cell->occupied_by_object = 0;
            }
        }
    }

    next.turn = (uint64_t)cave->turn;
    next.dungeon_depth = cave->depth;
    next.player_x = player->grid.x;
    next.player_y = player->grid.y;
    next.cells = snapshot_cells;
    next.cell_count = ABFP_SNAPSHOT_CELLS;
    next.entities = NULL;
    next.entity_count = 0;

    abfp_bridge_set_snapshot(&next);
    return true;
}
