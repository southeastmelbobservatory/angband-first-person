#include "../include/abfp_frontend.h"
#include "../include/abfp_adapter.h"

/* The host owns and advances Angband's normal game loop. This layer only
 * initializes the bridge; it never runs a second simulation. */
bool abfp_frontend_register(void)
{
    return abfp_adapter_init();
}

void abfp_frontend_begin_frame(void) {}
void abfp_frontend_end_frame(void) {}
