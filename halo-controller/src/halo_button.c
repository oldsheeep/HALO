// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#include "halo_button.h"

enum { S_IDLE = 0, S_DOWN1, S_WAIT, S_DOWN2, S_IGNORE };

void halo_button_init(halo_button_t *b, bool initially_pressed) {
    b->state = initially_pressed ? S_IGNORE : S_IDLE;
    b->stable = initially_pressed;
    b->candidate = initially_pressed;
    b->since = 0;
    b->t = 0;
}

halo_btn_event_t halo_button_update(halo_button_t *b, uint32_t now, bool raw) {
    // デバウンス: 入力が HALO_BUTTON_DEBOUNCE_MS 変わらなければ確定する
    bool pressed_edge = false, released_edge = false;
    if (raw != b->candidate) {
        b->candidate = raw;
        b->since = now;
    } else if (b->candidate != b->stable && (uint32_t)(now - b->since) >= HALO_BUTTON_DEBOUNCE_MS) {
        b->stable = b->candidate;
        pressed_edge = b->stable;
        released_edge = !b->stable;
    }

    switch (b->state) {
    case S_IDLE:
        if (pressed_edge) {
            b->state = S_DOWN1;
            b->t = now;
        }
        break;
    case S_DOWN1:
        if (b->stable && (uint32_t)(now - b->t) >= HALO_BUTTON_LONG_MS) {
            b->state = S_IGNORE; // 長押しは、押している間に確定する
            return HALO_BTN_LONG;
        }
        if (released_edge) {
            b->state = S_WAIT;
            b->t = now;
        }
        break;
    case S_WAIT:
        if (pressed_edge) {
            b->state = S_DOWN2;
            b->t = now;
        } else if ((uint32_t)(now - b->t) >= HALO_BUTTON_DOUBLE_MS) {
            b->state = S_IDLE;
            return HALO_BTN_SINGLE;
        }
        break;
    case S_DOWN2:
        if (b->stable && (uint32_t)(now - b->t) >= HALO_BUTTON_LONG_MS) {
            b->state = S_IGNORE; // 2 回目が長押しなら、長押しとして扱う
            return HALO_BTN_LONG;
        }
        if (released_edge) {
            b->state = S_IDLE;
            return HALO_BTN_DOUBLE;
        }
        break;
    default: // S_IGNORE: 離されるまで無視する(起動時に押されていた、長押しのあと)
        if (released_edge)
            b->state = S_IDLE;
        break;
    }
    return HALO_BTN_NONE;
}
