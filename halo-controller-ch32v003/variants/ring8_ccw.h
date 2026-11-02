// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// 8 個を 45° 間隔に並べ、配線が反時計回り(偏角が減る向き)に数珠つなぎになっているリング。
// 偏角は HALO の基準からの角度で、LED の接続順に並べる。等間隔でない・逆向きの配線は明示する。
#define HALO_VARIANT_NAME "ring8_ccw"
#define HALO_LED_COUNT 8
#define HALO_LED_ANGLES_DEG 0, 315, 270, 225, 180, 135, 90, 45

static const halo_preset_t halo_fixed_presets[] = {
    HALO_PRESET_B(64, 4000, HALO_RGB(255, 0, 0), HALO_RGB(255, 255, 0), HALO_RGB(0, 255, 0),
                  HALO_RGB(0, 0, 255)),
    HALO_PRESET_B(64, 0, HALO_RGB(255, 120, 20)),
    HALO_PRESET_B(48, 8000, HALO_RGB(255, 0, 255), HALO_RGB(0, 255, 255)),
};
