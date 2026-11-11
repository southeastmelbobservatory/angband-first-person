#include "../include/abfp_bridge.h"

#include <stdlib.h>
#include <string.h>

#ifdef ABFP_WITH_ANGBAND
#include "../include/abfp_upstream_adapter.h"
#endif

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
#ifdef ABFP_WITH_ANGBAND
    return abfp_upstream_queue_command(command);
#else
    (void)command;
    return false;
#endif
}

const abfp_snapshot *abfp_bridge_snapshot(void)
{
    return &snapshot;
}

void abfp_bridge_set_snapshot(const abfp_snapshot *next)
{
    if (next) snapshot = *next;
}

size_t abfp_bridge_event_count(void)
{
    return event_count;
}

const abfp_event *abfp_bridge_event_at(size_t index)
{
    return index < event_count ? &events[index] : NULL;
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
