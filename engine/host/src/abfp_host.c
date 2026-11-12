#include "angband.h"
#include "cmd-core.h"
#include "ui-game.h"
#include "abfp_frontend.h"
#include "abfp_transport.h"
#include "abfp_host.h"
#include <stdlib.h>
#include <stdio.h>

static bool bridge_registered;
static bool initial_state_sent;

static errr abfp_host_get_command(cmd_context context)
{
    abfp_command_type command;

    fprintf(stderr, "ABFP: command hook entered (context=%d)\n", (int)context);
    fflush(stderr);

    /* Birth, store, death and other non-game contexts retain the text UI. */
    if (context != CTX_GAME) return textui_get_cmd(context);

    if (!bridge_registered) {
        fprintf(stderr, "ABFP: registering bridge\n"); fflush(stderr);
        if (!abfp_frontend_register()) {
            fprintf(stderr, "ABFP: bridge registration failed\n"); fflush(stderr);
            return -1;
        }
        bridge_registered = true;
        fprintf(stderr, "ABFP: bridge registered\n"); fflush(stderr);
    }

    while (1) {
        fprintf(stderr, "ABFP: waiting for client\n"); fflush(stderr);
        if (!abfp_transport_wait_for_client()) return -1;
        fprintf(stderr, "ABFP: client connected\n"); fflush(stderr);
        if (!initial_state_sent) {
            initial_state_sent = abfp_host_publish_state(true);
            fprintf(stderr, "ABFP: initial snapshot publish %s\n", initial_state_sent ? "succeeded" : "FAILED");
            fflush(stderr);
        }
        fprintf(stderr, "ABFP: waiting for command\n"); fflush(stderr);
        if (!abfp_transport_wait_command(&command)) {
            fprintf(stderr, "ABFP: client disconnected while waiting\n"); fflush(stderr);
            initial_state_sent = false;
            continue;
        }
        fprintf(stderr, "ABFP: command received (%d)\n", (int)command); fflush(stderr);
        if (abfp_frontend_submit_command(command)) return 0;
        fprintf(stderr, "ABFP: command rejected\n"); fflush(stderr);
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
