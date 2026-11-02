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
