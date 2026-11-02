// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// HALO コントローラ(Pico W): B 側の CH32V003 に I2C でつなぎ、A 側を操作するデモ。
//   - 起動時に halo_new で接続(失敗したら再試行)
//   - 3 秒ごとに、固定プリセットを順に実行(halo_run)
//   - INT 線(アクティブ L)でボタンの変化に気づいたら、イベントを読んで、状態を読み直す
// 出力は USB のシリアル(stdio)。BLE(モバイル端末からの操作)は、この上に足す。
#include <stdio.h>

#include "halo_bridge.h"
#include "hardware/gpio.h"
#include "i2c_port.h"
#include "pico/stdlib.h"

#ifndef HALO_I2C_SDA_GPIO
#define HALO_I2C_SDA_GPIO 4 // I2C0 の SDA(GP4)
#endif
#ifndef HALO_I2C_SCL_GPIO
#define HALO_I2C_SCL_GPIO 5 // I2C0 の SCL(GP5)
#endif
#ifndef HALO_INT_GPIO
#define HALO_INT_GPIO 6 // B 側の PA2(アクティブ L)。内部プルアップを使う(外付けの 10kΩ を勧める)
#endif
#define PRESET_SHOW_MS 3000
#define RETRY_MS 500

static halo_bridge_master_t bridge;
static halo_t halo;

static void log_status(void) {
    const halo_state_t *s = &halo.res.status;
    printf("LED %u 個、固定プリセット %u 個、mode %u、preset %u(%s)\n", s->led_count,
           s->preset_count, s->mode, s->preset_id, s->fresh ? "最新" : "古い");
}

// ボタンなど、B 側の変化に気づいたら、イベントを読んで、状態を読み直す。
static void check_events(void) {
    if (gpio_get(HALO_INT_GPIO))
        return; // INT が H = 何も無い
    uint8_t st = 0;
    if (halo_bridge_read_status(&bridge, &st) != 0 || !(st & HALO_BRIDGE_ST_EVENT))
        return;
    halo_bridge_event_t ev;
    if (halo_bridge_read_event(&bridge, &ev) == 0) {
        printf("イベント: 種類 %u、引数 %u、通番 %u\n", ev.type, ev.arg, ev.seq);
        if (halo_call(&halo))
            log_status();
    }
}

int main(void) {
    stdio_init_all();
    halo_bridge_i2c_t i2c = pico_i2c_port_init(HALO_I2C_SDA_GPIO, HALO_I2C_SCL_GPIO);
    gpio_init(HALO_INT_GPIO);
    gpio_set_dir(HALO_INT_GPIO, GPIO_IN);
    gpio_pull_up(HALO_INT_GPIO);

    halo_bridge_master_init(&bridge, &i2c, HALO_BRIDGE_DEFAULT_ADDR);
    halo_link_port_t port = halo_bridge_master_port(&bridge);
    while (!halo_new(&halo, &port)) { // B 側・A 側がまだ起動していないときは、再試行する
        printf("接続できない(種別 %d、STATUS %d)。再試行する\n", halo.res.error.kind,
               halo.res.error.device_status);
        sleep_ms(RETRY_MS);
    }
    log_status();

    uint8_t n = 0;
    absolute_time_t next = get_absolute_time();
    for (;;) {
        check_events();
        if (absolute_time_diff_us(get_absolute_time(), next) <= 0) {
            next = delayed_by_ms(next, PRESET_SHOW_MS);
            n = (uint8_t)(n % halo.res.status.preset_count + 1);
            if (halo_run(&halo, n)) {
                printf("プリセット %u を実行\n", n);
            } else {
                printf("実行に失敗(種別 %d)。再接続を試す\n", halo.res.error.kind);
                (void)halo_call(&halo);
            }
        }
        sleep_ms(20);
    }
}
