// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// 表示の状態管理(AUTO / PRESET / DIRECT / OFF、ステージングバッファ、全体の明るさ)。
// ハードウェアに依存しないので、ホストでテストできる(tests/test.sh)。
// 通信ライブラリの型には依存しない(値は docs/halo-link-spec.md の 5.2 / 4.2 に合わせる)。
#ifndef HALO_DISPLAY_H
#define HALO_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

// mode(GET_STATE の mode と同じ値)
#define HALO_DISPLAY_AUTO 0   // 固定プリセットを順に表示するデモ(起動時)
#define HALO_DISPLAY_PRESET 1 // 固定プリセット 1 つを表示
#define HALO_DISPLAY_DIRECT 2 // SHOW で反映した直接指定の色を表示
#define HALO_DISPLAY_OFF 3    // 消灯

// halo_display_apply_preset() の特別な id(APPLY_PRESET と同じ値)
#define HALO_DISPLAY_ID_OFF 0xFE
#define HALO_DISPLAY_ID_AUTO 0xFF

// 戻り値
#define HALO_DISPLAY_OK 0
#define HALO_DISPLAY_BAD_ARG 1

// AUTO で各プリセットを表示する時間[ms]
#define HALO_DISPLAY_AUTO_SHOW_MS 8000

// 起動時の状態にする(AUTO、明るさ 255、ステージングは消灯)。
void halo_display_init(void);

void halo_display_get(uint8_t *mode, uint8_t *preset_id, uint8_t *brightness);

// id: 0..(固定プリセット数 - 1) = PRESET、HALO_DISPLAY_ID_OFF = 消灯、HALO_DISPLAY_ID_AUTO = AUTO
uint8_t halo_display_apply_preset(uint8_t id);
void halo_display_set_brightness(uint8_t value);
// ステージングの start から n 個(rgb は RGB 順で n * 3 バイト)。表示は変わらない。
uint8_t halo_display_set_leds(uint8_t start, uint8_t n, const uint8_t *rgb);
void halo_display_fill(uint8_t r, uint8_t g, uint8_t b);
// ステージングを表示に反映し、DIRECT にする。
void halo_display_show(void);

// 現在の状態を GRB 順(HALO_LED_COUNT * 3 バイト)に描く。t_ms は起動からの時間。
// 全体の明るさを掛けた値になる(255 のときは掛けない場合と同じ)。
void halo_display_render(uint8_t *grb, uint32_t t_ms);

// variant の値(通信の HELLO / GET_ANGLES 用)
uint16_t halo_display_led_count(void);
uint8_t halo_display_preset_count(void);
uint16_t halo_display_led_angle(uint16_t index);

#endif
