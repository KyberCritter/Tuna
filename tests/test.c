#include "tuna.h"
#include "convert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
static uint8_t storage[TUNA_CAPACITY], request[TUNA_PACKET_MAX], response[TUNA_PACKET_MAX];
static TunaPacket transact(TunaReceiver *r, uint8_t type, uint32_t seq, uint32_t offset, const void *body, uint16_t length) {
    TunaPacket p; size_t n = tuna_packet_write(request, type, seq, offset, body, length);
    n = tuna_receiver_packet(r, request, n, response);
    CHECK(tuna_packet_read(&p, response, n) == TUNA_OK); return p;
}
static void format_codec(void) {
    uint8_t block[132], header[96]; int16_t pcm[256], decoded[256]; TunaTrack t, parsed; unsigned i;
    CHECK(tuna_crc("123456789", 9) == 0xcbf43926);
    CHECK(tuna_encoded_size(UINT32_MAX) == 2214592512u);
    memset(pcm, 0, sizeof(pcm)); tuna_adpcm_encode(pcm, 256, block);
    CHECK(tuna_adpcm_decode(block, decoded) == TUNA_OK);
    for (i = 0; i < 256; ++i) CHECK(decoded[i] == 0);
    /* Independent IMA vector: predictor 0/index 0, nibble 7 -> +11/index 8, nibble 15 -> -30. */
    memset(block, 0, sizeof(block)); block[4] = 0xf7;
    CHECK(tuna_adpcm_decode(block, decoded) == TUNA_OK); CHECK(decoded[1] == 11); CHECK(decoded[2] == -19);
    block[2] = 89; CHECK(tuna_adpcm_decode(block, decoded) == TUNA_FORMAT);
    memset(&t, 0, sizeof(t)); t.samples = 257; t.bytes = 264; strcpy(t.title, "Test title");
    CHECK(tuna_header_write(header, &t) == TUNA_OK); CHECK(header[16] == 1 && header[17] == 1);
    CHECK(tuna_header_read(&parsed, header, 96, TUNA_CAPACITY) == TUNA_OK);
    CHECK(!strcmp(parsed.title, t.title)); CHECK(tuna_header_read(&parsed, header, 95, TUNA_CAPACITY) == TUNA_FORMAT);
    CHECK(tuna_header_read(&parsed, header, 96, 100) == TUNA_SIZE);
    header[40] ^= 1; CHECK(tuna_header_read(&parsed, header, 96, TUNA_CAPACITY) == TUNA_CRC);
}
static void transfer_player(void) {
    uint8_t *track; size_t length; TunaReceiver r; TunaPacket p; TunaTrack info; TunaPlayer player;
    uint32_t offset = 0, seq = 1; int8_t audio[256], first[256];
    CHECK(tuna_make_jingle(&track, &length) == TUNA_OK);
    CHECK(tuna_track_validate(track, length, &info) == TUNA_OK);
    tuna_receiver_init(&r, storage, sizeof(storage));
    p = transact(&r, TUNA_HELLO, 0, 0, NULL, 0); CHECK(p.type == TUNA_CAPS); CHECK(tuna_u32(p.body) == TUNA_CAPACITY);
    p = transact(&r, TUNA_BEGIN, 0, 0, track, 96); CHECK(p.type == TUNA_ACK);
    p = transact(&r, TUNA_DATA, 99, 0, track + 96, 256); CHECK(p.type == TUNA_NACK && r.received == 0);
    p = transact(&r, TUNA_COMMIT, 1, info.bytes, NULL, 0); CHECK(p.type == TUNA_NACK);
    while (offset < info.bytes) {
        uint16_t n = (uint16_t)(info.bytes - offset > 256 ? 256 : info.bytes - offset);
        p = transact(&r, TUNA_DATA, seq, offset, track + 96 + offset, n); CHECK(p.type == TUNA_ACK);
        p = transact(&r, TUNA_DATA, seq, offset, track + 96 + offset, n); CHECK(p.type == TUNA_ACK && r.received == offset + n);
        offset += n; ++seq;
    }
    p = transact(&r, TUNA_COMMIT, seq, offset, NULL, 0); CHECK(p.type == TUNA_READY);
    p = transact(&r, TUNA_COMMIT, seq, offset, NULL, 0); CHECK(p.type == TUNA_READY);
    CHECK(r.state == TUNA_VERIFIED && !memcmp(track, storage, length));
    /* No receiver or link call below: retained RAM, pause, exact count, replay. */
    tuna_player_init(&player, storage + 96, info.samples);
    CHECK(tuna_player_render(&player, audio, 256) == 0 && player.position == 0);
    tuna_player_key(&player, 1); tuna_player_key(&player, 1); CHECK(player.state == TUNA_PLAYING);
    CHECK(tuna_player_render(&player, first, 256) == 256);
    tuna_player_key(&player, 0); tuna_player_key(&player, 1); CHECK(player.state == TUNA_PAUSED);
    CHECK(tuna_player_render(&player, audio, 256) == 0 && player.position == 256);
    tuna_player_key(&player, 0); tuna_player_key(&player, 1);
    while (player.state == TUNA_PLAYING) tuna_player_render(&player, audio, 256);
    CHECK(player.position == info.samples && player.state == TUNA_ENDED);
    tuna_player_key(&player, 0); tuna_player_key(&player, 1);
    CHECK(tuna_player_render(&player, audio, 256) == 256); CHECK(!memcmp(first, audio, 256));
    p = transact(&r, TUNA_BEGIN, 0, 0, track, 96); CHECK(p.type == TUNA_ACK && r.received == offset);
    p = transact(&r, TUNA_ABORT, 0, 0, NULL, 0); CHECK(p.type == TUNA_ACK && r.state == TUNA_WAIT);
    transact(&r, TUNA_BEGIN, 0, 0, track, 96); tuna_receiver_fail(&r, TUNA_TIMEOUT);
    CHECK(r.state == TUNA_FAILED); p = transact(&r, TUNA_COMMIT, 1, offset, NULL, 0); CHECK(p.type == TUNA_NACK);
    free(track);
}
static int16_t constant(void *context, uint32_t i) { (void)context; (void)i; return 1234; }
static int16_t high_tone(void *context, uint32_t i) { (void)context; return (int16_t)(10000 * sin(6.283185307179586 * 12000 * i / 48000)); }
static void resampler(void) {
    unsigned i; double energy = 0;
    CHECK(abs(tuna_resample(constant, NULL, 48000, 48000, 200) - 1234) <= 1);
    for (i = 100; i < 200; ++i) { double v = tuna_resample(high_tone, NULL, 48000, 48000, i); energy += v * v; }
    CHECK(sqrt(energy / 100) < 50);
}
static void fuzz(void) {
    uint32_t random = 0x12345678; unsigned i, j; TunaPacket p; TunaTrack t; TunaReceiver r;
    tuna_receiver_init(&r, storage, sizeof(storage));
    for (i = 0; i < 10000; ++i) {
        for (j = 0; j < sizeof(request); ++j) { random ^= random << 13; random ^= random >> 17; random ^= random << 5; request[j] = (uint8_t)random; }
        tuna_packet_read(&p, request, i % sizeof(request)); tuna_header_read(&t, request, i % sizeof(request), TUNA_CAPACITY);
        tuna_receiver_packet(&r, request, i % sizeof(request), response);
        CHECK(r.received <= TUNA_CAPACITY);
    }
}
int main(void) { format_codec(); transfer_player(); resampler(); fuzz(); printf("PASS: %u checks (codec, format, protocol, retained-RAM player, FIR, malformed input)\n", checks); return 0; }
