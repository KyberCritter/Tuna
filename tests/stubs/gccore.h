#ifndef TUNA_TEST_GCCORE_H
#define TUNA_TEST_GCCORE_H
#include <stdint.h>
typedef int32_t s32;
typedef uint32_t u32;
#define ATTRIBUTE_ALIGN(n)
#define SI_CHAN_BIT(n) (1u << (7u - (n)))
void SI_DisablePolling(uint32_t mask);
int SI_IsChanBusy(s32 channel);
int SI_Transfer(s32 channel, void *out, u32 out_length, void *in, u32 in_length,
                void (*callback)(s32, u32), u32 delay);
#endif
