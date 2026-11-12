#define _POSIX_C_SOURCE 200809L
#include "abfp_transport.h"
#include <arpa/inet.h>
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
#define ABFP_LINE_CAPACITY 64
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static abfp_command_type queue[ABFP_QUEUE_CAPACITY];
static size_t head, count;
static int listen_fd = -1;
static int active_fd = -1;
static pthread_t worker;
static bool running, worker_started;

bool abfp_transport_parse_command(const char *line, abfp_command_type *out)
{
    static const struct { const char *name; abfp_command_type command; } names[] = {
        {"WAIT", ABFP_COMMAND_WAIT},
        {"MOVE_NORTH", ABFP_COMMAND_MOVE_NORTH},
        {"MOVE_SOUTH", ABFP_COMMAND_MOVE_SOUTH},
        {"MOVE_WEST", ABFP_COMMAND_MOVE_WEST},
        {"MOVE_EAST", ABFP_COMMAND_MOVE_EAST},
        {"MOVE_NORTH_WEST", ABFP_COMMAND_MOVE_NORTH_WEST},
        {"MOVE_NORTH_EAST", ABFP_COMMAND_MOVE_NORTH_EAST},
        {"MOVE_SOUTH_WEST", ABFP_COMMAND_MOVE_SOUTH_WEST},
        {"MOVE_SOUTH_EAST", ABFP_COMMAND_MOVE_SOUTH_EAST}
    };
    size_t i, length;
    if (!line || !out) return false;
    length = strcspn(line, "\r\n");
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (strlen(names[i].name) == length && memcmp(line, names[i].name, length) == 0) {
            *out = names[i].command;
            return true;
        }
    }
    return false;
}
static bool enqueue(abfp_command_type command)
{
    bool accepted = false;
    pthread_mutex_lock(&mutex);
    if (running && count < ABFP_QUEUE_CAPACITY) {
        queue[(head + count) % ABFP_QUEUE_CAPACITY] = command;
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
    while ((n = recv(fd, buffer, sizeof(buffer), 0)) > 0) {
        ssize_t i;
        for (i = 0; i < n; ++i) {
            char ch = buffer[i];
            if (ch == '\n') {
                abfp_command_type command;
                if (!discarding) {
                    line[used] = '\0';
                    if (abfp_transport_parse_command(line, &command)) (void)enqueue(command);
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
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
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
bool abfp_transport_wait_for_client(void)
{
    pthread_mutex_lock(&mutex);
    while (running && active_fd < 0) pthread_cond_wait(&ready, &mutex);
    const bool connected = active_fd >= 0;
    pthread_mutex_unlock(&mutex);
    return connected;
}

bool abfp_transport_wait_command(abfp_command_type *out)
{
    if (!out) return false;
    pthread_mutex_lock(&mutex);
    while (running && count == 0 && active_fd >= 0) pthread_cond_wait(&ready, &mutex);
    if (count == 0) { pthread_mutex_unlock(&mutex); return false; }
    *out = queue[head]; head = (head + 1) % ABFP_QUEUE_CAPACITY; --count;
    pthread_mutex_unlock(&mutex); return true;
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
        (event_count && !events) || event_count > 4096) return false;
    capacity = 512 + (size_t)snapshot->cell_count * 96 +
        (size_t)snapshot->entity_count * 192 + event_count * 192;
    buffer = (char *)malloc(capacity);
    if (!buffer) return false;
    cursor = buffer;
    remaining = capacity;

    if (!append_json(&cursor, &remaining,
        "{\"type\":\"state\",\"turn\":%" PRIu64
        ",\"depth\":%d,\"initial\":%s,\"player\":[%d,%d],\"cells\":[",
        snapshot->turn, snapshot->dungeon_depth,
        initial_state ? "true" : "false", snapshot->player_x, snapshot->player_y)) goto done;

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
            "%s[\"%" PRIu64 "\",%d,%d,%d,%d,%d,%d,%d,%d,%d]",
            i ? "," : "", entity->id, entity->x, entity->y,
            (int)entity->type, entity->kind_id, entity->category_id,
            entity->variant_id, entity->hitpoints, entity->max_hitpoints,
            entity->quantity)) goto done;
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
