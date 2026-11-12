#include "../include/abfp_adapter.h"

#ifdef ABFP_WITH_ANGBAND
#include "../include/abfp_upstream_adapter.h"
#endif

bool abfp_adapter_init(void)
{
    return abfp_bridge_init();
}

bool abfp_adapter_execute(abfp_command_type command)
{
    return abfp_bridge_submit_command(command);
}

bool abfp_adapter_refresh_snapshot(void)
{
#ifdef ABFP_WITH_ANGBAND
    return abfp_upstream_refresh_snapshot();
#else
    return false;
#endif
}
