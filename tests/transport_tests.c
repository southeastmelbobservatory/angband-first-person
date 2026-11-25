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
    assert(abfp_transport_parse_command("ALTER_NORTH", &command));
    assert(command == ABFP_COMMAND_ALTER_NORTH);
    assert(abfp_transport_parse_command("ALTER_SOUTH_WEST\n", &command));
    assert(command == ABFP_COMMAND_ALTER_SOUTH_WEST);
    abfp_command full_cmd;
    assert(abfp_transport_parse_command_ex("REST\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_REST);
    assert(abfp_transport_parse_command_ex("USE 3\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_USE_ITEM && full_cmd.arg1 == 3 && full_cmd.arg2 == 1);
    assert(abfp_transport_parse_command_ex("BUY 2 5\r\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_SHOP_BUY && full_cmd.arg1 == 2 && full_cmd.arg2 == 5);
    assert(abfp_transport_parse_command_ex("SELL 0 1", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_SHOP_SELL && full_cmd.arg1 == 0 && full_cmd.arg2 == 1);
    assert(abfp_transport_parse_command_ex("FIRE -1 8 15 9\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_FIRE_ITEM && full_cmd.arg1 == -1 && full_cmd.arg2 == 8 &&
        full_cmd.arg3 == 15 && full_cmd.arg4 == 9);
    assert(abfp_transport_parse_command_ex("THROW 4 6 20 11", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_THROW_ITEM && full_cmd.arg1 == 4 && full_cmd.arg2 == 6 &&
        full_cmd.arg3 == 20 && full_cmd.arg4 == 11);
    assert(abfp_transport_parse_command_ex("CAST 0 2 10 14\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_CAST_SPELL && full_cmd.arg1 == 0 && full_cmd.arg2 == 2 &&
        full_cmd.arg3 == 10 && full_cmd.arg4 == 14);
    assert(abfp_transport_parse_command_ex("STUDY 2 5\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_STUDY_SPELL && full_cmd.arg1 == 2 && full_cmd.arg2 == 5);
    assert(abfp_transport_parse_command_ex("NEW_CHAR\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_NEW_CHAR);
    assert(abfp_transport_parse_command_ex("BIRTH_RACE 3\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_BIRTH_RACE && full_cmd.arg1 == 3);
    assert(abfp_transport_parse_command_ex("BIRTH_CLASS 1\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_BIRTH_CLASS && full_cmd.arg1 == 1);
    assert(abfp_transport_parse_command_ex("BIRTH_BUY 0\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_BIRTH_BUY_STAT && full_cmd.arg1 == 0);
    assert(abfp_transport_parse_command_ex("BIRTH_SELL 4\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_BIRTH_SELL_STAT && full_cmd.arg1 == 4);
    assert(abfp_transport_parse_command_ex("BIRTH_ROLL\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_BIRTH_ROLL);
    assert(abfp_transport_parse_command_ex("BIRTH_RESET\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_BIRTH_RESET_STATS && full_cmd.arg1 == 1);
    assert(abfp_transport_parse_command_ex("BIRTH_NAME Thorin Oak\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_BIRTH_NAME && strcmp(full_cmd.text, "Thorin Oak") == 0);
    assert(abfp_transport_parse_command_ex("BIRTH_ACCEPT Gandalf\n", &full_cmd));
    assert(full_cmd.type == ABFP_COMMAND_BIRTH_ACCEPT && strcmp(full_cmd.text, "Gandalf") == 0);
    assert(!abfp_transport_parse_command_ex("BUY -1 1", &full_cmd));
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
    abfp_message message = {0};
    abfp_inventory_item inv_item = {0};
    abfp_shop_state shop = {0};
    abfp_snapshot snapshot = {0};
    abfp_command_type command = ABFP_COMMAND_WAIT;
    char response[8192];
    size_t used = 0;
    int fd;
    ssize_t n;

    cell.x = 12;
    cell.y = 7;
    cell.terrain = ABFP_TERRAIN_DOOR_OPEN;
    cell.visible = 1;
    cell.lit = 1;
    message.type = 2;
    strcpy(message.text, "You open the \"oak\" door.");
    inv_item.index = 0;
    inv_item.label = 'a';
    inv_item.equipped = 0;
    inv_item.flags = ABFP_ITEM_FLAG_USABLE | ABFP_ITEM_FLAG_POTION | ABFP_ITEM_FLAG_SELLABLE;
    inv_item.quantity = 2;
    inv_item.sell_price = 15;
    strcpy(inv_item.name, "2 Potions of Cure Light Wounds");
    shop.active = 1;
    shop.is_home = 0;
    strcpy(shop.name, "General Store");
    strcpy(shop.owner, "Bilbo");
    shop.item_count = 1;
    shop.items[0].index = 0;
    shop.items[0].quantity = 5;
    shop.items[0].price = 25;
    strcpy(shop.items[0].name, "5 Rations of Food");

    snapshot.turn = 99;
    snapshot.dungeon_depth = 5;
    snapshot.player_x = 12;
    snapshot.player_y = 7;
    snapshot.player.hitpoints = 31;
    snapshot.player.max_hitpoints = 45;
    snapshot.player.level = 7;
    snapshot.player.gold = 1234;
    snapshot.player.mana = 12;
    snapshot.player.max_mana = 20;
    snapshot.player.is_dead = 0;
    snapshot.player.exp = 500;
    snapshot.player.max_exp = 500;
    snapshot.player.armor_class = 18;
    snapshot.player.speed = 0;
    snapshot.player.food_pct = 88;
    snapshot.cells = &cell;
    snapshot.cell_count = 1;
    snapshot.messages = &message;
    snapshot.message_count = 1;
    snapshot.inventory = &inv_item;
    snapshot.inventory_count = 1;
    snapshot.shop = &shop;

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
        if (memchr(response, '\n', used)) break;
    }
    response[used] = '\0';
    assert(strstr(response, "\"type\":\"state\"") != NULL);
    assert(strstr(response, "\"turn\":99") != NULL);
    assert(strstr(response, "\"depth\":5") != NULL);
    assert(strstr(response, "\"player\":[12,7]") != NULL);
    assert(strstr(response, "\"player_stats\":[31,45,7,1234,12,20,0,500,500,18,0,88]") != NULL);
    assert(strstr(response, "\"messages\":[[2,\"You open the \\\"oak\\\" door.\"]]") != NULL);
    assert(strstr(response, "\"inventory\":[[0,\"a\",0,") != NULL);
    assert(strstr(response, "\"shop\":{\"is_home\":false,\"name\":\"General Store\",\"owner\":\"Bilbo\"") != NULL);
    assert(strstr(response, "\"initial\":true") != NULL);

    assert(send(fd, "INVALID\nWAIT\n", 13, 0) == 13);
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
