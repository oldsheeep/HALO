// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#include "halo_ui.h"

void halo_ui_init(halo_ui_t *ui) { ui->last_run = 1; }

static bool run(halo_t *halo, uint8_t n) { return halo_run(halo, n); }

static void set_event(halo_bridge_event_t *ev, halo_bridge_evt_t type, uint8_t arg) {
    ev->type = (uint8_t)type;
    ev->arg = arg;
}

bool halo_ui_handle(halo_ui_t *ui, halo_t *halo, halo_btn_event_t e, halo_bridge_event_t *ev) {
    if (e == HALO_BTN_NONE)
        return false;
    // ESP32 / Pico W が先に変えていても、それを前提に動くため、状態を読み直す
    if (!halo_call(halo))
        return false;
    const halo_state_t *st = &halo->res.status;
    uint8_t count = st->preset_count;
    if (count == 0)
        return false;
    bool off = (st->mode == HALO_MODE_OFF);
    bool preset = (st->mode == HALO_MODE_PRESET);
    uint8_t cur =
        preset ? (uint8_t)(st->preset_id + 1) : 0; // halo_run の番号。0 = 固定プリセットではない
    uint8_t last = ui->last_run;
    if (last == 0 || last > count)
        last = 1;

    switch (e) {
    case HALO_BTN_SINGLE:
        if (off) { // 点灯に戻すだけで、進めない
            if (!run(halo, last))
                return false;
            set_event(ev, HALO_BRIDGE_EVT_ON, last);
        } else {
            uint8_t n = preset ? (uint8_t)(cur % count + 1) : 1;
            if (!run(halo, n))
                return false;
            ui->last_run = n;
            set_event(ev, HALO_BRIDGE_EVT_PRESET, n);
        }
        return true;

    case HALO_BTN_LONG:
        if (off) {
            if (!run(halo, last))
                return false;
            set_event(ev, HALO_BRIDGE_EVT_ON, last);
        } else {
            if (!run(halo, 0))
                return false;
            if (preset)
                ui->last_run = cur;
            set_event(ev, HALO_BRIDGE_EVT_OFF, 0);
        }
        return true;

    case HALO_BTN_DOUBLE:
        if (!run(halo, 1))
            return false;
        ui->last_run = 1;
        set_event(ev, HALO_BRIDGE_EVT_PRESET, 1);
        return true;

    default:
        return false;
    }
}
