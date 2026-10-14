#ifndef TUNA_LINK_H
#define TUNA_LINK_H
#include "tuna.h"
void link_port(unsigned port);
TunaError link_boot(const uint8_t *image, size_t length);
TunaError link_send_track(const uint8_t *track, size_t length);
#endif
