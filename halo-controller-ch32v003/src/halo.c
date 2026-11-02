// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// HALO CH32V003 ファームウェア: 点灯デモ。
// 固定プリセット 3 パターンを順に表示する(各 8 秒)。
#include "ch32fun.h"
#include "halo_config.h"
#include "sk6812.h"

#define FRAME_MS 16
#define PRESET_SHOW_MS 8000

static uint8_t frame[HALO_LED_COUNT * 3]; // GRB

static void render(const halo_preset_t *p, uint32_t t_ms) {
    uint16_t phase = 0;
    if (p->period_ms)
        phase = (uint16_t)((t_ms % p->period_ms) * 360u / p->period_ms);

    for (int i = 0; i < HALO_LED_COUNT; i++) {
        halo_rgb_t c = halo_preset_color_at(p, HALO_LED_ANGLE_DEG(i) % 360, phase);
        frame[i * 3 + 0] = c.g;
        frame[i * 3 + 1] = c.r;
        frame[i * 3 + 2] = c.b;
    }
}

int main(void) {
    SystemInit();
    sk6812_init();

    uint32_t t_ms = 0;
    for (;;) {
        const halo_preset_t *p =
            &halo_fixed_presets[(t_ms / PRESET_SHOW_MS) % HALO_FIXED_PRESET_COUNT];
        render(p, t_ms);
        sk6812_send(frame, HALO_LED_COUNT);
        Delay_Ms(FRAME_MS);
        t_ms += FRAME_MS;
    }
}
