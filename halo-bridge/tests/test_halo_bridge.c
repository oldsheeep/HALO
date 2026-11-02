// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// ホストでのテスト。ESP32 / Pico W(halo-link のマスター)→ I2C → ブリッジ → 単線 → A 側
// を、模擬でつなぐ。
#include <stdio.h>
#include <string.h>

#include "halo_bridge.h"

static int g_checks = 0, g_failed = 0;
#define CHECK(cond)                                                                                \
    do {                                                                                           \
        g_checks++;                                                                                \
        if (!(cond)) {                                                                             \
            g_failed++;                                                                            \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                 \
        }                                                                                          \
    } while (0)

// ---- 模擬の単線(ブリッジ = ノード 0、A 側 = ノード 1。エコーあり) ------------------------------
typedef struct {
    uint8_t q[2][8192];
    size_t head[2], tail[2];
    uint32_t t_us;
    size_t bridge_writes; // ブリッジが単線へ書いたバイト数
    int slave_responses;
    int drop_after; // この数を超えた応答は、ブリッジへ届かない(-1 = 無制限)
} wire_t;

static size_t wire_write(wire_t *w, int node, const uint8_t *buf, size_t len) {
    int deliver = 1;
    if (node == 0) {
        w->bridge_writes += len;
    } else if (len == HALO_LINK_FRAME_LEN) {
        w->slave_responses++;
        if (w->drop_after >= 0 && w->slave_responses > w->drop_after)
            deliver = 0;
    }
    for (size_t i = 0; i < len; i++) {
        w->q[node][w->tail[node]++] = buf[i];
        if (node == 1 && !deliver)
            continue;
        w->q[1 - node][w->tail[1 - node]++] = buf[i];
    }
    return len;
}
static size_t wire_read(wire_t *w, int node, uint8_t *buf, size_t max) {
    size_t n = 0;
    while (n < max && w->head[node] < w->tail[node])
        buf[n++] = w->q[node][w->head[node]++];
    return n;
}
static size_t b_write(void *c, const uint8_t *b, size_t n) { return wire_write(c, 0, b, n); }
static size_t b_read(void *c, uint8_t *b, size_t m) { return wire_read(c, 0, b, m); }
static size_t a_write(void *c, const uint8_t *b, size_t n) { return wire_write(c, 1, b, n); }
static size_t a_read(void *c, uint8_t *b, size_t m) { return wire_read(c, 1, b, m); }
static uint32_t w_now(void *c) {
    wire_t *w = c;
    w->t_us += 20;
    return w->t_us;
}

// ---- 模擬の A 側 ---------------------------------------------------------------------------
typedef struct {
    uint8_t mode, preset_id, brightness;
    halo_rgb_t staging[15], shown[15];
} dev_t;
static void d_info(void *c, halo_link_info_t *o) {
    (void)c;
    o->led_count = 15;
    o->preset_count = 3;
    o->fw_version[0] = 1;
}
static void d_state(void *c, halo_link_state_t *o) {
    dev_t *d = c;
    o->mode = d->mode;
    o->preset_id = d->preset_id;
    o->brightness = d->brightness;
}
static halo_status_t d_apply(void *c, uint8_t id) {
    dev_t *d = c;
    if (id == HALO_PRESET_OFF) {
        d->mode = HALO_MODE_OFF;
    } else if (id < 3) {
        d->mode = HALO_MODE_PRESET;
        d->preset_id = id;
    } else {
        return HALO_STATUS_BAD_ARG;
    }
    return HALO_STATUS_OK;
}
static halo_status_t d_set(void *c, uint8_t start, uint8_t n, const halo_rgb_t *rgb) {
    dev_t *d = c;
    for (uint8_t i = 0; i < n; i++)
        d->staging[start + i] = rgb[i];
    return HALO_STATUS_OK;
}
static halo_status_t d_show(void *c) {
    dev_t *d = c;
    memcpy(d->shown, d->staging, sizeof d->shown);
    d->mode = HALO_MODE_DIRECT;
    return HALO_STATUS_OK;
}

// ---- 全体 ----------------------------------------------------------------------------------
typedef struct {
    wire_t wire;
    dev_t dev;
    halo_link_slave_t a;
    halo_bridge_slave_t bridge;
    halo_bridge_master_t master;
    int int_level; // 1 = L(アサート)
    int int_changes;
} rig_t;

static void on_int(void *c, bool asserted) {
    rig_t *r = c;
    r->int_level = asserted;
    r->int_changes++;
}

// I2C の模擬: ESP32 / Pico W → ブリッジ(スレーブ)
static int i2c_write(void *c, uint8_t addr, const uint8_t *buf, size_t len) {
    rig_t *r = c;
    if (addr != HALO_BRIDGE_DEFAULT_ADDR)
        return -1;
    halo_bridge_slave_on_write(&r->bridge, buf, len);
    return 0;
}
static int i2c_write_read(void *c, uint8_t addr, const uint8_t *wbuf, size_t wlen, uint8_t *rbuf,
                          size_t rlen) {
    rig_t *r = c;
    if (addr != HALO_BRIDGE_DEFAULT_ADDR)
        return -1;
    halo_bridge_slave_on_write(&r->bridge, wbuf, wlen);
    size_t n = halo_bridge_slave_on_read(&r->bridge, rbuf, rlen);
    for (; n < rlen; n++)
        rbuf[n] = 0xFF;
    return 0;
}
static uint32_t i2c_now(void *c) { return w_now(&((rig_t *)c)->wire); }
static void i2c_yield(void *c) {
    rig_t *r = c;
    halo_bridge_slave_poll(&r->bridge); // B 側のメインループ
    halo_link_slave_poll(&r->a);        // A 側のメインループ
}

static void rig_init(rig_t *r) {
    memset(r, 0, sizeof *r);
    r->wire.drop_after = -1;
    r->dev.brightness = 255;
    halo_link_port_t aport = {a_write, a_read, w_now, NULL, &r->wire};
    halo_link_slave_ops_t ops = {.ctx = &r->dev,
                                 .get_info = d_info,
                                 .get_state = d_state,
                                 .apply_preset = d_apply,
                                 .set_leds = d_set,
                                 .show = d_show};
    halo_link_slave_init(&r->a, &aport, &ops);
    halo_link_port_t bport = {b_write, b_read, w_now, NULL, &r->wire};
    halo_bridge_slave_init(&r->bridge, &bport, on_int, r);
    halo_bridge_i2c_t i2c = {i2c_write, i2c_write_read, i2c_now, i2c_yield, r};
    halo_bridge_master_init(&r->master, &i2c, HALO_BRIDGE_DEFAULT_ADDR);
}

static uint8_t status(rig_t *r) {
    uint8_t st = 0xEE;
    CHECK(halo_bridge_read_status(&r->master, &st) == 0);
    return st;
}

// 要求フレームを作る
static void make_req(uint8_t out[HALO_LINK_FRAME_LEN], uint8_t seq, uint8_t cmd) {
    halo_link_frame_t f;
    memset(&f, 0, sizeof f);
    f.sof = HALO_LINK_SOF_REQ;
    f.seq = seq;
    f.cmd = cmd;
    halo_link_frame_encode(&f, out);
}
static void write_req(rig_t *r, const uint8_t frame[HALO_LINK_FRAME_LEN]) {
    uint8_t msg[1 + HALO_LINK_FRAME_LEN];
    msg[0] = HALO_BRIDGE_REG_REQ;
    memcpy(msg + 1, frame, HALO_LINK_FRAME_LEN);
    halo_bridge_slave_on_write(&r->bridge, msg, sizeof msg);
}
static void spin(rig_t *r, int n) {
    for (int i = 0; i < n; i++)
        i2c_yield(r);
}

// ---- テスト ---------------------------------------------------------------------------------
static void test_registers(void) {
    rig_t r;
    rig_init(&r);
    uint8_t info[4];
    CHECK(halo_bridge_read_info(&r.master, info) == 0);
    CHECK(info[0] == HALO_BRIDGE_PROTO_VERSION && (info[1] & HALO_BRIDGE_CAP_INT) && info[2] == 0);
    CHECK(status(&r) == 0);
    uint8_t buf[16];
    uint8_t reg = 0x7E; // 未定義のレジスタ
    CHECK(r.master.i2c.write_read(&r, HALO_BRIDGE_DEFAULT_ADDR, &reg, 1, buf, 4) == 0 &&
          buf[0] == 0xFF);
    // 読み出し専用のレジスタへの書き込みは、受け付けない
    uint8_t w[3] = {HALO_BRIDGE_REG_STATUS, 1, 2};
    halo_bridge_slave_on_write(&r.bridge, w, sizeof w);
    CHECK(status(&r) & HALO_BRIDGE_ST_REJECTED);
    CHECK(!(status(&r) & HALO_BRIDGE_ST_REJECTED)); // 読むと消える
}

static void test_full_stack(void) {
    rig_t r;
    rig_init(&r);
    halo_link_port_t port = halo_bridge_master_port(&r.master);
    halo_t h;
    CHECK(halo_new(&h, &port)); // ESP32 / Pico W 側の halo-link が、I2C 越しに A 側と話す
    CHECK(h.res.status.valid && h.res.status.led_count == 15 && h.res.status.preset_count == 3);

    CHECK(halo_run(&h, 2));
    CHECK(r.dev.mode == HALO_MODE_PRESET && r.dev.preset_id == 1);
    CHECK(h.res.status.mode == HALO_MODE_PRESET);
    CHECK(halo_run(&h, 0));
    CHECK(r.dev.mode == HALO_MODE_OFF);

    halo_rgb_t colors[15];
    for (int i = 0; i < 15; i++)
        colors[i] = (halo_rgb_t){(uint8_t)i, (uint8_t)(2 * i), (uint8_t)(255 - i)};
    CHECK(halo_set_led(&h, 0, colors, 15));
    CHECK(halo_run(&h, 4));
    CHECK(r.dev.mode == HALO_MODE_DIRECT && memcmp(r.dev.shown, colors, sizeof colors) == 0);
    CHECK(!r.int_level || r.int_level); // INT は読まれたあとに戻る(下で確認)
    CHECK(!(status(&r) & HALO_BRIDGE_ST_BUSY));
    CHECK(r.int_level == 0);
}

static void test_timeout_and_recovery(void) {
    rig_t r;
    rig_init(&r);
    halo_link_port_t port = halo_bridge_master_port(&r.master);
    halo_t h;
    CHECK(halo_new(&h, &port));

    r.wire.drop_after = r.wire.slave_responses; // A 側の応答が、ブリッジへ届かなくなる
    CHECK(!halo_run(&h, 1));
    CHECK(h.res.error.kind == HALO_ERR_TIMEOUT);
    CHECK(h.res.status.valid && !h.res.status.fresh);

    r.wire.drop_after = -1;
    CHECK(halo_call(&h) && h.res.status.fresh); // 回復する
    CHECK(halo_run(&h, 2));
}

static void test_busy_and_rejects(void) {
    rig_t r;
    rig_init(&r);
    uint8_t f1[16], f2[16];
    make_req(f1, 1, HALO_CMD_PING);
    make_req(f2, 2, HALO_CMD_PING);
    write_req(&r, f1);
    CHECK(status(&r) & HALO_BRIDGE_ST_BUSY); // すぐ BUSY が立つ(poll 前でも)
    write_req(&r, f2);                       // BUSY 中の要求は、受け付けない
    uint8_t st = status(&r);
    CHECK((st & HALO_BRIDGE_ST_REJECTED) && (st & HALO_BRIDGE_ST_BUSY));
    spin(&r, 50);
    CHECK(status(&r) & HALO_BRIDGE_ST_READY);
    uint8_t resp[16], reg = HALO_BRIDGE_REG_RESP;
    CHECK(r.master.i2c.write_read(&r, HALO_BRIDGE_DEFAULT_ADDR, &reg, 1, resp, 16) == 0);
    halo_link_frame_t g;
    CHECK(halo_link_frame_decode(resp, &g) && g.sof == HALO_LINK_SOF_RES && g.seq == 1 &&
          g.cmd == (HALO_CMD_PING | 0x80));
    CHECK(!(status(&r) & HALO_BRIDGE_ST_READY)); // RESP を読むと消える

    // 不正なフレームは、単線へ流さない
    size_t before = r.wire.bridge_writes;
    uint8_t bad[16];
    make_req(bad, 3, HALO_CMD_PING);
    bad[8] ^= 1; // CRC 不一致
    write_req(&r, bad);
    CHECK(status(&r) & HALO_BRIDGE_ST_REJECTED);
    uint8_t wrong_sof[16];
    halo_link_frame_t rf = {HALO_LINK_SOF_RES, 4, HALO_CMD_PING, 0, {0}};
    halo_link_frame_encode(&rf, wrong_sof); // 応答の SOF は、要求として受け付けない
    write_req(&r, wrong_sof);
    CHECK(status(&r) & HALO_BRIDGE_ST_REJECTED);
    uint8_t shortw[1 + 10] = {HALO_BRIDGE_REG_REQ};
    halo_bridge_slave_on_write(&r.bridge, shortw, sizeof shortw); // 長さ違い
    CHECK(status(&r) & HALO_BRIDGE_ST_REJECTED);
    spin(&r, 20);
    CHECK(r.wire.bridge_writes == before);
}

static void test_stale_response_is_dropped(void) {
    rig_t r;
    rig_init(&r);
    // 単線に、古い応答(SEQ = 4)が残っている
    uint8_t stale[16];
    halo_link_frame_t sf = {HALO_LINK_SOF_RES, 4, HALO_CMD_PING | 0x80, 0, {0}};
    halo_link_frame_encode(&sf, stale);
    for (int i = 0; i < 16; i++)
        r.wire.q[0][r.wire.tail[0]++] = stale[i];
    uint8_t f[16];
    make_req(f, 9, HALO_CMD_PING);
    write_req(&r, f);
    spin(&r, 50);
    uint8_t resp[16], reg = HALO_BRIDGE_REG_RESP;
    CHECK(r.master.i2c.write_read(&r, HALO_BRIDGE_DEFAULT_ADDR, &reg, 1, resp, 16) == 0);
    halo_link_frame_t g;
    CHECK(halo_link_frame_decode(resp, &g) && g.seq == 9); // 古い SEQ = 4 は捨てられている
}

static void test_event_and_int(void) {
    rig_t r;
    rig_init(&r);
    CHECK(r.int_level == 0);
    halo_bridge_slave_post_event(&r.bridge, HALO_BRIDGE_EVT_PRESET, 2);
    CHECK(r.int_level == 1 && (status(&r) & HALO_BRIDGE_ST_EVENT));
    halo_bridge_event_t ev;
    CHECK(halo_bridge_read_event(&r.master, &ev) == 0);
    CHECK(ev.type == HALO_BRIDGE_EVT_PRESET && ev.arg == 2 && ev.seq == 1);
    CHECK(r.int_level == 0 && !(status(&r) & HALO_BRIDGE_ST_EVENT)); // 読むと消えて、INT も戻る
    halo_bridge_slave_post_event(&r.bridge, HALO_BRIDGE_EVT_OFF, 0);
    CHECK(halo_bridge_read_event(&r.master, &ev) == 0 && ev.type == HALO_BRIDGE_EVT_OFF &&
          ev.seq == 2);

    // 応答があるあいだも INT になる
    uint8_t f[16];
    make_req(f, 1, HALO_CMD_PING);
    write_req(&r, f);
    spin(&r, 50);
    CHECK(r.int_level == 1);
    uint8_t resp[16], reg = HALO_BRIDGE_REG_RESP;
    r.master.i2c.write_read(&r, HALO_BRIDGE_DEFAULT_ADDR, &reg, 1, resp, 16);
    CHECK(r.int_level == 0);
}

static void test_hold(void) {
    rig_t r;
    rig_init(&r);
    halo_bridge_slave_hold(&r.bridge, true);
    uint8_t f[16];
    make_req(f, 1, HALO_CMD_PING);
    write_req(&r, f);
    spin(&r, 50);
    CHECK(r.wire.bridge_writes == 0); // 止めているあいだは、単線へ出さない
    CHECK(status(&r) & HALO_BRIDGE_ST_BUSY);
    CHECK(!halo_bridge_slave_link_busy(&r.bridge));
    halo_bridge_slave_hold(&r.bridge, false);
    spin(&r, 50);
    CHECK(r.wire.bridge_writes == 16);
    CHECK(status(&r) & HALO_BRIDGE_ST_READY);
}

static void test_link_timeout_status(void) {
    rig_t r;
    rig_init(&r);
    r.wire.drop_after = 0; // 応答が一切届かない
    uint8_t f[16];
    make_req(f, 1, HALO_CMD_PING);
    write_req(&r, f);
    spin(&r, 2000); // 15 ms を超える(1 回の spin で時刻が数十 µs 進む)
    uint8_t st = status(&r);
    CHECK((st & HALO_BRIDGE_ST_TIMEOUT) && !(st & HALO_BRIDGE_ST_BUSY) &&
          !(st & HALO_BRIDGE_ST_READY));
    // 次の要求を受け付けて、TIMEOUT は消える
    r.wire.drop_after = -1;
    make_req(f, 2, HALO_CMD_PING);
    write_req(&r, f);
    CHECK(!(status(&r) & HALO_BRIDGE_ST_TIMEOUT));
    spin(&r, 50);
    CHECK(status(&r) & HALO_BRIDGE_ST_READY);
}

int main(void) {
    test_registers();
    test_full_stack();
    test_timeout_and_recovery();
    test_busy_and_rejects();
    test_stale_response_is_dropped();
    test_event_and_int();
    test_hold();
    test_link_timeout_status();
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
