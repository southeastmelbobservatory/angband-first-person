#include "../include/abfp_upstream_adapter.h"

/* Pinned against 42cd156ac9f95ddf4311e88dd7ad462292d6d1d6 */
#include "angband.h"
#include "cave.h"
#include "cmd-core.h"
#include "game-event.h"

static bool handlers_installed = false;

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
    event.to_x = player->grid.x;
    event.to_y = player->grid.y;
    abfp_bridge_emit(&event);
}

bool abfp_upstream_install_handlers(void)
{
    if (handlers_installed) return true;
    event_add_handler(EVENT_PLAYERMOVED, abfp_player_moved, NULL);
    handlers_installed = true;
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
    if (dir != DIR_UNKNOWN) {
        if (cmdq_push(CMD_WALK)) return false;
        cmd_set_arg_direction(cmdq_peek(), "direction", dir);
        return true;
    }
    if (command == ABFP_COMMAND_WAIT) return cmdq_push(CMD_HOLD) == 0;
    return false;
}
