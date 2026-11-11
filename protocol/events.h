#pragma once
#include <stdint.h>

typedef enum abfp_event_type {
    ABFP_EVENT_NONE = 0,
    ABFP_EVENT_ENTITY_MOVED,
    ABFP_EVENT_ATTACK,
    ABFP_EVENT_DAMAGE,
    ABFP_EVENT_SPELL,
    ABFP_EVENT_PICKUP,
    ABFP_EVENT_DEATH,
    ABFP_EVENT_TERRAIN_CHANGED
} abfp_event_type;

typedef struct abfp_event {
    abfp_event_type type;
    uint64_t turn, subject_id, target_id;
    int32_t from_x, from_y, to_x, to_y;
    int32_t value;
} abfp_event;
