#include "../include/abfp_bridge.h"

#include <stdlib.h>
#include <string.h>

/*
 * This file intentionally contains no Angband gameplay logic.
 *
 * Phase 1 provides the event queue and public boundary. The upstream adapter
 * will populate snapshots and translate abstract commands into the appropriate
 * Angband command path.
 */

static abfp_event *events;
static size_t event_count;
static size_t event_capacity;

static abfp_snapshot snapshot;

bool abfp_bridge_init(void)
{
    memset(&snapshot, 0, sizeof(snapshot));
    event_count = 0;
    event_capacity = 64;
    events = calloc(event_capacity, sizeof(*events));
    return events != NULL;
}

void abfp_bridge_shutdown(void)
{
    free(events);
    events = NULL;
    event_count = 0;
    event_capacity = 0;
    memset(&snapshot, 0, sizeof(snapshot));
}

bool abfp_bridge_submit_command(abfp_command_type command)
{
    /*
     * TODO: implemented by the upstream adapter.
     *
     * Important: do not update snapshot positions here. The Angband engine
     * advances first; the adapter then refreshes the snapshot and emits
     * presentation events.
     */
    (void)command;
    return false;
}

const abfp_snapshot *abfp_bridge_snapshot(void)
{
    return &snapshot;
}

size_t abfp_bridge_event_count(void)
{
    return event_count;
}

const abfp_event *abfp_bridge_event_at(size_t index)
{
    if (index >= event_count) return NULL;
    return &events[index];
}

void abfp_bridge_clear_events(void)
{
    event_count = 0;
}

void abfp_bridge_emit(const abfp_event *event)
{
    if (!event) return;

    if (event_count == event_capacity) {
        size_t new_capacity = event_capacity ? event_capacity * 2 : 64;
        abfp_event *replacement =
            realloc(events, new_capacity * sizeof(*replacement));

        if (!replacement) return;

        events = replacement;
        event_capacity = new_capacity;
    }

    events[event_count++] = *event;
}
