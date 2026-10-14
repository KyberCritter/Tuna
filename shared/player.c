#include "tuna.h"
#include <string.h>

void tuna_player_init(TunaPlayer *p, const uint8_t *payload, uint32_t samples) {
    memset(p, 0, sizeof(*p)); p->payload = payload; p->samples = samples;
    p->block = UINT32_MAX; p->state = TUNA_PAUSED; p->codec = TUNA_IMA4;
}
void tuna_player_init_track(TunaPlayer *p, const uint8_t *payload, const TunaTrack *track) {
    tuna_player_init(p, payload, track->samples); p->codec = track->codec;
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
        out[i++] = (int8_t)(p->decoded[p->position % TUNA_BLOCK_SAMPLES] / 256); ++p->position;
    }
    if (p->position == p->samples) p->state = TUNA_ENDED;
    return i;
}
