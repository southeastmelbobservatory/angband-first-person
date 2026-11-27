#define _POSIX_C_SOURCE 200809L
#include "abfp_transport.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define ABFP_QUEUE_CAPACITY 64
#define ABFP_LINE_CAPACITY 128
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static abfp_command queue[ABFP_QUEUE_CAPACITY];
static size_t head, count;
static int listen_fd = -1;
static int active_fd = -1;
static pthread_t worker;
static bool running, worker_started;

bool abfp_transport_parse_command_ex(const char *line, abfp_command *out)
{
    static const struct { const char *name; abfp_command_type command; } simple_names[] = {
        {"WAIT", ABFP_COMMAND_WAIT},
        {"REST", ABFP_COMMAND_REST},
        {"AUTOPICKUP", ABFP_COMMAND_AUTOPICKUP},
        {"GO_UP", ABFP_COMMAND_GO_UP},
        {"GO_DOWN", ABFP_COMMAND_GO_DOWN},
        {"MOVE_NORTH", ABFP_COMMAND_MOVE_NORTH},
        {"MOVE_SOUTH", ABFP_COMMAND_MOVE_SOUTH},
        {"MOVE_WEST", ABFP_COMMAND_MOVE_WEST},
        {"MOVE_EAST", ABFP_COMMAND_MOVE_EAST},
        {"MOVE_NORTH_WEST", ABFP_COMMAND_MOVE_NORTH_WEST},
        {"MOVE_NORTH_EAST", ABFP_COMMAND_MOVE_NORTH_EAST},
        {"MOVE_SOUTH_WEST", ABFP_COMMAND_MOVE_SOUTH_WEST},
        {"MOVE_SOUTH_EAST", ABFP_COMMAND_MOVE_SOUTH_EAST},
        {"ALTER_NORTH", ABFP_COMMAND_ALTER_NORTH},
        {"ALTER_SOUTH", ABFP_COMMAND_ALTER_SOUTH},
        {"ALTER_WEST", ABFP_COMMAND_ALTER_WEST},
        {"ALTER_EAST", ABFP_COMMAND_ALTER_EAST},
        {"ALTER_NORTH_WEST", ABFP_COMMAND_ALTER_NORTH_WEST},
        {"ALTER_NORTH_EAST", ABFP_COMMAND_ALTER_NORTH_EAST},
        {"ALTER_SOUTH_WEST", ABFP_COMMAND_ALTER_SOUTH_WEST},
        {"ALTER_SOUTH_EAST", ABFP_COMMAND_ALTER_SOUTH_EAST},
        {"NEW_CHAR", ABFP_COMMAND_NEW_CHAR},
        {"SAVE", ABFP_COMMAND_SAVE_GAME},
        {"BIRTH_ROLL", ABFP_COMMAND_BIRTH_ROLL},
        {"BIRTH_RESET", ABFP_COMMAND_BIRTH_RESET_STATS},
        {"BIRTH_ACCEPT", ABFP_COMMAND_BIRTH_ACCEPT}
    };
    static const struct { const char *prefix; abfp_command_type command; bool allow_neg_first; } param_names[] = {
        {"USE ", ABFP_COMMAND_USE_ITEM, false},
        {"QUAFF ", ABFP_COMMAND_QUAFF_ITEM, false},
        {"READ ", ABFP_COMMAND_READ_ITEM, false},
        {"EAT ", ABFP_COMMAND_EAT_ITEM, false},
        {"WIELD ", ABFP_COMMAND_WIELD_ITEM, false},
        {"TAKEOFF ", ABFP_COMMAND_TAKEOFF_ITEM, false},
        {"DROP ", ABFP_COMMAND_DROP_ITEM, false},
        {"REFILL ", ABFP_COMMAND_REFILL_ITEM, false},
        {"BUY ", ABFP_COMMAND_SHOP_BUY, false},
        {"SELL ", ABFP_COMMAND_SHOP_SELL, false},
        {"FIRE ", ABFP_COMMAND_FIRE_ITEM, true},
        {"THROW ", ABFP_COMMAND_THROW_ITEM, false},
        {"CAST ", ABFP_COMMAND_CAST_SPELL, false},
        {"STUDY ", ABFP_COMMAND_STUDY_SPELL, false},
        {"BIRTH_RACE ", ABFP_COMMAND_BIRTH_RACE, false},
        {"BIRTH_CLASS ", ABFP_COMMAND_BIRTH_CLASS, false},
        {"BIRTH_BUY ", ABFP_COMMAND_BIRTH_BUY_STAT, false},
        {"BIRTH_SELL ", ABFP_COMMAND_BIRTH_SELL_STAT, false},
        {"BIRTH_RESET ", ABFP_COMMAND_BIRTH_RESET_STATS, false}
    };
    static const struct { const char *prefix; abfp_command_type command; } text_names[] = {
        {"BIRTH_NAME ", ABFP_COMMAND_BIRTH_NAME},
        {"BIRTH_ACCEPT ", ABFP_COMMAND_BIRTH_ACCEPT},
        {"SAVE ", ABFP_COMMAND_SAVE_GAME},
        {"LOAD ", ABFP_COMMAND_LOAD_GAME},
        {"DELETE_SAVE ", ABFP_COMMAND_DELETE_SAVE}
    };
    size_t i, length;
    if (!line || !out) return false;
    length = strcspn(line, "\r\n");
    for (i = 0; i < sizeof(simple_names) / sizeof(simple_names[0]); ++i) {
        if (strlen(simple_names[i].name) == length &&
            memcmp(line, simple_names[i].name, length) == 0) {
            memset(out, 0, sizeof(*out));
            out->type = simple_names[i].command;
            out->arg1 = (simple_names[i].command == ABFP_COMMAND_BIRTH_RESET_STATS) ? 1 : 0;
            out->arg2 = 1;
            return true;
        }
    }
    for (i = 0; i < sizeof(text_names) / sizeof(text_names[0]); ++i) {
        size_t plen = strlen(text_names[i].prefix);
        if (length > plen && memcmp(line, text_names[i].prefix, plen) == 0) {
            size_t j, out_len = 0;
            memset(out, 0, sizeof(*out));
            out->type = text_names[i].command;
            for (j = plen; j < length && out_len + 1 < sizeof(out->text) && out_len < 31; ++j) {
                unsigned char ch = (unsigned char)line[j];
                if (isalnum(ch) || ch == ' ' || ch == '-' || ch == '_' || ch == '\'') {
                    out->text[out_len++] = (char)ch;
                }
            }
            out->text[out_len] = '\0';
            return out_len > 0 || text_names[i].command == ABFP_COMMAND_BIRTH_ACCEPT;
        }
    }
    for (i = 0; i < sizeof(param_names) / sizeof(param_names[0]); ++i) {
        size_t plen = strlen(param_names[i].prefix);
        if (length > plen && memcmp(line, param_names[i].prefix, plen) == 0) {
            char buf[ABFP_LINE_CAPACITY];
            char *p = buf, *end = NULL;
            long vals[4] = { 0, 1, 0, 0 };
            int nvals = 0;
            long min_first = param_names[i].allow_neg_first ? -1 : 0;
            if (length - plen >= sizeof(buf)) return false;
            memcpy(buf, line + plen, length - plen);
            buf[length - plen] = '\0';
            while (*p && nvals < 4) {
                long v;
                while (*p == ' ') ++p;
                if (!*p) break;
                v = strtol(p, &end, 10);
                if (end == p) return false;
                if (nvals == 0) {
                    if (v < min_first || v > 1000) return false;
                } else {
                    if (v < 0 || v > 999) return false;
                }
                vals[nvals++] = v;
                p = end;
            }
            while (*p == ' ') ++p;
            if (nvals < 1 || *p != '\0') return false;
            memset(out, 0, sizeof(*out));
            out->type = param_names[i].command;
            out->arg1 = (int32_t)vals[0];
            out->arg2 = (int32_t)vals[1];
            out->arg3 = (int32_t)vals[2];
            out->arg4 = (int32_t)vals[3];
            return true;
        }
    }
    return false;
}

bool abfp_transport_parse_command(const char *line, abfp_command_type *out)
{
    abfp_command cmd;
    if (!out || !abfp_transport_parse_command_ex(line, &cmd)) return false;
    *out = cmd.type;
    return true;
}

static bool enqueue(const abfp_command *command)
{
    bool accepted = false;
    pthread_mutex_lock(&mutex);
    if (running && count < ABFP_QUEUE_CAPACITY) {
        queue[(head + count) % ABFP_QUEUE_CAPACITY] = *command;
        ++count; accepted = true; pthread_cond_signal(&ready);
    }
    pthread_mutex_unlock(&mutex);
    return accepted;
}
static void serve_client(int fd)
{
    char line[ABFP_LINE_CAPACITY], buffer[128];
    size_t used = 0;
    bool discarding = false;
    ssize_t n;
    while (1) {
        ssize_t i;
        n = recv(fd, buffer, sizeof(buffer), 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        for (i = 0; i < n; ++i) {
            char ch = buffer[i];
            if (ch == '\n') {
                abfp_command command;
                if (!discarding) {
                    line[used] = '\0';
                    if (abfp_transport_parse_command_ex(line, &command)) (void)enqueue(&command);
                }
                used = 0;
                discarding = false;
            } else if (ch != '\r' && !discarding) {
                if (used + 1 < sizeof(line)) line[used++] = ch;
                else { used = 0; discarding = true; }
            }
        }
        pthread_mutex_lock(&mutex);
        if (!running) { pthread_mutex_unlock(&mutex); break; }
        pthread_mutex_unlock(&mutex);
    }
}
static void *worker_main(void *unused)
{
    (void)unused;
    while (1) {
        int server_fd, fd;
        bool serve, keep_running;
        pthread_mutex_lock(&mutex);
        server_fd = listen_fd;
        keep_running = running;
        pthread_mutex_unlock(&mutex);
        if (!keep_running || server_fd < 0) break;
        fd = accept(server_fd, NULL, NULL);
        if (fd < 0) { if (errno == EINTR) continue; break; }
        pthread_mutex_lock(&mutex);
        active_fd = fd;
        serve = running;
        if (!serve) shutdown(fd, SHUT_RDWR);
        pthread_cond_broadcast(&ready);
        pthread_mutex_unlock(&mutex);
        { struct timeval timeout = {0, 200000};
          (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)); }
        if (serve) serve_client(fd);
        pthread_mutex_lock(&send_mutex);
        close(fd);
        pthread_mutex_lock(&mutex);
        active_fd = -1;
        pthread_cond_broadcast(&ready);
        pthread_mutex_unlock(&mutex);
        pthread_mutex_unlock(&send_mutex);
        pthread_mutex_lock(&mutex);
        if (!running) { pthread_mutex_unlock(&mutex); break; }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}
bool abfp_transport_start(uint16_t port)
{
    struct sockaddr_in address;
    int fd, reuse = 1;
    pthread_mutex_lock(&mutex);
    if (running || worker_started) { pthread_mutex_unlock(&mutex); return false; }
    head = count = 0;
    pthread_mutex_unlock(&mutex);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    {
        const char *bind_address = getenv("ABFP_BIND_ADDRESS");
        if (!bind_address || !*bind_address) bind_address = "127.0.0.1";
        if (inet_pton(AF_INET, bind_address, &address.sin_addr) != 1) {
            close(fd);
            return false;
        }
    }
    address.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0 || listen(fd, 1) < 0) {
        close(fd); return false;
    }
    listen_fd = fd;
    pthread_mutex_lock(&mutex); running = true; pthread_mutex_unlock(&mutex);
    if (pthread_create(&worker, NULL, worker_main, NULL) != 0) {
        pthread_mutex_lock(&mutex); running = false; pthread_mutex_unlock(&mutex);
        close(listen_fd); listen_fd = -1; return false;
    }
    worker_started = true;
    return true;
}
bool abfp_transport_has_client(void)
{
    bool connected;
    pthread_mutex_lock(&mutex);
    connected = running && active_fd >= 0;
    pthread_mutex_unlock(&mutex);
    return connected;
}

bool abfp_transport_wait_for_client(void)
{
    pthread_mutex_lock(&mutex);
    while (running && active_fd < 0) pthread_cond_wait(&ready, &mutex);
    const bool connected = active_fd >= 0;
    pthread_mutex_unlock(&mutex);
    return connected;
}

bool abfp_transport_wait_command_ex(abfp_command *out)
{
    if (!out) return false;
    pthread_mutex_lock(&mutex);
    while (running && count == 0 && active_fd >= 0) pthread_cond_wait(&ready, &mutex);
    if (count == 0) { pthread_mutex_unlock(&mutex); return false; }
    *out = queue[head]; head = (head + 1) % ABFP_QUEUE_CAPACITY; --count;
    pthread_mutex_unlock(&mutex); return true;
}

bool abfp_transport_wait_command(abfp_command_type *out)
{
    abfp_command cmd;
    if (!out || !abfp_transport_wait_command_ex(&cmd)) return false;
    *out = cmd.type;
    return true;
}

void abfp_transport_stop(void)
{
    int fd;
    pthread_mutex_lock(&mutex);
    running = false;
    pthread_cond_broadcast(&ready);
    if (active_fd >= 0) shutdown(active_fd, SHUT_RDWR);
    fd = listen_fd;
    if (fd >= 0) shutdown(fd, SHUT_RDWR);
    listen_fd = -1;
    pthread_mutex_unlock(&mutex);
    if (fd >= 0) close(fd);
    if (worker_started) { pthread_join(worker, NULL); worker_started = false; }
}


static bool append_json(char **cursor, size_t *remaining, const char *format, ...)
{
    va_list args;
    int written;
    va_start(args, format);
    written = vsnprintf(*cursor, *remaining, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= *remaining) return false;
    *cursor += written;
    *remaining -= (size_t)written;
    return true;
}

static bool append_json_string(char **cursor, size_t *remaining,
    const char *value, size_t max_length)
{
    size_t i;
    if (!append_json(cursor, remaining, "\"")) return false;
    if (!value) value = "";
    for (i = 0; i < max_length && value[i]; ++i) {
        const unsigned char ch = (unsigned char)value[i];
        switch (ch) {
        case '"':
            if (!append_json(cursor, remaining, "\\\"")) return false;
            break;
        case '\\':
            if (!append_json(cursor, remaining, "\\\\")) return false;
            break;
        case '\b':
            if (!append_json(cursor, remaining, "\\b")) return false;
            break;
        case '\f':
            if (!append_json(cursor, remaining, "\\f")) return false;
            break;
        case '\n':
            if (!append_json(cursor, remaining, "\\n")) return false;
            break;
        case '\r':
            if (!append_json(cursor, remaining, "\\r")) return false;
            break;
        case '\t':
            if (!append_json(cursor, remaining, "\\t")) return false;
            break;
        default:
            if (ch < 0x20) {
                if (!append_json(cursor, remaining, "\\u%04x", (unsigned)ch))
                    return false;
            } else if (!append_json(cursor, remaining, "%c", (int)ch)) {
                return false;
            }
            break;
        }
    }
    return append_json(cursor, remaining, "\"");
}

static bool send_all(int fd, const char *data, size_t length)
{
    while (length) {
#ifdef MSG_NOSIGNAL
        ssize_t sent = send(fd, data, length, MSG_NOSIGNAL);
#else
        ssize_t sent = send(fd, data, length, 0);
#endif
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) return false;
        data += sent;
        length -= (size_t)sent;
    }
    return true;
}

bool abfp_transport_publish_state(const abfp_snapshot *snapshot,
    const abfp_event *events, size_t event_count, bool initial_state)
{
    size_t capacity, remaining, i;
    char *buffer, *cursor;
    int fd;
    bool sent = false;

    if (!snapshot || (snapshot->cell_count && !snapshot->cells) ||
        (snapshot->entity_count && !snapshot->entities) ||
        (snapshot->message_count && !snapshot->messages) ||
        (snapshot->inventory_count && !snapshot->inventory) ||
        (snapshot->spell_count && !snapshot->spells) ||
        (snapshot->save_count && !snapshot->saves) ||
        (event_count && !events) || event_count > 4096) return false;
    capacity = 2048 + (size_t)snapshot->cell_count * 96 +
        (size_t)snapshot->entity_count * 512 +
        (size_t)snapshot->message_count * (ABFP_MAX_MESSAGE_LENGTH * 6 + 32) +
        (size_t)snapshot->inventory_count * 512 +
        (size_t)snapshot->spell_count * 256 +
        (size_t)snapshot->save_count * 384 +
        (snapshot->shop && snapshot->shop->active ? (1024 + (size_t)snapshot->shop->item_count * 512) : 64) +
        (snapshot->birth && snapshot->birth->active ? 6144 : 64) +
        event_count * 192;
    buffer = (char *)malloc(capacity);
    if (!buffer) return false;
    cursor = buffer;
    remaining = capacity;

    if (!append_json(&cursor, &remaining,
        "{\"type\":\"state\",\"turn\":%" PRIu64
        ",\"depth\":%d,\"initial\":%s,\"player\":[%d,%d]"
        ",\"player_stats\":[%d,%d,%d,%d,%d,%d,%u,%d,%d,%d,%d,%d],\"player_meta\":[%d,%u,%u,",
        snapshot->turn, snapshot->dungeon_depth,
        initial_state ? "true" : "false", snapshot->player_x, snapshot->player_y,
        snapshot->player.hitpoints, snapshot->player.max_hitpoints,
        snapshot->player.level, snapshot->player.gold,
        snapshot->player.mana, snapshot->player.max_mana,
        (unsigned)snapshot->player.is_dead,
        snapshot->player.exp, snapshot->player.max_exp,
        snapshot->player.armor_class, snapshot->player.speed,
        snapshot->player.food_pct,
        snapshot->player.new_spells,
        (unsigned)snapshot->player.can_choose_spells,
        (unsigned)snapshot->player.has_launcher) ||
        !append_json_string(&cursor, &remaining, snapshot->player.name, sizeof(snapshot->player.name)) ||
        !append_json(&cursor, &remaining, ",") ||
        !append_json_string(&cursor, &remaining, snapshot->player.race_name, sizeof(snapshot->player.race_name)) ||
        !append_json(&cursor, &remaining, ",") ||
        !append_json_string(&cursor, &remaining, snapshot->player.class_name, sizeof(snapshot->player.class_name)) ||
        !append_json(&cursor, &remaining, "],\"cells\":[")) goto done;

    for (i = 0; i < snapshot->cell_count; ++i) {
        const abfp_cell *cell = &snapshot->cells[i];
        if (!append_json(&cursor, &remaining,
            "%s[%d,%d,%d,%u,%u,%u,%u]",
            i ? "," : "", cell->x, cell->y, (int)cell->terrain,
            (unsigned)cell->visible, (unsigned)cell->lit,
            (unsigned)cell->occupied_by_monster,
            (unsigned)cell->occupied_by_object)) goto done;
    }
    if (!append_json(&cursor, &remaining, "],\"entities\":[")) goto done;
    for (i = 0; i < snapshot->entity_count; ++i) {
        const abfp_entity_state *entity = &snapshot->entities[i];
        if (!append_json(&cursor, &remaining,
            "%s[\"%" PRIu64 "\",%d,%d,%d,%d,%d,%d,%d,%d,%d,",
            i ? "," : "", entity->id, entity->x, entity->y,
            (int)entity->type, entity->kind_id, entity->category_id,
            entity->variant_id, entity->hitpoints, entity->max_hitpoints,
            entity->quantity)) goto done;
        if (!append_json_string(&cursor, &remaining, entity->name,
                sizeof(entity->name)) ||
            !append_json(&cursor, &remaining, "]")) goto done;
    }
    if (!append_json(&cursor, &remaining, "],\"events\":[")) goto done;
    for (i = 0; i < event_count; ++i) {
        const abfp_event *event = &events[i];
        if (!append_json(&cursor, &remaining,
            "%s[%d,%" PRIu64 ",\"%" PRIu64 "\",\"%" PRIu64 "\",%d,%d,%d,%d,%d]",
            i ? "," : "", (int)event->type, event->turn,
            event->subject_id, event->target_id, event->from_x,
            event->from_y, event->to_x, event->to_y, event->value)) goto done;
    }
    if (!append_json(&cursor, &remaining, "],\"messages\":[")) goto done;
    for (i = 0; i < snapshot->message_count; ++i) {
        const abfp_message *message = &snapshot->messages[i];
        if (!append_json(&cursor, &remaining, "%s[%d,", i ? "," : "",
                message->type) ||
            !append_json_string(&cursor, &remaining, message->text,
                sizeof(message->text)) ||
            !append_json(&cursor, &remaining, "]")) goto done;
    }
    if (!append_json(&cursor, &remaining, "],\"inventory\":[")) goto done;
    for (i = 0; i < snapshot->inventory_count; ++i) {
        const abfp_inventory_item *item = &snapshot->inventory[i];
        char label_str[2] = { item->label ? item->label : '?', '\0' };
        if (!append_json(&cursor, &remaining, "%s[%d,", i ? "," : "", item->index) ||
            !append_json_string(&cursor, &remaining, label_str, 1) ||
            !append_json(&cursor, &remaining, ",%u,%u,%d,%d,%d,%d,",
                (unsigned)item->equipped, (unsigned)item->flags,
                item->quantity, item->category_id, item->variant_id,
                item->sell_price) ||
            !append_json_string(&cursor, &remaining, item->slot_name,
                sizeof(item->slot_name)) ||
            !append_json(&cursor, &remaining, ",") ||
            !append_json_string(&cursor, &remaining, item->name,
                sizeof(item->name)) ||
            !append_json(&cursor, &remaining, "]")) goto done;
    }
    if (snapshot->shop && snapshot->shop->active) {
        const abfp_shop_state *shop = snapshot->shop;
        if (!append_json(&cursor, &remaining, "],\"shop\":{\"is_home\":%s,\"name\":",
                shop->is_home ? "true" : "false") ||
            !append_json_string(&cursor, &remaining, shop->name, sizeof(shop->name)) ||
            !append_json(&cursor, &remaining, ",\"owner\":") ||
            !append_json_string(&cursor, &remaining, shop->owner, sizeof(shop->owner)) ||
            !append_json(&cursor, &remaining, ",\"items\":[")) goto done;
        for (i = 0; i < shop->item_count; ++i) {
            const abfp_shop_item *si = &shop->items[i];
            if (!append_json(&cursor, &remaining, "%s[%d,%d,%d,%d,%d,",
                    i ? "," : "", si->index, si->quantity, si->price,
                    si->category_id, si->variant_id) ||
                !append_json_string(&cursor, &remaining, si->name, sizeof(si->name)) ||
                !append_json(&cursor, &remaining, "]")) goto done;
        }
        if (!append_json(&cursor, &remaining, "]}") ) goto done;
    } else {
        if (!append_json(&cursor, &remaining, "],\"shop\":null")) goto done;
    }

    if (!append_json(&cursor, &remaining, ",\"spells\":[")) goto done;
    for (i = 0; i < snapshot->spell_count; ++i) {
        const abfp_spell_entry *sp = &snapshot->spells[i];
        if (!append_json(&cursor, &remaining, "%s[%d,%d,%d,%d,%d,%u,",
                i ? "," : "", sp->index, sp->book_index, sp->level,
                sp->mana, sp->fail, (unsigned)sp->flags) ||
            !append_json_string(&cursor, &remaining, sp->name, sizeof(sp->name)) ||
            !append_json(&cursor, &remaining, ",") ||
            !append_json_string(&cursor, &remaining, sp->info, sizeof(sp->info)) ||
            !append_json(&cursor, &remaining, "]")) goto done;
    }

    if (snapshot->birth && snapshot->birth->active) {
        const abfp_birth_state *b = snapshot->birth;
        size_t s;
        if (!append_json(&cursor, &remaining,
                "],\"birth\":{\"rolled\":%s,\"race\":%d,\"class\":%d,\"points_left\":%d,\"hp\":%d,\"gold\":%d,\"name\":",
                b->rolled ? "true" : "false", b->race_index, b->class_index,
                b->points_left, b->hitpoints, b->gold) ||
            !append_json_string(&cursor, &remaining, b->name, sizeof(b->name)) ||
            !append_json(&cursor, &remaining, ",\"stats_base\":[")) goto done;
        for (s = 0; s < ABFP_NUM_STATS; ++s)
            if (!append_json(&cursor, &remaining, "%s%d", s ? "," : "", b->stats_base[s])) goto done;
        if (!append_json(&cursor, &remaining, "],\"stats_total\":[")) goto done;
        for (s = 0; s < ABFP_NUM_STATS; ++s)
            if (!append_json(&cursor, &remaining, "%s%d", s ? "," : "", b->stats_total[s])) goto done;
        if (!append_json(&cursor, &remaining, "],\"points_spent\":[")) goto done;
        for (s = 0; s < ABFP_NUM_STATS; ++s)
            if (!append_json(&cursor, &remaining, "%s%d", s ? "," : "", b->points_spent[s])) goto done;
        if (!append_json(&cursor, &remaining, "],\"points_inc\":[")) goto done;
        for (s = 0; s < ABFP_NUM_STATS; ++s)
            if (!append_json(&cursor, &remaining, "%s%d", s ? "," : "", b->points_inc[s])) goto done;
        if (!append_json(&cursor, &remaining, "],\"races\":[")) goto done;
        for (i = 0; i < b->race_count; ++i) {
            const abfp_birth_option *r = &b->races[i];
            if (!append_json(&cursor, &remaining, "%s[%d,", i ? "," : "", r->index) ||
                !append_json_string(&cursor, &remaining, r->name, sizeof(r->name)) ||
                !append_json(&cursor, &remaining, ",[%d,%d,%d,%d,%d],%d,%d,%u]",
                    r->stat_adj[0], r->stat_adj[1], r->stat_adj[2],
                    r->stat_adj[3], r->stat_adj[4],
                    r->hit_die, r->exp_factor, (unsigned)r->has_magic)) goto done;
        }
        if (!append_json(&cursor, &remaining, "],\"classes\":[")) goto done;
        for (i = 0; i < b->class_count; ++i) {
            const abfp_birth_option *c = &b->classes[i];
            if (!append_json(&cursor, &remaining, "%s[%d,", i ? "," : "", c->index) ||
                !append_json_string(&cursor, &remaining, c->name, sizeof(c->name)) ||
                !append_json(&cursor, &remaining, ",[%d,%d,%d,%d,%d],%d,%d,%u]",
                    c->stat_adj[0], c->stat_adj[1], c->stat_adj[2],
                    c->stat_adj[3], c->stat_adj[4],
                    c->hit_die, c->exp_factor, (unsigned)c->has_magic)) goto done;
        }
        if (!append_json(&cursor, &remaining, "]}")) goto done;
    } else {
        if (!append_json(&cursor, &remaining, "],\"birth\":null")) goto done;
    }

    if (!append_json(&cursor, &remaining, ",\"saves\":[")) goto done;
    for (i = 0; i < snapshot->save_count; ++i) {
        const abfp_save_entry *sv = &snapshot->saves[i];
        if (!append_json(&cursor, &remaining, "%s[", i ? "," : "") ||
            !append_json_string(&cursor, &remaining, sv->name, sizeof(sv->name)) ||
            !append_json(&cursor, &remaining, ",") ||
            !append_json_string(&cursor, &remaining, sv->description, sizeof(sv->description)) ||
            !append_json(&cursor, &remaining, ",%u,%u]",
                (unsigned)sv->is_current, (unsigned)sv->is_dead)) goto done;
    }
    if (!append_json(&cursor, &remaining, "]}\n")) goto done;

    pthread_mutex_lock(&send_mutex);
    pthread_mutex_lock(&mutex);
    fd = active_fd;
    pthread_mutex_unlock(&mutex);
    if (fd >= 0) {
        sent = send_all(fd, buffer, (size_t)(cursor - buffer));
        if (!sent) shutdown(fd, SHUT_RDWR);
    }
    pthread_mutex_unlock(&send_mutex);

done:
    free(buffer);
    return sent;
}
