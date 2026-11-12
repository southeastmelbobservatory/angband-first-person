#include "angband.h"
#include "cmd-core.h"
#include "option.h"
#include "ui-game.h"
#include "abfp_frontend.h"
#include "abfp_transport.h"
#include "abfp_host.h"
#include <stdlib.h>

static bool bridge_registered;
static bool initial_state_sent;

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
        /* CI-only: exercise Angband's own path-to-stairs behaviour. */
        if (getenv("ABFP_TEST_AUTOEXPLORE_STAIRS"))
            OPT(player, autoexplore_commands) = true;
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
    const abfp_snapshot *snapshot = abfp_frontend_snapshot();
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
}

void abfp_host_shutdown(void)
{
    abfp_transport_stop();
    abfp_host_game_closed();
}
