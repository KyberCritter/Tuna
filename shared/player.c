#include "tuna.h"
#include <string.h>

#define TUNA_GAIN_UNITY 256u
/* Target headroom: leave a little room so codec overshoot does not clip. */
#define TUNA_GAIN_TARGET 31130u
#define TUNA_GAIN_MAX 4096u /* 16x; bounds noise/silence amplification */

uint32_t tuna_track_peak(uint8_t codec, const uint8_t *payload, uint32_t samples) {
    uint32_t block = tuna_profile_block(codec), index, peak = 0;
    int16_t decoded[TUNA_BLOCK_SAMPLES];
    if (!block || !payload || !samples) return 0;
    for (index = 0; index < samples; index += TUNA_BLOCK_SAMPLES) {
        unsigned i, count = samples - index > TUNA_BLOCK_SAMPLES ? TUNA_BLOCK_SAMPLES : samples - index;
        if (tuna_block_decode(codec, payload + (size_t)(index / TUNA_BLOCK_SAMPLES) * block, decoded)) break;
        for (i = 0; i < count; ++i) {
            uint32_t magnitude = decoded[i] < 0 ? (uint32_t)(-(int32_t)decoded[i]) : (uint32_t)decoded[i];
            if (magnitude > peak) peak = magnitude;
        }
    }
    return peak;
}
uint32_t tuna_gain_for_peak(uint32_t peak) {
    uint64_t gain;
    if (!peak) return TUNA_GAIN_UNITY;
    gain = ((uint64_t)TUNA_GAIN_TARGET * TUNA_GAIN_UNITY) / peak;
    if (gain < TUNA_GAIN_UNITY) return TUNA_GAIN_UNITY; /* never attenuate */
    if (gain > TUNA_GAIN_MAX) return TUNA_GAIN_MAX;
    return (uint32_t)gain;
}
int8_t tuna_scale_sample(int16_t sample, uint32_t gain) {
    /* Unity gain (256) must reproduce the original 16-bit to 8-bit truncation,
       i.e. sample / 256, so the fixed-point division is by 256 * 256. */
    int32_t value = ((int32_t)sample * (int32_t)gain) / 65536;
    if (value > 127) value = 127;
    if (value < -128) value = -128;
    return (int8_t)value;
}
void tuna_player_init(TunaPlayer *p, const uint8_t *payload, uint32_t samples) {
    memset(p, 0, sizeof(*p)); p->payload = payload; p->samples = samples;
    p->block = UINT32_MAX; p->state = TUNA_PAUSED; p->codec = TUNA_IMA4; p->gain = TUNA_GAIN_UNITY;
}
void tuna_player_init_track(TunaPlayer *p, const uint8_t *payload, const TunaTrack *track) {
    tuna_player_init(p, payload, track->samples); p->codec = track->codec;
    p->gain = tuna_gain_for_peak(tuna_track_peak(track->codec, payload, track->samples));
}
void tuna_player_key(TunaPlayer *p, int a_down) {
    if (a_down && !p->a_held) {
        if (p->state == TUNA_ENDED) { p->position = 0; p->block = UINT32_MAX; p->state = TUNA_PLAYING; }
        else p->state = p->state == TUNA_PLAYING ? TUNA_PAUSED : TUNA_PLAYING;
    }
    p->a_held = (uint8_t)!!a_down;
}
size_t tuna_player_render(TunaPlayer *p, int8_t *out, size_t count) {
    size_t i = 0;
    memset(out, 0, count);
    if (p->state != TUNA_PLAYING) return 0;
    while (i < count && p->position < p->samples) {
        uint32_t block = p->position / TUNA_BLOCK_SAMPLES;
        if (block != p->block) {
            if (tuna_block_decode(p->codec, p->payload + block * tuna_profile_block(p->codec), p->decoded)) { p->state = TUNA_ENDED; break; }
            p->block = block;
        }
        out[i++] = tuna_scale_sample(p->decoded[p->position % TUNA_BLOCK_SAMPLES], p->gain); ++p->position;
    }
    if (p->position == p->samples) p->state = TUNA_ENDED;
    return i;
}
