#include "../include/abfp_frontend.h"

/*
 * Thin frontend registration layer.
 *
 * Once the exact Angband revision is present locally this layer will register
 * for upstream events and translate ABFP commands into the existing command
 * queue. It must never run a second simulation loop.
 */
bool abfp_frontend_register(void) { return false; }
void abfp_frontend_begin_frame(void) {}
void abfp_frontend_end_frame(void) {}
