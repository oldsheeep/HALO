// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#include "link_slave.h"

#if HALO_LINK_ENABLE

#include "halo_display.h"
#include "halo_link.h"

#ifndef HALO_FW_VERSION_MAJOR
#define HALO_FW_VERSION_MAJOR 0
#define HALO_FW_VERSION_MINOR 1
#define HALO_FW_VERSION_PATCH 0
#endif
#ifndef HALO_VARIANT_ID
#define HALO_VARIANT_ID 0 // 未設定(spec の U3)
#endif

// halo_display.h の値は通信の値と同じ(型は共有しないので、ここで一致を保証する)
_Static_assert(HALO_DISPLAY_AUTO == HALO_MODE_AUTO && HALO_DISPLAY_PRESET == HALO_MODE_PRESET &&
                   HALO_DISPLAY_DIRECT == HALO_MODE_DIRECT && HALO_DISPLAY_OFF == HALO_MODE_OFF,
               "mode");
_Static_assert(HALO_DISPLAY_ID_OFF == HALO_PRESET_OFF && HALO_DISPLAY_ID_AUTO == HALO_PRESET_AUTO,
               "preset id");

static halo_link_slave_t slave;

static halo_status_t to_status(uint8_t r) {
    return r == HALO_DISPLAY_OK ? HALO_STATUS_OK : HALO_STATUS_BAD_ARG;
}

static void op_get_info(void *ctx, halo_link_info_t *out) {
    (void)ctx;
    out->fw_version[0] = HALO_FW_VERSION_MAJOR;
    out->fw_version[1] = HALO_FW_VERSION_MINOR;
    out->fw_version[2] = HALO_FW_VERSION_PATCH;
    out->led_count = halo_display_led_count();
    out->preset_count = halo_display_preset_count();
    out->variant_id = HALO_VARIANT_ID;
}

static void op_get_state(void *ctx, halo_link_state_t *out) {
    (void)ctx;
    halo_display_get(&out->mode, &out->preset_id, &out->brightness);
}

static uint16_t op_get_angle(void *ctx, uint8_t index) {
    (void)ctx;
    return halo_display_led_angle(index);
}

static halo_status_t op_apply_preset(void *ctx, uint8_t id) {
    (void)ctx;
    return to_status(halo_display_apply_preset(id));
}

static halo_status_t op_set_brightness(void *ctx, uint8_t value) {
    (void)ctx;
    halo_display_set_brightness(value);
    return HALO_STATUS_OK;
}

static halo_status_t op_set_leds(void *ctx, uint8_t start, uint8_t n, const halo_rgb_t *rgb) {
    (void)ctx;
    uint8_t bytes[3 * 3];
    for (uint8_t i = 0; i < n && i < 3; i++) {
        bytes[i * 3 + 0] = rgb[i].r;
        bytes[i * 3 + 1] = rgb[i].g;
        bytes[i * 3 + 2] = rgb[i].b;
    }
    return to_status(halo_display_set_leds(start, n, bytes));
}

static halo_status_t op_fill(void *ctx, halo_rgb_t rgb) {
    (void)ctx;
    halo_display_fill(rgb.r, rgb.g, rgb.b);
    return HALO_STATUS_OK;
}

static halo_status_t op_show(void *ctx) {
    (void)ctx;
    halo_display_show();
    return HALO_STATUS_OK;
}

void link_slave_init(void) {
    static const halo_link_slave_ops_t ops = {
        .ctx = 0,
        .get_info = op_get_info,
        .get_state = op_get_state,
        .get_angle = op_get_angle,
        .apply_preset = op_apply_preset,
        .set_brightness = op_set_brightness,
        .set_leds = op_set_leds,
        .fill = op_fill,
        .show = op_show,
    };
    const halo_link_port_t port = {
        .write = link_uart_write,
        .read = link_uart_read,
        .now_us = link_uart_now_us,
        .yield = 0,
        .ctx = 0,
    };
    link_uart_init();
    halo_link_slave_init(&slave, &port, &ops);
}

void link_slave_poll(void) {
    link_uart_now_us(0); // 時刻の積算を進める(SysTick の一周より短い間隔で呼ぶため)
    halo_link_slave_poll(&slave);
}

#endif // HALO_LINK_ENABLE
