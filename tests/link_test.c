/* Executes production sender/link.c with a deterministic SI register model.
   This is a host regression, NOT BIOS/emulator/hardware acceptance. */
#include <gccore.h>
#include "tuna.h"
#include "convert.h"
#include "link.h"
#include "joybus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
static TunaReceiver receiver;
static uint8_t storage[TUNA_CAPACITY], request[TUNA_PACKET_MAX], reply[TUNA_PACKET_MAX];
static size_t received, target = TUNA_PACKET_HEADER, reply_bytes, reply_offset;
static unsigned boot_writes, boot_polls, stale_drained, busy, corruption, duplicates, disconnect_data;
static int boot_mode = 1, bad_magic;
static uint64_t clock_ms;
uint64_t gettime(void) { return ++clock_ms; }
uint64_t ticks_to_millisecs(uint64_t ticks) { return ticks; }
void SI_DisablePolling(uint32_t mask) { CHECK(mask == SI_CHAN_BIT(1)); }
int SI_IsChanBusy(s32 channel) { CHECK(channel == 1); return 0; }
int SI_Transfer(s32 channel, void *out, u32 out_length, void *in, u32 in_length,
                void (*callback)(s32, u32), u32 delay) {
    const uint8_t *command = (const uint8_t *)out; uint8_t *response = (uint8_t *)in; uint32_t error = 0;
    CHECK(channel == 1 && delay == 50); memset(response, 0, in_length);
    switch (command[0]) {
    case 0xff:
        CHECK(boot_mode && out_length == 1 && in_length == 3); response[2] = 0x10; break;
    case 0:
        CHECK(out_length == 1 && in_length == 3);
        if (boot_mode) {
            if (!boot_writes) response[2] = 0x10;
            else response[2] = ++boot_polls < 3 ? TUNA_JOY_TRANSMIT : TUNA_JOY_APPLICATION | TUNA_JOY_TRANSMIT;
        } else {
            response[2] = TUNA_JOY_APPLICATION;
            if (busy) { response[2] |= TUNA_JOY_RECEIVE; --busy; }
            if (reply_bytes) response[2] |= TUNA_JOY_TRANSMIT;
        }
        break;
    case 0x15:
        CHECK(out_length == 5 && in_length == 1);
        if (boot_mode) { ++boot_writes; break; }
        CHECK(!busy && !reply_bytes);
        if (disconnect_data && received >= 8 && request[5] == TUNA_DATA) { error = 8; break; }
        memcpy(request + received, command + 1, target - received < 4 ? target - received : 4);
        received += target - received < 4 ? target - received : 4;
        if (received == TUNA_PACKET_HEADER) target += tuna_u16(request + 6);
        CHECK(target <= sizeof(request));
        busy = 1;
        if (received == target) {
            TunaPacket packet;
            CHECK(tuna_packet_read(&packet, request, received) == TUNA_OK);
            if (packet.type == TUNA_DATA && packet.seq == receiver.next_seq - 1) ++duplicates;
            reply_bytes = tuna_receiver_packet(&receiver, request, received, reply); reply_offset = 0;
            if (corruption && packet.type == TUNA_DATA) { reply[16] ^= 1; --corruption; }
            if (bad_magic) reply[0] = 'X';
            received = 0; target = TUNA_PACKET_HEADER;
        }
        break;
    case 0x14:
        CHECK(out_length == 1 && in_length == 5);
        if (boot_mode) {
            tuna_put32(response, 0x12345678);
            if (boot_polls >= 3) { ++stale_drained; boot_mode = 0; }
        } else {
            CHECK(reply_bytes && reply_offset < reply_bytes);
            memcpy(response, reply + reply_offset, reply_bytes - reply_offset < 4 ? reply_bytes - reply_offset : 4);
            response[4] = TUNA_JOY_APPLICATION | TUNA_JOY_TRANSMIT;
            reply_offset += 4;
            if (reply_offset >= reply_bytes) reply_bytes = reply_offset = 0;
        }
        break;
    default: CHECK(0);
    }
    callback(channel, error); return 1;
}
static void reset_transport(void) {
    received = reply_bytes = reply_offset = busy = 0; target = TUNA_PACKET_HEADER;
    tuna_receiver_init(&receiver, storage, sizeof(storage));
}
int main(void) {
    uint8_t image[512] = {0}, *track; size_t length;
    reset_transport(); link_port(1);
    CHECK(link_boot(image, sizeof(image)) == TUNA_OK);
    CHECK(boot_writes == 130 && boot_polls == 3 && stale_drained == 1);
    CHECK(tuna_make_jingle(&track, &length) == TUNA_OK);
    corruption = 1;
    CHECK(link_send_track(track, length) == TUNA_OK);
    CHECK(receiver.state == TUNA_VERIFIED && !memcmp(storage, track, length));
    CHECK(duplicates == 1); /* Corrupt ACK -> duplicate DATA must not append twice. */
    CHECK(link_send_track(track, length) == TUNA_OK); /* Explicit replacement. */
    CHECK(receiver.state == TUNA_VERIFIED && !memcmp(storage, track, length));
    reset_transport(); bad_magic = 1;
    CHECK(link_send_track(track, length) == TUNA_FORMAT && receiver.state != TUNA_VERIFIED);
    reset_transport(); bad_magic = 0; disconnect_data = 1;
    CHECK(link_send_track(track, length) == TUNA_IO && receiver.state != TUNA_VERIFIED);
    reset_transport(); disconnect_data = 0;
    CHECK(link_send_track(track, length) == TUNA_OK); /* Retry identical prepared bytes. */
    CHECK(receiver.state == TUNA_VERIFIED && !memcmp(storage, track, length));
    free(track); printf("PASS: %u production sender checks with SI model (NOT emulator acceptance)\n", checks);
    return 0;
}
