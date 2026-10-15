#ifndef TUNA_TRACK_STORE_H
#define TUNA_TRACK_STORE_H
#include "tuna.h"

#define TUNA_PATH_MAX 512u
/* Cache identity includes source path/content, conversion revision and policy. */
TunaError tuna_cache_path(const char *directory, const char *source, unsigned excerpt,
                          char out[TUNA_PATH_MAX]);
TunaError tuna_track_load(const char *path, uint8_t **track, size_t *length);
/* Publish only a fully validated, completely written container. */
TunaError tuna_track_save(const char *path, const uint8_t *track, size_t length);
#endif
