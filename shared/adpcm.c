#include "tuna.h"
#include <string.h>

static const int steps[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,
    73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,
    449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,
    2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,
    7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,
    24623,27086,29794,32767
};
static const int adjust[8] = {-1,-1,-1,-1,2,4,6,8};
static int apply(unsigned code, int *predictor, int *index) {
    int s = steps[*index], delta = s >> 3;
    if (code & 1) delta += s >> 2;
    if (code & 2) delta += s >> 1;
    if (code & 4) delta += s;
    *predictor += (code & 8) ? -delta : delta;
    if (*predictor > 32767) *predictor = 32767;
    if (*predictor < -32768) *predictor = -32768;
    *index += adjust[code & 7];
    if (*index < 0) *index = 0;
    if (*index > 88) *index = 88;
    return *predictor;
}
void tuna_adpcm_encode(const int16_t *pcm, unsigned count, uint8_t out[TUNA_BLOCK_BYTES]) {
    int predictor = count ? pcm[0] : 0, index = 0; unsigned i;
    memset(out, 0, TUNA_BLOCK_BYTES); tuna_put16(out, (uint16_t)predictor);
    for (i = 1; i < TUNA_BLOCK_SAMPLES; ++i) {
        int diff = (i < count ? pcm[i] : predictor) - predictor, s = steps[index];
        unsigned code = diff < 0 ? 8 : 0;
        if (diff < 0) diff = -diff;
        if (diff >= s) { code |= 4; diff -= s; }
        if (diff >= (s >> 1)) { code |= 2; diff -= s >> 1; }
        if (diff >= (s >> 2)) code |= 1;
        apply(code, &predictor, &index);
        out[4 + (i - 1) / 2] |= (uint8_t)(code << (((i - 1) & 1) * 4));
    }
}
TunaError tuna_adpcm_decode(const uint8_t in[TUNA_BLOCK_BYTES], int16_t out[TUNA_BLOCK_SAMPLES]) {
    int predictor = tuna_u16(in), index = in[2]; unsigned i;
    if (predictor >= 32768) predictor -= 65536;
    if (index > 88 || in[3] || (in[131] & 0xf0)) return TUNA_FORMAT;
    out[0] = (int16_t)predictor;
    for (i = 1; i < TUNA_BLOCK_SAMPLES; ++i)
        out[i] = (int16_t)apply((in[4 + (i - 1) / 2] >> (((i - 1) & 1) * 4)) & 15, &predictor, &index);
    return TUNA_OK;
}
