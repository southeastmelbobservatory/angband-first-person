#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "commands.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Loopback-only command ingress. Worker parses/enqueues; game thread consumes. */
bool abfp_transport_parse_command(const char *line, abfp_command_type *out);
bool abfp_transport_start(uint16_t port);
bool abfp_transport_wait_command(abfp_command_type *out);
void abfp_transport_stop(void);
#ifdef __cplusplus
}
#endif
