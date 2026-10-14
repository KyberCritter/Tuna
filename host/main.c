#include "convert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    uint8_t *track = NULL; size_t length = 0; TunaError e; FILE *out; unsigned excerpt = 0;
    const char *output;
    if (argc == 3 && !strcmp(argv[1], "--jingle")) { e = tuna_make_jingle(&track, &length); output = argv[2]; }
    else if (argc == 3 || (argc == 5 && !strcmp(argv[3], "--excerpt"))) {
        if (argc == 5) { char *end; unsigned long v = strtoul(argv[4], &end, 10); if (*end || !v || v > 46) { fprintf(stderr, "Excerpt must be 1..46 seconds.\n"); return 2; } excerpt = (unsigned)v; }
        e = tuna_convert_mp3(argv[1], argv[1], excerpt, &track, &length); output = argv[2];
    } else { fprintf(stderr, "Usage: tuna-convert input.mp3 output.tuna [--excerpt 1..46]\n       tuna-convert --jingle output.tuna\n"); return 2; }
    if (e) { fprintf(stderr, "%s; nothing written.\n", tuna_error(e)); return 1; }
    out = fopen(output, "wb");
    if (!out) { free(track); return 1; }
    { int failed = fwrite(track, 1, length, out) != length; if (fclose(out)) failed = 1; if (failed) { free(track); return 1; } }
    printf("%s: %lu bytes, %.3f seconds%s\n", output, (unsigned long)length, (double)tuna_u32(track + 16) / TUNA_RATE, excerpt ? " (explicit excerpt)" : " (complete decoded track)");
    free(track); return 0;
}
