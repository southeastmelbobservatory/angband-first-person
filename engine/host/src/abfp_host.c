#include "angband.h"
#include "cmd-core.h"
#include "option.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "mon-make.h"
#include "mon-util.h"
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
static bool test_item_grid_valid;
static struct loc test_item_grid;

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

    for (direction = 0; direction < sizeof(dx) / sizeof(dx[0]); ++direction) {
        struct loc candidate = loc(player->grid.x + dx[direction],
            player->grid.y + dy[direction]);
        if (!square_in_bounds_fully(cave, candidate) ||
            !square_isobjectholding(cave, candidate) ||
            !square_ispassable(cave, candidate) ||
            square_isoccupied(cave, candidate)) continue;
        item_grid = candidate;
        found_grid = true;
        break;
    }
    if (!found_grid) return false;

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

static bool abfp_host_seed_test_monster(void)
{
    struct monster_race *race = lookup_monster("giant white mouse");
    struct monster_group_info group_info = {0};
    static const int dx[] = { 0, 1, 0, -1 };
    static const int dy[] = { -1, 0, 1, 0 };
    size_t direction;

    if (!race) return false;
    for (direction = 0; direction < sizeof(dx) / sizeof(dx[0]); ++direction) {
        struct loc middle = loc(player->grid.x + dx[direction],
            player->grid.y + dy[direction]);
        struct loc candidate = loc(player->grid.x + 2 * dx[direction],
            player->grid.y + 2 * dy[direction]);
        if (!square_in_bounds_fully(cave, middle) ||
            !square_ispassable(cave, middle) ||
            !square_in_bounds_fully(cave, candidate) ||
            !square_is_monster_walkable(cave, candidate) ||
            square_isoccupied(cave, candidate)) continue;
        if (!place_new_monster(cave, candidate, race, false, false,
                group_info, ORIGIN_DROP_SPECIAL)) continue;
        update_monsters(true);
        square_light_spot(cave, candidate);
        test_monster_seeded = true;
        return true;
    }
    return false;
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
    if (getenv("ABFP_TEST_FLOOR_ITEM") && !test_floor_item_seeded &&
        player && cave && cave->depth > 0) {
        if (!abfp_host_seed_test_floor_item() ||
            !abfp_frontend_refresh_snapshot()) return false;
    }
    if (getenv("ABFP_TEST_MONSTER") && !test_monster_seeded &&
        player && cave && cave->depth > 0) {
        if (!abfp_host_seed_test_monster() ||
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
    test_item_grid_valid = false;
}

void abfp_host_shutdown(void)
{
    abfp_transport_stop();
    abfp_host_game_closed();
}
