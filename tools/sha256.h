/* SPDX-License-Identifier: MIT */
#ifndef TUNA_SHA256_H
#define TUNA_SHA256_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t state[8];
    uint64_t bytes;
    uint8_t buffer[64];
    unsigned used;
} TunaSha256;
void tuna_sha256_init(TunaSha256 *s);
void tuna_sha256_update(TunaSha256 *s, const void *data, size_t length);
void tuna_sha256_finish(TunaSha256 *s, uint8_t digest[32]);
void tuna_sha256_hex(const uint8_t digest[32], char text[65]);
#endif
