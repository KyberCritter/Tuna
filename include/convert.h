#ifndef TUNA_CONVERT_H
#define TUNA_CONVERT_H
#include "tuna.h"
/* excerpt_seconds == 0 means complete track only; never silently crop. */
TunaError tuna_convert_mp3(const char *path, const char *title, unsigned excerpt_seconds,
                           uint8_t **track, size_t *length);
TunaError tuna_convert_pcm(const int16_t *mono, uint32_t count, uint32_t rate,
                           const char *title, uint8_t **track, size_t *length);
TunaError tuna_make_jingle(uint8_t **track, size_t *length);
#endif
