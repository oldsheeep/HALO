// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// HALO コントローラ(B 側 / CH32V003J4M6)。
//   - A 側へ HALO Link(単線)でつなぐマスター
//   - ESP32 / Pico W からの I2C を、単線へ中継する(halo-bridge)
//   - ボタン(PC4): 単押し = 次のプリセット、長押し = 消灯 ⇔ 点灯、2 回押し = プリセット 1
// 仕様: docs/halo-button-spec.md、docs/halo-bridge-spec.md、docs/halo-link-spec.md
#include "ch32fun.h"
#include "halo_bridge.h"
#include "halo_button.h"
#include "halo_link.h"
#include "halo_ui.h"
#include "i2c_slave.h"
#include "port.h"

#ifndef HALO_I2C_ADDR
#define HALO_I2C_ADDR HALO_BRIDGE_DEFAULT_ADDR
#endif

// ボタン = PC4(入力、内部プルアップ。押すと L)、INT = PA2(オープンドレイン、アクティブ L)
#define BTN_PIN 4
#define INT_PIN 2

static halo_t halo;
static halo_ui_t ui;
static halo_button_t button;
static halo_bridge_slave_t bridge;

static bool button_pressed(void) { return (GPIOC->INDR & (1u << BTN_PIN)) == 0; }

static void int_set(void *ctx, bool asserted) {
    (void)ctx;
    GPIOA->BSHR = asserted ? (1u << (16 + INT_PIN)) : (1u << INT_PIN); // L = アサート、H = 開放
}

static void gpio_init(void) {
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;
    // PC4: 入力(プルアップ)。CNF = 10、MODE = 00 → 0x8、ODR = 1 でプルアップ
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << (4 * BTN_PIN))) | (0x8u << (4 * BTN_PIN));
    GPIOC->BSHR = 1u << BTN_PIN;
    // PA2: オープンドレイン出力(10MHz)。CNF = 01、MODE = 01 → 0x5。最初は開放(H)
    GPIOA->BSHR = 1u << INT_PIN;
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xFu << (4 * INT_PIN))) | (0x5u << (4 * INT_PIN));
}

// ボタンの操作を実行する。単線を使うあいだ、ブリッジの新しい送信を止める。
static void handle_button(halo_btn_event_t e) {
    halo_bridge_slave_hold(&bridge, true);
    while (halo_bridge_slave_link_busy(&bridge))
        halo_bridge_slave_poll(&bridge); // 送信中のものが終わるまで待つ
    halo_bridge_event_t ev;
    bool ok = halo_ui_handle(&ui, &halo, e, &ev);
    halo_bridge_slave_hold(&bridge, false);
    if (ok)
        halo_bridge_slave_post_event(&bridge, (halo_bridge_evt_t)ev.type, ev.arg);
}

int main(void) {
    SystemInit();
    gpio_init();
    halo_link_port_t port = port_init();
    halo_bridge_slave_init(&bridge, &port, int_set, NULL);
    halo_ui_init(&ui);
    i2c_slave_init(&bridge, HALO_I2C_ADDR);
    (void)halo_new(&halo, &port); // A 側が無くても進める。ボタンの操作のたびに、再接続を試す

    halo_button_init(&button, button_pressed());
    uint32_t last_ms = 0;
    for (;;) {
        halo_bridge_slave_poll(&bridge);

        uint32_t ms = port.now_us(port.ctx) / 1000u;
        if (ms != last_ms) { // 1 ms ごとに、ボタンを更新する
            last_ms = ms;
            halo_btn_event_t e = halo_button_update(&button, ms, button_pressed());
            if (e != HALO_BTN_NONE)
                handle_button(e);
        }
    }
}
