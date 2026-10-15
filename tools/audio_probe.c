/* SPDX-License-Identifier: MIT
 * Render the production decoder to a reference WAV; no emulator acceptance. */
#include "tuna.h"
#include "track_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main(int argc, char **argv) {
    uint8_t *track, header[44] = {0}; size_t length; TunaTrack info; TunaPlayer player;
    FILE *out; uint32_t count = 0, peak = 0, step = 0, seam = 0, peak_after = 0; int previous = 0;
    double energy = 0; int8_t samples[256];
    if (argc != 3) { fprintf(stderr, "Usage: tuna-audio-probe track.tuna reference.wav\n"); return 2; }
    if (tuna_track_load(argv[1], &track, &length) || tuna_track_validate(track, length, &info)) return 1;
    out = fopen(argv[2], "wb"); if (!out) { free(track); return 1; }
    memcpy(header, "RIFF", 4); tuna_put32(header + 4, 36 + info.samples * 2);
    memcpy(header + 8, "WAVEfmt ", 8); tuna_put32(header + 16, 16);
    tuna_put16(header + 20, 1); tuna_put16(header + 22, 1); tuna_put32(header + 24, info.rate);
    tuna_put32(header + 28, info.rate * 2); tuna_put16(header + 32, 2); tuna_put16(header + 34, 16);
    memcpy(header + 36, "data", 4); tuna_put32(header + 40, info.samples * 2);
    if (fwrite(header, 1, sizeof(header), out) != sizeof(header)) { fclose(out); free(track); return 1; }
    peak = tuna_track_peak(info.codec, track + TUNA_HEADER, info.samples);
    tuna_player_init_track(&player, track + TUNA_HEADER, &info); tuna_player_key(&player, 1);
    while (player.state == TUNA_PLAYING) {
        size_t i, n = tuna_player_render(&player, samples, sizeof(samples));
        if (!n) break;
        for (i = 0; i < n; ++i) {
            uint8_t word[2]; int value = samples[i] * 256;
            uint32_t magnitude = (uint32_t)abs(value), delta = (uint32_t)abs(value - previous);
            if (magnitude > peak_after) peak_after = magnitude;
            if (count && delta > step) step = delta;
            if (count && !(count % 256) && delta > seam) seam = delta;
            energy += (double)value * value; previous = value; ++count;
            tuna_put16(word, (uint16_t)value);
            if (fwrite(word, 1, 2, out) != 2) { fclose(out); free(track); return 1; }
        }
    }
    free(track);
    if (fclose(out) || count != info.samples) return 1;
    printf("{\"samples\":%lu,\"rate\":%lu,\"peak\":%lu,\"rms\":%.9f,\"gain\":%lu,\"gainDb\":%.3f,\"peakAfterGain\":%lu,\"maximumSampleStep\":%lu,\"maximumBlockSeamStep\":%lu,\"scope\":\"host decoder reference, not emulator\"}\n",
           (unsigned long)count, (unsigned long)info.rate, (unsigned long)peak, sqrt(energy / count),
           (unsigned long)player.gain, 20.0 * log10((double)player.gain / 256.0), (unsigned long)peak_after,
           (unsigned long)step, (unsigned long)seam);
    return 0;
}
