// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// HALO コントローラ(ESP32): B 側の CH32V003 に I2C でつなぎ、A 側を操作するデモ。
//   - 起動時に halo_new で接続(失敗したら再試行)
//   - 3 秒ごとに、固定プリセットを順に実行(halo_run)
//   - INT 線(アクティブ L)でボタンの変化に気づいたら、イベントを読んで、状態を読み直す
// BLE(モバイル端末からの操作)は、この上に足す。
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "halo_bridge.h"
#include "i2c_port.h"

#ifndef HALO_I2C_SDA_GPIO
#define HALO_I2C_SDA_GPIO 21
#endif
#ifndef HALO_I2C_SCL_GPIO
#define HALO_I2C_SCL_GPIO 22
#endif
#ifndef HALO_INT_GPIO
#define HALO_INT_GPIO 4 // B 側の PA2(アクティブ L)。外付けで 3.3V へ 10kΩ のプルアップ
#endif
#define PRESET_SHOW_MS 3000
#define RETRY_MS 500

static const char *TAG = "halo";
static halo_bridge_master_t bridge;
static halo_t halo;

static void log_status(void) {
    const halo_state_t *s = &halo.res.status;
    ESP_LOGI(TAG, "LED %u 個、固定プリセット %u 個、mode %u、preset %u(%s)", s->led_count,
             s->preset_count, s->mode, s->preset_id, s->fresh ? "最新" : "古い");
}

// ボタンなど、B 側の変化に気づいたら、イベントを読んで、状態を読み直す。
static void check_events(void) {
    if (gpio_get_level(HALO_INT_GPIO) != 0)
        return; // INT が H = 何も無い
    uint8_t st = 0;
    if (halo_bridge_read_status(&bridge, &st) != 0 || !(st & HALO_BRIDGE_ST_EVENT))
        return;
    halo_bridge_event_t ev;
    if (halo_bridge_read_event(&bridge, &ev) == 0) {
        ESP_LOGI(TAG, "イベント: 種類 %u、引数 %u、通番 %u", ev.type, ev.arg, ev.seq);
        if (halo_call(&halo))
            log_status();
    }
}

void app_main(void) {
    halo_bridge_i2c_t i2c =
        esp_i2c_port_init(HALO_I2C_SDA_GPIO, HALO_I2C_SCL_GPIO, HALO_BRIDGE_DEFAULT_ADDR);
    if (!i2c.write) {
        ESP_LOGE(TAG, "I2C の初期化に失敗した");
        return;
    }
    gpio_config_t int_cfg = {.pin_bit_mask = 1ULL << HALO_INT_GPIO,
                             .mode = GPIO_MODE_INPUT,
                             .pull_up_en = GPIO_PULLUP_ENABLE};
    gpio_config(&int_cfg);

    halo_bridge_master_init(&bridge, &i2c, HALO_BRIDGE_DEFAULT_ADDR);
    halo_link_port_t port = halo_bridge_master_port(&bridge);
    while (!halo_new(&halo, &port)) { // B 側・A 側がまだ起動していないときは、再試行する
        ESP_LOGW(TAG, "接続できない(種別 %d、STATUS %d)。再試行する", halo.res.error.kind,
                 halo.res.error.device_status);
        vTaskDelay(pdMS_TO_TICKS(RETRY_MS));
    }
    log_status();

    uint8_t n = 0;
    int64_t next_us = esp_timer_get_time();
    for (;;) {
        check_events();
        if (esp_timer_get_time() >= next_us) {
            next_us += (int64_t)PRESET_SHOW_MS * 1000;
            n = (uint8_t)(n % halo.res.status.preset_count + 1);
            if (halo_run(&halo, n)) {
                ESP_LOGI(TAG, "プリセット %u を実行", n);
            } else {
                ESP_LOGW(TAG, "実行に失敗(種別 %d)。再接続を試す", halo.res.error.kind);
                (void)halo_call(&halo);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
