#pragma once

#include <stdbool.h>
#include "../include/abfp_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize once after Angband has initialized its game modules. */
bool abfp_frontend_register(void);

/* Queue one authoritative Angband command. Call on the Angband game thread. */
bool abfp_frontend_submit_command(abfp_command_type command);

/* Read the latest bridge snapshot. Copy its arrays before the next sync. */
const abfp_snapshot *abfp_frontend_snapshot(void);

/* Call immediately after Angband's run_game_loop() returns. */
void abfp_frontend_after_game_loop(void);

/* Legacy frame hooks retained for renderer integrations; they do not tick Angband. */
void abfp_frontend_begin_frame(void);
void abfp_frontend_end_frame(void);

#ifdef __cplusplus
}
#endif
