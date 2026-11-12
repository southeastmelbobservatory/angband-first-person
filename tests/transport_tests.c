#include "abfp_transport.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_parse(void)
{
    abfp_command_type command;

    assert(abfp_transport_parse_command("MOVE_NORTH\n", &command));
    assert(command == ABFP_COMMAND_MOVE_NORTH);
    assert(abfp_transport_parse_command("MOVE_SOUTH_EAST\r\n", &command));
    assert(command == ABFP_COMMAND_MOVE_SOUTH_EAST);
    assert(abfp_transport_parse_command("WAIT", &command));
    assert(command == ABFP_COMMAND_WAIT);
    assert(!abfp_transport_parse_command("MOVE_UP", &command));
    assert(!abfp_transport_parse_command("", &command));
    assert(!abfp_transport_parse_command(NULL, &command));
    assert(!abfp_transport_parse_command("WAIT", NULL));
}

int main(void)
{
    test_parse();
    puts("abfp_transport_tests: OK");
    return 0;
}
