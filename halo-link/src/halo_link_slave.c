// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// スレーブ(A 側): 受信したフレームを解析し、コマンドごとにハンドラを呼んで応答する。
#include <string.h>

#include "halo_link.h"

static void send_response(halo_link_slave_t *s, const halo_link_frame_t *req, uint8_t status,
                          const uint8_t *payload, size_t plen) {
    halo_link_frame_t res;
    memset(&res, 0, sizeof res);
    res.sof = HALO_LINK_SOF_RES;
    res.seq = req->seq;
    res.cmd = (uint8_t)(req->cmd | 0x80);
    res.flags_status = status;
    if (payload && plen <= HALO_LINK_PAYLOAD_LEN)
        memcpy(res.payload, payload, plen);

    uint8_t raw[HALO_LINK_FRAME_LEN];
    halo_link_frame_encode(&res, raw);
    size_t sent = 0;
    while (sent < sizeof raw) {
        size_t n = s->port.write(s->port.ctx, raw + sent, sizeof raw - sent);
        if (n == 0)
            break;
        sent += n;
    }
}

static void fill_state(const halo_link_slave_t *s, uint8_t out[3]) {
    halo_link_state_t st = {0, 0, 0};
    if (s->ops.get_state)
        s->ops.get_state(s->ops.ctx, &st);
    out[0] = st.mode;
    out[1] = st.preset_id;
    out[2] = st.brightness;
}

static void handle(halo_link_slave_t *s, const halo_link_frame_t *req) {
    const halo_link_slave_ops_t *ops = &s->ops;
    const uint8_t *p = req->payload;
    uint8_t out[HALO_LINK_PAYLOAD_LEN] = {0};
    size_t outlen = 0;
    halo_status_t st = HALO_STATUS_OK;
    halo_link_info_t info;
    memset(&info, 0, sizeof info);

    switch (req->cmd) {
    case HALO_CMD_PING:
        break;

    case HALO_CMD_HELLO:
        if (!ops->get_info) {
            st = HALO_STATUS_UNSUPPORTED;
            break;
        }
        ops->get_info(ops->ctx, &info);
        out[0] = HALO_LINK_PROTO_VERSION;
        out[1] = info.fw_version[0];
        out[2] = info.fw_version[1];
        out[3] = info.fw_version[2];
        out[4] = (uint8_t)(info.led_count & 0xFF);
        out[5] = (uint8_t)(info.led_count >> 8);
        out[6] = info.preset_count;
        out[7] = info.variant_id;
        outlen = 10;
        break;

    case HALO_CMD_GET_STATE:
        fill_state(s, out);
        outlen = 3;
        break;

    case HALO_CMD_GET_ANGLES: {
        if (!ops->get_info || !ops->get_angle) {
            st = HALO_STATUS_UNSUPPORTED;
            break;
        }
        ops->get_info(ops->ctx, &info);
        uint8_t start = p[0], count = p[1];
        if (count < 1 || count > 4 || (unsigned)start + count > info.led_count) {
            st = HALO_STATUS_BAD_ARG;
            break;
        }
        out[0] = start;
        out[1] = count;
        for (uint8_t i = 0; i < count; i++) {
            uint16_t a = ops->get_angle(ops->ctx, (uint8_t)(start + i));
            out[2 + i * 2] = (uint8_t)(a & 0xFF);
            out[3 + i * 2] = (uint8_t)(a >> 8);
        }
        outlen = 10;
        break;
    }

    case HALO_CMD_APPLY_PRESET:
        if (!ops->apply_preset) {
            st = HALO_STATUS_UNSUPPORTED;
            break;
        }
        st = ops->apply_preset(ops->ctx, p[0]);
        if (st == HALO_STATUS_OK) {
            fill_state(s, out);
            outlen = 3;
        }
        break;

    case HALO_CMD_SET_BRIGHTNESS:
        st = ops->set_brightness ? ops->set_brightness(ops->ctx, p[0]) : HALO_STATUS_UNSUPPORTED;
        break;

    case HALO_CMD_SET_LEDS: {
        if (!ops->get_info || !ops->set_leds) {
            st = HALO_STATUS_UNSUPPORTED;
            break;
        }
        ops->get_info(ops->ctx, &info);
        uint8_t n = req->flags_status ? req->flags_status : 3; // 0 は 3 個(互換)
        uint8_t start = p[0];
        if (n > 3 || start >= info.led_count) {
            st = HALO_STATUS_BAD_ARG;
            break;
        }
        if ((unsigned)start + n > info.led_count)
            n = (uint8_t)(info.led_count - start); // 個数を超える分は切り詰める
        halo_rgb_t rgb[3];
        for (uint8_t i = 0; i < 3; i++) {
            rgb[i].r = p[1 + i * 3];
            rgb[i].g = p[2 + i * 3];
            rgb[i].b = p[3 + i * 3];
        }
        st = ops->set_leds(ops->ctx, start, n, rgb);
        break;
    }

    case HALO_CMD_FILL: {
        halo_rgb_t rgb = {p[0], p[1], p[2]};
        st = ops->fill ? ops->fill(ops->ctx, rgb) : HALO_STATUS_UNSUPPORTED;
        break;
    }

    case HALO_CMD_SHOW:
        if (!ops->show) {
            st = HALO_STATUS_UNSUPPORTED;
            break;
        }
        st = ops->show(ops->ctx);
        if (st == HALO_STATUS_OK) {
            fill_state(s, out);
            outlen = 3;
        }
        break;

    default:
        // 0x40〜0x4F は将来(プリセットの書き込みなど)用に予約
        st = (req->cmd >= 0x40 && req->cmd <= 0x4F) ? HALO_STATUS_UNSUPPORTED : HALO_STATUS_BAD_CMD;
        break;
    }

    send_response(s, req, (uint8_t)st, out, st == HALO_STATUS_OK ? outlen : 0);
}

void halo_link_slave_init(halo_link_slave_t *s, const halo_link_port_t *port,
                          const halo_link_slave_ops_t *ops) {
    s->port = *port;
    s->ops = *ops;
    halo_link_parser_init(&s->parser, HALO_LINK_SOF_REQ);
}

void halo_link_slave_poll(halo_link_slave_t *s) {
    uint8_t tmp[HALO_LINK_FRAME_LEN];
    size_t n;
    while ((n = s->port.read(s->port.ctx, tmp, sizeof tmp)) > 0) {
        uint32_t t = s->port.now_us(s->port.ctx);
        for (size_t i = 0; i < n; i++) {
            halo_link_frame_t f;
            if (halo_link_parser_feed(&s->parser, tmp[i], t, &f))
                handle(s, &f);
        }
    }
}
