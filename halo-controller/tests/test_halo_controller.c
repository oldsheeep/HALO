// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// ホストでのテスト: ボタンの判定(halo_button)と、操作から A 側への命令への変換(halo_ui)。
#include <stdio.h>
#include <string.h>

#include "halo_button.h"
#include "halo_ui.h"

static int g_checks = 0, g_failed = 0;
#define CHECK(cond)                                                                                \
    do {                                                                                           \
        g_checks++;                                                                                \
        if (!(cond)) {                                                                             \
            g_failed++;                                                                            \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                 \
        }                                                                                          \
    } while (0)

// ---- ボタン ---------------------------------------------------------------------------------
typedef struct {
    int n;
    halo_btn_event_t ev[8];
    uint32_t at[8];
} log_t;

// 押している区間 {開始, 終了}(ms)の一覧に従って、1 ms ごとに更新する。
// glitch: true なら、各区間の最初と最後に、チャタリング(数 ms の反転)を入れる。
static log_t run_button(const uint32_t (*presses)[2], int np, uint32_t t0, uint32_t t_end,
                        bool boot_held, bool chatter) {
    halo_button_t b;
    log_t lg = {0};
    halo_button_init(&b, boot_held);
    for (uint32_t t = t0; t != t_end; t++) {
        bool pressed = boot_held && (t - t0) < presses[0][0] && np == 0;
        for (int i = 0; i < np; i++) {
            uint32_t s = presses[i][0] + t0, e = presses[i][1] + t0;
            if ((uint32_t)(t - s) < (uint32_t)(e - s))
                pressed = true;
            if (chatter && ((t - s) < 6 || (uint32_t)(t - e) < 6) && ((t & 1) == 0) &&
                (uint32_t)(t - s) < 0x80000000u) {
                // 押し始めと離した直後は、数 ms のあいだ、入力が交互に反転する
                pressed = !pressed ? ((t - s) < 6) : pressed;
            }
        }
        halo_btn_event_t e = halo_button_update(&b, t, pressed);
        if (e != HALO_BTN_NONE && lg.n < 8) {
            lg.ev[lg.n] = e;
            lg.at[lg.n] = t - t0;
            lg.n++;
        }
    }
    return lg;
}

static void test_button(void) {
    // 単押し: 80 ms 押す。離した(確定 = 200)から 300 ms 後に確定する
    {
        uint32_t p[][2] = {{100, 180}};
        log_t l = run_button(p, 1, 0, 2000, false, false);
        CHECK(l.n == 1 && l.ev[0] == HALO_BTN_SINGLE);
        CHECK(l.at[0] >= 480 && l.at[0] <= 520);
    }
    // 2 回押し: 2 回目を離した(確定)ときに出る。単押しは出ない
    {
        uint32_t p[][2] = {{100, 180}, {300, 380}};
        log_t l = run_button(p, 2, 0, 2000, false, false);
        CHECK(l.n == 1 && l.ev[0] == HALO_BTN_DOUBLE);
        CHECK(l.at[0] >= 390 && l.at[0] <= 420);
    }
    // 長押し: 押している間に出る(離すのを待たない)。離したあとは何も起きない
    {
        uint32_t p[][2] = {{100, 1500}};
        log_t l = run_button(p, 1, 0, 3000, false, false);
        CHECK(l.n == 1 && l.ev[0] == HALO_BTN_LONG);
        CHECK(l.at[0] >= 800 && l.at[0] <= 840);
    }
    // 2 回目が長押しなら、長押し(2 回押しにはしない)
    {
        uint32_t p[][2] = {{100, 180}, {300, 1500}};
        log_t l = run_button(p, 2, 0, 3000, false, false);
        CHECK(l.n == 1 && l.ev[0] == HALO_BTN_LONG);
    }
    // 窓を過ぎてからの 2 回目は、別の単押し
    {
        uint32_t p[][2] = {{100, 180}, {800, 880}};
        log_t l = run_button(p, 2, 0, 3000, false, false);
        CHECK(l.n == 2 && l.ev[0] == HALO_BTN_SINGLE && l.ev[1] == HALO_BTN_SINGLE);
    }
    // 3 回続けて押す: 2 回押し + 次の単押し
    {
        uint32_t p[][2] = {{100, 180}, {260, 340}, {420, 500}};
        log_t l = run_button(p, 3, 0, 3000, false, false);
        CHECK(l.n == 2 && l.ev[0] == HALO_BTN_DOUBLE && l.ev[1] == HALO_BTN_SINGLE);
    }
    // デバウンス: 数 ms の短いノイズは無視する
    {
        halo_button_t b;
        halo_button_init(&b, false);
        int n = 0;
        for (uint32_t t = 0; t < 1000; t++) {
            bool pressed = (t >= 100 && t < 105) || (t >= 400 && t < 408);
            if (halo_button_update(&b, t, pressed) != HALO_BTN_NONE)
                n++;
        }
        CHECK(n == 0);
    }
    // チャタリングがあっても、1 回の押下として数える
    {
        halo_button_t b;
        halo_button_init(&b, false);
        halo_btn_event_t seen[4];
        int n = 0;
        for (uint32_t t = 0; t < 2000; t++) {
            bool pressed = (t >= 100 && t < 300);
            if ((t >= 100 && t < 108) ||
                (t >= 300 && t < 308)) // 押し始めと離した直後に、交互に反転する
                pressed = (t & 1) == 0 ? pressed : !pressed;
            halo_btn_event_t e = halo_button_update(&b, t, pressed);
            if (e != HALO_BTN_NONE && n < 4)
                seen[n++] = e;
        }
        CHECK(n == 1 && seen[0] == HALO_BTN_SINGLE);
    }
    // 起動時に押されていたら、離されるまで無視する(離したあとの単押しは、通常どおり)
    {
        halo_button_t b;
        halo_button_init(&b, true);
        int n = 0;
        halo_btn_event_t last = HALO_BTN_NONE;
        for (uint32_t t = 0; t < 3000; t++) {
            bool pressed = (t < 1500) || (t >= 2000 && t < 2080);
            halo_btn_event_t e = halo_button_update(&b, t, pressed);
            if (e != HALO_BTN_NONE) {
                n++;
                last = e;
            }
        }
        CHECK(n == 1 && last == HALO_BTN_SINGLE);
    }
    // 時刻が 32 ビットで一周しても動く
    {
        uint32_t p[][2] = {{100, 180}};
        log_t l = run_button(p, 1, 0xFFFFFF00u, 0xFFFFFF00u + 2000u, false, false);
        CHECK(l.n == 1 && l.ev[0] == HALO_BTN_SINGLE);
    }
}

// ---- 模擬の A 側と単線 -------------------------------------------------------------------------
typedef struct {
    uint8_t q[2][8192];
    size_t head[2], tail[2];
    uint32_t t_us;
    int slave_responses;
    int drop_after; // この数を超えた応答は届かない(-1 = 無制限)
} wire_t;

static size_t wire_write(wire_t *w, int node, const uint8_t *buf, size_t len) {
    int deliver = 1;
    if (node == 1 && len == HALO_LINK_FRAME_LEN) {
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
static size_t m_write(void *c, const uint8_t *b, size_t n) { return wire_write(c, 0, b, n); }
static size_t m_read(void *c, uint8_t *b, size_t m) { return wire_read(c, 0, b, m); }
static size_t s_write(void *c, const uint8_t *b, size_t n) { return wire_write(c, 1, b, n); }
static size_t s_read(void *c, uint8_t *b, size_t m) { return wire_read(c, 1, b, m); }
static uint32_t w_now(void *c) {
    wire_t *w = c;
    w->t_us += 20;
    return w->t_us;
}

typedef struct {
    uint8_t mode, preset_id, brightness;
    halo_rgb_t staging[15], shown[15];
} dev_t;
static void d_info(void *c, halo_link_info_t *o) {
    (void)c;
    o->led_count = 15;
    o->preset_count = 3;
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
static halo_status_t d_set(void *c, uint8_t s, uint8_t n, const halo_rgb_t *rgb) {
    dev_t *d = c;
    for (uint8_t i = 0; i < n; i++)
        d->staging[s + i] = rgb[i];
    return HALO_STATUS_OK;
}
static halo_status_t d_show(void *c) {
    dev_t *d = c;
    memcpy(d->shown, d->staging, sizeof d->shown);
    d->mode = HALO_MODE_DIRECT;
    return HALO_STATUS_OK;
}

typedef struct {
    wire_t wire;
    dev_t dev;
    halo_link_slave_t a;
    halo_link_port_t mport;
    halo_t halo;
    halo_ui_t ui;
} rig_t;

static void a_yield(void *c) { halo_link_slave_poll(&((rig_t *)c)->a); }
static size_t m_write2(void *c, const uint8_t *b, size_t n) {
    return m_write(&((rig_t *)c)->wire, b, n);
}
static size_t m_read2(void *c, uint8_t *b, size_t m) { return m_read(&((rig_t *)c)->wire, b, m); }
static uint32_t m_now2(void *c) { return w_now(&((rig_t *)c)->wire); }

static void rig_init(rig_t *r) {
    memset(r, 0, sizeof *r);
    r->wire.drop_after = -1;
    r->dev.brightness = 255;
    r->dev.mode = HALO_MODE_AUTO;
    halo_link_port_t sport = {s_write, s_read, w_now, NULL, &r->wire};
    halo_link_slave_ops_t ops = {.ctx = &r->dev,
                                 .get_info = d_info,
                                 .get_state = d_state,
                                 .apply_preset = d_apply,
                                 .set_leds = d_set,
                                 .show = d_show};
    halo_link_slave_init(&r->a, &sport, &ops);
    r->mport = (halo_link_port_t){m_write2, m_read2, m_now2, a_yield, r};
    (void)halo_new(&r->halo, &r->mport);
    halo_ui_init(&r->ui);
}

static bool press(rig_t *r, halo_btn_event_t e, halo_bridge_event_t *ev) {
    memset(ev, 0, sizeof *ev);
    return halo_ui_handle(&r->ui, &r->halo, e, ev);
}

static void test_ui(void) {
    halo_bridge_event_t ev;
    rig_t r;

    // 単押し: AUTO から、プリセット 1 → 2 → 3 → 1 …
    rig_init(&r);
    CHECK(press(&r, HALO_BTN_SINGLE, &ev) && r.dev.mode == HALO_MODE_PRESET &&
          r.dev.preset_id == 0);
    CHECK(ev.type == HALO_BRIDGE_EVT_PRESET && ev.arg == 1);
    CHECK(press(&r, HALO_BTN_SINGLE, &ev) && r.dev.preset_id == 1 && ev.arg == 2);
    CHECK(press(&r, HALO_BTN_SINGLE, &ev) && r.dev.preset_id == 2 && ev.arg == 3);
    CHECK(press(&r, HALO_BTN_SINGLE, &ev) && r.dev.preset_id == 0 &&
          ev.arg == 1); // 最後の次は 1 に戻る

    // 長押し: 消灯 → 点灯で、直前のプリセットに戻る
    CHECK(press(&r, HALO_BTN_SINGLE, &ev) && r.dev.preset_id == 1); // プリセット 2
    CHECK(press(&r, HALO_BTN_LONG, &ev) && r.dev.mode == HALO_MODE_OFF &&
          ev.type == HALO_BRIDGE_EVT_OFF);
    CHECK(r.ui.last_run == 2);
    CHECK(press(&r, HALO_BTN_LONG, &ev) && r.dev.mode == HALO_MODE_PRESET && r.dev.preset_id == 1);
    CHECK(ev.type == HALO_BRIDGE_EVT_ON && ev.arg == 2);

    // 消灯中の単押しは、点灯に戻すだけで、進めない
    CHECK(press(&r, HALO_BTN_LONG, &ev) && r.dev.mode == HALO_MODE_OFF);
    CHECK(press(&r, HALO_BTN_SINGLE, &ev) && r.dev.mode == HALO_MODE_PRESET &&
          r.dev.preset_id == 1);
    CHECK(ev.type == HALO_BRIDGE_EVT_ON && ev.arg == 2);

    // 2 回押し: どの状態からでも、プリセット 1
    CHECK(press(&r, HALO_BTN_DOUBLE, &ev) && r.dev.preset_id == 0 &&
          ev.type == HALO_BRIDGE_EVT_PRESET && ev.arg == 1);
    CHECK(press(&r, HALO_BTN_LONG, &ev) && r.dev.mode == HALO_MODE_OFF);
    CHECK(press(&r, HALO_BTN_DOUBLE, &ev) && r.dev.mode == HALO_MODE_PRESET &&
          r.dev.preset_id == 0);

    // ESP32 / Pico W が先に変えていても、読み直した状態から進む
    r.dev.preset_id = 2; // プリセット 3 に変えられた
    CHECK(press(&r, HALO_BTN_SINGLE, &ev) && r.dev.preset_id == 0 && ev.arg == 1);

    // カスタム点灯(DIRECT)から: 単押し = プリセット 1、長押し = 消灯(last_run は変わらない)
    rig_init(&r);
    r.dev.mode = HALO_MODE_DIRECT;
    r.ui.last_run = 3;
    CHECK(press(&r, HALO_BTN_LONG, &ev) && r.dev.mode == HALO_MODE_OFF && r.ui.last_run == 3);
    CHECK(press(&r, HALO_BTN_LONG, &ev) && r.dev.mode == HALO_MODE_PRESET &&
          r.dev.preset_id == 2); // 3 に戻る
    r.dev.mode = HALO_MODE_DIRECT;
    CHECK(press(&r, HALO_BTN_SINGLE, &ev) && r.dev.preset_id == 0 &&
          r.dev.mode == HALO_MODE_PRESET);

    // 通信に失敗したら、何も変えず、イベントも出さない
    rig_init(&r);
    CHECK(press(&r, HALO_BTN_SINGLE, &ev) && r.dev.preset_id == 0);
    r.wire.drop_after = r.wire.slave_responses; // これ以降は応答が届かない
    uint8_t before = r.ui.last_run;
    CHECK(!press(&r, HALO_BTN_SINGLE, &ev));
    CHECK(r.ui.last_run == before && ev.type == HALO_BRIDGE_EVT_NONE);
    r.wire.drop_after = -1;
    CHECK(press(&r, HALO_BTN_SINGLE, &ev) && r.dev.preset_id == 1); // 回復する

    // 何も操作が無いときは、何もしない
    CHECK(!press(&r, HALO_BTN_NONE, &ev));
}

int main(void) {
    test_button();
    test_ui();
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
