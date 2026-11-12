#include "../include/abfp_frontend.h"
#include "../include/abfp_adapter.h"

/*
 * The host owns and advances Angband's normal game loop. This facade never
 * runs a second simulation and must be called on Angband's game thread.
 */
bool abfp_frontend_register(void)
{
    return abfp_adapter_init();
}

bool abfp_frontend_submit_command(abfp_command_type command)
{
    return abfp_adapter_execute(command);
}

const abfp_snapshot *abfp_frontend_snapshot(void)
{
    return abfp_bridge_snapshot();
}

bool abfp_frontend_refresh_snapshot(void)
{
    return abfp_adapter_refresh_snapshot();
}

void abfp_frontend_after_game_loop(void)
{
    abfp_adapter_sync_after_game_loop();
}

void abfp_frontend_begin_frame(void) {}
void abfp_frontend_end_frame(void) {}
