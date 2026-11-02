// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// ホストでのテスト。模擬バス(単線・エコーあり)でマスターとスレーブをつなぐ。
#include <stdio.h>
#include <string.h>

#include "halo_link.h"

static int g_checks = 0;
static int g_failed = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        g_checks++;                                                                                \
        if (!(cond)) {                                                                             \
            g_failed++;                                                                            \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                 \
        }                                                                                          \
    } while (0)

// ---- 模擬バス ---------------------------------------------------------------------------
// node 0 = マスター、node 1 = スレーブ。書いたバイトは相手と自分(エコー)の受信キューに入る。

typedef struct {
    uint8_t q[2][8192];
    size_t head[2], tail[2];
    uint32_t t_us;
    size_t master_writes;     // マスターが書いたフレーム数
    int slave_responses;      // スレーブが書いた応答の数
    int drop_after;           // この数を超えた応答は、マスターに届かない(-1 = 無制限に届く)
    int corrupt_next;         // 次の応答を 1 つ壊す
    int patch_hello_proto;    // 0 以外なら、HELLO 応答のプロトコル版を書き換える
    halo_link_slave_t *slave; // マスターの yield で動かす
} bus_t;

static void push(bus_t *b, int node, uint8_t v) { b->q[node][b->tail[node]++] = v; }

static size_t bus_write(bus_t *b, int node, const uint8_t *buf, size_t len) {
    uint8_t tmp[HALO_LINK_FRAME_LEN];
    int deliver = 1;
    if (node == 0) {
        b->master_writes++;
    } else if (len == HALO_LINK_FRAME_LEN) {
        memcpy(tmp, buf, len);
        b->slave_responses++;
        halo_link_frame_t f;
        if (b->patch_hello_proto && tmp[2] == (HALO_CMD_HELLO | 0x80) &&
            halo_link_frame_decode(tmp, &f)) {
            f.payload[0] = (uint8_t)b->patch_hello_proto;
            halo_link_frame_encode(&f, tmp);
        }
        if (b->corrupt_next) {
            tmp[6] ^= 0x40;
            b->corrupt_next = 0;
        }
        if (b->drop_after >= 0 && b->slave_responses > b->drop_after)
            deliver = 0;
        buf = tmp;
    }
    for (size_t i = 0; i < len; i++) {
        push(b, node, buf[i]); // エコー
        if (node == 1 && !deliver)
            continue;
        push(b, 1 - node, buf[i]);
    }
    return len;
}

static size_t bus_read(bus_t *b, int node, uint8_t *buf, size_t max) {
    size_t n = 0;
    while (n < max && b->head[node] < b->tail[node])
        buf[n++] = b->q[node][b->head[node]++];
    return n;
}

static size_t m_write(void *c, const uint8_t *buf, size_t len) { return bus_write(c, 0, buf, len); }
static size_t m_read(void *c, uint8_t *buf, size_t max) { return bus_read(c, 0, buf, max); }
static size_t s_write(void *c, const uint8_t *buf, size_t len) { return bus_write(c, 1, buf, len); }
static size_t s_read(void *c, uint8_t *buf, size_t max) { return bus_read(c, 1, buf, max); }
static uint32_t b_now(void *c) {
    bus_t *b = c;
    b->t_us += 20;
    return b->t_us;
}
static void m_yield(void *c) {
    bus_t *b = c;
    if (b->slave)
        halo_link_slave_poll(b->slave);
}

// ---- 模擬デバイス(A 側) -----------------------------------------------------------------

typedef struct {
    uint8_t mode, preset_id, brightness;
    halo_rgb_t staging[15], shown[15];
    int set_leds_calls;
    int last_n;
    halo_status_t apply_status;
} dev_t;

static void d_info(void *c, halo_link_info_t *o) {
    (void)c;
    o->fw_version[0] = 1;
    o->fw_version[1] = 2;
    o->fw_version[2] = 3;
    o->led_count = 15;
    o->preset_count = 3;
    o->variant_id = 7;
}
static void d_state(void *c, halo_link_state_t *o) {
    dev_t *d = c;
    o->mode = d->mode;
    o->preset_id = d->preset_id;
    o->brightness = d->brightness;
}
static uint16_t d_angle(void *c, uint8_t i) {
    (void)c;
    return (uint16_t)(i * 24);
}
static halo_status_t d_apply(void *c, uint8_t id) {
    dev_t *d = c;
    if (d->apply_status != HALO_STATUS_OK)
        return d->apply_status;
    if (id == HALO_PRESET_OFF) {
        d->mode = HALO_MODE_OFF;
    } else if (id == HALO_PRESET_AUTO) {
        d->mode = HALO_MODE_AUTO;
    } else if (id < 3) {
        d->mode = HALO_MODE_PRESET;
        d->preset_id = id;
    } else {
        return HALO_STATUS_BAD_ARG;
    }
    return HALO_STATUS_OK;
}
static halo_status_t d_bright(void *c, uint8_t v) {
    ((dev_t *)c)->brightness = v;
    return HALO_STATUS_OK;
}
static halo_status_t d_set(void *c, uint8_t start, uint8_t n, const halo_rgb_t *rgb) {
    dev_t *d = c;
    d->set_leds_calls++;
    d->last_n = n;
    for (uint8_t i = 0; i < n; i++)
        d->staging[start + i] = rgb[i];
    return HALO_STATUS_OK;
}
static halo_status_t d_fill(void *c, halo_rgb_t rgb) {
    dev_t *d = c;
    for (int i = 0; i < 15; i++)
        d->staging[i] = rgb;
    return HALO_STATUS_OK;
}
static halo_status_t d_show(void *c) {
    dev_t *d = c;
    memcpy(d->shown, d->staging, sizeof d->shown);
    d->mode = HALO_MODE_DIRECT;
    return HALO_STATUS_OK;
}

typedef struct {
    bus_t bus;
    dev_t dev;
    halo_link_slave_t slave;
    halo_link_port_t mport;
} rig_t;

static void rig_init(rig_t *r) {
    memset(r, 0, sizeof *r);
    r->bus.drop_after = -1;
    r->dev.brightness = 255;
    r->dev.mode = HALO_MODE_AUTO;
    halo_link_port_t sport = {s_write, s_read, b_now, NULL, &r->bus};
    halo_link_slave_ops_t ops = {&r->dev,  d_info, d_state, d_angle, d_apply,
                                 d_bright, d_set,  d_fill,  d_show};
    halo_link_slave_init(&r->slave, &sport, &ops);
    r->bus.slave = &r->slave;
    r->mport = (halo_link_port_t){m_write, m_read, b_now, m_yield, &r->bus};
}

// ---- テスト -----------------------------------------------------------------------------

static void test_crc_and_frame(void) {
    CHECK(halo_link_crc16((const uint8_t *)"123456789", 9) == 0x29B1); // CRC-16/CCITT-FALSE

    halo_link_frame_t f = {
        HALO_LINK_SOF_REQ, 7, HALO_CMD_SET_LEDS, 3, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};
    uint8_t raw[HALO_LINK_FRAME_LEN];
    halo_link_frame_encode(&f, raw);
    halo_link_frame_t g;
    CHECK(halo_link_frame_decode(raw, &g));
    CHECK(g.sof == f.sof && g.seq == 7 && g.cmd == HALO_CMD_SET_LEDS && g.flags_status == 3);
    CHECK(memcmp(g.payload, f.payload, 10) == 0);
    raw[8] ^= 1;
    CHECK(!halo_link_frame_decode(raw, &g)); // 壊れたフレームは弾く
}

static int feed_all(halo_link_parser_t *p, const uint8_t *b, size_t n, uint32_t *t, uint32_t dt,
                    halo_link_frame_t *out) {
    int found = 0;
    for (size_t i = 0; i < n; i++) {
        *t += dt;
        if (halo_link_parser_feed(p, b[i], *t, out))
            found++;
    }
    return found;
}

static void test_parser(void) {
    halo_link_frame_t f = {HALO_LINK_SOF_REQ, 9, HALO_CMD_PING, 0, {0}};
    uint8_t raw[HALO_LINK_FRAME_LEN];
    halo_link_frame_encode(&f, raw);
    halo_link_parser_t p;
    halo_link_frame_t out;
    uint32_t t = 0;

    // ごみ・エコー(SOF が違う)は捨てる
    halo_link_parser_init(&p, HALO_LINK_SOF_REQ);
    uint8_t junk[] = {0x00, 0x5A, 0x11, 0x5A, 0x22};
    CHECK(feed_all(&p, junk, sizeof junk, &t, 90, &out) == 0);
    CHECK(feed_all(&p, raw, sizeof raw, &t, 90, &out) == 1 && out.seq == 9);

    // 連続した 2 フレーム
    uint8_t two[2 * HALO_LINK_FRAME_LEN];
    memcpy(two, raw, 16);
    memcpy(two + 16, raw, 16);
    CHECK(feed_all(&p, two, sizeof two, &t, 90, &out) == 2);

    // 先頭に SOF に見えるごみ(0xA5 を含む)があっても、再走査で本物のフレームを拾う
    uint8_t bad[] = {0xA5, 0x01, 0x02, 0x03};
    uint8_t mix[sizeof bad + HALO_LINK_FRAME_LEN];
    memcpy(mix, bad, sizeof bad);
    memcpy(mix + sizeof bad, raw, 16);
    CHECK(feed_all(&p, mix, sizeof mix, &t, 90, &out) == 1);

    // バイト間が空きすぎたら、途中のフレームを捨てる
    halo_link_parser_init(&p, HALO_LINK_SOF_REQ);
    CHECK(feed_all(&p, raw, 6, &t, 90, &out) == 0);
    t += 3000; // 3 ms 空く
    CHECK(feed_all(&p, raw, sizeof raw, &t, 90, &out) == 1);
}

static void test_basic_flow(void) {
    rig_t r;
    rig_init(&r);
    halo_t h;
    CHECK(halo_new(&h, &r.mport));
    CHECK(h.res.ok && h.res.error.kind == HALO_ERR_NONE);
    CHECK(h.res.status.valid && h.res.status.fresh);
    CHECK(h.res.status.led_count == 15 && h.res.status.preset_count == 3);
    CHECK(h.res.status.proto_ver == HALO_LINK_PROTO_VERSION);
    CHECK(h.res.status.fw_version[0] == 1 && h.res.status.fw_version[2] == 3);
    CHECK(h.res.status.variant_id == 7 && h.res.status.mode == HALO_MODE_AUTO);

    // run: 1〜3 = 固定プリセット(id は n - 1)
    CHECK(halo_run(&h, 2));
    CHECK(r.dev.mode == HALO_MODE_PRESET && r.dev.preset_id == 1);
    CHECK(h.res.status.mode == HALO_MODE_PRESET && h.res.status.preset_id == 1);
    CHECK(halo_run(&h, 0)); // none = 消灯
    CHECK(r.dev.mode == HALO_MODE_OFF && h.res.status.mode == HALO_MODE_OFF);

    // カスタム: 15 個を 5 回に分けて書き、run(4) で反映
    halo_rgb_t colors[15];
    for (int i = 0; i < 15; i++)
        colors[i] = (halo_rgb_t){(uint8_t)i, (uint8_t)(i * 2), (uint8_t)(255 - i)};
    r.dev.set_leds_calls = 0;
    CHECK(halo_set_led(&h, 0, colors, 15));
    CHECK(r.dev.set_leds_calls == 5);
    CHECK(r.dev.mode == HALO_MODE_OFF); // まだ表示は変わらない
    CHECK(halo_run(&h, 4));
    CHECK(r.dev.mode == HALO_MODE_DIRECT && h.res.status.mode == HALO_MODE_DIRECT);
    CHECK(memcmp(r.dev.shown, colors, sizeof colors) == 0);

    // 端数(2 個)は FLAGS で個数を伝えるので、隣の LED を壊さない
    halo_rgb_t two[2] = {{1, 1, 1}, {2, 2, 2}};
    CHECK(halo_set_led(&h, 4, two, 2));
    CHECK(r.dev.last_n == 2);
    CHECK(r.dev.staging[4].r == 1 && r.dev.staging[5].r == 2);
    CHECK(r.dev.staging[6].r == 6); // 3 個目は書き換わらない

    CHECK(halo_set_fill(&h, (halo_rgb_t){9, 8, 7}));
    CHECK(r.dev.staging[0].r == 9 && r.dev.staging[14].b == 7);
    CHECK(halo_set_brightness(&h, 100));
    CHECK(r.dev.brightness == 100 && h.res.status.brightness == 100);

    CHECK(halo_run(&h, 3)); // 固定プリセット 3 番目
    CHECK(r.dev.preset_id == 2);
    CHECK(halo_call(&h) && h.res.status.fresh);
}

static void test_args_and_not_ready(void) {
    rig_t r;
    rig_init(&r);
    halo_t h;
    CHECK(halo_new(&h, &r.mport));
    size_t w = r.bus.master_writes;

    // 範囲外は通信せずに BAD_ARG
    CHECK(!halo_run(&h, 5));
    CHECK(!h.res.ok && h.res.error.kind == HALO_ERR_BAD_ARG);
    CHECK(!halo_run(&h, 200));
    halo_rgb_t c = {1, 2, 3};
    CHECK(!halo_set_led(&h, 15, &c, 1));
    CHECK(h.res.error.kind == HALO_ERR_BAD_ARG);
    CHECK(!halo_set_led(&h, 14, &c, 2));
    CHECK(!halo_set_led(&h, 0, &c, 0));
    CHECK(r.bus.master_writes == w);

    // 接続できていない(valid = false)と、call 以外は NOT_READY
    rig_t d;
    rig_init(&d);
    d.bus.drop_after = 0; // 応答が一切届かない
    halo_t dead;
    CHECK(!halo_new(&dead, &d.mport));
    CHECK(dead.res.error.kind == HALO_ERR_TIMEOUT);
    CHECK(!dead.res.status.valid && dead.res.status.led_count == 0);
    size_t w2 = d.bus.master_writes;
    CHECK(!halo_run(&dead, 1));
    CHECK(dead.res.error.kind == HALO_ERR_NOT_READY);
    CHECK(!halo_set_led(&dead, 0, &c, 1));
    CHECK(dead.res.error.kind == HALO_ERR_NOT_READY);
    CHECK(d.bus.master_writes == w2);
}

static void test_timeout_and_stale(void) {
    rig_t r;
    rig_init(&r);
    halo_t h;
    CHECK(halo_new(&h, &r.mport));
    CHECK(halo_run(&h, 1) && h.res.status.fresh);

    r.bus.drop_after = r.bus.slave_responses; // これ以降の応答は届かない
    size_t w = r.bus.master_writes;
    CHECK(!halo_run(&h, 2));
    CHECK(h.res.error.kind == HALO_ERR_TIMEOUT);
    CHECK(r.bus.master_writes - w == HALO_LINK_MAX_TRIES); // 同じ要求を最大 4 回
    // 失敗しても値は残り、fresh だけ false になる
    CHECK(h.res.status.valid && !h.res.status.fresh);
    CHECK(h.res.status.led_count == 15 && h.res.status.preset_id == 0);

    r.bus.drop_after = -1;
    CHECK(halo_call(&h) && h.res.status.fresh);
    CHECK(h.res.status.preset_id == 1); // A 側は 2 番目に変わっていた(要求は届いていた)
}

static void test_set_led_midway_failure(void) {
    rig_t r;
    rig_init(&r);
    halo_t h;
    CHECK(halo_new(&h, &r.mport));
    halo_rgb_t colors[15];
    memset(colors, 0x11, sizeof colors);
    r.bus.drop_after = r.bus.slave_responses + 2; // 2 回目まで届き、3 回目以降は届かない
    CHECK(!halo_set_led(&h, 0, colors, 15));
    CHECK(h.res.error.kind == HALO_ERR_TIMEOUT);
    CHECK(h.res.error.index == 6); // 3 回目 = LED 6 番から
    CHECK(!h.res.status.fresh);
    CHECK(r.dev.mode == HALO_MODE_AUTO); // 表示は変わっていない
}

static void test_device_status_corrupt_version(void) {
    rig_t r;
    rig_init(&r);
    halo_t h;
    CHECK(halo_new(&h, &r.mport));

    r.dev.apply_status = HALO_STATUS_BAD_ARG;
    CHECK(!halo_run(&h, 1));
    CHECK(h.res.error.kind == HALO_ERR_DEVICE_STATUS);
    CHECK(h.res.error.device_status == HALO_STATUS_BAD_ARG);
    r.dev.apply_status = HALO_STATUS_OK;

    // 応答が壊れたら再送で回復する
    r.bus.corrupt_next = 1;
    size_t w = r.bus.master_writes;
    CHECK(halo_run(&h, 1));
    CHECK(r.bus.master_writes - w == 2);

    // A のプロトコル版が新しい
    r.bus.patch_hello_proto = HALO_LINK_PROTO_VERSION + 1;
    CHECK(!halo_call(&h));
    CHECK(h.res.error.kind == HALO_ERR_VERSION);
}

// スレーブ単体: 生のフレームを送って応答を調べる
static int raw_request(rig_t *r, uint8_t cmd, uint8_t flags, const uint8_t *payload, size_t plen,
                       halo_link_frame_t *res) {
    halo_link_frame_t req;
    memset(&req, 0, sizeof req);
    req.sof = HALO_LINK_SOF_REQ;
    req.seq = 42;
    req.cmd = cmd;
    req.flags_status = flags;
    memcpy(req.payload, payload, plen);
    uint8_t raw[HALO_LINK_FRAME_LEN];
    halo_link_frame_encode(&req, raw);
    bus_write(&r->bus, 0, raw, sizeof raw);
    uint8_t scratch[HALO_LINK_FRAME_LEN];
    bus_read(&r->bus, 0, scratch, sizeof scratch); // 自分のエコーを捨てる
    halo_link_slave_poll(&r->slave);
    uint8_t rx[HALO_LINK_FRAME_LEN];
    if (bus_read(&r->bus, 0, rx, sizeof rx) != sizeof rx)
        return 0;
    return halo_link_frame_decode(rx, res) && res->sof == HALO_LINK_SOF_RES && res->seq == 42 &&
           res->cmd == (uint8_t)(cmd | 0x80);
}

static void test_slave_raw(void) {
    rig_t r;
    rig_init(&r);
    halo_link_frame_t res;
    uint8_t none[1] = {0};

    CHECK(raw_request(&r, 0x41, 0, none, 1, &res) && res.flags_status == HALO_STATUS_UNSUPPORTED);
    CHECK(raw_request(&r, 0x7F, 0, none, 1, &res) && res.flags_status == HALO_STATUS_BAD_CMD);
    CHECK(raw_request(&r, HALO_CMD_PING, 0, none, 1, &res) && res.flags_status == HALO_STATUS_OK);

    uint8_t ga_bad[2] = {13, 4}; // 13 + 4 > 15
    CHECK(raw_request(&r, HALO_CMD_GET_ANGLES, 0, ga_bad, 2, &res) &&
          res.flags_status == HALO_STATUS_BAD_ARG);
    uint8_t ga[2] = {1, 3};
    CHECK(raw_request(&r, HALO_CMD_GET_ANGLES, 0, ga, 2, &res) &&
          res.flags_status == HALO_STATUS_OK);
    CHECK(res.payload[0] == 1 && res.payload[1] == 3);
    CHECK(res.payload[2] == 24 && res.payload[4] == 48 && res.payload[6] == 72);

    // SET_LEDS: FLAGS = 0 は 3 個、LED 個数を超える分は切り詰め、範囲外の start は BAD_ARG
    uint8_t sl[10] = {0, 1, 1, 1, 2, 2, 2, 3, 3, 3};
    CHECK(raw_request(&r, HALO_CMD_SET_LEDS, 0, sl, 10, &res) && r.dev.last_n == 3);
    sl[0] = 14;
    CHECK(raw_request(&r, HALO_CMD_SET_LEDS, 3, sl, 10, &res) && r.dev.last_n == 1);
    sl[0] = 15;
    CHECK(raw_request(&r, HALO_CMD_SET_LEDS, 3, sl, 10, &res) &&
          res.flags_status == HALO_STATUS_BAD_ARG);
    sl[0] = 0;
    CHECK(raw_request(&r, HALO_CMD_SET_LEDS, 4, sl, 10, &res) &&
          res.flags_status == HALO_STATUS_BAD_ARG);
}

int main(void) {
    test_crc_and_frame();
    test_parser();
    test_basic_flow();
    test_args_and_not_ready();
    test_timeout_and_stale();
    test_set_led_midway_failure();
    test_device_status_corrupt_version();
    test_slave_raw();
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
