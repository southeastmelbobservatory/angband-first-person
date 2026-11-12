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
    ABFP_TERRAIN_STAIRS_DOWN,
    ABFP_TERRAIN_RUBBLE,
    ABFP_TERRAIN_SHOP
} abfp_terrain_type;

typedef enum abfp_entity_type {
    ABFP_ENTITY_MONSTER = 1,
    ABFP_ENTITY_OBJECT = 2
} abfp_entity_type;

typedef struct abfp_cell {
    int32_t x, y;
    abfp_terrain_type terrain;
    uint8_t visible, lit;
    uint8_t occupied_by_monster, occupied_by_object;
} abfp_cell;

/* kind_id is an Angband race index or known object-kind index.
 * Object category/variant are tval/sval and are zero for unidentified items.
 * The renderer maps these game IDs to its own models and animations. */
typedef struct abfp_entity_state {
    char name[64];
    uint64_t id;
    int32_t x, y;
    abfp_entity_type type;
    int32_t kind_id;
    int32_t category_id;
    int32_t variant_id;
    int32_t hitpoints, max_hitpoints;
    int32_t quantity;
} abfp_entity_state;

typedef struct abfp_player_state {
    int32_t hitpoints, max_hitpoints;
    int32_t level;
    int32_t gold;
} abfp_player_state;

typedef struct abfp_snapshot {
    uint64_t turn;
    int32_t dungeon_depth;
    int32_t player_x, player_y;
    abfp_player_state player;
    const abfp_cell* cells;
    uint32_t cell_count;
    const abfp_entity_state* entities;
    uint32_t entity_count;
} abfp_snapshot;

#ifdef __cplusplus
}
#endif
