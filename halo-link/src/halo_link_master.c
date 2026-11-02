// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// マスター(B 側): halo_t。要求の送信・応答待ち・再送と、halo.res の更新。
#include <string.h>

#include "halo_link.h"

#define MIN3(a, b) ((a) < (b) ? (a) : (b))

static uint32_t now_us(halo_t *h) { return h->port.now_us(h->port.ctx); }

static void yield_now(halo_t *h) {
    if (h->port.yield)
        h->port.yield(h->port.ctx);
}

// ---- res の更新 -----------------------------------------------------------------------

static void begin(halo_t *h) {
    h->res.ok = false;
    memset(&h->res.error, 0, sizeof h->res.error);
}

static bool done(halo_t *h, halo_err_kind_t kind, uint8_t device_status, uint8_t index) {
    h->res.error.kind = kind;
    h->res.error.device_status = device_status;
    h->res.error.index = index;
    h->res.ok = (kind == HALO_ERR_NONE);
    if (!h->res.ok)
        h->res.status.fresh = false;
    h->res.status.age_ms = halo_status_age_ms(h);
    return h->res.ok;
}

uint32_t halo_status_age_ms(halo_t *h) {
    if (!h->res.status.valid)
        return 0;
    return (uint32_t)(now_us(h) - h->status_us) / 1000u;
}

static void stamp_status(halo_t *h) {
    h->res.status.fresh = true;
    h->status_us = now_us(h);
}

static void read_state(halo_t *h, const uint8_t *p) {
    h->res.status.mode = p[0];
    h->res.status.preset_id = p[1];
    h->res.status.brightness = p[2];
}

// ---- 送受信 ---------------------------------------------------------------------------

static void drain(halo_t *h) {
    uint8_t tmp[HALO_LINK_FRAME_LEN];
    for (int guard = 0; guard < 64; guard++)
        if (h->port.read(h->port.ctx, tmp, sizeof tmp) == 0)
            break;
}

// 要求を送り、対応する応答を受ける。最大 HALO_LINK_MAX_TRIES 回(同じ SEQ で再送)。
// 有効な応答を受けたら true(STATUS が OK とは限らない)。
static bool transact(halo_t *h, uint8_t cmd, uint8_t flags, const uint8_t *payload, size_t plen,
                     halo_link_frame_t *resp) {
    halo_link_frame_t req;
    memset(&req, 0, sizeof req);
    req.sof = HALO_LINK_SOF_REQ;
    req.seq = ++h->seq;
    req.cmd = cmd;
    req.flags_status = flags;
    if (payload && plen <= HALO_LINK_PAYLOAD_LEN)
        memcpy(req.payload, payload, plen);
    uint8_t raw[HALO_LINK_FRAME_LEN];
    halo_link_frame_encode(&req, raw);

    for (int attempt = 0; attempt < HALO_LINK_MAX_TRIES; attempt++) {
        while (h->has_last && (uint32_t)(now_us(h) - h->last_us) < HALO_LINK_MIN_GAP_US)
            yield_now(h);
        drain(h);
        halo_link_parser_init(&h->parser, HALO_LINK_SOF_RES);

        size_t sent = 0;
        while (sent < sizeof raw) {
            size_t n = h->port.write(h->port.ctx, raw + sent, sizeof raw - sent);
            if (n == 0)
                break;
            sent += n;
        }
        h->last_us = now_us(h);
        h->has_last = true;
        if (sent < sizeof raw)
            continue;

        uint32_t start = h->last_us;
        bool busy = false;
        while (!busy && (uint32_t)(now_us(h) - start) < HALO_LINK_RESPONSE_TIMEOUT_US) {
            uint8_t tmp[HALO_LINK_FRAME_LEN];
            size_t n = h->port.read(h->port.ctx, tmp, sizeof tmp);
            if (n == 0) {
                yield_now(h);
                continue;
            }
            uint32_t t = now_us(h);
            h->last_us = t;
            for (size_t i = 0; i < n && !busy; i++) {
                halo_link_frame_t f;
                if (!halo_link_parser_feed(&h->parser, tmp[i], t, &f))
                    continue;
                if (f.seq != req.seq || f.cmd != (uint8_t)(cmd | 0x80))
                    continue; // 古い応答など
                if (f.flags_status == HALO_STATUS_BUSY) {
                    busy = true; // 再送へ
                    break;
                }
                *resp = f;
                return true;
            }
        }
    }
    return false;
}

// transact + エラーの記録。成功(STATUS が OK)なら true。
static bool request(halo_t *h, uint8_t cmd, uint8_t flags, const uint8_t *payload, size_t plen,
                    uint8_t index, halo_link_frame_t *resp) {
    if (!transact(h, cmd, flags, payload, plen, resp)) {
        done(h, HALO_ERR_TIMEOUT, 0, index);
        return false;
    }
    if (resp->flags_status != HALO_STATUS_OK) {
        done(h, HALO_ERR_DEVICE_STATUS, resp->flags_status, index);
        return false;
    }
    return true;
}

// ---- 公開 API -------------------------------------------------------------------------

bool halo_call(halo_t *h) {
    begin(h);
    halo_link_frame_t hello, state;
    if (!request(h, HALO_CMD_HELLO, 0, NULL, 0, 0, &hello))
        return false;
    if (hello.payload[0] > HALO_LINK_PROTO_VERSION)
        return done(h, HALO_ERR_VERSION, 0, 0);
    if (!request(h, HALO_CMD_GET_STATE, 0, NULL, 0, 0, &state))
        return false;

    halo_state_t *s = &h->res.status;
    s->proto_ver = hello.payload[0];
    s->fw_version[0] = hello.payload[1];
    s->fw_version[1] = hello.payload[2];
    s->fw_version[2] = hello.payload[3];
    s->led_count = (uint16_t)(hello.payload[4] | (hello.payload[5] << 8));
    s->preset_count = hello.payload[6];
    s->variant_id = hello.payload[7];
    read_state(h, state.payload);
    s->valid = true;
    stamp_status(h);
    return done(h, HALO_ERR_NONE, 0, 0);
}

bool halo_new(halo_t *h, const halo_link_port_t *port) {
    memset(h, 0, sizeof *h);
    h->port = *port;
    return halo_call(h);
}

bool halo_set_led(halo_t *h, uint8_t start, const halo_rgb_t *colors, uint8_t n) {
    begin(h);
    if (!h->res.status.valid)
        return done(h, HALO_ERR_NOT_READY, 0, 0);
    uint16_t limit = MIN3(h->res.status.led_count, 256u);
    if (!colors || n == 0 || (uint16_t)start + n > limit)
        return done(h, HALO_ERR_BAD_ARG, 0, start);

    for (uint16_t i = 0; i < n; i += 3) {
        uint8_t chunk = (uint8_t)MIN3(3u, (unsigned)(n - i));
        uint8_t payload[HALO_LINK_PAYLOAD_LEN] = {0};
        uint8_t index = (uint8_t)(start + i);
        payload[0] = index;
        for (uint8_t k = 0; k < chunk; k++) {
            payload[1 + k * 3] = colors[i + k].r;
            payload[2 + k * 3] = colors[i + k].g;
            payload[3 + k * 3] = colors[i + k].b;
        }
        halo_link_frame_t resp;
        // FLAGS = このフレームの有効な LED の数(1〜3)
        if (!request(h, HALO_CMD_SET_LEDS, chunk, payload, sizeof payload, index, &resp))
            return false;
    }
    return done(h, HALO_ERR_NONE, 0, 0);
}

bool halo_set_fill(halo_t *h, halo_rgb_t color) {
    begin(h);
    if (!h->res.status.valid)
        return done(h, HALO_ERR_NOT_READY, 0, 0);
    uint8_t payload[3] = {color.r, color.g, color.b};
    halo_link_frame_t resp;
    if (!request(h, HALO_CMD_FILL, 0, payload, sizeof payload, 0, &resp))
        return false;
    return done(h, HALO_ERR_NONE, 0, 0);
}

bool halo_set_brightness(halo_t *h, uint8_t value) {
    begin(h);
    if (!h->res.status.valid)
        return done(h, HALO_ERR_NOT_READY, 0, 0);
    halo_link_frame_t resp;
    if (!request(h, HALO_CMD_SET_BRIGHTNESS, 0, &value, 1, 0, &resp))
        return false;
    h->res.status.brightness = value; // 応答に状態は載らないので、キャッシュを直接更新する
    return done(h, HALO_ERR_NONE, 0, 0);
}

bool halo_run(halo_t *h, uint8_t n) {
    begin(h);
    if (!h->res.status.valid)
        return done(h, HALO_ERR_NOT_READY, 0, 0);
    uint8_t presets = h->res.status.preset_count;

    uint8_t cmd;
    uint8_t id = 0;
    if (n == 0) {
        cmd = HALO_CMD_APPLY_PRESET;
        id = HALO_PRESET_OFF;
    } else if (n <= presets) {
        cmd = HALO_CMD_APPLY_PRESET;
        id = (uint8_t)(n - 1);
    } else if (n == (uint8_t)(presets + 1) && presets < 255) {
        cmd = HALO_CMD_SHOW;
    } else {
        return done(h, HALO_ERR_BAD_ARG, 0, 0);
    }

    halo_link_frame_t resp;
    if (!request(h, cmd, 0, cmd == HALO_CMD_APPLY_PRESET ? &id : NULL,
                 cmd == HALO_CMD_APPLY_PRESET ? 1 : 0, 0, &resp))
        return false;
    read_state(h, resp.payload);
    stamp_status(h);
    return done(h, HALO_ERR_NONE, 0, 0);
}
