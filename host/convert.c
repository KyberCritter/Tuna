#include "convert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "minimp3.h"

#define MAX_MP3 (8u * 1024u * 1024u)
#define MAX_SAMPLES (((TUNA_CAPACITY - TUNA_HEADER) / TUNA_BLOCK_BYTES) * TUNA_BLOCK_SAMPLES)
static int16_t sample(void *context, uint32_t i) { return ((const int16_t *)context)[i]; }
TunaError tuna_convert_pcm(const int16_t *mono, uint32_t count, uint32_t rate, const char *title, uint8_t **track, size_t *length) {
    TunaTrack info; uint64_t samples; uint32_t pos; uint8_t *data; int16_t pcm[256]; unsigned i;
    *track = NULL; *length = 0;
    if (!mono || !count || rate < 8000 || rate > 48000) return TUNA_FORMAT;
    samples = (uint64_t)count * TUNA_RATE / rate;
    if (!samples) return TUNA_FORMAT;
    if (samples > MAX_SAMPLES) return TUNA_SIZE;
    memset(&info, 0, sizeof(info)); info.samples = (uint32_t)samples; info.bytes = tuna_encoded_size(info.samples);
    for (i = 0; i < TUNA_TITLE && title[i]; ++i) info.title[i] = title[i] >= 32 && title[i] <= 126 ? title[i] : '?';
    data = (uint8_t *)malloc(TUNA_HEADER + info.bytes);
    if (!data) return TUNA_IO;
    for (pos = 0; pos < info.samples; pos += 256) {
        unsigned n = info.samples - pos > 256 ? 256 : info.samples - pos;
        for (i = 0; i < n; ++i) pcm[i] = rate == TUNA_RATE ? mono[pos + i] : tuna_resample(sample, (void *)mono, count, rate, pos + i);
        tuna_adpcm_encode(pcm, n, data + TUNA_HEADER + (pos / 256) * TUNA_BLOCK_BYTES);
    }
    info.crc = tuna_crc(data + TUNA_HEADER, info.bytes);
    if (tuna_header_write(data, &info)) { free(data); return TUNA_FORMAT; }
    *track = data; *length = TUNA_HEADER + info.bytes; return TUNA_OK;
}
TunaError tuna_make_jingle(uint8_t **track, size_t *length) {
    int16_t *pcm = (int16_t *)malloc(4 * TUNA_RATE * sizeof(int16_t)); TunaError e;
    if (!pcm) return TUNA_IO;
    tuna_jingle(pcm, 4 * TUNA_RATE);
    e = tuna_convert_pcm(pcm, 4 * TUNA_RATE, TUNA_RATE, "Tuna original jingle", track, length);
    free(pcm); return e;
}
TunaError tuna_convert_mp3(const char *path, const char *title, unsigned excerpt_seconds, uint8_t **track, size_t *length) {
    FILE *f; long size; uint8_t *compressed = NULL; int16_t *mono = NULL;
    mp3dec_t *decoder = NULL; mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    uint32_t used = 0, rate = 0, limit = 0; size_t cursor = 0; TunaError e = TUNA_IO; int clipped = 0;
    *track = NULL; *length = 0;
    if (excerpt_seconds > MAX_SAMPLES / TUNA_RATE) return TUNA_SIZE;
    f = fopen(path, "rb"); if (!f) return TUNA_IO;
    if (fseek(f, 0, SEEK_END) || (size = ftell(f)) <= 0 || size > MAX_MP3 || fseek(f, 0, SEEK_SET)) { fclose(f); return TUNA_SIZE; }
    compressed = (uint8_t *)malloc((size_t)size); decoder = (mp3dec_t *)malloc(sizeof(*decoder));
    if (!compressed || !decoder || fread(compressed, 1, (size_t)size, f) != (size_t)size) { fclose(f); goto done; }
    fclose(f); mp3dec_init(decoder);
    /* Skip a bounded ID3v2 tag explicitly instead of scanning it for frame sync. */
    if (size >= 10 && !memcmp(compressed, "ID3", 3)) {
        uint32_t tag = 0; unsigned i;
        for (i = 6; i < 10; ++i) { if (compressed[i] & 128) { e = TUNA_FORMAT; goto done; } tag = (tag << 7) | compressed[i]; }
        cursor = 10u + tag + ((compressed[5] & 16) ? 10u : 0u);
        if (cursor > (size_t)size || tag > 1024 * 1024) { e = TUNA_FORMAT; goto done; }
    }
    while (cursor < (size_t)size) {
        mp3dec_frame_info_t frame; int n, i;
        n = mp3dec_decode_frame(decoder, compressed + cursor, (int)((size_t)size - cursor), pcm, &frame);
        if (frame.frame_bytes <= 0) break;
        cursor += (unsigned)frame.frame_bytes;
        if (!n) continue;
        if (frame.layer != 3 || frame.hz < 8000 || frame.hz > 48000 || frame.channels < 1 || frame.channels > 2) { e = TUNA_FORMAT; goto done; }
        if (!rate) {
            rate = (uint32_t)frame.hz;
            limit = excerpt_seconds ? excerpt_seconds * rate : (uint32_t)((uint64_t)MAX_SAMPLES * rate / TUNA_RATE);
            mono = (int16_t *)malloc((size_t)limit * sizeof(int16_t)); if (!mono) goto done;
        }
        if (rate != (uint32_t)frame.hz) { e = TUNA_FORMAT; goto done; }
        for (i = 0; i < n; ++i) {
            if (used == limit) { clipped = 1; break; }
            mono[used++] = frame.channels == 1 ? pcm[i] : (int16_t)(((int32_t)pcm[2 * i] + pcm[2 * i + 1]) / 2);
        }
        if (clipped) break;
    }
    if (clipped && !excerpt_seconds) { e = TUNA_SIZE; goto done; }
    if (!used) { e = TUNA_FORMAT; goto done; }
    e = tuna_convert_pcm(mono, used, rate, title, track, length);
done:
    free(compressed); free(mono); free(decoder); return e;
}
