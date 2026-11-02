// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// 12 個を等間隔(30°)に並べたリング。偏角を省略すると等間隔(i * 360 / 個数)になる。
#define HALO_VARIANT_NAME "ring12"
#define HALO_LED_COUNT 12

// 固定プリセット。書き込みでは変更できない(ファームウェアの const データ)。
// 書き方は src/halo_preset.h の「プリセットを簡単に書くためのマクロ」を参照。
static const halo_preset_t halo_fixed_presets[] = {
    // 0: 赤→緑→青 の虹っぽいグラデーションが 6 秒で一周
    HALO_PRESET_B(64, 6000, HALO_RGB(255, 0, 0), HALO_RGB(0, 255, 0), HALO_RGB(0, 0, 255)),
    // 1: 暖色の静止グラデーション
    HALO_PRESET_B(64, 0, HALO_RGB(255, 80, 0), HALO_RGB(255, 0, 40)),
    // 2: 白と青のゆっくり回転
    HALO_PRESET_B(48, 12000, HALO_RGB(255, 255, 255), HALO_RGB(0, 40, 255)),
};
