#include "convert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244)
#endif
#include "minimp3.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define MAX_MP3 (16u * 1024u * 1024u)
static int16_t sample(void *context, uint32_t i) { return ((const int16_t *)context)[i]; }
static TunaError convert_source(TunaSampleFn fetch, void *context, uint32_t count, uint32_t rate, const char *title, uint8_t **track, size_t *length) {
    TunaTrack info; uint32_t pos; uint8_t *data; int16_t pcm[256]; unsigned i; TunaError e;
    *track = NULL; *length = 0;
    if (!fetch || !count || rate < 4096 || rate > 48000) return TUNA_FORMAT;
    memset(&info, 0, sizeof(info)); e = tuna_choose_profile(count, rate, &info); if (e) return e;
    for (i = 0; i < TUNA_TITLE && title[i]; ++i) info.title[i] = title[i] >= 32 && title[i] <= 126 ? title[i] : '?';
    data = (uint8_t *)malloc(TUNA_HEADER + info.bytes);
    if (!data) return TUNA_IO;
    for (pos = 0; pos < info.samples; pos += 256) {
        unsigned n = info.samples - pos > 256 ? 256 : info.samples - pos;
        for (i = 0; i < n; ++i) pcm[i] = rate == info.rate ? fetch(context, pos + i) : tuna_resample_rate(fetch, context, count, rate, info.rate, pos + i);
        tuna_block_encode(info.codec, pcm, n, data + TUNA_HEADER + (pos / 256) * tuna_profile_block(info.codec));
    }
    info.crc = tuna_crc(data + TUNA_HEADER, info.bytes);
    if (tuna_header_write(data, &info)) { free(data); return TUNA_FORMAT; }
    *track = data; *length = TUNA_HEADER + info.bytes; return TUNA_OK;
}
TunaError tuna_convert_pcm(const int16_t *mono, uint32_t count, uint32_t rate, const char *title, uint8_t **track, size_t *length) {
    if (!mono) { *track = NULL; *length = 0; return TUNA_FORMAT; }
    return convert_source(sample, (void *)mono, count, rate, title, track, length);
}
TunaError tuna_make_jingle(uint8_t **track, size_t *length) {
    int16_t *pcm = (int16_t *)malloc(4 * TUNA_RATE * sizeof(int16_t)); TunaError e;
    if (!pcm) return TUNA_IO;
    tuna_jingle(pcm, 4 * TUNA_RATE);
    e = tuna_convert_pcm(pcm, 4 * TUNA_RATE, TUNA_RATE, "Tuna original jingle", track, length);
    free(pcm); return e;
}
typedef struct {
    mp3dec_t decoder;
    const uint8_t *data;
    size_t cursor, end, start;
    uint32_t count, rate, delay, padding, expected_frames, frames;
    int16_t ring[4096];
    mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    TunaError error;
    int first;
} Mp3Stream;
static uint32_t big32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
/* Xing/LAME layout and 529-sample decoder latency follow minimp3_ex (CC0). */
static int gapless(Mp3Stream *s, const uint8_t *frame, unsigned bytes) {
    unsigned mono = (frame[3] >> 6) == 3, mpeg1 = (frame[1] & 0x18) == 0x18;
    unsigned pos = 4 + ((frame[1] & 1) ? 0 : 2) + (mpeg1 ? (mono ? 17 : 32) : (mono ? 9 : 17));
    uint32_t flags;
    if (pos + 8 > bytes || (memcmp(frame + pos, "Xing", 4) && memcmp(frame + pos, "Info", 4))) return 0;
    flags = big32(frame + pos + 4); pos += 8;
    if (flags & ~15u) { s->error = TUNA_FORMAT; return 1; }
    if (flags & 1) { if (pos + 4 > bytes) goto bad; s->expected_frames = big32(frame + pos); pos += 4; }
    if (flags & 2) pos += 4;
    if (flags & 4) pos += 100;
    if (flags & 8) pos += 4;
    if (pos > bytes) goto bad;
    if (bytes - pos >= 36 && frame[pos]) {
        unsigned delay = ((unsigned)frame[pos + 21] << 4) | (frame[pos + 22] >> 4);
        unsigned padding = ((frame[pos + 22] & 15u) << 8) | frame[pos + 23];
        s->delay = delay + 529; s->padding = padding > 529 ? padding - 529 : 0;
    }
    return 1;
bad: s->error = TUNA_FORMAT; return 1;
}
static void stream_reset(Mp3Stream *s) {
    mp3dec_init(&s->decoder); s->cursor = s->start; s->count = s->rate = s->delay = s->padding = s->expected_frames = s->frames = 0;
    s->error = TUNA_OK; s->first = 1;
}
static int stream_next(Mp3Stream *s) {
    while (s->cursor < s->end) {
        mp3dec_frame_info_t frame; size_t offset = s->cursor; int n, i;
        n = mp3dec_decode_frame(&s->decoder, s->data + offset, (int)(s->end - offset), s->pcm, &frame);
        if (frame.frame_bytes <= 0 || frame.frame_offset || !n || frame.layer != 3 || frame.hz < 8000 || frame.hz > 48000 || frame.channels < 1 || frame.channels > 2) { s->error = TUNA_FORMAT; return 0; }
        s->cursor += (unsigned)frame.frame_bytes;
        if (s->first) {
            s->first = 0;
            if (gapless(s, s->data + offset, (unsigned)frame.frame_bytes)) { mp3dec_init(&s->decoder); if (s->error) return 0; continue; }
        }
        if (!s->rate) s->rate = (uint32_t)frame.hz;
        if (s->rate != (uint32_t)frame.hz) { s->error = TUNA_FORMAT; return 0; }
        ++s->frames;
        for (i = 0; i < n; ++i) {
            if (s->delay) { --s->delay; continue; }
            s->ring[s->count % 4096] = frame.channels == 1 ? s->pcm[i] : (int16_t)(((int32_t)s->pcm[2 * i] + s->pcm[2 * i + 1]) / 2);
            ++s->count;
        }
        return 1;
    }
    return 0;
}
static int16_t stream_sample(void *context, uint32_t index) {
    Mp3Stream *s = (Mp3Stream *)context;
    while (s->count <= index) if (!stream_next(s)) { s->error = TUNA_FORMAT; return 0; }
    if (s->count - index > 4096) { s->error = TUNA_IO; return 0; }
    return s->ring[index % 4096];
}
TunaError tuna_convert_mp3(const char *path, const char *title, unsigned excerpt_seconds, uint8_t **track, size_t *length) {
    FILE *f; long size; uint8_t *compressed = NULL; Mp3Stream *s = NULL;
    size_t cursor = 0; uint32_t count = 0, rate = 0; TunaError e = TUNA_IO; int clipped = 0;
    *track = NULL; *length = 0;
    if (excerpt_seconds > TUNA_MAX_SECONDS) return TUNA_SIZE;
    f = fopen(path, "rb"); if (!f) return TUNA_IO;
    if (fseek(f, 0, SEEK_END) || (size = ftell(f)) <= 0 || size > (long)MAX_MP3 || fseek(f, 0, SEEK_SET)) { fclose(f); return TUNA_SIZE; }
    compressed = (uint8_t *)malloc((size_t)size); s = (Mp3Stream *)calloc(1, sizeof(*s));
    if (!compressed || !s || fread(compressed, 1, (size_t)size, f) != (size_t)size) { fclose(f); goto done; }
    fclose(f);
    /* Skip a bounded ID3v2 tag explicitly instead of scanning it for frame sync. */
    if (size >= 10 && !memcmp(compressed, "ID3", 3)) {
        uint32_t tag = 0; unsigned i;
        for (i = 6; i < 10; ++i) { if (compressed[i] & 128) { e = TUNA_FORMAT; goto done; } tag = (tag << 7) | compressed[i]; }
        cursor = 10u + tag + ((compressed[5] & 16) ? 10u : 0u);
        if (cursor > (size_t)size || tag > 1024 * 1024) { e = TUNA_FORMAT; goto done; }
    }
    s->data = compressed; s->start = cursor; s->end = (size_t)size;
    if (s->end >= 128 && !memcmp(compressed + s->end - 128, "TAG", 3)) s->end -= 128;
    stream_reset(s);
    while (stream_next(s)) {
        if (excerpt_seconds && s->count >= excerpt_seconds * s->rate) { clipped = 1; break; }
        if (s->count > TUNA_MAX_SECONDS * s->rate + 8192) { e = TUNA_SIZE; goto done; }
    }
    if (s->error) { e = s->error; goto done; }
    if (!clipped && s->expected_frames && s->frames != s->expected_frames) { e = TUNA_FORMAT; goto done; }
    count = clipped ? excerpt_seconds * s->rate : (s->count > s->padding ? s->count - s->padding : 0); rate = s->rate;
    if (!count) { e = TUNA_FORMAT; goto done; }
    stream_reset(s);
    e = convert_source(stream_sample, s, count, rate, title, track, length);
    if (!e && s->error) { free(*track); *track = NULL; *length = 0; e = s->error; }
done:
    free(compressed); free(s); return e;
}
