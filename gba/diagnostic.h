#ifndef TUNA_DIAGNOSTIC_H
#define TUNA_DIAGNOSTIC_H
#include "tuna.h"
void diagnostic_init(TunaReceiver *receiver, uint8_t *storage);
void diagnostic_service(unsigned frame);
#endif
