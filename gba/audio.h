#ifndef TUNA_AUDIO_H
#define TUNA_AUDIO_H
#include "tuna.h"
extern volatile uint32_t audio_played, audio_underruns;
extern volatile unsigned audio_active;
void audio_init(void);
void audio_load(const uint8_t *payload, const TunaTrack *track);
void audio_key(void);
void audio_service(void);
void audio_stop(void);
const char *audio_state(void);
#endif
