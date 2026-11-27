#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
bool abfp_host_install(uint16_t port);
void abfp_host_rebind_hooks(void);
bool abfp_host_run_birth(void);
bool abfp_host_has_load_request(void);
void abfp_host_consume_restart(char *next_savefile, size_t next_savefile_len,
    bool *out_load_save, bool *out_skip_autosave);
bool abfp_host_handle_game_end(void);
bool abfp_host_publish_state(bool initial_state);
void abfp_host_game_closed(void);
void abfp_host_shutdown(void);
#ifdef __cplusplus
}
#endif
