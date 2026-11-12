#define _POSIX_C_SOURCE 200809L
#include "abfp_transport.h"
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define ABFP_QUEUE_CAPACITY 64
#define ABFP_LINE_CAPACITY 64
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
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
        int fd = accept(listen_fd, NULL, NULL);
        bool serve;
        if (fd < 0) { if (errno == EINTR) continue; break; }
        pthread_mutex_lock(&mutex);
        active_fd = fd;
        serve = running;
        if (!serve) shutdown(fd, SHUT_RDWR);
        pthread_mutex_unlock(&mutex);
        if (serve) serve_client(fd);
        close(fd);
        pthread_mutex_lock(&mutex); active_fd = -1; pthread_mutex_unlock(&mutex);
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
bool abfp_transport_wait_command(abfp_command_type *out)
{
    if (!out) return false;
    pthread_mutex_lock(&mutex);
    while (running && count == 0) pthread_cond_wait(&ready, &mutex);
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
