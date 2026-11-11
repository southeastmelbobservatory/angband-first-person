#include "../include/abfp_adapter.h"

/*
 * Upstream adapter skeleton.
 *
 * The implementation is intentionally deferred until the exact pinned Angband
 * revision is present under engine/upstream/angband. That avoids guessing
 * internal APIs and keeps all version-sensitive code isolated in this file.
 */

bool abfp_adapter_init(void)
{
    return false;
}

bool abfp_adapter_execute(abfp_command_type command)
{
    (void)command;
    return false;
}

bool abfp_adapter_refresh_snapshot(void)
{
    return false;
}
