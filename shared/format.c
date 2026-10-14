#include "tuna.h"
#include <string.h>
#include <limits.h>

uint16_t tuna_u16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
uint32_t tuna_u32(const uint8_t *p) { return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
void tuna_put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
void tuna_put32(uint8_t *p, uint32_t v) { unsigned i; for (i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }
uint32_t tuna_crc_update(uint32_t c, const void *data, size_t n) {
    const uint8_t *p = (const uint8_t *)data;
    while (n--) { unsigned i; c ^= *p++; for (i = 0; i < 8; ++i) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u))); }
    return c;
}
uint32_t tuna_crc(const void *data, size_t n) { return ~tuna_crc_update(UINT32_MAX, data, n); }
uint32_t tuna_encoded_size(uint32_t samples) {
    uint32_t blocks = samples / TUNA_BLOCK_SAMPLES + !!(samples % TUNA_BLOCK_SAMPLES);
    return blocks > UINT32_MAX / TUNA_BLOCK_BYTES ? 0 : blocks * TUNA_BLOCK_BYTES;
}
static uint32_t header_crc(const uint8_t *p) {
    const uint8_t zero[4] = {0};
    uint32_t c = tuna_crc_update(UINT32_MAX, p, 28);
    c = tuna_crc_update(c, zero, 4);
    return ~tuna_crc_update(c, p + 32, 64);
}
TunaError tuna_header_write(uint8_t out[TUNA_HEADER], const TunaTrack *t) {
    size_t len = 0;
    while (len <= TUNA_TITLE && t->title[len]) ++len;
    if (len > TUNA_TITLE || !t->samples || t->bytes != tuna_encoded_size(t->samples)) return TUNA_FORMAT;
    memset(out, 0, TUNA_HEADER); memcpy(out, "TUNA", 4);
    tuna_put16(out + 4, TUNA_VERSION); tuna_put16(out + 6, TUNA_HEADER);
    out[8] = 1; tuna_put32(out + 12, TUNA_RATE); tuna_put32(out + 16, t->samples);
    tuna_put32(out + 20, t->bytes); out[24] = (uint8_t)len;
    /* Payload CRC precedes the title; header CRC covers all metadata. */
    tuna_put32(out + 92, t->crc);
    /* Title is 60 bytes on the wire, bounded printable ASCII. */
    if (len > 60) return TUNA_FORMAT;
    memcpy(out + 32, t->title, len);
    tuna_put32(out + 28, header_crc(out));
    return TUNA_OK;
}
TunaError tuna_header_read(TunaTrack *t, const uint8_t *p, size_t n, uint32_t capacity) {
    unsigned i;
    if (n < TUNA_HEADER || memcmp(p, "TUNA", 4) || tuna_u16(p + 4) != TUNA_VERSION ||
        tuna_u16(p + 6) != TUNA_HEADER || p[8] != 1 || p[9] || p[10] || p[11] ||
        tuna_u32(p + 12) != TUNA_RATE || p[24] > 60 || p[25] || p[26] || p[27]) return TUNA_FORMAT;
    if (tuna_u32(p + 28) != header_crc(p)) return TUNA_CRC;
    memset(t, 0, sizeof(*t)); t->samples = tuna_u32(p + 16); t->bytes = tuna_u32(p + 20); t->crc = tuna_u32(p + 92);
    if (!t->samples || !t->bytes || t->bytes != tuna_encoded_size(t->samples)) return TUNA_FORMAT;
    if (capacity < TUNA_HEADER || t->bytes > capacity - TUNA_HEADER) return TUNA_SIZE;
    for (i = 0; i < 60; ++i) {
        if (i < p[24]) { if (p[32 + i] < 32 || p[32 + i] > 126) return TUNA_FORMAT; t->title[i] = (char)p[32 + i]; }
        else if (p[32 + i]) return TUNA_FORMAT;
    }
    return TUNA_OK;
}
TunaError tuna_track_validate(const uint8_t *data, size_t n, TunaTrack *t) {
    uint32_t i; TunaError e = tuna_header_read(t, data, n, TUNA_CAPACITY);
    if (e) return e;
    if (n != TUNA_HEADER + t->bytes) return TUNA_SIZE;
    if (tuna_crc(data + TUNA_HEADER, t->bytes) != t->crc) return TUNA_CRC;
    for (i = 0; i < t->bytes; i += TUNA_BLOCK_BYTES)
        if (data[TUNA_HEADER + i + 2] > 88 || data[TUNA_HEADER + i + 3] || (data[TUNA_HEADER + i + 131] & 0xf0)) return TUNA_FORMAT;
    return TUNA_OK;
}
const char *tuna_error(TunaError e) {
    static const char *const names[] = {"OK", "Invalid format", "Checksum mismatch", "Track does not fit", "Invalid state", "Sequence mismatch", "Link timeout", "Storage/decoder error"};
    return (unsigned)e < sizeof(names) / sizeof(names[0]) ? names[e] : "Unknown error";
}
