#include "tuna.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846
int16_t tuna_resample(TunaSampleFn fetch, void *context, uint32_t samples, uint32_t rate, uint32_t index) {
    return tuna_resample_rate(fetch, context, samples, rate, TUNA_RATE, index);
}
int16_t tuna_resample_rate(TunaSampleFn fetch, void *context, uint32_t samples, uint32_t rate, uint32_t output_rate, uint32_t index) {
    double position, cutoff, sum = 0, weights = 0; int tap, radius; int64_t center;
    if (!samples || !rate || !output_rate) return 0;
    position = (double)index * rate / output_rate; center = (int64_t)position;
    cutoff = 0.90 * (rate > output_rate ? (double)output_rate / rate : 1.0);
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
TunaError tuna_resampler_init(TunaResampler *r, uint32_t rate, uint32_t output_rate) {
    uint32_t a = rate, b = output_rate, phase; double cutoff; size_t entries;
    memset(r, 0, sizeof(*r)); r->input_rate = rate; r->output_rate = output_rate;
    /* Power-of-two output rates make every phase fraction exact in double,
       including the original center+tap-position arithmetic at every index. */
    if (!rate || rate > 48000 || (output_rate != 4096 && output_rate != 8192)) return TUNA_FORMAT;
    while (b) { uint32_t remainder = a % b; a = b; b = remainder; }
    r->phases = output_rate / a;
    cutoff = 0.90 * (rate > output_rate ? (double)output_rate / rate : 1.0);
    r->radius = (unsigned)ceil(16.0 / cutoff); if (r->radius > 256) r->radius = 256;
    r->stride = 2 * r->radius + 2; /* taps followed by their unnormalized sum */
    entries = (size_t)r->phases * r->stride;
    if (entries > (4u * 1024u * 1024u) / sizeof(double)) return TUNA_SIZE;
    r->weights = (double *)malloc(entries * sizeof(double)); if (!r->weights) return TUNA_IO;
    for (phase = 0; phase < r->phases; ++phase) {
        double position = (double)phase * rate / output_rate, total = 0;
        int64_t center = (int64_t)position; int tap;
        double *row = r->weights + (size_t)phase * r->stride;
        for (tap = -(int)r->radius; tap <= (int)r->radius; ++tap) {
            double x = (double)(center + tap) - position;
            double sinc = fabs(x) < 1e-10 ? cutoff : sin(PI * cutoff * x) / (PI * x);
            double weight = sinc * (0.5 + 0.5 * cos(PI * x / (r->radius + 1)));
            row[tap + (int)r->radius] = weight; total += weight;
        }
        row[r->stride - 1] = total;
    }
    return TUNA_OK;
}
int16_t tuna_resampler_sample(const TunaResampler *r, TunaSampleFn fetch, void *context, uint32_t samples, uint32_t index) {
    int64_t center; int tap; double sum = 0, total; const double *row;
    if (!r->weights) return tuna_resample_rate(fetch, context, samples, r->input_rate, r->output_rate, index);
    if (!samples) return 0;
    center = (int64_t)((uint64_t)index * r->input_rate / r->output_rate);
    row = r->weights + (size_t)(index % r->phases) * r->stride;
    /* Keep the scalar multiply/add order and final division: pre-normalizing,
       vectorizing or using float could change quantized/encoded output bytes. */
    for (tap = -(int)r->radius; tap <= (int)r->radius; ++tap) {
        int64_t source = center + tap;
        uint32_t clamped = source < 0 ? 0 : (source >= samples ? samples - 1 : (uint32_t)source);
        sum += fetch(context, clamped) * row[tap + (int)r->radius];
    }
    total = row[r->stride - 1]; sum = total ? sum / total : 0;
    if (sum > 32767) sum = 32767;
    if (sum < -32768) sum = -32768;
    return (int16_t)sum;
}
void tuna_resampler_free(TunaResampler *r) { free(r->weights); r->weights = NULL; }
void tuna_jingle(int16_t *out, uint32_t count) {
    static const double notes[] = {261.6256,329.6276,391.9954,523.2511,391.9954,329.6276,293.6648,261.6256};
    uint32_t i;
    for (i = 0; i < count; ++i) {
        uint32_t note = (i / (TUNA_RATE / 2)) % 8, local = i % (TUNA_RATE / 2);
        double envelope = local < 128 ? local / 128.0 : (local > TUNA_RATE / 2 - 512 ? (TUNA_RATE / 2 - local) / 512.0 : 1.0);
        out[i] = (int16_t)(10000 * envelope * sin(2 * PI * notes[note] * i / TUNA_RATE));
    }
}
