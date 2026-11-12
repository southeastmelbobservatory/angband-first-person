#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
bool abfp_host_install(uint16_t port);
void abfp_host_game_closed(void);
void abfp_host_shutdown(void);
#ifdef __cplusplus
}
#endif
