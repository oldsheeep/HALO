// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// ボタンの判定(単押し・長押し・2 回押し)。MCU に依存しない。仕様は docs/halo-button-spec.md。
#ifndef HALO_BUTTON_H
#define HALO_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#ifndef HALO_BUTTON_DEBOUNCE_MS
#define HALO_BUTTON_DEBOUNCE_MS 20 // 入力がこの時間変わらなければ確定する
#endif
#ifndef HALO_BUTTON_LONG_MS
#define HALO_BUTTON_LONG_MS 700 // 押し続けてこの時間に達したら長押し
#endif
#ifndef HALO_BUTTON_DOUBLE_MS
#define HALO_BUTTON_DOUBLE_MS 300 // 1 回目を離してから、2 回目を押すまでの上限
#endif

typedef enum {
    HALO_BTN_NONE = 0,
    HALO_BTN_SINGLE, // 単押し(離してから HALO_BUTTON_DOUBLE_MS 待って確定する)
    HALO_BTN_DOUBLE, // 2 回押し(2 回目を離したとき)
    HALO_BTN_LONG,   // 長押し(押している間に HALO_BUTTON_LONG_MS に達したとき)
} halo_btn_event_t;

typedef struct {
    uint8_t state;
    bool stable;    // デバウンス後の入力(true = 押されている)
    bool candidate; // 直近の生の入力
    uint32_t since; // candidate になった時刻
    uint32_t t;     // 状態ごとの基準時刻(押した・離した時刻)
} halo_button_t;

// initially_pressed: 起動時にボタンが押されていたら true(離されるまで無視する)
void halo_button_init(halo_button_t *b, bool initially_pressed);
// 1 ms ごと程度に呼ぶ。now_ms は単調増加(32 ビットで一周してよい)。raw_pressed は生の入力。
halo_btn_event_t halo_button_update(halo_button_t *b, uint32_t now_ms, bool raw_pressed);

#endif
