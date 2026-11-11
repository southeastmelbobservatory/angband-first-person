#pragma once

#include "abfp_bridge.h"

/*
 * This is the ONLY header that should directly depend on upstream Angband
 * headers once integration begins.
 *
 * The protocol and presentation client must not include Angband internals.
 */

#ifdef __cplusplus
extern "C" {
#endif

bool abfp_adapter_init(void);
bool abfp_adapter_execute(abfp_command_type command);
bool abfp_adapter_refresh_snapshot(void);

#ifdef __cplusplus
}
#endif
