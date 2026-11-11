#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool abfp_frontend_register(void);
void abfp_frontend_begin_frame(void);
void abfp_frontend_end_frame(void);
#ifdef __cplusplus
}
#endif
