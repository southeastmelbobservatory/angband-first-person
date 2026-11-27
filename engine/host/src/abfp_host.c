#include "angband.h"
#include "cmd-core.h"
#include "cave.h"
#include "game-event.h"
#include "game-world.h"
#include "init.h"
#include "option.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-util.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "savefile.h"
#include "trap.h"
#include "ui-game.h"
#include "z-file.h"
#include "abfp_frontend.h"
#include "abfp_transport.h"
#include "abfp_host.h"
#include <stdlib.h>
#include <string.h>

static bool bridge_registered;
static bool initial_state_sent;
static bool initial_autosave_done;
static bool new_char_requested;
static bool load_save_requested;
static bool skip_autosave_on_restart;
static char requested_load_path[1024];
static abfp_save_entry cached_saves[ABFP_MAX_SAVES];
static uint32_t cached_save_count;
static bool saves_dirty = true;
static int last_seen_depth = -1;
static bool birth_rolled_stats;
static int birth_points_spent[ABFP_NUM_STATS];
static int birth_points_inc[ABFP_NUM_STATS];
static int birth_points_left;

static void abfp_host_extract_clean_save_name(const char *path_or_fname,
    char *out_name, size_t out_len)
{
    const char *base;
    if (!out_name || out_len == 0) return;
    out_name[0] = '\0';
    if (!path_or_fname || !path_or_fname[0]) return;
    base = strrchr(path_or_fname, '/');
    if (!base) base = strrchr(path_or_fname, '\\');
    base = base ? (base + 1) : path_or_fname;
#ifdef SETGID
    {
        char uid_prefix[32];
        strnfmt(uid_prefix, sizeof(uid_prefix), "%d.", player_uid);
        if (prefix(base, uid_prefix)) {
            base += strlen(uid_prefix);
        }
    }
#endif
    my_strcpy(out_name, base, out_len);
}

static void abfp_host_record_last_save(const char *save_path)
{
    char marker_path[1024];
    char clean_name[64];
    ang_file *f;
    if (!ANGBAND_DIR_USER || !ANGBAND_DIR_USER[0]) return;
    abfp_host_extract_clean_save_name(save_path, clean_name, sizeof(clean_name));
    if (!clean_name[0]) return;
    path_build(marker_path, sizeof(marker_path), ANGBAND_DIR_USER, "abfp_last_save.txt");
    f = file_open(marker_path, MODE_WRITE, FTYPE_TEXT);
    if (f) {
        file_putf(f, "%s\n", clean_name);
        file_close(f);
    }
}

static bool abfp_host_resolve_save_path(const char *clean_name,
    char *out_path, size_t out_len, bool *out_is_dead)
{
    savefile_getter g = NULL;
    bool found = false;
    if (out_path && out_len > 0) out_path[0] = '\0';
    if (out_is_dead) *out_is_dead = false;
    if (!clean_name || !clean_name[0]) return false;

    while (got_savefile(&g)) {
        const struct savefile_details *details = get_savefile_details(g);
        const char *entry_name;
        if (!details || !details->fnam) continue;
        entry_name = details->fnam + details->foff;
        if (streq(entry_name, clean_name)) {
            if (out_path && out_len > 0) {
                path_build(out_path, out_len, ANGBAND_DIR_SAVE, details->fnam);
            }
            if (out_is_dead) {
                *out_is_dead = (details->desc && strstr(details->desc, ", dead (") != NULL);
            }
            found = true;
            break;
        }
    }
    cleanup_savefile_getter(g);
    return found;
}

static void abfp_host_refresh_save_list(void)
{
    savefile_getter g = NULL;
    uint32_t count = 0;
    uint32_t i, j;

    if (!saves_dirty) return;
    memset(cached_saves, 0, sizeof(cached_saves));
    cached_save_count = 0;

    while (got_savefile(&g) && count < ABFP_MAX_SAVES) {
        const struct savefile_details *details = get_savefile_details(g);
        const char *entry_name;
        const char *desc;
        abfp_save_entry *dst;
        if (!details || !details->fnam) continue;
        entry_name = details->fnam + details->foff;
        if (!entry_name[0] || entry_name[0] == '.') continue;
        if (suffix(details->fnam, ".new") || suffix(details->fnam, ".old") ||
            suffix(details->fnam, ".panic")) continue;
        desc = details->desc;
        if (desc && streq(desc, "Invalid savefile")) continue;

        dst = &cached_saves[count++];
        memset(dst, 0, sizeof(*dst));
        my_strcpy(dst->name, entry_name, sizeof(dst->name));
        if (desc && desc[0]) {
            my_strcpy(dst->description, desc, sizeof(dst->description));
        } else {
            my_strcpy(dst->description, entry_name, sizeof(dst->description));
        }
        dst->is_dead = (desc && strstr(desc, ", dead (") != NULL) ? 1 : 0;
        dst->is_current = (savefile[0] && suffix(savefile, details->fnam)) ? 1 : 0;
    }
    cleanup_savefile_getter(g);

    /* Order: active save first, then living saves, then fallen heroes (alphabetical within group). */
    for (i = 0; i < count; ++i) {
        for (j = i + 1; j < count; ++j) {
            int rank_i = cached_saves[i].is_current ? 0 : (cached_saves[i].is_dead ? 2 : 1);
            int rank_j = cached_saves[j].is_current ? 0 : (cached_saves[j].is_dead ? 2 : 1);
            bool swap = false;
            if (rank_j < rank_i) {
                swap = true;
            } else if (rank_j == rank_i && strcmp(cached_saves[j].name, cached_saves[i].name) < 0) {
                swap = true;
            }
            if (swap) {
                abfp_save_entry tmp = cached_saves[i];
                cached_saves[i] = cached_saves[j];
                cached_saves[j] = tmp;
            }
        }
    }

    cached_save_count = count;
    saves_dirty = false;
}

static bool abfp_host_save_current(const char *optional_slot, bool announce)
{
    bool ok;
    if (!player || player->is_dead || !character_generated) {
        if (announce) msg("Cannot save: no active living character.");
        return false;
    }
    if (optional_slot && optional_slot[0]) {
        savefile_set_name(optional_slot, true, true);
    } else if (!savefile[0]) {
        const char *name = player->full_name[0] ? player->full_name : "Hero";
        savefile_set_name(name, true, true);
    }
    my_strcpy(player->died_from, "(saved)", sizeof(player->died_from));
    ok = savefile_save(savefile);
    (void)lore_save("lore.txt");
    my_strcpy(player->died_from, "(alive and well)", sizeof(player->died_from));
    if (ok) {
        abfp_host_record_last_save(savefile);
        saves_dirty = true;
        if (announce) {
            if (player->depth == 0) {
                msg("Game saved (%s, Lv %d, Town).",
                    player->full_name[0] ? player->full_name : "Hero", player->lev);
            } else {
                msg("Game saved (%s, Lv %d, Depth %d).",
                    player->full_name[0] ? player->full_name : "Hero", player->lev, player->depth);
            }
        }
    } else if (announce) {
        msg("Failed to save game!");
    }
    return ok;
}

static bool abfp_host_delete_save(const char *clean_name, bool announce)
{
    char full_path[1024];
    bool is_dead = false;
    bool deleted;

    if (!abfp_host_resolve_save_path(clean_name, full_path, sizeof(full_path), &is_dead)) {
        if (announce) msg("Save '%s' not found.", clean_name ? clean_name : "");
        return false;
    }
    if (player && !player->is_dead && character_generated &&
        savefile[0] && streq(savefile, full_path)) {
        if (announce) msg("Cannot delete the currently active hero.");
        return false;
    }
    safe_setuid_grab();
    deleted = file_delete(full_path);
    safe_setuid_drop();
    if (deleted) {
        saves_dirty = true;
        if (announce) msg("Deleted save '%s'.", clean_name);
    } else if (announce) {
        msg("Failed to delete save '%s'.", clean_name);
    }
    return deleted;
}

static bool abfp_host_request_load(const char *clean_name, bool announce)
{
    char full_path[1024];
    bool is_dead = false;

    if (!abfp_host_resolve_save_path(clean_name, full_path, sizeof(full_path), &is_dead)) {
        if (announce) msg("Save '%s' not found.", clean_name ? clean_name : "");
        return false;
    }
    if (is_dead) {
        if (announce) msg("Cannot load fallen hero '%s'. Create a new hero or pick a living save.", clean_name);
        return false;
    }
    my_strcpy(requested_load_path, full_path, sizeof(requested_load_path));
    load_save_requested = true;
    /* If reloading the exact same savefile, do NOT overwrite it with current in-memory state first. */
    skip_autosave_on_restart = (savefile[0] && streq(savefile, full_path));
    abfp_host_record_last_save(full_path);
    saves_dirty = true;
    return true;
}

bool abfp_host_has_load_request(void)
{
    return load_save_requested && requested_load_path[0] != '\0';
}

void abfp_host_consume_restart(char *next_savefile, size_t next_savefile_len,
    bool *out_load_save, bool *out_skip_autosave)
{
    if (next_savefile && next_savefile_len > 0) {
        if (load_save_requested && requested_load_path[0]) {
            my_strcpy(next_savefile, requested_load_path, next_savefile_len);
        } else {
            next_savefile[0] = '\0';
        }
    }
    if (out_load_save) *out_load_save = load_save_requested && requested_load_path[0] != '\0';
    if (out_skip_autosave) *out_skip_autosave = skip_autosave_on_restart;
    load_save_requested = false;
    skip_autosave_on_restart = false;
    new_char_requested = false;
    requested_load_path[0] = '\0';
    saves_dirty = true;
}
static bool test_floor_item_seeded;
static bool test_down_stair_seeded;
static bool test_monster_seeded;
static bool test_monster_attacked;
static bool test_monster_killed;
static int test_monster_midx;
static bool test_item_grid_valid;
static struct loc test_item_grid;
static bool test_stair_grid_valid;
static struct loc test_stair_grid;

static bool abfp_host_seed_test_floor_item(void)
{
    const int sval = lookup_sval(TV_FOOD, "Ration of Food");
    struct object_kind *kind;
    struct object *object;
    struct object *known;
    struct loc item_grid = loc(0, 0);
    bool found_grid = false;
    bool note = true;
    static const int dx[] = { 0, 1, 0, -1, 1, 1, -1, -1 };
    static const int dy[] = { -1, 0, 1, 0, -1, 1, 1, -1 };
    size_t direction;

    if (sval < 0) return false;
    kind = lookup_kind(TV_FOOD, sval);
    if (!kind) return false;

    /*
     * Prefer a naturally open adjacent floor square. If the generated
     * arrival tile is completely enclosed, reserve one adjacent in-bounds
     * square as floor for this CI-only ration fixture rather than failing
     * the entire first dungeon snapshot.
     */
    for (direction = 0; direction < sizeof(dx) / sizeof(dx[0]); ++direction) {
        struct loc candidate = loc(player->grid.x + dx[direction],
            player->grid.y + dy[direction]);
        if (!square_in_bounds_fully(cave, candidate) ||
            square_isoccupied(cave, candidate) ||
            square_object(cave, candidate)) continue;
        if (!found_grid) {
            item_grid = candidate;
            found_grid = true;
        }
        if (square_isobjectholding(cave, candidate) &&
            square_ispassable(cave, candidate)) {
            item_grid = candidate;
            break;
        }
    }
    if (!found_grid) return false;
    /* The fixture path must not accidentally trigger a generated trap/web. */
    square_remove_all_traps(cave, player->grid);
    square_remove_all_traps(cave, item_grid);
    if (!square_isobjectholding(cave, item_grid) ||
        !square_ispassable(cave, item_grid)) {
        square_set_feat(cave, item_grid, FEAT_FLOOR);
        square_memorize(cave, item_grid);
        square_light_spot(cave, item_grid);
    }

    object = object_new();
    known = object_new();
    if (!object || !known) {
        if (object) object_free(object);
        if (known) object_free(known);
        return false;
    }

    object_prep(object, kind, cave->depth, AVERAGE);
    object_prep(known, kind, cave->depth, AVERAGE);
    object->known = known;
    if (!floor_carry(cave, item_grid, object, &note)) {
        object->known = NULL;
        object_free(known);
        object_free(object);
        return false;
    }

    test_item_grid = item_grid;
    test_item_grid_valid = true;
    test_floor_item_seeded = true;
    return true;
}

static bool abfp_host_move_target(abfp_command_type command,
    struct loc *target)
{
    int dx = 0, dy = 0;
    if (!target || !player) return false;
    switch (command) {
    case ABFP_COMMAND_MOVE_NORTH: dy = -1; break;
    case ABFP_COMMAND_MOVE_SOUTH: dy = 1; break;
    case ABFP_COMMAND_MOVE_WEST: dx = -1; break;
    case ABFP_COMMAND_MOVE_EAST: dx = 1; break;
    case ABFP_COMMAND_MOVE_NORTH_WEST: dx = -1; dy = -1; break;
    case ABFP_COMMAND_MOVE_NORTH_EAST: dx = 1; dy = -1; break;
    case ABFP_COMMAND_MOVE_SOUTH_WEST: dx = -1; dy = 1; break;
    case ABFP_COMMAND_MOVE_SOUTH_EAST: dx = 1; dy = 1; break;
    default: return false;
    }
    *target = loc(player->grid.x + dx, player->grid.y + dy);
    return true;
}

static bool abfp_host_seed_test_monster(void)
{
    struct monster_race *race = lookup_monster("giant white mouse");
    struct monster_group_info group_info = {0};
    struct monster *mon;
    struct loc monster_grid;
    static const int dx[] = { 0, 1, 0, -1, 1, 1, -1, -1 };
    static const int dy[] = { -1, 0, 1, 0, -1, 1, 1, -1 };
    size_t direction;
    bool found_grid = false;

    if (!race || !test_stair_grid_valid) return false;

    /* Prefer the ration tile after pickup; otherwise use another free neighbour. */
    if (test_item_grid_valid &&
        square_in_bounds_fully(cave, test_item_grid) &&
        !square_isoccupied(cave, test_item_grid) &&
        !square_object(cave, test_item_grid)) {
        monster_grid = test_item_grid;
        found_grid = true;
    }

    for (direction = 0; !found_grid &&
            direction < sizeof(dx) / sizeof(dx[0]); ++direction) {
        struct loc candidate = loc(test_stair_grid.x + dx[direction],
            test_stair_grid.y + dy[direction]);
        if (!square_in_bounds_fully(cave, candidate) ||
            square_isoccupied(cave, candidate) ||
            square_object(cave, candidate)) continue;
        monster_grid = candidate;
        found_grid = true;
    }
    if (!found_grid) return false;

    square_remove_all_traps(cave, monster_grid);
    square_set_feat(cave, monster_grid, FEAT_FLOOR);
    square_memorize(cave, monster_grid);
    square_light_spot(cave, monster_grid);

    if (!place_new_monster(cave, monster_grid, race, false, false,
            group_info, ORIGIN_DROP_SPECIAL)) return false;

    mon = square_monster(cave, monster_grid);
    if (!mon) return false;
    /* Deterministic target; Angband still resolves its real AI and combat. */
    mon->hp = 1;
    mon->maxhp = 1;
    monster_wake(mon, true, 100);
    test_monster_midx = mon->midx;

    /*
     * Give the monster a clear local approach and remove random traps/webs
     * from the small test arena so movement and combat are repeatable.
     */
    for (direction = 0; direction < sizeof(dx) / sizeof(dx[0]); ++direction) {
        struct loc approach = loc(test_stair_grid.x + dx[direction],
            test_stair_grid.y + dy[direction]);
        if (!square_in_bounds_fully(cave, approach) ||
            loc_eq(approach, monster_grid) ||
            square_isoccupied(cave, approach) ||
            square_object(cave, approach)) continue;
        square_remove_all_traps(cave, approach);
        square_set_feat(cave, approach, FEAT_FLOOR);
        square_memorize(cave, approach);
        square_light_spot(cave, approach);
    }

    square_remove_all_traps(cave, test_stair_grid);
    update_view(cave, player);
    update_monsters(true);
    square_light_spot(cave, monster_grid);
    test_monster_seeded = true;
    return true;
}

static errr abfp_host_get_command(cmd_context context)
{
    abfp_command cmd;
    abfp_command_type command;

    /* Birth, store, death and other non-game contexts retain the text UI. */
    if (context != CTX_GAME) return textui_get_cmd(context);

    /*
     * The 3D client receives authoritative state/events instead of reading
     * Angband's message line. Do not block the game thread on a terminal
     * "-more-" acknowledgement after messages such as "no staircase here".
     */
    if (player) {
        OPT(player, auto_more) = true;
        if (getenv("ABFP_TEST_FLOOR_ITEM"))
            OPT(player, pickup_always) = true;
    }

    if (!bridge_registered) {
        if (!abfp_frontend_register()) {
            return -1;
        }
        bridge_registered = true;
    }

    while (1) {
        if (!abfp_transport_wait_for_client()) return -1;
        if (!initial_state_sent) {
            initial_state_sent = abfp_host_publish_state(true);
        }
        if (!abfp_transport_wait_command_ex(&cmd)) {
            /* Auto-save living hero whenever the 3D client disconnects cleanly. */
            if (player && !player->is_dead && character_generated &&
                !getenv("ABFP_TEST_MONSTER") && !getenv("ABFP_TEST_FLOOR_ITEM") &&
                !getenv("ABFP_TEST_KILL_MONSTER")) {
                (void)abfp_host_save_current(NULL, false);
            }
            initial_state_sent = false;
            continue;
        }
        command = cmd.type;

        /*
         * CI-only fixture: put a real Angband down-stair under the player
         * immediately before the native GO_DOWN command is queued. The
         * command, level generation and return trip remain normal Angband.
         */
        if (getenv("ABFP_TEST_FORCE_STAIRS") &&
            command == ABFP_COMMAND_GO_DOWN && !test_down_stair_seeded &&
            player && cave && cave->depth == 0) {
            square_set_feat(cave, player->grid, FEAT_MORE);
            square_memorize(cave, player->grid);
            square_light_spot(cave, player->grid);
            test_down_stair_seeded = true;
            (void)abfp_frontend_refresh_snapshot();
        }

        if (getenv("ABFP_TEST_MONSTER") &&
            command == ABFP_COMMAND_WAIT && !test_monster_seeded &&
            player && cave && cave->depth > 0 && test_stair_grid_valid &&
            test_item_grid_valid && loc_eq(player->grid, test_stair_grid)) {
            if (!abfp_host_seed_test_monster() ||
                !abfp_frontend_refresh_snapshot()) return -1;
        }

        if (test_monster_seeded && test_monster_midx > 0) {
            struct loc target;
            struct monster *mon = cave_monster(cave, test_monster_midx);
            if (mon && abfp_host_move_target(command, &target) &&
                loc_eq(target, mon->grid))
                test_monster_attacked = true;
        }
        if (getenv("ABFP_TEST_KILL_MONSTER") &&
            command == ABFP_COMMAND_WAIT && test_monster_attacked &&
            !test_monster_killed) {
            struct monster *mon = test_monster_midx > 0 ?
                cave_monster(cave, test_monster_midx) : NULL;
            if (mon) {
                bool fear = false;
                (void)mon_take_hit(mon, player, mon->hp, &fear, NULL);
            }
            test_monster_killed = true;
        }

        if (command == ABFP_COMMAND_SAVE_GAME) {
            (void)abfp_host_save_current(cmd.text[0] ? cmd.text : NULL, true);
            (void)abfp_frontend_refresh_snapshot();
            (void)abfp_host_publish_state(false);
            continue;
        }

        if (command == ABFP_COMMAND_DELETE_SAVE) {
            (void)abfp_host_delete_save(cmd.text, true);
            (void)abfp_frontend_refresh_snapshot();
            (void)abfp_host_publish_state(false);
            continue;
        }

        if (command == ABFP_COMMAND_LOAD_GAME) {
            if (abfp_host_request_load(cmd.text, true)) {
                if (player && player->upkeep) {
                    player->upkeep->playing = false;
                }
                (void)cmdq_push(CMD_NULL);
                return 0;
            }
            (void)abfp_frontend_refresh_snapshot();
            (void)abfp_host_publish_state(false);
            continue;
        }

        if (command == ABFP_COMMAND_NEW_CHAR) {
            new_char_requested = true;
            if (player && player->upkeep) {
                player->upkeep->playing = false;
            }
            (void)cmdq_push(CMD_NULL);
            return 0;
        }

        if (abfp_frontend_submit_command_ex(&cmd)) return 0;
    }
}

static void abfp_birth_points_handler(game_event_type type,
    game_event_data *data, void *user)
{
    int s;
    (void)type;
    (void)user;
    if (!data) return;
    birth_points_left = data->birthpoints.remaining;
    for (s = 0; s < ABFP_NUM_STATS && s < STAT_MAX; ++s) {
        birth_points_spent[s] = data->birthpoints.points ? data->birthpoints.points[s] : 0;
        birth_points_inc[s] = data->birthpoints.inc_points ? data->birthpoints.inc_points[s] : 0;
    }
}

static bool abfp_host_publish_birth_state(void)
{
    abfp_snapshot snap;
    abfp_birth_state b;
    struct player_race *r;
    struct player_class *c;
    int i, s, n_races = 0, n_classes = 0;

    memset(&snap, 0, sizeof(snap));
    memset(&b, 0, sizeof(b));
    b.active = 1;
    b.rolled = birth_rolled_stats ? 1 : 0;
    b.points_left = birth_points_left;
    b.race_index = (player && player->race) ? (int32_t)player->race->ridx : 0;
    b.class_index = (player && player->class) ? (int32_t)player->class->cidx : 0;

    if (player) {
        b.gold = player->au_birth;
        b.hitpoints = player->mhp > 0 ? player->mhp :
            (player->hitdie + (player->race ? player->race->r_mhp : 0) +
             (player->class ? player->class->c_mhp : 0));
        if (player->full_name[0]) {
            strncpy(b.name, player->full_name, sizeof(b.name) - 1);
        }
        for (s = 0; s < ABFP_NUM_STATS && s < STAT_MAX; ++s) {
            b.stats_base[s] = player->stat_cur[s];
            b.stats_total[s] = player->state.stat_use[s];
            b.points_spent[s] = birth_points_spent[s];
            b.points_inc[s] = birth_points_inc[s];
        }
    }

    for (r = races; r; r = r->next) ++n_races;
    for (i = 0; i < n_races && b.race_count < ABFP_MAX_RACES; ++i) {
        r = player_id2race((guid)i);
        if (!r) continue;
        abfp_birth_option *dst = &b.races[b.race_count++];
        memset(dst, 0, sizeof(*dst));
        dst->index = (int32_t)r->ridx;
        if (r->name) strncpy(dst->name, r->name, sizeof(dst->name) - 1);
        for (s = 0; s < ABFP_NUM_STATS && s < STAT_MAX; ++s) dst->stat_adj[s] = r->r_adj[s];
        dst->hit_die = r->r_mhp;
        dst->exp_factor = r->r_exp;
        dst->has_magic = 0;
    }

    for (c = classes; c; c = c->next) ++n_classes;
    for (i = 0; i < n_classes && b.class_count < ABFP_MAX_CLASSES; ++i) {
        c = player_id2class((guid)i);
        if (!c) continue;
        abfp_birth_option *dst = &b.classes[b.class_count++];
        memset(dst, 0, sizeof(*dst));
        dst->index = (int32_t)c->cidx;
        if (c->name) strncpy(dst->name, c->name, sizeof(dst->name) - 1);
        for (s = 0; s < ABFP_NUM_STATS && s < STAT_MAX; ++s) dst->stat_adj[s] = c->c_adj[s];
        dst->hit_die = c->c_mhp;
        dst->exp_factor = c->c_exp;
        dst->has_magic = (c->magic.total_spells > 0) ? 1 : 0;
    }

    abfp_host_refresh_save_list();
    snap.birth = &b;
    snap.saves = cached_saves;
    snap.save_count = cached_save_count;
    return abfp_transport_publish_state(&snap, NULL, 0, true);
}

bool abfp_host_run_birth(void)
{
    const char *env_client_birth = getenv("ABFP_CLIENT_BIRTH");
    bool use_client_birth = new_char_requested ||
        (env_client_birth && env_client_birth[0] == '1');
    abfp_command cmd;
    int n_races = 0, n_classes = 0;
    struct player_race *r;
    struct player_class *c;

    if (!use_client_birth) return false;

    for (r = races; r; r = r->next) ++n_races;
    for (c = classes; c; c = c->next) ++n_classes;

    birth_rolled_stats = false;
    memset(birth_points_spent, 0, sizeof(birth_points_spent));
    memset(birth_points_inc, 0, sizeof(birth_points_inc));
    birth_points_left = 20;
    saves_dirty = true;

    event_add_handler(EVENT_BIRTHPOINTS, abfp_birth_points_handler, NULL);

    cmdq_push(CMD_BIRTH_INIT);
    cmdq_execute(CTX_BIRTH);
    cmdq_push(CMD_BIRTH_RESET);
    cmdq_execute(CTX_BIRTH);
    cmdq_push(CMD_CHOOSE_RACE);
    cmd_set_arg_choice(cmdq_peek(), "choice", 0);
    cmdq_execute(CTX_BIRTH);
    cmdq_push(CMD_CHOOSE_CLASS);
    cmd_set_arg_choice(cmdq_peek(), "choice", 0);
    cmdq_execute(CTX_BIRTH);
    cmdq_push(CMD_RESET_STATS);
    cmd_set_arg_choice(cmdq_peek(), "choice", 1);
    cmdq_execute(CTX_BIRTH);
    cmdq_push(CMD_REFRESH_STATS);
    cmdq_execute(CTX_BIRTH);

    if (player && !player->full_name[0]) {
        cmdq_push(CMD_NAME_CHOICE);
        cmd_set_arg_string(cmdq_peek(), "name", "Hero");
        cmdq_execute(CTX_BIRTH);
    }

    while (1) {
        if (!abfp_transport_wait_for_client()) {
            event_remove_handler(EVENT_BIRTHPOINTS, abfp_birth_points_handler, NULL);
            return false;
        }
        (void)abfp_host_publish_birth_state();
        if (!abfp_transport_wait_command_ex(&cmd)) {
            continue;
        }
        if (cmd.type == ABFP_COMMAND_BIRTH_RACE && cmd.arg1 >= 0 && cmd.arg1 < n_races) {
            birth_rolled_stats = false;
            cmdq_push(CMD_CHOOSE_RACE);
            cmd_set_arg_choice(cmdq_peek(), "choice", cmd.arg1);
            cmdq_execute(CTX_BIRTH);
            cmdq_push(CMD_REFRESH_STATS);
            cmdq_execute(CTX_BIRTH);
        } else if (cmd.type == ABFP_COMMAND_BIRTH_CLASS && cmd.arg1 >= 0 && cmd.arg1 < n_classes) {
            birth_rolled_stats = false;
            cmdq_push(CMD_CHOOSE_CLASS);
            cmd_set_arg_choice(cmdq_peek(), "choice", cmd.arg1);
            cmdq_execute(CTX_BIRTH);
            cmdq_push(CMD_REFRESH_STATS);
            cmdq_execute(CTX_BIRTH);
        } else if (cmd.type == ABFP_COMMAND_BIRTH_BUY_STAT && cmd.arg1 >= 0 && cmd.arg1 < STAT_MAX) {
            cmdq_push(CMD_BUY_STAT);
            cmd_set_arg_choice(cmdq_peek(), "choice", cmd.arg1);
            cmdq_execute(CTX_BIRTH);
        } else if (cmd.type == ABFP_COMMAND_BIRTH_SELL_STAT && cmd.arg1 >= 0 && cmd.arg1 < STAT_MAX) {
            cmdq_push(CMD_SELL_STAT);
            cmd_set_arg_choice(cmdq_peek(), "choice", cmd.arg1);
            cmdq_execute(CTX_BIRTH);
        } else if (cmd.type == ABFP_COMMAND_BIRTH_ROLL) {
            birth_rolled_stats = true;
            cmdq_push(CMD_ROLL_STATS);
            cmdq_execute(CTX_BIRTH);
        } else if (cmd.type == ABFP_COMMAND_BIRTH_RESET_STATS) {
            birth_rolled_stats = false;
            cmdq_push(CMD_RESET_STATS);
            cmd_set_arg_choice(cmdq_peek(), "choice", cmd.arg1 ? 1 : 0);
            cmdq_execute(CTX_BIRTH);
            cmdq_push(CMD_REFRESH_STATS);
            cmdq_execute(CTX_BIRTH);
        } else if (cmd.type == ABFP_COMMAND_BIRTH_NAME && cmd.text[0]) {
            cmdq_push(CMD_NAME_CHOICE);
            cmd_set_arg_string(cmdq_peek(), "name", cmd.text);
            cmdq_execute(CTX_BIRTH);
        } else if (cmd.type == ABFP_COMMAND_LOAD_GAME) {
            if (abfp_host_request_load(cmd.text, false)) {
                break;
            }
        } else if (cmd.type == ABFP_COMMAND_DELETE_SAVE) {
            (void)abfp_host_delete_save(cmd.text, false);
        } else if (cmd.type == ABFP_COMMAND_BIRTH_ACCEPT) {
            if (cmd.text[0]) {
                cmdq_push(CMD_NAME_CHOICE);
                cmd_set_arg_string(cmdq_peek(), "name", cmd.text);
                cmdq_execute(CTX_BIRTH);
            }
            if (player && player->full_name[0]) {
                savefile_set_name(player->full_name, true, true);
            }
            cmdq_push(CMD_ACCEPT_CHARACTER);
            cmdq_execute(CTX_BIRTH);
            saves_dirty = true;
            break;
        }
    }

    event_remove_handler(EVENT_BIRTHPOINTS, abfp_birth_points_handler, NULL);
    new_char_requested = false;
    initial_state_sent = false;
    return true;
}

bool abfp_host_handle_game_end(void)
{
    if (new_char_requested || load_save_requested) {
        return true;
    }
    if (player && player->is_dead &&
        !getenv("ABFP_TEST_KILL_MONSTER") && !getenv("ABFP_TEST_MONSTER") &&
        (abfp_transport_has_client() || getenv("ABFP_CLIENT_BIRTH"))) {
        abfp_command cmd;
        bool first_pass = true;
        saves_dirty = true;
        while (abfp_transport_wait_for_client()) {
            if (!first_pass) {
                (void)abfp_host_publish_state(true);
            }
            first_pass = false;
            while (abfp_transport_wait_command_ex(&cmd)) {
                if (cmd.type == ABFP_COMMAND_NEW_CHAR) {
                    new_char_requested = true;
                    return true;
                }
                if (cmd.type == ABFP_COMMAND_LOAD_GAME) {
                    if (abfp_host_request_load(cmd.text, false)) {
                        return true;
                    }
                    (void)abfp_host_publish_state(false);
                }
                if (cmd.type == ABFP_COMMAND_DELETE_SAVE) {
                    (void)abfp_host_delete_save(cmd.text, false);
                    (void)abfp_host_publish_state(false);
                }
            }
        }
    }
    return false;
}

void abfp_host_rebind_hooks(void)
{
    cmd_get_hook = abfp_host_get_command;
}

bool abfp_host_install(uint16_t port)
{
    if (!abfp_transport_start(port)) return false;
    cmd_get_hook = abfp_host_get_command;
    return true;
}

/*
 * Retain queued events if delivery fails (for example, a client disconnects
 * between command submission and the post-turn publish). The next client
 * receives the latest full snapshot plus those still-pending events.
 */
bool abfp_host_publish_state(bool initial_state)
{
    const abfp_snapshot *snapshot;
    if (getenv("ABFP_TEST_MONSTER") && !test_stair_grid_valid &&
        player && cave && cave->depth > 0) {
        test_stair_grid = player->grid;
        test_stair_grid_valid = true;
    }
    if (getenv("ABFP_TEST_FLOOR_ITEM") && !test_floor_item_seeded &&
        player && cave && cave->depth > 0) {
        if (!abfp_host_seed_test_floor_item() ||
            !abfp_frontend_refresh_snapshot()) return false;
    }
    if (initial_state && !initial_autosave_done && player && !player->is_dead &&
        character_generated && !getenv("ABFP_TEST_MONSTER") &&
        !getenv("ABFP_TEST_FLOOR_ITEM") && !getenv("ABFP_TEST_KILL_MONSTER")) {
        initial_autosave_done = true;
        if (!savefile[0] && player->full_name[0]) {
            savefile_set_name(player->full_name, true, true);
        }
        if (savefile[0]) {
            bool exists;
            safe_setuid_grab();
            exists = file_exists(savefile);
            safe_setuid_drop();
            if (!exists) {
                (void)abfp_host_save_current(NULL, false);
            } else {
                abfp_host_record_last_save(savefile);
                saves_dirty = true;
            }
        }
    }
    if (player && player->depth != last_seen_depth) {
        last_seen_depth = player->depth;
        saves_dirty = true;
    }
    abfp_host_refresh_save_list();

    snapshot = abfp_frontend_snapshot();
    size_t count = abfp_bridge_event_count();
    abfp_event *events = count ? (abfp_event *)malloc(count * sizeof(*events)) : NULL;
    size_t i;
    bool published;
    abfp_snapshot outgoing;

    if (!snapshot) {
        free(events);
        return false;
    }
    if (count && !events) return false;
    for (i = 0; i < count; ++i) {
        const abfp_event *event = abfp_bridge_event_at(i);
        if (!event) {
            free(events);
            return false;
        }
        events[i] = *event;
    }
    outgoing = *snapshot;
    outgoing.saves = cached_saves;
    outgoing.save_count = cached_save_count;
    published = abfp_transport_publish_state(&outgoing, events, count, initial_state);
    free(events);
    if (published) abfp_bridge_clear_events();
    return published;
}

void abfp_host_game_closed(void)
{
    if (bridge_registered) {
        abfp_bridge_shutdown();
        bridge_registered = false;
    }
    initial_state_sent = false;
    initial_autosave_done = false;
    saves_dirty = true;
    last_seen_depth = -1;
    test_floor_item_seeded = false;
    test_down_stair_seeded = false;
    test_monster_seeded = false;
    test_monster_attacked = false;
    test_monster_killed = false;
    test_monster_midx = 0;
    test_item_grid_valid = false;
    test_stair_grid_valid = false;
}

void abfp_host_shutdown(void)
{
    abfp_transport_stop();
    abfp_host_game_closed();
}
