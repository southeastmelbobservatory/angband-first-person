#include "angband.h"
#include "cmd-core.h"
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

    if (!bridge_registered) {
        if (!abfp_frontend_register()) return -1;
        bridge_registered = true;
    }

    while (abfp_transport_wait_command(&command)) {
        if (!initial_state_sent) {
            abfp_host_publish_state(true);
            initial_state_sent = true;
        }
        if (abfp_frontend_submit_command(command)) return 0;
    }
    return -1;
}

bool abfp_host_install(uint16_t port)
{
    if (!abfp_transport_start(port)) return false;
    cmd_get_hook = abfp_host_get_command;
    return true;
}

void abfp_host_publish_state(bool initial_state)
{
    const abfp_snapshot *snapshot = abfp_frontend_snapshot();
    size_t count = abfp_bridge_event_count();
    abfp_event *events = count ? (abfp_event *)malloc(count * sizeof(*events)) : NULL;
    size_t i;

    if (count && !events) {
        abfp_bridge_clear_events();
        return;
    }
    for (i = 0; i < count; ++i) {
        const abfp_event *event = abfp_bridge_event_at(i);
        if (event) events[i] = *event;
    }
    (void)abfp_transport_publish_state(snapshot, events, count, initial_state);
    free(events);
    abfp_bridge_clear_events();
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
