// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// HALO 制御デモ(B 側 / マスター)。A 側へ接続し、固定プリセット → 虹色のカスタム点灯 →
// 消灯を繰り返す。
#include "ch32fun.h"
#include "halo_link.h"
#include "port.h"

#define PRESET_SHOW_MS 3000
#define RAINBOW_STEP_DEG 10
#define RAINBOW_STEP_MS 60
#define OFF_MS 1500
#define RETRY_MS 500
#define MAX_LEDS 48 // RAM 節約のための上限。これを超える LED は虹の対象外

static halo_t halo;
static halo_rgb_t colors[MAX_LEDS];

// 色相 h(0〜359)→ RGB(彩度・明度 最大。明るさは A 側の brightness に任せる)。
static halo_rgb_t hue_to_rgb(uint16_t h) {
    uint8_t region = (uint8_t)(h / 60);
    uint8_t up = (uint8_t)((h % 60) * 255 / 59);
    uint8_t down = (uint8_t)(255 - up);
    switch (region) {
    case 0:
        return (halo_rgb_t){255, up, 0};
    case 1:
        return (halo_rgb_t){down, 255, 0};
    case 2:
        return (halo_rgb_t){0, 255, up};
    case 3:
        return (halo_rgb_t){0, down, 255};
    case 4:
        return (halo_rgb_t){up, 0, 255};
    default:
        return (halo_rgb_t){255, 0, down};
    }
}

// 接続できるまで再試行する(halo_call は HELLO + GET_STATE)。
static void connect(void) {
    while (!halo_call(&halo))
        Delay_Ms(RETRY_MS);
}

// 虹色を shift 度ずらして書く。LED が多いときは MAX_LEDS 個までを書く。
static bool write_rainbow(uint16_t led_count, uint16_t shift) {
    uint8_t n = led_count > MAX_LEDS ? MAX_LEDS : (uint8_t)led_count;
    for (uint8_t i = 0; i < n; i++)
        colors[i] = hue_to_rgb((uint16_t)((i * 360u / led_count + shift) % 360));
    return halo_set_led(&halo, 0, colors, n);
}

// 1 周分のデモ。途中で通信に失敗したら false(呼び出し側で再接続する)。
static bool run_once(void) {
    uint16_t led_count = halo.res.status.led_count;
    uint8_t preset_count = halo.res.status.preset_count;

    // (a) 固定プリセットを順に
    for (uint8_t n = 1; n <= preset_count; n++) {
        if (!halo_run(&halo, n))
            return false;
        Delay_Ms(PRESET_SHOW_MS);
    }

    // (b) 虹色のカスタム点灯を一周
    for (uint16_t shift = 0; shift < 360; shift += RAINBOW_STEP_DEG) {
        if (!write_rainbow(led_count, shift) || !halo_run(&halo, preset_count + 1))
            return false;
        Delay_Ms(RAINBOW_STEP_MS);
    }

    // (c) 消灯
    if (!halo_run(&halo, 0))
        return false;
    Delay_Ms(OFF_MS);
    return true;
}

int main(void) {
    SystemInit();
    halo_link_port_t port = port_init();
    halo_new(&halo, &port); // 失敗しても connect() で再試行する
    for (;;) {
        connect();
        while (run_once())
            ;
        // 失敗: A 側の再起動や配線の抜けを想定し、少し待って再接続する。
        Delay_Ms(RETRY_MS);
    }
}
