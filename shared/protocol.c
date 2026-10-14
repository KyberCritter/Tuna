#include "tuna.h"
#include <string.h>
#include <limits.h>

size_t tuna_packet_write(uint8_t *out, uint8_t type, uint32_t seq, uint32_t offset, const void *body, uint16_t length) {
    uint32_t crc;
    if (length > TUNA_CHUNK || (length && !body)) return 0;
    memcpy(out, "TNPK", 4); out[4] = TUNA_VERSION; out[5] = type;
    tuna_put16(out + 6, length); tuna_put32(out + 8, seq); tuna_put32(out + 12, offset);
    if (length) memcpy(out + 20, body, length);
    crc = tuna_crc_update(UINT32_MAX, out, 16);
    tuna_put32(out + 16, ~tuna_crc_update(crc, out + 20, length));
    return 20u + length;
}
TunaError tuna_packet_read(TunaPacket *p, const uint8_t *data, size_t n) {
    uint32_t c;
    if (n < 20 || memcmp(data, "TNPK", 4) || data[4] != TUNA_VERSION) return TUNA_FORMAT;
    p->length = tuna_u16(data + 6);
    if (p->length > TUNA_CHUNK || n != 20u + p->length) return TUNA_SIZE;
    c = tuna_crc_update(UINT32_MAX, data, 16);
    if (~tuna_crc_update(c, data + 20, p->length) != tuna_u32(data + 16)) return TUNA_CRC;
    p->type = data[5]; p->seq = tuna_u32(data + 8); p->offset = tuna_u32(data + 12); p->body = data + 20;
    return TUNA_OK;
}
void tuna_receiver_init(TunaReceiver *r, uint8_t *storage, uint32_t capacity) {
    memset(r, 0, sizeof(*r)); r->storage = storage; r->capacity = capacity; r->state = TUNA_WAIT;
}
void tuna_receiver_fail(TunaReceiver *r, TunaError error) { r->state = TUNA_FAILED; r->error = error; }
size_t tuna_receiver_packet(TunaReceiver *r, const uint8_t *request, size_t n, uint8_t *reply) {
    TunaPacket p; uint8_t body[12], type = TUNA_ACK; uint16_t len = 0;
    TunaError e = tuna_packet_read(&p, request, n); uint32_t seq = n >= 12 ? tuna_u32(request + 8) : 0;
    if (e) goto nack;
    seq = p.seq;
    switch (p.type) {
    case TUNA_HELLO:
        if (p.length) { e = TUNA_FORMAT; break; }
        type = TUNA_CAPS; len = 12; tuna_put32(body, r->capacity); tuna_put32(body + 4, TUNA_MAX_SECONDS); tuna_put32(body + 8, 14); break;
    case TUNA_BEGIN:
        if (p.length != TUNA_HEADER || p.seq || p.offset) { e = TUNA_FORMAT; break; }
        if ((r->state == TUNA_RECEIVE || r->state == TUNA_VERIFIED) && !memcmp(r->storage, p.body, TUNA_HEADER)) break;
        if (r->state != TUNA_WAIT) { e = TUNA_STATE; break; }
        e = tuna_header_read(&r->track, p.body, p.length, r->capacity);
        if (!e) { memcpy(r->storage, p.body, TUNA_HEADER); r->received = 0; r->next_seq = 1; r->state = TUNA_RECEIVE; }
        break;
    case TUNA_DATA:
        if (r->state != TUNA_RECEIVE || !p.length) { e = TUNA_STATE; break; }
        if (p.seq == r->next_seq - 1 && p.offset <= r->received && p.length <= r->received - p.offset &&
            p.offset + p.length == r->received && !memcmp(r->storage + TUNA_HEADER + p.offset, p.body, p.length)) break;
        if (p.seq != r->next_seq || p.offset != r->received) { e = TUNA_SEQUENCE; break; }
        if (p.offset > r->track.bytes || p.length > r->track.bytes - p.offset) { e = TUNA_SIZE; break; }
        memcpy(r->storage + TUNA_HEADER + p.offset, p.body, p.length); r->received += p.length; ++r->next_seq;
        break;
    case TUNA_COMMIT:
        if (p.length || p.offset != r->track.bytes || p.seq != r->next_seq) { e = TUNA_SEQUENCE; break; }
        if (r->state == TUNA_VERIFIED) { type = TUNA_READY; break; }
        if (r->state != TUNA_RECEIVE || r->received != r->track.bytes) { e = TUNA_STATE; break; }
        e = tuna_track_validate(r->storage, TUNA_HEADER + r->received, &r->track);
        if (e) tuna_receiver_fail(r, e); else { r->state = TUNA_VERIFIED; type = TUNA_READY; }
        break;
    case TUNA_ABORT:
        if (p.length) { e = TUNA_FORMAT; break; }
        tuna_receiver_init(r, r->storage, r->capacity); break;
    case TUNA_STATUS:
        if (p.length) { e = TUNA_FORMAT; break; }
        len = 8; tuna_put32(body, r->state); tuna_put32(body + 4, r->error); break;
    default: e = TUNA_FORMAT; break;
    }
nack:
    if (e) { type = TUNA_NACK; len = 4; tuna_put32(body, e); }
    return tuna_packet_write(reply, type, seq, r->received, body, len);
}
