// 8 個を 45° 間隔に並べ、配線が反時計回り(偏角が減る向き)に数珠つなぎになっているリング。
// 偏角は HALO の基準からの角度で、LED の接続順に並べる。
#define HALO_VARIANT_NAME "ring8_ccw"
#define HALO_LED_COUNT 8
#define HALO_LED_ANGLES_DEG 0, 315, 270, 225, 180, 135, 90, 45

static const halo_preset_t halo_fixed_presets[] = {
    {.brightness = 64,
     .color_count = 4,
     .period_ms = 4000,
     .colors = {{255, 0, 0}, {255, 255, 0}, {0, 255, 0}, {0, 0, 255}}},
    {.brightness = 64, .color_count = 1, .period_ms = 0, .colors = {{255, 120, 20}}},
    {.brightness = 48,
     .color_count = 2,
     .period_ms = 8000,
     .colors = {{255, 0, 255}, {0, 255, 255}}},
};
