#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../../protocol/commands.h"
#include "../../../protocol/events.h"
#include "../../../protocol/game_state.h"

#ifdef __cplusplus
extern "C" {
#endif

bool abfp_bridge_init(void);
void abfp_bridge_shutdown(void);
bool abfp_bridge_submit_command(abfp_command_type command);
const abfp_snapshot *abfp_bridge_snapshot(void);
void abfp_bridge_set_snapshot(const abfp_snapshot *snapshot);
size_t abfp_bridge_event_count(void);
const abfp_event *abfp_bridge_event_at(size_t index);
void abfp_bridge_clear_events(void);
void abfp_bridge_emit(const abfp_event *event);

#ifdef __cplusplus
}
#endif
