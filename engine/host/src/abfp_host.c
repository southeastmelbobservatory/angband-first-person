#include "angband.h"
#include "cmd-core.h"
#include "ui-game.h"
#include "abfp_frontend.h"
#include "abfp_transport.h"
#include "abfp_host.h"

static bool bridge_registered;

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

void abfp_host_game_closed(void)
{
    if (bridge_registered) {
        abfp_bridge_shutdown();
        bridge_registered = false;
    }
}

void abfp_host_shutdown(void)
{
    abfp_transport_stop();
    abfp_host_game_closed();
}
