#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum abfp_terrain_type {
    ABFP_TERRAIN_UNKNOWN = 0,
    ABFP_TERRAIN_FLOOR,
    ABFP_TERRAIN_WALL,
    ABFP_TERRAIN_DOOR,
    ABFP_TERRAIN_STAIRS_UP,
    ABFP_TERRAIN_STAIRS_DOWN
} abfp_terrain_type;

typedef struct abfp_cell {
    int32_t x, y;
    abfp_terrain_type terrain;
    uint8_t visible, lit;
    uint8_t occupied_by_monster, occupied_by_object;
} abfp_cell;

typedef struct abfp_entity_state {
    uint64_t id;
    int32_t x, y;
    int32_t kind;
    int32_t hitpoints, max_hitpoints;
} abfp_entity_state;

typedef struct abfp_snapshot {
    uint64_t turn;
    int32_t dungeon_depth;
    int32_t player_x, player_y;
    const abfp_cell* cells;
    uint32_t cell_count;
    const abfp_entity_state* entities;
    uint32_t entity_count;
} abfp_snapshot;

#ifdef __cplusplus
}
#endif
