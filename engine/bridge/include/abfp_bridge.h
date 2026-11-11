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

/**
 * Presentation bridge lifecycle.
 *
 * The bridge is called by an Angband frontend integration point. It never
 * resolves game rules itself.
 */
bool abfp_bridge_init(void);
void abfp_bridge_shutdown(void);

/**
 * Submit one abstract command to the authoritative engine integration.
 *
 * Returns false when the command cannot be accepted at the current engine
 * state. A successful submission may still result in no player movement.
 */
bool abfp_bridge_submit_command(abfp_command_type command);

/**
 * Export the current presentation snapshot.
 *
 * Storage is bridge-owned and remains valid until the next bridge mutation.
 */
const abfp_snapshot *abfp_bridge_snapshot(void);

/**
 * Presentation events are ordered and consumed exactly once by the client.
 */
size_t abfp_bridge_event_count(void);
const abfp_event *abfp_bridge_event_at(size_t index);
void abfp_bridge_clear_events(void);

/**
 * Called by the thin Angband instrumentation layer.
 */
void abfp_bridge_emit(const abfp_event *event);

#ifdef __cplusplus
}
#endif
