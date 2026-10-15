#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "link.h"
#include "joybus.h"

static unsigned channel = 1;
static uint8_t command[32] ATTRIBUTE_ALIGN(32), result[32] ATTRIBUTE_ALIGN(32);
static volatile unsigned completed = 1;
static volatile uint32_t transfer_error;
static void callback(s32 chan, u32 status) { (void)chan; transfer_error = status; completed = 1; }
static int expired(uint64_t start, unsigned ms) { return ticks_to_millisecs(gettime() - start) >= ms; }
void link_port(unsigned port) { channel = port; SI_DisablePolling(SI_CHAN_BIT(channel)); }
static TunaError transfer(const uint8_t *out, unsigned out_length, unsigned in_length) {
    uint64_t start = gettime();
    /* Static buffers remain alive if libogc completes after a timeout. */
    while (!completed || SI_IsChanBusy((s32)channel)) { if (expired(start, 250)) return TUNA_TIMEOUT; usleep(50); }
    memcpy(command, out, out_length); memset(result, 0, sizeof(result)); completed = 0;
    if (!SI_Transfer((s32)channel, command, out_length, result, in_length, callback, 50)) { completed = 1; return TUNA_IO; }
    start = gettime();
    while (!completed) { if (expired(start, 250)) return TUNA_TIMEOUT; usleep(50); }
    return (transfer_error & 0x0f) ? TUNA_IO : TUNA_OK;
}
static TunaError status(uint8_t *value) {
    const uint8_t cmd = 0; TunaError e = transfer(&cmd, 1, 3);
    if (!e) *value = result[2];
    return e;
}
static TunaError write_word(uint32_t word, int runtime) {
    uint8_t cmd[5] = {0x15}; TunaError e;
    if (runtime) {
        uint64_t start = gettime(); uint8_t s = 0;
        do { e = status(&s); if (e) return e; if (!(s & TUNA_JOY_RECEIVE)) break; if (expired(start, 500)) return TUNA_TIMEOUT; usleep(100); } while (1);
    }
    tuna_put32(cmd + 1, word); return transfer(cmd, 5, 1);
}
static TunaError read_word(uint32_t *word, int runtime) {
    uint8_t cmd = 0x14; TunaError e = TUNA_OK;
    if (runtime) {
        uint64_t start = gettime(); uint8_t s = 0; unsigned budget = runtime == 2 ? 10000 : 1000;
        do {
            e = status(&s);
            if (!e && (s & TUNA_JOY_TRANSMIT)) break;
            /* Before it can answer, the GBA may spend longer than one 250 ms SI
               poll validating a large payload or computing output gain. Treat a
               transient timeout as "still busy" and keep polling to the overall
               budget; a real unplug only surfaces when that budget expires. */
            if (expired(start, budget)) return e ? e : TUNA_TIMEOUT;
            usleep(100);
        } while (1);
    }
    e = transfer(&cmd, 1, 5); if (!e) *word = tuna_u32(result); return e;
}
TunaError link_boot(const uint8_t *image, size_t length) {
    uint32_t size = (uint32_t)((length + 7) & ~(size_t)7), key, crc = 0x15a0, word;
    uint32_t offset; uint8_t s = 0, reset = 0xff; uint64_t start = gettime(); TunaError e;
    if (length < 0x200 || length > 0x10000 || !tuna_boot_key(size)) return TUNA_SIZE;
    SI_DisablePolling(SI_CHAN_BIT(channel));
    do {
        e = transfer(&reset, 1, 3); if (e && e != TUNA_IO) return e;
        e = status(&s); if (!e && (s & 0x10)) break;
        if (expired(start, 15000)) return TUNA_TIMEOUT;
        usleep(10000);
    } while (1);
    e = read_word(&word, 0); if (e) return e; key = word ^ 0x6f646573;
    word = tuna_boot_key(size);
    word = ((word & 255) << 24) | ((word & 0xff00) << 8) | ((word >> 8) & 0xff00) | (word >> 24);
    e = write_word(word, 0); if (e) return e;
    for (offset = 0; offset < 0xc0; offset += 4) { e = write_word(tuna_u32(image + offset), 0); if (e) return e; }
    for (; offset < size; offset += 4) {
        uint8_t padded[4] = {0}; unsigned n = offset < length ? (unsigned)(length - offset > 4 ? 4 : length - offset) : 0;
        if (n) memcpy(padded, image + offset, n);
        word = tuna_u32(padded); crc = tuna_boot_crc(crc, word);
        e = write_word(tuna_boot_encrypt(word, offset, &key), 0); if (e) return e;
    }
    e = write_word(tuna_boot_encrypt(crc | (size << 16), offset, &key), 0); if (e) return e;
    e = read_word(&word, 0); if (e) return e;
    printf("BIOS reply %08lx; checking application HELLO next.\n", (unsigned long)word);
    /* Wait for main(), not a fixed delay. The BIOS and application share the
       Joybus registers; a pending BIOS response is not a TNPK reply. */
    start = gettime();
    do {
        e = status(&s); if (e) return e;
        if (s & TUNA_JOY_APPLICATION) break;
        if (expired(start, 10000)) return TUNA_TIMEOUT;
        usleep(100);
    } while (1);
    if (s & TUNA_JOY_TRANSMIT) { e = read_word(&word, 0); if (e) return e; }
    return TUNA_OK;
}
static TunaError exchange(uint8_t type, uint32_t seq, uint32_t offset, const void *body, uint16_t length, TunaPacket *reply, uint8_t *response) {
    uint8_t packet[TUNA_PACKET_MAX]; size_t n = tuna_packet_write(packet, type, seq, offset, body, length);
    unsigned attempt;
    for (attempt = 0; attempt < 3; ++attempt) {
        size_t i, target = 20; TunaError e;
        for (i = 0; i < n; i += 4) {
            uint8_t word[4] = {0}; size_t count = n - i > 4 ? 4 : n - i;
            memcpy(word, packet + i, count); e = write_word(tuna_u32(word), 1); if (e) return e;
        }
        for (i = 0; i < target; i += 4) {
            uint32_t word; e = read_word(&word, type == TUNA_COMMIT ? 2 : 1); if (e) return e;
            tuna_put32(response + i, word);
            if (i == 16) {
                if (memcmp(response, "TNPK", 4) || tuna_u16(response + 6) > TUNA_CHUNK) {
                    printf("Invalid GBA reply during message %u: %02x %02x %02x %02x (expected TNPK).\n", type, response[0], response[1], response[2], response[3]);
                    return TUNA_FORMAT;
                }
                target += tuna_u16(response + 6);
            }
        }
        e = tuna_packet_read(reply, response, target);
        if (e == TUNA_CRC) continue;
        if (e) return e;
        if (reply->seq != seq) return TUNA_SEQUENCE;
        if (reply->type == TUNA_NACK) {
            if (reply->length != 4 || tuna_u32(reply->body) > TUNA_IO) return TUNA_FORMAT;
            e = (TunaError)tuna_u32(reply->body); if (e == TUNA_CRC) continue; return e ? e : TUNA_FORMAT;
        }
        return TUNA_OK;
    }
    return TUNA_CRC;
}
TunaError link_send_track(const uint8_t *track, size_t length) {
    TunaTrack info; TunaPacket reply; uint8_t response[TUNA_PACKET_MAX];
    uint32_t offset = 0, seq = 1; TunaError e = tuna_track_validate(track, length, &info);
    if (e) return e;
    e = exchange(TUNA_HELLO, 0, 0, NULL, 0, &reply, response); if (e) return e;
    if (reply.type != TUNA_CAPS || reply.length != 12 || tuna_u32(reply.body + 4) < TUNA_MAX_SECONDS || !(tuna_u32(reply.body + 8) & (1u << info.codec))) return TUNA_FORMAT;
    if (length > tuna_u32(reply.body)) return TUNA_SIZE;
    /* UI explicitly confirms replacement before calling this function. */
    e = exchange(TUNA_ABORT, 0, 0, NULL, 0, &reply, response); if (e) return e;
    if (reply.type != TUNA_ACK) return TUNA_STATE;
    e = exchange(TUNA_BEGIN, 0, 0, track, TUNA_HEADER, &reply, response); if (e) return e;
    if (reply.type != TUNA_ACK || reply.offset) return TUNA_STATE;
    while (offset < info.bytes) {
        uint16_t count = (uint16_t)(info.bytes - offset > TUNA_CHUNK ? TUNA_CHUNK : info.bytes - offset);
        e = exchange(TUNA_DATA, seq, offset, track + TUNA_HEADER + offset, count, &reply, response); if (e) return e;
        if (reply.type != TUNA_ACK || reply.offset != offset + count) return TUNA_SEQUENCE;
        offset += count; ++seq;
        if (!(seq % 32)) printf("Transferred %lu / %lu bytes\n", (unsigned long)offset, (unsigned long)info.bytes);
    }
    e = exchange(TUNA_COMMIT, seq, offset, NULL, 0, &reply, response); if (e) return e;
    if (reply.type != TUNA_READY || reply.offset != offset) return TUNA_STATE;
    printf("GBA verified CRC %08lx: READY. Safe to unplug.\n", (unsigned long)info.crc);
    return TUNA_OK;
}
