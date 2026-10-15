#ifndef TUNA_CONVERT_H
#define TUNA_CONVERT_H
#include "tuna.h"
/* Bounded printable metadata carried in the container; artist/album may be empty. */
typedef struct {
    char title[TUNA_TITLE + 1];
    char artist[TUNA_ARTIST + 1];
    char album[TUNA_ALBUM + 1];
} TunaMeta;
/* Fill meta from ID3v2 (leading) then ID3v1 (trailing) tags; empty when absent.
   Bounded and tolerant: unrecognised/unsupported tags are simply ignored. */
void tuna_id3_meta(TunaMeta *meta, const uint8_t *data, size_t size);
/* excerpt_seconds == 0 means complete track only; never silently crop. */
TunaError tuna_convert_mp3(const char *path, const char *title, unsigned excerpt_seconds,
                           uint8_t **track, size_t *length);
TunaError tuna_convert_pcm(const int16_t *mono, uint32_t count, uint32_t rate,
                           const char *title, uint8_t **track, size_t *length);
TunaError tuna_make_jingle(uint8_t **track, size_t *length);
#endif
