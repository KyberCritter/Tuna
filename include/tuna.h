#ifndef TUNA_H
#define TUNA_H
#include <stddef.h>
#include <stdint.h>

#define TUNA_VERSION 1u
#define TUNA_RATE 8192u
#define TUNA_CAPACITY (192u * 1024u)
#define TUNA_HEADER 96u
#define TUNA_TITLE 60u
#define TUNA_BLOCK_SAMPLES 256u
#define TUNA_BLOCK_BYTES 132u
#define TUNA_MAX_SECONDS 300u
typedef enum { TUNA_IMA4 = 1, TUNA_ADPCM2 = 2, TUNA_DELTA1 = 3 } TunaCodec;
#define TUNA_CHUNK 256u
#define TUNA_PACKET_HEADER 20u
#define TUNA_PACKET_MAX (TUNA_PACKET_HEADER + TUNA_CHUNK)

typedef enum { TUNA_OK, TUNA_FORMAT, TUNA_CRC, TUNA_SIZE, TUNA_STATE,
               TUNA_SEQUENCE, TUNA_TIMEOUT, TUNA_IO } TunaError;
typedef struct {
    uint32_t samples, bytes, crc, rate;
    uint8_t codec;
    char title[TUNA_TITLE + 1];
} TunaTrack;
uint16_t tuna_u16(const uint8_t *p);
uint32_t tuna_u32(const uint8_t *p);
void tuna_put16(uint8_t *p, uint16_t v);
void tuna_put32(uint8_t *p, uint32_t v);
uint32_t tuna_crc_update(uint32_t crc, const void *data, size_t n);
uint32_t tuna_crc(const void *data, size_t n);
uint32_t tuna_encoded_size(uint32_t samples);
uint32_t tuna_profile_size(uint32_t samples, uint8_t codec);
unsigned tuna_profile_block(uint8_t codec);
TunaError tuna_choose_profile(uint32_t count, uint32_t rate, TunaTrack *track);
const char *tuna_profile_name(uint8_t codec);
TunaError tuna_header_write(uint8_t out[TUNA_HEADER], const TunaTrack *t);
TunaError tuna_header_read(TunaTrack *t, const uint8_t *p, size_t n, uint32_t capacity);
TunaError tuna_track_validate(const uint8_t *data, size_t n, TunaTrack *t);
void tuna_adpcm_encode(const int16_t *pcm, unsigned count, uint8_t out[TUNA_BLOCK_BYTES]);
TunaError tuna_adpcm_decode(const uint8_t in[TUNA_BLOCK_BYTES], int16_t out[TUNA_BLOCK_SAMPLES]);
void tuna_block_encode(uint8_t codec, const int16_t *pcm, unsigned count, uint8_t *out);
TunaError tuna_block_decode(uint8_t codec, const uint8_t *in, int16_t out[TUNA_BLOCK_SAMPLES]);
const char *tuna_error(TunaError e);

typedef enum { TUNA_HELLO = 1, TUNA_CAPS, TUNA_BEGIN, TUNA_DATA, TUNA_ACK,
               TUNA_NACK, TUNA_COMMIT, TUNA_READY, TUNA_ABORT, TUNA_STATUS } TunaMessage;
typedef struct {
    uint8_t type;
    uint16_t length;
    uint32_t seq, offset;
    const uint8_t *body;
} TunaPacket;
size_t tuna_packet_write(uint8_t *out, uint8_t type, uint32_t seq, uint32_t offset,
                         const void *body, uint16_t length);
TunaError tuna_packet_read(TunaPacket *p, const uint8_t *data, size_t n);
typedef enum { TUNA_WAIT, TUNA_RECEIVE, TUNA_VERIFIED, TUNA_FAILED } TunaReceiveState;
typedef struct {
    uint8_t *storage;
    uint32_t capacity, received, next_seq;
    TunaTrack track;
    TunaReceiveState state;
    TunaError error;
} TunaReceiver;
void tuna_receiver_init(TunaReceiver *r, uint8_t *storage, uint32_t capacity);
void tuna_receiver_fail(TunaReceiver *r, TunaError error);
size_t tuna_receiver_packet(TunaReceiver *r, const uint8_t *request, size_t n, uint8_t *reply);

typedef enum { TUNA_PAUSED, TUNA_PLAYING, TUNA_ENDED } TunaPlayState;
typedef struct {
    const uint8_t *payload;
    uint32_t samples, position, block;
    unsigned in_block;
    int16_t decoded[TUNA_BLOCK_SAMPLES];
    TunaPlayState state;
    uint8_t a_held, codec;
} TunaPlayer;
void tuna_player_init(TunaPlayer *p, const uint8_t *payload, uint32_t samples);
void tuna_player_init_track(TunaPlayer *p, const uint8_t *payload, const TunaTrack *track);
void tuna_player_key(TunaPlayer *p, int a_down);
size_t tuna_player_render(TunaPlayer *p, int8_t *out, size_t count);

/* Pull-based mono FIR resampler. Fetch must return the same sample on repeated indices. */
typedef int16_t (*TunaSampleFn)(void *context, uint32_t index);
int16_t tuna_resample(TunaSampleFn fetch, void *context, uint32_t input_samples,
                      uint32_t input_rate, uint32_t output_index);
int16_t tuna_resample_rate(TunaSampleFn fetch, void *context, uint32_t input_samples,
                           uint32_t input_rate, uint32_t output_rate, uint32_t output_index);
void tuna_jingle(int16_t *out, uint32_t count);
uint32_t tuna_boot_key(uint32_t padded_size);
uint32_t tuna_boot_crc(uint32_t crc, uint32_t word);
uint32_t tuna_boot_encrypt(uint32_t word, uint32_t offset, uint32_t *key);
#endif
