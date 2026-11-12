#include "abfp_bridge.h"
#include "abfp_transport.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    abfp_cell cell = {0};
    abfp_entity_state entity = {0};
    abfp_snapshot snapshot = {0};
    abfp_event event = {0};
    size_t i;

    abfp_command_type command;

    assert(abfp_transport_parse_command("MOVE_NORTH", &command));
    assert(command == ABFP_COMMAND_MOVE_NORTH);
    assert(abfp_transport_parse_command("WAIT", &command));
    assert(command == ABFP_COMMAND_WAIT);
    assert(!abfp_transport_parse_command("MOVE_UP", &command));
    assert(!abfp_transport_parse_command(NULL, &command));

    assert(abfp_bridge_init());

    cell.x = 12;
    cell.y = 7;
    cell.terrain = ABFP_TERRAIN_FLOOR;
    cell.visible = 1;
    cell.lit = 1;

    entity.id = UINT64_C(0x1000000000000001);
    entity.x = 12;
    entity.y = 7;
    entity.type = ABFP_ENTITY_MONSTER;
    entity.kind_id = 42;
    entity.hitpoints = 8;
    entity.max_hitpoints = 12;
    entity.quantity = 1;

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
    snapshot.entities = &entity;
    snapshot.entity_count = 1;
    abfp_bridge_set_snapshot(&snapshot);

    assert(abfp_bridge_snapshot()->turn == 99);
    assert(abfp_bridge_snapshot()->player.hitpoints == 31);
    assert(abfp_bridge_snapshot()->player.max_hitpoints == 45);
    assert(abfp_bridge_snapshot()->player.level == 7);
    assert(abfp_bridge_snapshot()->player.gold == 1234);
    assert(abfp_bridge_snapshot()->cells == &cell);
    assert(abfp_bridge_snapshot()->cell_count == 1);
    assert(abfp_bridge_snapshot()->entities == &entity);
    assert(abfp_bridge_snapshot()->entity_count == 1);
    assert(abfp_bridge_snapshot()->entities[0].kind_id == 42);

    event.type = ABFP_EVENT_ENTITY_MOVED;
    event.subject_id = 1;
    event.from_x = 12;
    event.from_y = 7;
    event.to_x = 13;
    event.to_y = 7;
    event.turn = 100;

    for (i = 0; i < 130; ++i) {
        event.value = (int32_t)i;
        abfp_bridge_emit(&event);
    }

    assert(abfp_bridge_event_count() == 130);
    assert(abfp_bridge_event_at(0) != NULL);
    assert(abfp_bridge_event_at(0)->value == 0);
    assert(abfp_bridge_event_at(129)->value == 129);
    assert(abfp_bridge_event_at(130) == NULL);

    abfp_bridge_clear_events();
    assert(abfp_bridge_event_count() == 0);

    abfp_bridge_shutdown();
    puts("ABFP bridge tests passed");
    return 0;
}
