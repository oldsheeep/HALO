// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#include "i2c_port.h"

#include "driver/i2c_master.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#define XFER_TIMEOUT_MS 20

static i2c_master_dev_handle_t s_dev;

static int port_write(void *ctx, uint8_t addr, const uint8_t *buf, size_t len) {
    (void)ctx;
    (void)addr; // デバイスのハンドルが、アドレスを持つ
    return i2c_master_transmit(s_dev, buf, len, XFER_TIMEOUT_MS) == ESP_OK ? 0 : -1;
}

static int port_write_read(void *ctx, uint8_t addr, const uint8_t *wbuf, size_t wlen, uint8_t *rbuf,
                           size_t rlen) {
    (void)ctx;
    (void)addr;
    return i2c_master_transmit_receive(s_dev, wbuf, wlen, rbuf, rlen, XFER_TIMEOUT_MS) == ESP_OK
               ? 0
               : -1;
}

static uint32_t port_now_us(void *ctx) {
    (void)ctx;
    return (uint32_t)esp_timer_get_time();
}

// 待ち時間: STATUS のポーリングの間隔を、少し空ける(タスクを譲ると 10 ms 単位になり、遅すぎる)
static void port_yield(void *ctx) {
    (void)ctx;
    esp_rom_delay_us(300);
}

halo_bridge_i2c_t esp_i2c_port_init(int sda, int scl, uint8_t addr) {
    halo_bridge_i2c_t p = {0};
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup =
            true, // 外付けのプルアップの補助(内部は弱いので、4.7kΩ を付ける)
    };
    i2c_master_bus_handle_t bus;
    if (i2c_new_master_bus(&bus_cfg, &bus) != ESP_OK)
        return p;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(bus, &dev_cfg, &s_dev) != ESP_OK)
        return p;
    p.write = port_write;
    p.write_read = port_write_read;
    p.now_us = port_now_us;
    p.yield = port_yield;
    return p;
}
