// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#include "halo_display.h"

#include <string.h>

#include "halo_config.h"

_Static_assert(HALO_LED_COUNT <= 255, "LED の番号は u8(通信の start)");

static struct {
    uint8_t mode;
    uint8_t preset_id;
    uint8_t brightness;
    uint8_t staging[HALO_LED_COUNT * 3]; // RGB。SET_LEDS / FILL の書き込み先
    uint8_t direct[HALO_LED_COUNT * 3];  // RGB。SHOW で staging から写した、DIRECT の表示
} st;

void halo_display_init(void) {
    memset(&st, 0, sizeof st);
    st.mode = HALO_DISPLAY_AUTO;
    st.brightness = 255;
}

void halo_display_get(uint8_t *mode, uint8_t *preset_id, uint8_t *brightness) {
    *mode = st.mode;
    *preset_id = st.preset_id;
    *brightness = st.brightness;
}

uint8_t halo_display_apply_preset(uint8_t id) {
    if (id == HALO_DISPLAY_ID_AUTO) {
        st.mode = HALO_DISPLAY_AUTO;
        st.preset_id = 0;
    } else if (id == HALO_DISPLAY_ID_OFF) {
        st.mode = HALO_DISPLAY_OFF;
        st.preset_id = 0;
    } else if (id < HALO_FIXED_PRESET_COUNT) {
        st.mode = HALO_DISPLAY_PRESET;
        st.preset_id = id;
    } else {
        return HALO_DISPLAY_BAD_ARG;
    }
    return HALO_DISPLAY_OK;
}

void halo_display_set_brightness(uint8_t value) { st.brightness = value; }

uint8_t halo_display_set_leds(uint8_t start, uint8_t n, const uint8_t *rgb) {
    if (n == 0 || (unsigned)start + n > HALO_LED_COUNT)
        return HALO_DISPLAY_BAD_ARG;
    memcpy(&st.staging[start * 3], rgb, (size_t)n * 3);
    return HALO_DISPLAY_OK;
}

void halo_display_fill(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < HALO_LED_COUNT; i++) {
        st.staging[i * 3 + 0] = r;
        st.staging[i * 3 + 1] = g;
        st.staging[i * 3 + 2] = b;
    }
}

void halo_display_show(void) {
    memcpy(st.direct, st.staging, sizeof st.direct);
    st.mode = HALO_DISPLAY_DIRECT;
    st.preset_id = 0;
}

static uint8_t scale(uint8_t v) { return (uint8_t)(((uint16_t)v * st.brightness) / 255); }

static void put(uint8_t *grb, int i, uint8_t r, uint8_t g, uint8_t b) {
    grb[i * 3 + 0] = scale(g);
    grb[i * 3 + 1] = scale(r);
    grb[i * 3 + 2] = scale(b);
}

static void render_preset(uint8_t *grb, const halo_preset_t *p, uint32_t t_ms) {
    uint16_t phase = 0;
    if (p->period_ms)
        phase = (uint16_t)((t_ms % p->period_ms) * 360u / p->period_ms);

    for (int i = 0; i < HALO_LED_COUNT; i++) {
        halo_rgb_t c = halo_preset_color_at(p, HALO_LED_ANGLE_DEG(i) % 360, phase);
        put(grb, i, c.r, c.g, c.b);
    }
}

void halo_display_render(uint8_t *grb, uint32_t t_ms) {
    switch (st.mode) {
    case HALO_DISPLAY_PRESET:
        render_preset(grb, &halo_fixed_presets[st.preset_id], t_ms);
        break;
    case HALO_DISPLAY_DIRECT:
        for (int i = 0; i < HALO_LED_COUNT; i++)
            put(grb, i, st.direct[i * 3], st.direct[i * 3 + 1], st.direct[i * 3 + 2]);
        break;
    case HALO_DISPLAY_OFF:
        memset(grb, 0, HALO_LED_COUNT * 3);
        break;
    default: // AUTO
        render_preset(
            grb, &halo_fixed_presets[(t_ms / HALO_DISPLAY_AUTO_SHOW_MS) % HALO_FIXED_PRESET_COUNT],
            t_ms);
        break;
    }
}

uint16_t halo_display_led_count(void) { return HALO_LED_COUNT; }

uint8_t halo_display_preset_count(void) { return HALO_FIXED_PRESET_COUNT; }

uint16_t halo_display_led_angle(uint16_t index) { return HALO_LED_ANGLE_DEG(index); }
