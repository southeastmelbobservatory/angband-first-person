#pragma once

#include "abfp_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

bool abfp_upstream_queue_command(abfp_command_type command);
bool abfp_upstream_install_handlers(void);
void abfp_upstream_remove_handlers(void);
bool abfp_upstream_refresh_snapshot(void);
struct monster;
void abfp_upstream_monster_died(const struct monster *mon);
/* Call immediately after Angband's run_game_loop() returns to the host. */
void abfp_upstream_sync_after_game_loop(void);

#ifdef __cplusplus
}
#endif
