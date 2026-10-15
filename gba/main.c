#include <gba.h>
#include "tuna.h"
#include "display.h"
#include "audio.h"
#include "joybus.h"
#include <string.h>
#ifdef TUNA_DIAGNOSTIC
#include "diagnostic.h"
#endif

static uint8_t track[TUNA_CAPACITY] __attribute__((section(".track"), aligned(4)));
static TunaReceiver receiver;
static uint8_t request[TUNA_PACKET_MAX], reply[TUNA_PACKET_MAX];
static unsigned rx_bytes, tx_bytes, tx_offset, target_bytes = 20;
static volatile unsigned frames;
static unsigned last_activity;
static int player_loaded;
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
                audio_stop(); player_loaded = 0;
            }
            tx_bytes = (unsigned)tuna_receiver_packet(&receiver, request, rx_bytes, reply);
            tx_offset = 0; rx_bytes = 0; target_bytes = 20;
            if (receiver.state == TUNA_VERIFIED && !player_loaded) {
                audio_load(track + TUNA_HEADER, &receiver.track); player_loaded = 1;
            }
        }
    }
    if ((receiver.state == TUNA_RECEIVE || rx_bytes) && frames - last_activity > 180) {
        link_reset(); tuna_receiver_fail(&receiver, TUNA_TIMEOUT);
    }
}
static void show(void) {
    char line[64], *out; unsigned seconds; static TunaReceiveState previous = TUNA_FAILED;
    if (receiver.state != previous) { display_clear(); previous = receiver.state; }
    display_text(0, "TUNA V1 - RAM MUSIC PLAYER");
    if (receiver.state == TUNA_VERIFIED) {
        char title[31];
        memcpy(title, receiver.track.title, 30); title[30] = 0;
        display_text(3, title); display_text(4, receiver.track.title + (strlen(receiver.track.title) > 30 ? 30 : strlen(receiver.track.title)));
        display_text(7, audio_state()); display_text(10, "SAFE TO UNPLUG");
        seconds = audio_played / receiver.track.rate;
        out = number(line, seconds / 60, 1); *out++ = ':'; out = number(out, seconds % 60, 2);
        memcpy(out, " / ", 3); out += 3; seconds = receiver.track.samples / receiver.track.rate;
        out = number(out, seconds / 60, 1); *out++ = ':'; number(out, seconds % 60, 2);
        display_text(8, line); display_text(12, "KEEP GBA POWERED ON");
        display_text(14, tuna_profile_name(receiver.track.codec));
    } else if (receiver.state == TUNA_RECEIVE) {
        display_text(7, "RECEIVING - KEEP CONNECTED");
        out = number(line, receiver.received, 1); memcpy(out, " / ", 3); out += 3;
        out = number(out, receiver.track.bytes, 1); memcpy(out, " BYTES", 7);
        display_text(8, line); display_text(10, "NOT SAFE TO UNPLUG");
    } else if (receiver.state == TUNA_FAILED) {
        display_text(7, "TRANSFER ERROR"); display_text(8, tuna_error(receiver.error)); display_text(10, "HOST: ABORT THEN RETRY");
    } else {
        display_text(7, "WAITING FOR TRACK"); display_text(8, "CONNECT GBA-GCN CABLE"); display_text(10, "NOT SAFE TO UNPLUG");
    }
}
int main(void) {
    unsigned drawn = UINT32_MAX; int a_held = 0;
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
        int a;
        audio_service(); service_link();
#ifdef TUNA_DIAGNOSTIC
        diagnostic_service(frames);
#endif
        a = !(REG_KEYINPUT & 1);
        if (a && !a_held && receiver.state == TUNA_VERIFIED) audio_key();
        a_held = a;
        if (drawn != frames) { drawn = frames; show(); }
        /* No STOP, no link dependency, no heap. Busy service avoids missing words. */
    }
}
