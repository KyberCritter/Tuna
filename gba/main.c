#include <gba.h>
#include "tuna.h"
#include "display.h"
#include "audio.h"
#include "joybus.h"
#include <string.h>
#ifdef TUNA_DIAGNOSTIC
#include "diagnostic.h"
#endif

/* L/R step seek; holding repeats after a short delay. Frames are ~1/60 s. */
#define SEEK_SECONDS 5
#define SEEK_HOLD_DELAY 24
#define SEEK_HOLD_RATE 6

static uint8_t track[TUNA_CAPACITY] __attribute__((section(".track"), aligned(4)));
static TunaReceiver receiver;
static uint8_t request[TUNA_PACKET_MAX], reply[TUNA_PACKET_MAX];
static unsigned rx_bytes, tx_bytes, tx_offset, target_bytes = 20;
static volatile unsigned frames;
static unsigned last_activity;
static int player_loaded, load_pending;
static char *number(char *out, uint32_t value, unsigned width) {
    char digits[10]; unsigned n = 0;
    do { digits[n++] = (char)('0' + value % 10); value /= 10; } while (value);
    while (width > n) { *out++ = '0'; --width; }
    while (n) *out++ = digits[--n];
    *out = 0; return out;
}
static void vblank(void) { ++frames; }
static void link_reset(void) {
    rx_bytes = tx_bytes = tx_offset = 0; target_bytes = 20;
    REG_HS_CTRL = 7;
}
static void service_link(void) {
    if (REG_HS_CTRL & 1) { /* Host reset cannot invalidate a verified offline track. */
        link_reset();
        if (receiver.state == TUNA_RECEIVE) tuna_receiver_fail(&receiver, TUNA_IO);
    }
    /* Load only after the READY reply has fully drained. The output-gain scan
       decodes the whole track, so doing it while the host waits for READY can
       gap the link past the sender's SI timeout. */
    if (load_pending && !player_loaded && !tx_bytes) {
        audio_load(track + TUNA_HEADER, &receiver.track);
        player_loaded = 1; load_pending = 0;
    }
    if (tx_bytes) {
        /* Do not overwrite a word until the host has read it. */
        if (!(REG_JSTAT & TUNA_JOY_TRANSMIT)) {
            if (tx_offset < tx_bytes) {
                uint8_t word[4] = {0}; unsigned n = tx_bytes - tx_offset > 4 ? 4 : tx_bytes - tx_offset;
                memcpy(word, reply + tx_offset, n); REG_JOYTR = tuna_u32(word); tx_offset += n;
            } else { tx_bytes = tx_offset = 0; }
            last_activity = frames;
        }
        if (frames - last_activity > 180) {
            link_reset();
            if (receiver.state == TUNA_RECEIVE) tuna_receiver_fail(&receiver, TUNA_TIMEOUT);
        }
        return;
    }
    if (REG_JSTAT & TUNA_JOY_RECEIVE) {
        uint32_t word = REG_JOYRE; unsigned i;
        for (i = 0; i < 4 && rx_bytes < target_bytes; ++i) request[rx_bytes++] = (uint8_t)(word >> (i * 8));
        last_activity = frames;
        if (rx_bytes == 20 && target_bytes == 20) {
            unsigned body = tuna_u16(request + 6);
            if (body > TUNA_CHUNK || memcmp(request, "TNPK", 4)) { link_reset(); tuna_receiver_fail(&receiver, TUNA_FORMAT); return; }
            target_bytes = 20 + body;
        }
        if (rx_bytes == target_bytes) {
            TunaPacket packet;
            if (!tuna_packet_read(&packet, request, rx_bytes) && packet.type == TUNA_ABORT) {
                audio_stop(); player_loaded = 0; load_pending = 0;
            }
            tx_bytes = (unsigned)tuna_receiver_packet(&receiver, request, rx_bytes, reply);
            tx_offset = 0; rx_bytes = 0; target_bytes = 20;
            if (receiver.state == TUNA_VERIFIED && !player_loaded) load_pending = 1;
        }
    }
    if ((receiver.state == TUNA_RECEIVE || rx_bytes) && frames - last_activity > 180) {
        link_reset(); tuna_receiver_fail(&receiver, TUNA_TIMEOUT);
    }
}
static const char *codec_short(uint8_t codec) {
    return codec == TUNA_IMA4 ? "IMA4" : codec == TUNA_ADPCM2 ? "ADPCM2" : codec == TUNA_DELTA1 ? "DELTA1" : "?";
}
static void show(void) {
    char line[64], *out; uint32_t seconds; static TunaReceiveState previous = TUNA_FAILED;
    if (receiver.state != previous) { display_clear(); previous = receiver.state; }
    display_text_inv(0, "TUNA " TUNA_VERSION_STRING " MUSIC PLAYER");
    if (receiver.state == TUNA_VERIFIED) {
        const TunaTrack *tr = &receiver.track; char t0[31], t1[31], info[32]; const char *cs; unsigned n;
        memcpy(t0, tr->title, 30); t0[30] = 0;
        if (strlen(tr->title) > 30) { memcpy(t1, tr->title + 30, 30); t1[30] = 0; } else t1[0] = 0;
        display_text(2, t0); display_text(3, t1);
        display_text(4, tr->artist); display_text(5, tr->album);
        display_bar(7, 30, tr->samples ? (unsigned)((uint64_t)audio_played * 30 / tr->samples) : 0);
        seconds = audio_played / tr->rate;
        out = number(line, seconds / 60, 1); *out++ = ':'; out = number(out, seconds % 60, 2);
        memcpy(out, " / ", 3); out += 3; seconds = tr->samples / tr->rate;
        out = number(out, seconds / 60, 1); *out++ = ':'; number(out, seconds % 60, 2);
        display_text(8, line);
        display_text(9, audio_state());
        cs = codec_short(tr->codec); for (n = 0; cs[n]; ++n) info[n] = cs[n];
        info[n++] = ' '; out = number(info + n, tr->rate, 1); n = (unsigned)(out - info);
        memcpy(info + n, "HZ MONO", 8); display_text(10, info);
        out = number(line, tr->bytes, 1); *out++ = '/'; out = number(out, TUNA_CAPACITY - TUNA_HEADER, 1); memcpy(out, " B", 3);
        display_text(11, line);
        display_text(13, "SAFE TO UNPLUG"); display_text(14, "KEEP GBA POWERED ON");
        display_text_inv(16, "A:PLAY  L/R:SEEK  B:START");
    } else if (receiver.state == TUNA_RECEIVE) {
        char t0[31];
        memcpy(t0, receiver.track.title, 30); t0[30] = 0;
        display_text(2, t0);
        display_text(4, "RECEIVING - KEEP CONNECTED");
        display_bar(7, 30, receiver.track.bytes ? (unsigned)((uint64_t)receiver.received * 30 / receiver.track.bytes) : 0);
        out = number(line, receiver.received, 1); memcpy(out, " / ", 3); out += 3;
        out = number(out, receiver.track.bytes, 1); memcpy(out, " BYTES", 7);
        display_text(8, line);
        display_text_inv(10, "NOT SAFE TO UNPLUG");
    } else if (receiver.state == TUNA_FAILED) {
        display_text(4, "TRANSFER ERROR"); display_text(5, tuna_error(receiver.error));
        display_text_inv(10, "HOST: ABORT THEN RETRY");
    } else {
        display_text(4, "WAITING FOR TRACK"); display_text(5, "CONNECT GBA-GCN CABLE");
        display_text_inv(10, "NOT SAFE TO UNPLUG");
    }
}
int main(void) {
    unsigned drawn = UINT32_MAX; int a_held = 0, l_held = 0, r_held = 0, b_held = 0;
    unsigned l_repeat = 0, r_repeat = 0;
    irqInit(); irqSet(IRQ_VBLANK, vblank); irqEnable(IRQ_VBLANK);
    display_init(); audio_init(); tuna_receiver_init(&receiver, track, sizeof(track));
    REG_RCNT = 0xc000;
    /* JOYSTAT's receive/transmit flags are read-only. Writing zero does not
       clear the last BIOS multiboot word: consume it before framing TNPK. */
    if (REG_JSTAT & TUNA_JOY_RECEIVE) { volatile uint32_t discarded = REG_JOYRE; (void)discarded; }
    link_reset(); REG_JSTAT = TUNA_JOY_APPLICATION;
#ifdef TUNA_DIAGNOSTIC
    diagnostic_init(&receiver, track); player_loaded = receiver.state == TUNA_VERIFIED;
#endif
    while (1) {
        int a, l, r, b;
        audio_service(); service_link();
#ifdef TUNA_DIAGNOSTIC
        diagnostic_service(frames);
#endif
        a = !(REG_KEYINPUT & 1);
        l = !(REG_KEYINPUT & (1 << 9));
        r = !(REG_KEYINPUT & (1 << 8));
        b = !(REG_KEYINPUT & (1 << 1));
        if (a && !a_held && receiver.state == TUNA_VERIFIED && player_loaded) audio_key();
        if (receiver.state == TUNA_VERIFIED) {
            int32_t step = (int32_t)receiver.track.rate * SEEK_SECONDS;
            if (l) {
                if (!l_held) { audio_seek(-step); l_repeat = SEEK_HOLD_DELAY; }
                else if (l_repeat) --l_repeat;
                else { audio_seek(-step); l_repeat = SEEK_HOLD_RATE; }
            } else l_repeat = 0;
            if (r) {
                if (!r_held) { audio_seek(step); r_repeat = SEEK_HOLD_DELAY; }
                else if (r_repeat) --r_repeat;
                else { audio_seek(step); r_repeat = SEEK_HOLD_RATE; }
            } else r_repeat = 0;
            if (b && !b_held) audio_restart();
        }
        a_held = a; l_held = l; r_held = r; b_held = b;
        if (drawn != frames) { drawn = frames; show(); }
        /* No STOP, no link dependency, no heap. Busy service avoids missing words. */
    }
}
