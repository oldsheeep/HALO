// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// 15 個を等間隔(24°)に並べたリング。
#define HALO_VARIANT_NAME "ring15"
#define HALO_LED_COUNT 15

static const halo_preset_t halo_fixed_presets[] = {
    // 0: 単色の薄い青 #4444BB88(アルファ 0x88 = 約 53% の明るさ。静止)
    HALO_PRESET(0, HALO_HEXA(0x4444BB88)),
    // 1: 薄い青 → 青 → 薄い青 が 6 秒で一周する
    HALO_PRESET(6000, HALO_HEXA(0x4444BB88), HALO_HEX(0x0000FF)),
    // 2: 暖色の静止(全体の明るさ 25%)
    HALO_PRESET_B(64, 0, HALO_HEX(0xFFB060)),
};
