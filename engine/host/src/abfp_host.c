#include "angband.h"
#include "cmd-core.h"
#include "option.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "mon-make.h"
#include "mon-util.h"
#include "trap.h"
#include "ui-game.h"
#include "abfp_frontend.h"
#include "abfp_transport.h"
#include "abfp_host.h"
#include <stdlib.h>

static bool bridge_registered;
static bool initial_state_sent;
static bool test_floor_item_seeded;
static bool test_down_stair_seeded;
static bool test_monster_seeded;
static bool test_monster_attacked;
static bool test_monster_killed;
static int test_monster_midx;
static bool test_item_grid_valid;
static struct loc test_item_grid;
static bool test_stair_grid_valid;
static struct loc test_stair_grid;

static bool abfp_host_seed_test_floor_item(void)
{
    const int sval = lookup_sval(TV_FOOD, "Ration of Food");
    struct object_kind *kind;
    struct object *object;
    struct object *known;
    struct loc item_grid = loc(0, 0);
    bool found_grid = false;
    bool note = true;
    static const int dx[] = { 0, 1, 0, -1, 1, 1, -1, -1 };
    static const int dy[] = { -1, 0, 1, 0, -1, 1, 1, -1 };
    size_t direction;

    if (sval < 0) return false;
    kind = lookup_kind(TV_FOOD, sval);
    if (!kind) return false;

    /*
     * Prefer a naturally open adjacent floor square. If the generated
     * arrival tile is completely enclosed, reserve one adjacent in-bounds
     * square as floor for this CI-only ration fixture rather than failing
     * the entire first dungeon snapshot.
     */
    for (direction = 0; direction < sizeof(dx) / sizeof(dx[0]); ++direction) {
        struct loc candidate = loc(player->grid.x + dx[direction],
            player->grid.y + dy[direction]);
        if (!square_in_bounds_fully(cave, candidate) ||
            square_isoccupied(cave, candidate) ||
            square_object(cave, candidate)) continue;
        if (!found_grid) {
            item_grid = candidate;
            found_grid = true;
        }
        if (square_isobjectholding(cave, candidate) &&
            square_ispassable(cave, candidate)) {
            item_grid = candidate;
            break;
        }
    }
    if (!found_grid) return false;
    /* The fixture path must not accidentally trigger a generated trap/web. */
    square_remove_all_traps(cave, player->grid);
    square_remove_all_traps(cave, item_grid);
    if (!square_isobjectholding(cave, item_grid) ||
        !square_ispassable(cave, item_grid)) {
        square_set_feat(cave, item_grid, FEAT_FLOOR);
        square_memorize(cave, item_grid);
        square_light_spot(cave, item_grid);
    }

    object = object_new();
    known = object_new();
    if (!object || !known) {
        if (object) object_free(object);
        if (known) object_free(known);
        return false;
    }

    object_prep(object, kind, cave->depth, AVERAGE);
    object_prep(known, kind, cave->depth, AVERAGE);
    object->known = known;
    if (!floor_carry(cave, item_grid, object, &note)) {
        object->known = NULL;
        object_free(known);
        object_free(object);
        return false;
    }

    test_item_grid = item_grid;
    test_item_grid_valid = true;
    test_floor_item_seeded = true;
    return true;
}

static bool abfp_host_move_target(abfp_command_type command,
    struct loc *target)
{
    int dx = 0, dy = 0;
    if (!target || !player) return false;
    switch (command) {
    case ABFP_COMMAND_MOVE_NORTH: dy = -1; break;
    case ABFP_COMMAND_MOVE_SOUTH: dy = 1; break;
    case ABFP_COMMAND_MOVE_WEST: dx = -1; break;
    case ABFP_COMMAND_MOVE_EAST: dx = 1; break;
    case ABFP_COMMAND_MOVE_NORTH_WEST: dx = -1; dy = -1; break;
    case ABFP_COMMAND_MOVE_NORTH_EAST: dx = 1; dy = -1; break;
    case ABFP_COMMAND_MOVE_SOUTH_WEST: dx = -1; dy = 1; break;
    case ABFP_COMMAND_MOVE_SOUTH_EAST: dx = 1; dy = 1; break;
    default: return false;
    }
    *target = loc(player->grid.x + dx, player->grid.y + dy);
    return true;
}

static bool abfp_host_seed_test_monster(void)
{
    struct monster_race *race = lookup_monster("giant white mouse");
    struct monster_group_info group_info = {0};
    struct monster *mon;
    static const int dx[] = { 0, 1, 0, -1, 1, 1, -1, -1 };
    static const int dy[] = { -1, 0, 1, 0, -1, 1, 1, -1 };
    size_t direction;

    if (!race || !test_item_grid_valid ||
        !square_in_bounds_fully(cave, test_item_grid) ||
        !square_is_monster_walkable(cave, test_item_grid) ||
        !square_isempty(cave, test_item_grid)) return false;
    if (!place_new_monster(cave, test_item_grid, race, false, false,
            group_info, ORIGIN_DROP_SPECIAL)) return false;

    mon = square_monster(cave, test_item_grid);
    if (!mon) return false;
    /* Deterministic target; Angband still resolves its real AI and combat. */
    mon->hp = 1;
    mon->maxhp = 1;
    monster_wake(mon, true, 100);

    /*
     * Ensure the player has an open neighbouring tile to step away onto.
     * This lets the normal Angband monster phase pursue the player and
     * gives the bridge a visible before/after pair for its movement event.
     */
    for (direction = 0; direction < sizeof(dx) / sizeof(dx[0]); ++direction) {
        struct loc approach = loc(test_stair_grid.x + dx[direction],
            test_stair_grid.y + dy[direction]);
        if (!square_in_bounds_fully(cave, approach) ||
            loc_eq(approach, test_item_grid) ||
            square_isoccupied(cave, approach) ||
            square_object(cave, approach)) continue;
        square_remove_all_traps(cave, approach);
        square_set_feat(cave, approach, FEAT_FLOOR);
        square_memorize(cave, approach);
        square_light_spot(cave, approach);
    }

    update_monsters(true);
    square_light_spot(cave, test_item_grid);
    test_monster_seeded = true;
    return true;
}

static errr abfp_host_get_command(cmd_context context)
{
    abfp_command_type command;

    /* Birth, store, death and other non-game contexts retain the text UI. */
    if (context != CTX_GAME) return textui_get_cmd(context);

    /*
     * The 3D client receives authoritative state/events instead of reading
     * Angband's message line. Do not block the game thread on a terminal
     * "-more-" acknowledgement after messages such as "no staircase here".
     */
    if (player) {
        OPT(player, auto_more) = true;
        if (getenv("ABFP_TEST_FLOOR_ITEM"))
            OPT(player, pickup_always) = true;
    }

    if (!bridge_registered) {
        if (!abfp_frontend_register()) {
            return -1;
        }
        bridge_registered = true;
    }

    while (1) {
        if (!abfp_transport_wait_for_client()) return -1;
        if (!initial_state_sent) {
            initial_state_sent = abfp_host_publish_state(true);
        }
        if (!abfp_transport_wait_command(&command)) {
            initial_state_sent = false;
            continue;
        }

        /*
         * CI-only fixture: put a real Angband down-stair under the player
         * immediately before the native GO_DOWN command is queued. The
         * command, level generation and return trip remain normal Angband.
         */
        if (getenv("ABFP_TEST_FORCE_STAIRS") &&
            command == ABFP_COMMAND_GO_DOWN && !test_down_stair_seeded &&
            player && cave && cave->depth == 0) {
            square_set_feat(cave, player->grid, FEAT_MORE);
            square_memorize(cave, player->grid);
            square_light_spot(cave, player->grid);
            test_down_stair_seeded = true;
            (void)abfp_frontend_refresh_snapshot();
        }

        if (getenv("ABFP_TEST_MONSTER") &&
            command == ABFP_COMMAND_WAIT && !test_monster_seeded &&
            player && cave && cave->depth > 0 && test_stair_grid_valid &&
            test_item_grid_valid && !square_object(cave, test_item_grid) &&
            loc_eq(player->grid, test_stair_grid)) {
            if (!abfp_host_seed_test_monster() ||
                !abfp_frontend_refresh_snapshot()) return -1;
        }

        if (test_monster_seeded && test_monster_midx > 0) {
            struct loc target;
            struct monster *mon = cave_monster(cave, test_monster_midx);
            if (mon && abfp_host_move_target(command, &target) &&
                loc_eq(target, mon->grid))
                test_monster_attacked = true;
        }
        if (getenv("ABFP_TEST_KILL_MONSTER") &&
            command == ABFP_COMMAND_WAIT && test_monster_attacked &&
            !test_monster_killed) {
            struct monster *mon = test_monster_midx > 0 ?
                cave_monster(cave, test_monster_midx) : NULL;
            if (mon) {
                bool fear = false;
                (void)mon_take_hit(mon, player, mon->hp, &fear, NULL);
            }
            test_monster_killed = true;
        }

        if (abfp_frontend_submit_command(command)) return 0;
    }
}

bool abfp_host_install(uint16_t port)
{
    if (!abfp_transport_start(port)) return false;
    cmd_get_hook = abfp_host_get_command;
    return true;
}

/*
 * Retain queued events if delivery fails (for example, a client disconnects
 * between command submission and the post-turn publish). The next client
 * receives the latest full snapshot plus those still-pending events.
 */
bool abfp_host_publish_state(bool initial_state)
{
    const abfp_snapshot *snapshot;
    if (getenv("ABFP_TEST_MONSTER") && !test_stair_grid_valid &&
        player && cave && cave->depth > 0) {
        test_stair_grid = player->grid;
        test_stair_grid_valid = true;
    }
    if (getenv("ABFP_TEST_FLOOR_ITEM") && !test_floor_item_seeded &&
        player && cave && cave->depth > 0) {
        if (!abfp_host_seed_test_floor_item() ||
            !abfp_frontend_refresh_snapshot()) return false;
    }
    snapshot = abfp_frontend_snapshot();
    size_t count = abfp_bridge_event_count();
    abfp_event *events = count ? (abfp_event *)malloc(count * sizeof(*events)) : NULL;
    size_t i;
    bool published;

    if (count && !events) return false;
    for (i = 0; i < count; ++i) {
        const abfp_event *event = abfp_bridge_event_at(i);
        if (!event) {
            free(events);
            return false;
        }
        events[i] = *event;
    }
    published = abfp_transport_publish_state(snapshot, events, count, initial_state);
    free(events);
    if (published) abfp_bridge_clear_events();
    return published;
}

void abfp_host_game_closed(void)
{
    if (bridge_registered) {
        abfp_bridge_shutdown();
        bridge_registered = false;
    }
    initial_state_sent = false;
    test_floor_item_seeded = false;
    test_down_stair_seeded = false;
    test_monster_seeded = false;
    test_monster_attacked = false;
    test_monster_killed = false;
    test_monster_midx = 0;
    test_item_grid_valid = false;
    test_stair_grid_valid = false;
}

void abfp_host_shutdown(void)
{
    abfp_transport_stop();
    abfp_host_game_closed();
}
