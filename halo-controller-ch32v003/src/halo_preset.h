// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// プリセットのデータ形式(暫定)とレンダリング。
//
// 書き換え可能プリセットの書き込みプロトコルは別途 docs/ で検討中のため、ここでは
// 「ファームウェア内の固定プリセット」の表現だけを定義する。形式は README に明記してある。
#ifndef HALO_PRESET_H
#define HALO_PRESET_H

#include <stdint.h>

#define HALO_PRESET_MAX_COLORS 4

typedef struct {
    uint8_t r, g, b;
} halo_rgb_t;

// 16 バイト固定(リトルエンディアン)。
//   +0  brightness   全体の明るさ 0..255
//   +1  color_count  使う色数 1..HALO_PRESET_MAX_COLORS
//   +2  period_ms    一周(360°)回転する時間[ms]。0 なら静止。(uint16 LE)
//   +4  colors[4]    RGB 各 1 バイト x 4。color_count 以降は未使用(0 埋め)
// 色は円周上に等間隔で並べ、隣同士を線形補間する(最後の色は最初の色へ戻る)。
typedef struct {
    uint8_t brightness;
    uint8_t color_count;
    uint16_t period_ms;
    halo_rgb_t colors[HALO_PRESET_MAX_COLORS];
} halo_preset_t;

_Static_assert(sizeof(halo_preset_t) == 16, "halo_preset_t must be 16 bytes");

// ---- プリセットを簡単に書くためのマクロ ----
// すべてコンパイル時に計算される定数式。関数呼び出しや計算は実行時に残らず、
// 計算済みの値が const データとしてフラッシュに入る。
//
//   色     HALO_RGB(r, g, b)         各 0..255
//          HALO_RGBA(r, g, b, a)     a: 0..255 (255 = 100%)。色にあらかじめ掛けて暗くする
//          HALO_HEX(0xRRGGBB)
//          HALO_HEXA(0xRRGGBBAA)     例: HALO_HEXA(0x4444BB88)
//   プリセット
//          HALO_PRESET(period_ms, 色, 色, ...)               明るさ 100%。色は 1..4 個
//          HALO_PRESET_B(brightness, period_ms, 色, ...)     全体の明るさ 0..255 を指定
//          period_ms: 一周(360°)する時間[ms]。0 なら静止
#define HALO_SCALE255_(v, a) ((uint8_t)((((v) & 0xFF) * ((a) & 0xFF) + 127) / 255))
#define HALO_RGBA(r, g, b, a) {HALO_SCALE255_(r, a), HALO_SCALE255_(g, a), HALO_SCALE255_(b, a)}
#define HALO_RGB(r, g, b) HALO_RGBA(r, g, b, 255)
#define HALO_HEXA(v) HALO_RGBA((v) >> 24, (v) >> 16, (v) >> 8, (v))
#define HALO_HEX(v) HALO_RGB((v) >> 16, (v) >> 8, (v))

// 色の個数は引数から数える。1..HALO_PRESET_MAX_COLORS の範囲外はコンパイルエラー
// (サイズが負の配列型になる)。
#define HALO_COLOR_COUNT_(...)                                                                     \
    (sizeof(char[(sizeof((halo_rgb_t[]){__VA_ARGS__}) / sizeof(halo_rgb_t)) >= 1 &&                \
                         (sizeof((halo_rgb_t[]){__VA_ARGS__}) / sizeof(halo_rgb_t)) <=             \
                             HALO_PRESET_MAX_COLORS                                                \
                     ? (sizeof((halo_rgb_t[]){__VA_ARGS__}) / sizeof(halo_rgb_t))                  \
                     : -1]))
#define HALO_PRESET_B(brightness_, period_ms_, ...)                                                \
    {                                                                                              \
        .brightness = (brightness_), .color_count = HALO_COLOR_COUNT_(__VA_ARGS__),                \
        .period_ms = (period_ms_), .colors = {__VA_ARGS__}                                         \
    }
#define HALO_PRESET(period_ms_, ...) HALO_PRESET_B(255, period_ms_, __VA_ARGS__)

// 角度 angle_deg(0..359)の LED の色。phase_deg(0..359)は模様の回転量で、
// 増えるにつれ模様が角度の増える向きへ動く。
static inline halo_rgb_t halo_preset_color_at(const halo_preset_t *p, uint16_t angle_deg,
                                              uint16_t phase_deg) {
    uint16_t n = p->color_count;
    uint16_t pos = (uint16_t)((angle_deg + 360 - phase_deg) % 360);
    uint16_t scaled = (uint16_t)(pos * n); // 0..360n-1
    uint16_t idx = scaled / 360;
    uint16_t frac = scaled % 360; // 0..359
    const halo_rgb_t *a = &p->colors[idx];
    const halo_rgb_t *b = &p->colors[(idx + 1) % n];

    uint16_t b8 = p->brightness;
    halo_rgb_t out;
    out.r = (uint8_t)(((a->r * (360 - frac) + b->r * frac) / 360 * b8) / 255);
    out.g = (uint8_t)(((a->g * (360 - frac) + b->g * frac) / 360 * b8) / 255);
    out.b = (uint8_t)(((a->b * (360 - frac) + b->b * frac) / 360 * b8) / 255);
    return out;
}

#endif
