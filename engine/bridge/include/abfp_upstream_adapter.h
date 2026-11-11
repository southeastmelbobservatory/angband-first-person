#pragma once

#include "abfp_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

bool abfp_upstream_queue_command(abfp_command_type command);
bool abfp_upstream_install_handlers(void);
void abfp_upstream_remove_handlers(void);
bool abfp_upstream_refresh_snapshot(void);

#ifdef __cplusplus
}
#endif
