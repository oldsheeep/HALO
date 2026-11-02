// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#include "i2c_port.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#define XFER_TIMEOUT_US 20000

static int port_write(void *ctx, uint8_t addr, const uint8_t *buf, size_t len) {
    (void)ctx;
    int n = i2c_write_timeout_us(i2c0, addr, buf, len, false, XFER_TIMEOUT_US);
    return n == (int)len ? 0 : -1;
}

// 書いて(STOP なし)、繰り返しスタートで読む
static int port_write_read(void *ctx, uint8_t addr, const uint8_t *wbuf, size_t wlen, uint8_t *rbuf,
                           size_t rlen) {
    (void)ctx;
    if (i2c_write_timeout_us(i2c0, addr, wbuf, wlen, true, XFER_TIMEOUT_US) != (int)wlen)
        return -1;
    return i2c_read_timeout_us(i2c0, addr, rbuf, rlen, false, XFER_TIMEOUT_US) == (int)rlen ? 0
                                                                                            : -1;
}

static uint32_t port_now_us(void *ctx) {
    (void)ctx;
    return (uint32_t)time_us_64();
}

// 待ち時間: STATUS のポーリングの間隔を、少し空ける
static void port_yield(void *ctx) {
    (void)ctx;
    busy_wait_us(300);
}

halo_bridge_i2c_t pico_i2c_port_init(unsigned sda, unsigned scl) {
    i2c_init(i2c0, 100 * 1000);
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);
    gpio_pull_up(sda);
    gpio_pull_up(scl);
    halo_bridge_i2c_t p = {port_write, port_write_read, port_now_us, port_yield, NULL};
    return p;
}
