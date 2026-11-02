// 12 個を等間隔(30°)に並べたリング。
#define HALO_VARIANT_NAME "ring12"
#define HALO_LED_COUNT 12
#define HALO_LED_ANGLES_DEG 0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330

// 固定プリセット。書き込みでは変更できない(ファームウェアの const データ)。
static const halo_preset_t halo_fixed_presets[] = {
    // 0: 赤→緑→青 の虹っぽいグラデーションが 6 秒で一周
    {.brightness = 64,
     .color_count = 3,
     .period_ms = 6000,
     .colors = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}}},
    // 1: 暖色の静止グラデーション
    {.brightness = 64,
     .color_count = 2,
     .period_ms = 0,
     .colors = {{255, 80, 0}, {255, 0, 40}}},
    // 2: 白と青のゆっくり回転
    {.brightness = 48,
     .color_count = 2,
     .period_ms = 12000,
     .colors = {{255, 255, 255}, {0, 40, 255}}},
};
