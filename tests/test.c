#include "tuna.h"
#include "convert.h"
#include "track_store.h"
#include "joybus.h"
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
static int16_t phase_signal(void *context, uint32_t index) {
    uint32_t value = index * 1664525u + *(const uint32_t *)context;
    return (int16_t)(value >> 16);
}
static void cached_resampler(void) {
    static const uint32_t rates[] = {8000,11025,16000,22050,32000,44100,48000};
    uint32_t seed = 0x6f726967, random = 0x12345678; unsigned i, out, j; TunaResampler r;
    for (i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i) for (out = 4096; out <= 8192; out *= 2) {
        uint32_t samples = rates[i] * 300, count = out * 300;
        CHECK(tuna_resampler_init(&r, rates[i], out) == TUNA_OK && r.weights);
        CHECK((size_t)r.phases * r.stride * sizeof(double) <= 4u * 1024u * 1024u);
        for (j = 0; j < 2000; ++j) {
            uint32_t index;
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            index = j < 64 ? j : j < 128 ? count - 1 - (j - 64) : random % count;
            CHECK(tuna_resampler_sample(&r, phase_signal, &seed, samples, index) ==
                  tuna_resample_rate(phase_signal, &seed, samples, rates[i], out, index));
        }
        CHECK(tuna_resampler_sample(&r, phase_signal, &seed, 1, 0) == tuna_resample_rate(phase_signal, &seed, 1, rates[i], out, 0));
        CHECK(tuna_resampler_sample(&r, phase_signal, &seed, 0, 0) == 0);
        tuna_resampler_free(&r); CHECK(!r.weights);
    }
    CHECK(tuna_resampler_init(&r, 47999, 8192) == TUNA_SIZE && !r.weights);
    CHECK(tuna_resampler_sample(&r, phase_signal, &seed, 47999, 37) == tuna_resample_rate(phase_signal, &seed, 47999, 47999, 8192, 37));
    tuna_resampler_free(&r);
    CHECK(tuna_resampler_init(&r, 48000, 10000) == TUNA_FORMAT && !r.weights);
    CHECK(tuna_resampler_sample(&r, phase_signal, &seed, 48000, 37) == tuna_resample_rate(phase_signal, &seed, 48000, 48000, 10000, 37));
    tuna_resampler_free(&r);
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
static void boundaries(void) {
    uint8_t block[132], *track; int16_t pcm[257]; TunaPlayer player; TunaTrack info;
    int8_t output[300]; size_t length; unsigned i; TunaReceiver r; TunaPacket p;
    for (i = 0; i < 257; ++i) pcm[i] = (int16_t)((int)i * 128 - 16384);
    CHECK(tuna_convert_pcm(pcm, 257, TUNA_RATE, "Partial block", &track, &length) == TUNA_OK);
    CHECK(length == 360); CHECK(tuna_track_validate(track, length, &info) == TUNA_OK);
    tuna_player_init(&player, track + 96, 257); tuna_player_key(&player, 1);
    CHECK(tuna_player_render(&player, output, 300) == 257); CHECK(player.position == 257);
    for (i = 257; i < 300; ++i) CHECK(output[i] == 0);
    tuna_receiver_init(&r, storage, sizeof(storage)); transact(&r, TUNA_BEGIN, 0, 0, track, 96);
    p = transact(&r, TUNA_DATA, 1, UINT32_MAX, track + 96, 1); CHECK(p.type == TUNA_NACK && r.received == 0);
    transact(&r, TUNA_DATA, 1, 0, track + 96, 256);
    track[100] ^= 1;
    p = transact(&r, TUNA_DATA, 1, 0, track + 96, 256); CHECK(p.type == TUNA_NACK && r.received == 256);
    transact(&r, TUNA_DATA, 2, 256, track + 352, 8);
    storage[100] ^= 1;
    p = transact(&r, TUNA_COMMIT, 3, 264, NULL, 0); CHECK(p.type == TUNA_NACK && r.state == TUNA_FAILED);
    free(track);
    CHECK(tuna_convert_pcm(pcm, 257, 0, "Invalid rate", &track, &length) == TUNA_FORMAT);
    CHECK(tuna_convert_pcm(pcm, UINT32_MAX, 8192, "Oversize", &track, &length) == TUNA_SIZE);
    CHECK(!track && !length);
    memset(block, 0, sizeof(block)); block[131] = 0x10;
    CHECK(tuna_adpcm_decode(block, pcm) == TUNA_FORMAT);
    { FILE *f = fopen("malformed-test.mp3", "wb"); CHECK(f != NULL); CHECK(fwrite("ID3\4\0\0\x7f\x7f\x7f\x7f", 1, 10, f) == 10); CHECK(!fclose(f));
      CHECK(tuna_convert_mp3("malformed-test.mp3", "Bad metadata", 0, &track, &length) == TUNA_FORMAT); CHECK(!remove("malformed-test.mp3")); }
    CHECK(tuna_convert_mp3("__missing_tuna_test__.mp3", "Unavailable", 0, &track, &length) == TUNA_IO);
    CHECK(tuna_convert_mp3("__missing_tuna_test__.mp3", "Too long excerpt", 301, &track, &length) == TUNA_SIZE);
    CHECK(tuna_boot_key(0) == 0); CHECK(tuna_boot_key(513) == 0); CHECK(tuna_boot_key(0x40008) == 0);
    CHECK(tuna_boot_key(0x200) == 0xcbe1cfd9); /* independent FIX94 vector */
    { uint32_t key = 0; CHECK(tuna_boot_encrypt(0, 0xc0, &key) == 0xdd869d61); }
}
static void adaptive_profiles(void) {
    TunaTrack t, parsed; uint8_t block[132], *track; int16_t decoded[256];
    int16_t *pcm; size_t length; TunaPlayer p; int8_t output[256]; unsigned i;
    memset(&t, 0, sizeof(t));
    CHECK(tuna_choose_profile(40 * 48000, 48000, &t) == TUNA_OK && t.codec == TUNA_IMA4 && t.rate == 8192);
    CHECK(tuna_choose_profile(60 * 48000, 48000, &t) == TUNA_OK && t.codec == TUNA_ADPCM2 && t.rate == 8192);
    CHECK(tuna_choose_profile(180 * 48000, 48000, &t) == TUNA_OK && t.codec == TUNA_ADPCM2 && t.rate == 4096);
    CHECK(t.bytes + 96 == 195936);
    CHECK(tuna_choose_profile(300 * 48000, 48000, &t) == TUNA_OK && t.codec == TUNA_DELTA1 && t.rate == 4096);
    CHECK(t.bytes + 96 == 172896);
    CHECK(tuna_choose_profile(301 * 48000, 48000, &t) == TUNA_SIZE);
    memset(block, 0, sizeof(block)); block[4] = 0x0d;
    CHECK(tuna_block_decode(TUNA_ADPCM2, block, decoded) == TUNA_OK);
    CHECK(decoded[1] == 10 && decoded[2] == -6);
    memset(block, 0, sizeof(block)); block[4] = 3;
    CHECK(tuna_block_decode(TUNA_DELTA1, block, decoded) == TUNA_OK);
    CHECK(decoded[1] == 7 && decoded[2] == 14);
    block[2] = 255; CHECK(tuna_block_decode(TUNA_DELTA1, block, decoded) == TUNA_FORMAT);
    pcm = (int16_t *)malloc(300u * 4096 * sizeof(int16_t)); CHECK(pcm != NULL);
    for (i = 0; i < 300u * 4096; ++i) pcm[i] = (int16_t)(10000 * sin(6.283185307179586 * 220 * i / 4096));
    CHECK(tuna_convert_pcm(pcm, 300u * 4096, 4096, "Complete five-minute tone", &track, &length) == TUNA_OK);
    CHECK(length == 172896); CHECK(tuna_track_validate(track, length, &parsed) == TUNA_OK);
    CHECK(parsed.samples == 1228800 && parsed.codec == TUNA_DELTA1 && parsed.rate == 4096);
    tuna_player_init_track(&p, track + 96, &parsed); tuna_player_key(&p, 1);
    while (p.state == TUNA_PLAYING) CHECK(tuna_player_render(&p, output, 256) == 256);
    CHECK(p.position == 300u * 4096);
    /* Report error as a diagnostic, not a subjective music-quality acceptance. */
    for (i = TUNA_ADPCM2; i <= TUNA_DELTA1; ++i) {
        double signal = 0, error = 0; unsigned j;
        tuna_block_encode((uint8_t)i, pcm, 256, block); CHECK(tuna_block_decode((uint8_t)i, block, decoded) == TUNA_OK);
        for (j = 0; j < 256; ++j) { double d = pcm[j] - decoded[j]; signal += (double)pcm[j] * pcm[j]; error += d * d; }
        CHECK(error < signal); printf("Profile %u / 220 Hz tone SNR: %.2f dB (not music acceptance)\n", i, 10 * log10(signal / error));
    }
    free(track); free(pcm);
}
static void track_storage(void) {
    uint8_t *track, *loaded = NULL; size_t length, loaded_length; FILE *f;
    char full[TUNA_PATH_MAX], again[TUNA_PATH_MAX], excerpt[TUNA_PATH_MAX], changed[TUNA_PATH_MAX];
    char oversized[TUNA_PATH_MAX + 1];
    CHECK(TUNA_JOY_RECEIVE == 2 && TUNA_JOY_TRANSMIT == 8 && TUNA_JOY_APPLICATION == 32);
    f = fopen("cache-source-test.mp3", "wb"); CHECK(f != NULL);
    CHECK(fwrite("source", 1, 6, f) == 6); CHECK(!fclose(f));
    CHECK(tuna_cache_path(".", "cache-source-test.mp3", 0, full) == TUNA_OK);
    CHECK(tuna_cache_path(".", "cache-source-test.mp3", 0, again) == TUNA_OK && !strcmp(full, again));
    CHECK(tuna_cache_path(".", "cache-source-test.mp3", 30, excerpt) == TUNA_OK && strcmp(full, excerpt));
    CHECK(tuna_cache_path(".", "cache-source-test.mp3", 301, changed) == TUNA_SIZE && !changed[0]);
    memset(oversized, 'x', sizeof(oversized) - 1); oversized[sizeof(oversized) - 1] = 0;
    CHECK(tuna_cache_path(oversized, "cache-source-test.mp3", 0, changed) == TUNA_SIZE && !changed[0]);
    CHECK(tuna_make_jingle(&track, &length) == TUNA_OK);
    CHECK(tuna_track_save(full, track, length) == TUNA_OK);
    /* A failed transfer does not remove the published conversion. */
    { TunaReceiver r; tuna_receiver_init(&r, storage, sizeof(storage));
      transact(&r, TUNA_BEGIN, 0, 0, track, TUNA_HEADER); tuna_receiver_fail(&r, TUNA_TIMEOUT); }
    CHECK(tuna_track_load(full, &loaded, &loaded_length) == TUNA_OK);
    CHECK(length == loaded_length && !memcmp(track, loaded, length)); free(loaded);
    CHECK(tuna_track_save(full, track, length) == TUNA_IO); /* No overwriting user files. */
    track[0] ^= 1; CHECK(tuna_track_save(excerpt, track, length) == TUNA_FORMAT); track[0] ^= 1;
    CHECK(tuna_track_load(excerpt, &loaded, &loaded_length) == TUNA_IO && !loaded && !loaded_length);
    CHECK(tuna_track_save("__missing_cache_dir__/test.tuna", track, length) == TUNA_IO);
    f = fopen("cache-source-test.mp3", "ab"); CHECK(f != NULL); CHECK(fputc('!', f) != EOF); CHECK(!fclose(f));
    CHECK(tuna_cache_path(".", "cache-source-test.mp3", 0, changed) == TUNA_OK && strcmp(full, changed));
    f = fopen(full, "wb"); CHECK(f != NULL); CHECK(fwrite(track, 1, 12, f) == 12); CHECK(!fclose(f));
    CHECK(tuna_track_load(full, &loaded, &loaded_length) == TUNA_SIZE && !loaded && !loaded_length);
    f = fopen(full, "wb"); CHECK(f != NULL); track[length - 1] ^= 1;
    CHECK(fwrite(track, 1, length, f) == length); CHECK(!fclose(f));
    CHECK(tuna_track_load(full, &loaded, &loaded_length) == TUNA_CRC && !loaded && !loaded_length);
    CHECK(!remove(full)); CHECK(!remove("cache-source-test.mp3")); free(track);
}
int main(void) { format_codec(); transfer_player(); resampler(); cached_resampler(); boundaries(); adaptive_profiles(); track_storage(); fuzz(); printf("PASS: %u checks (codec, format, protocol, retained-RAM player, FIR, storage, malformed input)\n", checks); return 0; }
