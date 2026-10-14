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
    return tuna_profile_size(samples, TUNA_IMA4);
}
unsigned tuna_profile_block(uint8_t codec) {
    return codec == TUNA_IMA4 ? 132 : codec == TUNA_ADPCM2 ? 68 : codec == TUNA_DELTA1 ? 36 : 0;
}
uint32_t tuna_profile_size(uint32_t samples, uint8_t codec) {
    uint32_t blocks = samples / TUNA_BLOCK_SAMPLES + !!(samples % TUNA_BLOCK_SAMPLES);
    unsigned bytes = tuna_profile_block(codec);
    return !bytes || blocks > UINT32_MAX / bytes ? 0 : blocks * bytes;
}
const char *tuna_profile_name(uint8_t codec) {
    return codec == TUNA_IMA4 ? "4-bit IMA ADPCM" : codec == TUNA_ADPCM2 ? "2-bit Tuna ADPCM (lo-fi)" : "1-bit adaptive delta (very lo-fi)";
}
TunaError tuna_choose_profile(uint32_t count, uint32_t rate, TunaTrack *t) {
    static const uint8_t codecs[] = {TUNA_IMA4, TUNA_ADPCM2, TUNA_ADPCM2, TUNA_DELTA1};
    static const uint32_t rates[] = {8192,8192,4096,4096};
    unsigned i;
    if (!rate || !count) return TUNA_FORMAT;
    if ((uint64_t)count > (uint64_t)rate * TUNA_MAX_SECONDS) return TUNA_SIZE;
    for (i = 0; i < 4; ++i) {
        uint32_t n = (uint32_t)((uint64_t)count * rates[i] / rate), size = tuna_profile_size(n, codecs[i]);
        if (n && size && size <= TUNA_CAPACITY - TUNA_HEADER) {
            t->samples = n; t->bytes = size; t->codec = codecs[i]; t->rate = rates[i]; return TUNA_OK;
        }
    }
    return TUNA_SIZE;
}
static uint32_t header_crc(const uint8_t *p) {
    const uint8_t zero[4] = {0};
    uint32_t c = tuna_crc_update(UINT32_MAX, p, 28);
    c = tuna_crc_update(c, zero, 4);
    return ~tuna_crc_update(c, p + 32, 64);
}
TunaError tuna_header_write(uint8_t out[TUNA_HEADER], const TunaTrack *t) {
    size_t len = 0; uint8_t codec = t->codec ? t->codec : TUNA_IMA4; uint32_t rate = t->rate ? t->rate : TUNA_RATE;
    while (len <= TUNA_TITLE && t->title[len]) ++len;
    if (len > TUNA_TITLE || !t->samples || !tuna_profile_block(codec) || t->bytes != tuna_profile_size(t->samples, codec) ||
        (codec == TUNA_IMA4 && rate != 8192) || (codec == TUNA_ADPCM2 && rate != 8192 && rate != 4096) ||
        (codec == TUNA_DELTA1 && rate != 4096) || t->samples > rate * TUNA_MAX_SECONDS) return TUNA_FORMAT;
    memset(out, 0, TUNA_HEADER); memcpy(out, "TUNA", 4);
    tuna_put16(out + 4, TUNA_VERSION); tuna_put16(out + 6, TUNA_HEADER);
    out[8] = codec; tuna_put32(out + 12, rate); tuna_put32(out + 16, t->samples);
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
        tuna_u16(p + 6) != TUNA_HEADER || !tuna_profile_block(p[8]) || p[9] || p[10] || p[11] ||
        p[24] > 60 || p[25] || p[26] || p[27]) return TUNA_FORMAT;
    if (tuna_u32(p + 28) != header_crc(p)) return TUNA_CRC;
    memset(t, 0, sizeof(*t)); t->samples = tuna_u32(p + 16); t->bytes = tuna_u32(p + 20); t->crc = tuna_u32(p + 92);
    t->codec = p[8]; t->rate = tuna_u32(p + 12);
    if ((t->codec == TUNA_IMA4 && t->rate != 8192) || (t->codec == TUNA_ADPCM2 && t->rate != 8192 && t->rate != 4096) ||
        (t->codec == TUNA_DELTA1 && t->rate != 4096)) return TUNA_FORMAT;
    if (!t->samples || !t->bytes || t->samples > t->rate * TUNA_MAX_SECONDS || t->bytes != tuna_profile_size(t->samples, t->codec)) return TUNA_FORMAT;
    if (capacity < TUNA_HEADER || t->bytes > capacity - TUNA_HEADER) return TUNA_SIZE;
    for (i = 0; i < 60; ++i) {
        if (i < p[24]) { if (p[32 + i] < 32 || p[32 + i] > 126) return TUNA_FORMAT; t->title[i] = (char)p[32 + i]; }
        else if (p[32 + i]) return TUNA_FORMAT;
    }
    return TUNA_OK;
}
TunaError tuna_track_validate(const uint8_t *data, size_t n, TunaTrack *t) {
    uint32_t i; unsigned block; TunaError e = tuna_header_read(t, data, n, TUNA_CAPACITY);
    if (e) return e;
    if (n != TUNA_HEADER + t->bytes) return TUNA_SIZE;
    if (tuna_crc(data + TUNA_HEADER, t->bytes) != t->crc) return TUNA_CRC;
    block = tuna_profile_block(t->codec);
    for (i = 0; i < t->bytes; i += block)
        if (data[TUNA_HEADER + i + 2] > 88 || data[TUNA_HEADER + i + 3] ||
            (data[TUNA_HEADER + i + block - 1] & (t->codec == TUNA_IMA4 ? 0xf0 : t->codec == TUNA_ADPCM2 ? 0xc0 : 0x80))) return TUNA_FORMAT;
    return TUNA_OK;
}
const char *tuna_error(TunaError e) {
    static const char *const names[] = {"OK", "Invalid format", "Checksum mismatch", "Track does not fit", "Invalid state", "Sequence mismatch", "Link timeout", "Storage/decoder error"};
    return (unsigned)e < sizeof(names) / sizeof(names[0]) ? names[e] : "Unknown error";
}
