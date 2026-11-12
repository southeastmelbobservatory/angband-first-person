#define _POSIX_C_SOURCE 200809L
#include "abfp_transport.h"

#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static void test_parse(void)
{
    abfp_command_type command;

    assert(abfp_transport_parse_command("MOVE_NORTH\n", &command));
    assert(command == ABFP_COMMAND_MOVE_NORTH);
    assert(abfp_transport_parse_command("MOVE_SOUTH_EAST\r\n", &command));
    assert(command == ABFP_COMMAND_MOVE_SOUTH_EAST);
    assert(abfp_transport_parse_command("WAIT", &command));
    assert(command == ABFP_COMMAND_WAIT);
    assert(abfp_transport_parse_command("GO_UP", &command));
    assert(command == ABFP_COMMAND_GO_UP);
    assert(abfp_transport_parse_command("GO_DOWN", &command));
    assert(command == ABFP_COMMAND_GO_DOWN);
    assert(abfp_transport_parse_command("AUTOPICKUP", &command));
    assert(command == ABFP_COMMAND_AUTOPICKUP);
    assert(!abfp_transport_parse_command("MOVE_UP", &command));
    assert(!abfp_transport_parse_command("", &command));
    assert(!abfp_transport_parse_command(NULL, &command));
    assert(!abfp_transport_parse_command("WAIT", NULL));
}

static void test_loopback_round_trip(void)
{
    const uint16_t port = 47653;
    struct sockaddr_in address = {0};
    struct timeval timeout = {3, 0};
    abfp_cell cell = {0};
    abfp_snapshot snapshot = {0};
    abfp_command_type command = ABFP_COMMAND_WAIT;
    char response[4096];
    size_t used = 0;
    int fd;
    ssize_t n;

    cell.x = 12;
    cell.y = 7;
    cell.terrain = ABFP_TERRAIN_FLOOR;
    cell.visible = 1;
    cell.lit = 1;
    snapshot.turn = 99;
    snapshot.dungeon_depth = 5;
    snapshot.player_x = 12;
    snapshot.player_y = 7;
    snapshot.player.hitpoints = 31;
    snapshot.player.max_hitpoints = 45;
    snapshot.player.level = 7;
    snapshot.player.gold = 1234;
    snapshot.cells = &cell;
    snapshot.cell_count = 1;

    assert(abfp_transport_start(port));

    fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    assert(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    assert(connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0);
    assert(abfp_transport_wait_for_client());

    assert(abfp_transport_publish_state(&snapshot, NULL, 0, true));
    while (used + 1 < sizeof(response)) {
        n = recv(fd, response + used, sizeof(response) - used - 1, 0);
        assert(n > 0);
        used += (size_t)n;
        if (memchr(response, '\\n', used)) break;
    }
    response[used] = '\\0';
    assert(strstr(response, "\\"type\\":\\"state\\"") != NULL);
    assert(strstr(response, "\\"turn\\":99") != NULL);
    assert(strstr(response, "\\"depth\\":5") != NULL);
    assert(strstr(response, "\\"player\\":[12,7]") != NULL);
    assert(strstr(response, "\\"initial\\":true") != NULL);

    assert(send(fd, "INVALID\\nWAIT\\n", 13, 0) == 13);
    assert(abfp_transport_wait_command(&command));
    assert(command == ABFP_COMMAND_WAIT);

    close(fd);
    abfp_transport_stop();
}

int main(void)
{
    test_parse();
    test_loopback_round_trip();
    puts("abfp_transport_tests: parser and loopback round-trip passed");
    return 0;
}
