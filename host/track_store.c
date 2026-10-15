#include "track_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TunaError tuna_cache_path(const char *directory, const char *source, unsigned excerpt, char out[TUNA_PATH_MAX]) {
    FILE *f; uint8_t buffer[4096]; size_t n, total = 0; uint32_t crc = UINT32_MAX;
    const char *name = strrchr(source, '/'); char stem[65]; unsigned i;
    out[0] = 0;
    if (excerpt > TUNA_MAX_SECONDS) return TUNA_SIZE;
    f = fopen(source, "rb"); if (!f) return TUNA_IO;
    while ((n = fread(buffer, 1, sizeof(buffer), f)) != 0) {
        total += n; if (total > 16u * 1024u * 1024u) { fclose(f); return TUNA_SIZE; }
        crc = tuna_crc_update(crc, buffer, n);
    }
    { int failed = ferror(f); if (fclose(f)) failed = 1; if (failed) return TUNA_IO; }
    if (!total) return TUNA_SIZE;
    name = name ? name + 1 : source;
    for (i = 0; i < sizeof(stem) - 1 && name[i] && name[i] != '.'; ++i) {
        unsigned char c = (unsigned char)name[i];
        stem[i] = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ? (char)c : '_';
    }
    if (!i) stem[i++] = '_';
    stem[i] = 0;
    /* v1 is the cache conversion revision, distinct from the wire version. */
    if (snprintf(out, TUNA_PATH_MAX, "%s/%s-%08lx-%08lx-v1-%us.tuna", directory, stem,
                 (unsigned long)tuna_crc(source, strlen(source)), (unsigned long)~crc, excerpt) >= (int)TUNA_PATH_MAX) {
        out[0] = 0; return TUNA_SIZE;
    }
    return TUNA_OK;
}
TunaError tuna_track_load(const char *path, uint8_t **track, size_t *length) {
    FILE *f; long size; TunaTrack info; TunaError e;
    *track = NULL; *length = 0;
    f = fopen(path, "rb"); if (!f) return TUNA_IO;
    if (fseek(f, 0, SEEK_END) || (size = ftell(f)) < (long)TUNA_HEADER || size > (long)TUNA_CAPACITY || fseek(f, 0, SEEK_SET)) { fclose(f); return TUNA_SIZE; }
    *track = (uint8_t *)malloc((size_t)size); if (!*track) { fclose(f); return TUNA_IO; }
    e = fread(*track, 1, (size_t)size, f) == (size_t)size ? TUNA_OK : TUNA_IO;
    if (fclose(f)) e = TUNA_IO;
    if (!e) e = tuna_track_validate(*track, (size_t)size, &info);
    if (e) { free(*track); *track = NULL; } else *length = (size_t)size;
    return e;
}
TunaError tuna_track_save(const char *path, const uint8_t *track, size_t length) {
    char temporary[TUNA_PATH_MAX]; FILE *f; TunaTrack info; TunaError e = tuna_track_validate(track, length, &info);
    if (e) return e;
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= (int)sizeof(temporary)) return TUNA_SIZE;
    /* Never overwrite an existing file (including a corrupt/user-created one). */
    f = fopen(path, "rb"); if (f) { fclose(f); return TUNA_IO; }
    f = fopen(temporary, "wb"); if (!f) return TUNA_IO;
    e = fwrite(track, 1, length, f) == length ? TUNA_OK : TUNA_IO;
    if (fflush(f)) e = TUNA_IO;
    if (fclose(f)) e = TUNA_IO;
    if (!e && rename(temporary, path)) e = TUNA_IO;
    if (e) remove(temporary);
    return e;
}
