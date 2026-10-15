#ifndef TUNA_TEST_WATCHDOG_H
#define TUNA_TEST_WATCHDOG_H
#include <stdint.h>
uint64_t gettime(void);
uint64_t ticks_to_millisecs(uint64_t ticks);
#endif
