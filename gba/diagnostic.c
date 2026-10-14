/* Standalone HLE lane ONLY. This does not test BIOS/Joybus/retained transfer. */
#include "diagnostic.h"
#include "diagnostic_track.h"
#include "audio.h"
#include <string.h>
volatile uint32_t diagnostic_result, diagnostic_pause_samples, diagnostic_passes;
static unsigned stage, paused_frame;
void __attribute__((noinline)) diagnostic_done(void) { __asm__ volatile("nop"); }
static void finish(int passed) {
    diagnostic_result = passed ? 0x50415353 : 0x4641494c;
    diagnostic_done(); stage = 99;
}
void diagnostic_init(TunaReceiver *r, uint8_t *storage) {
    if (diagnostic_track_size > TUNA_CAPACITY) { finish(0); return; }
    memcpy(storage, diagnostic_track, diagnostic_track_size);
    if (tuna_track_validate(storage, diagnostic_track_size, &r->track)) { finish(0); return; }
    r->received = r->track.bytes; r->state = TUNA_VERIFIED;
    audio_load(storage + TUNA_HEADER, &r->track); audio_key();
}
void diagnostic_service(unsigned frame) {
    if (stage == 99) return;
    if (audio_underruns) { finish(0); return; }
    if (stage == 0 && audio_played >= 512) { audio_key(); stage = 1; }
    if (stage == 1 && !audio_active) { diagnostic_pause_samples = audio_played; paused_frame = frame; stage = 2; }
    if (stage == 2) {
        if (audio_played != diagnostic_pause_samples) { finish(0); return; }
        if (frame - paused_frame >= 30) { audio_key(); stage = 3; }
    }
    if (stage == 3 && !audio_active) { ++diagnostic_passes; audio_key(); stage = 4; }
    if (stage == 4 && !audio_active) { ++diagnostic_passes; finish(1); }
}
