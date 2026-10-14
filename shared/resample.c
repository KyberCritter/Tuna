#include "tuna.h"
#include <math.h>

#define PI 3.14159265358979323846
int16_t tuna_resample(TunaSampleFn fetch, void *context, uint32_t samples, uint32_t rate, uint32_t index) {
    double position, cutoff, sum = 0, weights = 0; int tap, radius; int64_t center;
    if (!samples || !rate) return 0;
    position = (double)index * rate / TUNA_RATE; center = (int64_t)position;
    cutoff = 0.90 * (rate > TUNA_RATE ? (double)TUNA_RATE / rate : 1.0);
    radius = (int)ceil(16.0 / cutoff);
    if (radius > 256) radius = 256;
    for (tap = -radius; tap <= radius; ++tap) {
        int64_t source = center + tap; double x = (double)source - position;
        double sinc = fabs(x) < 1e-10 ? cutoff : sin(PI * cutoff * x) / (PI * x);
        double w = sinc * (0.5 + 0.5 * cos(PI * x / (radius + 1)));
        uint32_t clamped = source < 0 ? 0 : (source >= samples ? samples - 1 : (uint32_t)source);
        sum += fetch(context, clamped) * w; weights += w;
    }
    sum = weights ? sum / weights : 0;
    if (sum > 32767) sum = 32767;
    if (sum < -32768) sum = -32768;
    return (int16_t)sum;
}
void tuna_jingle(int16_t *out, uint32_t count) {
    static const double notes[] = {261.6256,329.6276,391.9954,523.2511,391.9954,329.6276,293.6648,261.6256};
    uint32_t i;
    for (i = 0; i < count; ++i) {
        uint32_t note = (i / (TUNA_RATE / 2)) % 8, local = i % (TUNA_RATE / 2);
        double envelope = local < 128 ? local / 128.0 : (local > TUNA_RATE / 2 - 512 ? (TUNA_RATE / 2 - local) / 512.0 : 1.0);
        out[i] = (int16_t)(10000 * envelope * sin(2 * PI * notes[note] * i / TUNA_RATE));
    }
}
