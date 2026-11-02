// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// マスター(ESP32 / Pico W): I2C から halo-link のポートを作る。
#include <string.h>

#include "halo_bridge.h"

static int read_reg(halo_bridge_master_t *m, uint8_t reg, uint8_t *out, size_t len) {
    return m->i2c.write_read(m->i2c.ctx, m->addr, &reg, 1, out, len);
}

int halo_bridge_read_info(halo_bridge_master_t *m, uint8_t out[4]) {
    return read_reg(m, HALO_BRIDGE_REG_INFO, out, 4);
}

int halo_bridge_read_status(halo_bridge_master_t *m, uint8_t *status) {
    return read_reg(m, HALO_BRIDGE_REG_STATUS, status, 1);
}

int halo_bridge_read_event(halo_bridge_master_t *m, halo_bridge_event_t *ev) {
    uint8_t b[4];
    int r = read_reg(m, HALO_BRIDGE_REG_EVENT, b, sizeof b);
    if (r == 0) {
        ev->type = b[0];
        ev->arg = b[1];
        ev->seq = b[2];
    }
    return r;
}

void halo_bridge_master_init(halo_bridge_master_t *m, const halo_bridge_i2c_t *i2c, uint8_t addr) {
    memset(m, 0, sizeof *m);
    m->i2c = *i2c;
    m->addr = addr;
}

// halo-link が書く 16 バイトの要求を貯めて、そろったら REQ に書く。
static size_t port_write(void *ctx, const uint8_t *buf, size_t len) {
    halo_bridge_master_t *m = ctx;
    size_t n = HALO_BRIDGE_FRAME_LEN - m->tx_len;
    if (len < n)
        n = len;
    memcpy(m->tx + m->tx_len, buf, n);
    m->tx_len += n;
    if (m->tx_len == HALO_BRIDGE_FRAME_LEN) {
        uint8_t msg[1 + HALO_BRIDGE_FRAME_LEN];
        msg[0] = HALO_BRIDGE_REG_REQ;
        memcpy(msg + 1, m->tx, HALO_BRIDGE_FRAME_LEN);
        m->tx_len = 0;
        m->rx_len = m->rx_pos = 0; // 古い応答は捨てる
        m->pending = (m->i2c.write(m->i2c.ctx, m->addr, msg, sizeof msg) == 0);
    }
    return n;
}

// 要求を出したあと、呼ばれるたびに STATUS を 1 回だけ読む。応答があれば RESP を読んで返す。
static size_t port_read(void *ctx, uint8_t *buf, size_t max) {
    halo_bridge_master_t *m = ctx;
    if (m->rx_pos >= m->rx_len && m->pending) {
        uint8_t st = 0;
        if (halo_bridge_read_status(m, &st) != 0)
            return 0;
        if (st & (HALO_BRIDGE_ST_READY | HALO_BRIDGE_ST_TIMEOUT)) {
            uint8_t resp[HALO_BRIDGE_FRAME_LEN];
            if (read_reg(m, HALO_BRIDGE_REG_RESP, resp, sizeof resp) != 0)
                return 0;
            m->pending = false;
            if (st & HALO_BRIDGE_ST_READY) {
                memcpy(m->rx, resp, sizeof resp);
                m->rx_len = sizeof resp;
                m->rx_pos = 0;
            }
        } else if ((st & HALO_BRIDGE_ST_REJECTED) && !(st & HALO_BRIDGE_ST_BUSY)) {
            m->pending = false; // 受け付けられなかった。応答は来ない(halo-link が再送する)
        }
    }
    size_t n = m->rx_len - m->rx_pos;
    if (n > max)
        n = max;
    memcpy(buf, m->rx + m->rx_pos, n);
    m->rx_pos += n;
    return n;
}

static uint32_t port_now_us(void *ctx) {
    halo_bridge_master_t *m = ctx;
    return m->i2c.now_us(m->i2c.ctx);
}

static void port_yield(void *ctx) {
    halo_bridge_master_t *m = ctx;
    if (m->i2c.yield)
        m->i2c.yield(m->i2c.ctx);
}

halo_link_port_t halo_bridge_master_port(halo_bridge_master_t *m) {
    halo_link_port_t p = {port_write, port_read, port_now_us, port_yield, m};
    return p;
}
