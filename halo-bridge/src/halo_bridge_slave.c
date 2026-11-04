// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// スレーブ(B 側): I2C のレジスタを受けて、HALO Link のフレームを A 側へ中継する。
#include <string.h>

#include "halo_bridge.h"

enum { ST_IDLE = 0, ST_PENDING = 1, ST_WAIT = 2 };

static void update_int(halo_bridge_slave_t *s) {
    bool want =
        (s->status & (HALO_BRIDGE_ST_READY | HALO_BRIDGE_ST_TIMEOUT | HALO_BRIDGE_ST_EVENT)) != 0;
    if (want != s->int_asserted) {
        s->int_asserted = want;
        if (s->int_set)
            s->int_set(s->int_ctx, want);
    }
}

void halo_bridge_slave_init(halo_bridge_slave_t *s, const halo_link_port_t *link,
                            void (*int_set)(void *ctx, bool asserted), void *int_ctx) {
    memset(s, 0, sizeof *s);
    s->link = *link;
    s->int_set = int_set;
    s->int_ctx = int_ctx;
    s->reg = HALO_BRIDGE_REG_STATUS;
    halo_link_parser_init(&s->parser, HALO_LINK_SOF_RES);
}

void halo_bridge_slave_on_write(halo_bridge_slave_t *s, const uint8_t *buf, size_t len) {
    if (len == 0)
        return;
    s->reg = buf[0];
    if (len == 1)
        return; // レジスタ番号だけ(次の読みの位置)

    if (s->reg != HALO_BRIDGE_REG_REQ || len != 1 + HALO_BRIDGE_FRAME_LEN) {
        s->status |= HALO_BRIDGE_ST_REJECTED;
        update_int(s);
        return;
    }
    halo_link_frame_t f;
    if (s->state != ST_IDLE || !halo_link_frame_decode(buf + 1, &f) || f.sof != HALO_LINK_SOF_REQ) {
        s->status |= HALO_BRIDGE_ST_REJECTED;
        update_int(s);
        return;
    }
    memcpy(s->req, buf + 1, HALO_BRIDGE_FRAME_LEN);
    s->status &=
        (uint8_t)~(HALO_BRIDGE_ST_READY | HALO_BRIDGE_ST_TIMEOUT | HALO_BRIDGE_ST_REJECTED);
    s->status |= HALO_BRIDGE_ST_BUSY; // すぐ立てる(割り込みの遅れで見えないことが無いように)
    s->state = ST_PENDING;
    update_int(s);
}

size_t halo_bridge_slave_on_read(halo_bridge_slave_t *s, uint8_t *out, size_t max) {
    size_t n = 0;
    switch (s->reg) {
    case HALO_BRIDGE_REG_INFO: {
        uint8_t info[4] = {HALO_BRIDGE_PROTO_VERSION,
                           (uint8_t)(s->int_set ? HALO_BRIDGE_CAP_INT : 0), 0, 0};
        n = max < sizeof info ? max : sizeof info;
        memcpy(out, info, n);
        break;
    }
    case HALO_BRIDGE_REG_STATUS:
        if (max >= 1) {
            out[0] = s->status;
            n = 1;
            s->status &= (uint8_t)~HALO_BRIDGE_ST_REJECTED;
        }
        break;
    case HALO_BRIDGE_REG_RESP:
        n = max < HALO_BRIDGE_FRAME_LEN ? max : HALO_BRIDGE_FRAME_LEN;
        memcpy(out, s->resp, n);
        s->status &= (uint8_t)~(HALO_BRIDGE_ST_READY | HALO_BRIDGE_ST_TIMEOUT);
        break;
    case HALO_BRIDGE_REG_EVENT: {
        uint8_t e[4] = {s->ev.type, s->ev.arg, s->ev.seq, 0};
        n = max < sizeof e ? max : sizeof e;
        memcpy(out, e, n);
        s->status &= (uint8_t)~HALO_BRIDGE_ST_EVENT;
        break;
    }
    default:
        n = max < HALO_BRIDGE_FRAME_LEN ? max : HALO_BRIDGE_FRAME_LEN;
        memset(out, 0xFF, n);
        break;
    }
    update_int(s);
    return n;
}

void halo_bridge_slave_poll(halo_bridge_slave_t *s) {
    uint32_t now = s->link.now_us(s->link.ctx);

    if (s->state == ST_PENDING && !s->hold) {
        size_t sent = 0;
        while (sent < HALO_BRIDGE_FRAME_LEN) {
            size_t n = s->link.write(s->link.ctx, s->req + sent, HALO_BRIDGE_FRAME_LEN - sent);
            if (n == 0)
                break;
            sent += n;
        }
        if (sent < HALO_BRIDGE_FRAME_LEN) {
            HALO_BRIDGE_CRITICAL_ENTER();
            s->status = (uint8_t)((s->status & ~HALO_BRIDGE_ST_BUSY) | HALO_BRIDGE_ST_TIMEOUT);
            s->state = ST_IDLE;
            HALO_BRIDGE_CRITICAL_EXIT();
        } else {
            s->t0 = s->link.now_us(s->link.ctx);
            halo_link_parser_init(&s->parser, HALO_LINK_SOF_RES);
            s->state = ST_WAIT;
        }
    } else if (s->state == ST_WAIT) {
        uint8_t tmp[HALO_LINK_FRAME_LEN];
        size_t n;
        while (s->state == ST_WAIT && (n = s->link.read(s->link.ctx, tmp, sizeof tmp)) > 0) {
            uint32_t t = s->link.now_us(s->link.ctx);
            for (size_t i = 0; i < n && s->state == ST_WAIT; i++) {
                halo_link_frame_t f;
                if (!halo_link_parser_feed(&s->parser, tmp[i], t, &f))
                    continue;
                // 照合: 要求と同じ SEQ とコマンド。単線に残っていた古い応答は捨てる
                if (f.seq != s->req[1] || f.cmd != (uint8_t)(s->req[2] | 0x80))
                    continue;
                HALO_BRIDGE_CRITICAL_ENTER();
                halo_link_frame_encode(&f, s->resp);
                s->status = (uint8_t)((s->status & ~HALO_BRIDGE_ST_BUSY) | HALO_BRIDGE_ST_READY);
                s->state = ST_IDLE;
                HALO_BRIDGE_CRITICAL_EXIT();
            }
        }
        now = s->link.now_us(s->link.ctx);
        if (s->state == ST_WAIT && (uint32_t)(now - s->t0) >= HALO_BRIDGE_LINK_TIMEOUT_US) {
            HALO_BRIDGE_CRITICAL_ENTER();
            s->status = (uint8_t)((s->status & ~HALO_BRIDGE_ST_BUSY) | HALO_BRIDGE_ST_TIMEOUT);
            s->state = ST_IDLE;
            HALO_BRIDGE_CRITICAL_EXIT();
        }
    }
    HALO_BRIDGE_CRITICAL_ENTER();
    update_int(s);
    HALO_BRIDGE_CRITICAL_EXIT();
}

void halo_bridge_slave_hold(halo_bridge_slave_t *s, bool hold) { s->hold = hold; }

bool halo_bridge_slave_link_busy(const halo_bridge_slave_t *s) { return s->state == ST_WAIT; }

void halo_bridge_slave_post_event(halo_bridge_slave_t *s, halo_bridge_evt_t type, uint8_t arg) {
    HALO_BRIDGE_CRITICAL_ENTER();
    s->ev.type = (uint8_t)type;
    s->ev.arg = arg;
    s->ev.seq++;
    s->status |= HALO_BRIDGE_ST_EVENT;
    update_int(s);
    HALO_BRIDGE_CRITICAL_EXIT();
}
