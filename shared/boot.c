/* Joybus cipher/key derivation adapted from FIX94's MIT-licensed sender.
 * Copyright (C) 2018 FIX94. See LICENSES/FIX94-MIT.txt. */
#include "tuna.h"
uint32_t tuna_boot_key(uint32_t size) {
    uint32_t value, sum, key = 0, mask; unsigned i;
    if (size < 0x200 || size > 0x40000 || (size & 7)) return 0;
    size = (size - 0x200) >> 3;
    value = ((size & 0x3f80) << 1) | ((size & 0x4000) << 2) | (size & 0x7f) | 0x380000;
    sum = ((value + (value >> 8) + (value >> 16)) << 24) | value | 0x80808080;
    mask = (sum & 0x200) ? 0x7365646f : 0x4b617761;
    for (i = 0; i < 4; ++i) key |= ((sum >> (8 * i)) & 255) << (24 - 8 * i);
    return key ^ mask;
}
uint32_t tuna_boot_crc(uint32_t crc, uint32_t word) {
    unsigned i;
    for (i = 0; i < 32; ++i) { crc = (crc >> 1) ^ (((crc ^ word) & 1) ? 0xa1c1 : 0); word >>= 1; }
    return crc;
}
uint32_t tuna_boot_encrypt(uint32_t word, uint32_t offset, uint32_t *key) {
    *key = *key * 0x6177614b + 1;
    return word ^ *key ^ (0u - (offset + 0x02000000u)) ^ 0x20796220u;
}
