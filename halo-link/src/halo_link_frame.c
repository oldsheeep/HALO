// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// フレームの組立・解析・CRC・受信パーサ(MCU 非依存)。
#include <string.h>

#include "halo_link.h"

uint16_t halo_link_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= (uint16_t)((uint16_t)*data++ << 8);
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

void halo_link_frame_encode(const halo_link_frame_t *f, uint8_t out[HALO_LINK_FRAME_LEN]) {
    out[0] = f->sof;
    out[1] = f->seq;
    out[2] = f->cmd;
    out[3] = f->flags_status;
    memcpy(&out[4], f->payload, HALO_LINK_PAYLOAD_LEN);
    uint16_t crc = halo_link_crc16(&out[1], 13); // byte 1〜13
    out[14] = (uint8_t)(crc & 0xFF);
    out[15] = (uint8_t)(crc >> 8);
}

bool halo_link_frame_decode(const uint8_t in[HALO_LINK_FRAME_LEN], halo_link_frame_t *out) {
    uint16_t crc = halo_link_crc16(&in[1], 13);
    if (in[14] != (uint8_t)(crc & 0xFF) || in[15] != (uint8_t)(crc >> 8))
        return false;
    out->sof = in[0];
    out->seq = in[1];
    out->cmd = in[2];
    out->flags_status = in[3];
    memcpy(out->payload, &in[4], HALO_LINK_PAYLOAD_LEN);
    return true;
}

void halo_link_parser_init(halo_link_parser_t *p, uint8_t expect_sof) {
    p->len = 0;
    p->sof = expect_sof;
    p->last_us = 0;
}

// CRC が合わなかったとき: 先頭の 1 バイトを捨て、次の SOF 候補が先頭になるまで詰める。
static void resync(halo_link_parser_t *p) {
    uint8_t i = 1;
    while (i < p->len && p->buf[i] != p->sof)
        i++;
    if (i >= p->len) {
        p->len = 0;
        return;
    }
    memmove(p->buf, &p->buf[i], (size_t)(p->len - i));
    p->len = (uint8_t)(p->len - i);
}

bool halo_link_parser_feed(halo_link_parser_t *p, uint8_t byte, uint32_t now_us,
                           halo_link_frame_t *out) {
    if (p->len > 0 && (uint32_t)(now_us - p->last_us) >= HALO_LINK_BYTE_TIMEOUT_US)
        p->len = 0;
    p->last_us = now_us;

    if (p->len == 0 && byte != p->sof)
        return false; // エコーなど。フレームの先頭では捨てる

    p->buf[p->len++] = byte;
    if (p->len < HALO_LINK_FRAME_LEN)
        return false;

    if (halo_link_frame_decode(p->buf, out)) {
        p->len = 0;
        return true;
    }
    resync(p);
    return false;
}
