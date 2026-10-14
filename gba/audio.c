#include <gba.h>
#include "audio.h"
#include <string.h>

typedef struct {
    int8_t pcm[256 + 32]; /* DMA can prefetch beyond the exact final sample. */
    unsigned count, halt;
    volatile unsigned ready;
} __attribute__((aligned(4))) AudioBuffer;
static AudioBuffer buffers[2];
static TunaPlayer decoder;
static unsigned timer_reload = 2048;
static volatile unsigned current, pause_pending, stopped_for_pause;
volatile uint32_t audio_played, audio_underruns;
volatile unsigned audio_active;

void __attribute__((section(".iwram"), long_call)) audio_stop(void) {
    REG_TM0CNT_H = 0; REG_TM1CNT_H = 0; REG_DMA1CNT = 0;
    REG_IF = IRQ_TIMER1;
    REG_SOUNDCNT_H = SNDA_RESET_FIFO; audio_active = 0;
}
static void __attribute__((section(".iwram"), long_call)) start_buffer(unsigned slot) {
    AudioBuffer *b = &buffers[slot];
    REG_DMA1CNT = 0;
    REG_SOUNDCNT_H = SNDA_VOL_100 | SNDA_R_ENABLE | SNDA_L_ENABLE | SNDA_RESET_FIFO;
    REG_DMA1SAD = (unsigned long)b->pcm; REG_DMA1DAD = (unsigned long)&REG_FIFO_A;
    REG_DMA1CNT = 0xb6400000; /* enabled, FIFO special, 32-bit, repeat, dest fixed */
    REG_TM1CNT_H = 0;
    REG_TM1CNT_L = (unsigned short)(0u - b->count);
    REG_TM1CNT_H = TIMER_START | TIMER_IRQ | TIMER_COUNT;
}
/* ISR swaps prepared buffers only. No decoding, UI, CRC or link work. */
static void __attribute__((section(".iwram"), long_call)) boundary(void) {
    AudioBuffer *old = &buffers[current];
    audio_played += old->count;
    if (old->halt || audio_played >= decoder.samples) {
        stopped_for_pause = old->halt && audio_played < decoder.samples;
        audio_stop(); old->ready = 0; return;
    }
    old->ready = 0; current ^= 1;
    if (!buffers[current].ready) { ++audio_underruns; audio_stop(); return; }
    start_buffer(current);
}
void audio_init(void) {
    REG_SOUNDCNT_X = 128; REG_SOUNDCNT_L = 0; audio_stop();
    irqSet(IRQ_TIMER1, boundary); irqEnable(IRQ_TIMER1);
}
void audio_load(const uint8_t *payload, const TunaTrack *track) {
    audio_stop(); tuna_player_init_track(&decoder, payload, track);
    timer_reload = 16777216u / track->rate;
    audio_played = 0; audio_underruns = 0; pause_pending = 0; stopped_for_pause = 1;
    buffers[0].ready = buffers[1].ready = 0;
}
static void fill(unsigned slot, int fade_in) {
    AudioBuffer *b = &buffers[slot]; unsigned i;
    b->count = (unsigned)tuna_player_render(&decoder, b->pcm, 256);
    b->halt = pause_pending;
    memset(b->pcm + b->count, 0, sizeof(b->pcm) - b->count);
    if (fade_in) for (i = 0; i < b->count && i < 32; ++i) b->pcm[i] = (int8_t)(b->pcm[i] * (int)i / 32);
    if (b->halt || decoder.position == decoder.samples) {
        unsigned n = b->count < 32 ? b->count : 32;
        for (i = 0; i < n; ++i) b->pcm[b->count - n + i] = (int8_t)(b->pcm[b->count - n + i] * (int)(n - i - 1) / (int)n);
    }
    __asm__ volatile("" ::: "memory"); b->ready = !!b->count;
}
void audio_key(void) {
    if (audio_underruns) return;
    if (audio_active) { pause_pending = 1; return; }
    if (audio_played >= decoder.samples) audio_played = 0;
    decoder.position = audio_played; decoder.block = UINT32_MAX; decoder.state = TUNA_PLAYING;
    pause_pending = 0; stopped_for_pause = 0; current = 0;
    buffers[0].ready = buffers[1].ready = 0;
    fill(0, 1); fill(1, 0);
    if (!buffers[0].ready) return;
    start_buffer(0); REG_TM0CNT_L = (unsigned short)(0u - timer_reload); audio_active = 1; REG_TM0CNT_H = TIMER_START;
}
void audio_service(void) {
    unsigned slot;
    if (!audio_active) return;
    slot = current ^ 1;
    if (!buffers[slot].ready && decoder.state == TUNA_PLAYING) fill(slot, 0);
}
const char *audio_state(void) {
    if (audio_underruns) return "ERROR: AUDIO UNDERRUN";
    if (audio_active) return pause_pending ? "PAUSING" : "PLAYING";
    return stopped_for_pause ? "PAUSED - A TO PLAY" : "ENDED - A TO REPLAY";
}
