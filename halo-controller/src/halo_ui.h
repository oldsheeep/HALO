// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// ボタンの操作を、A 側への命令(halo_run)に変える。MCU に依存しない。仕様は docs/halo-button-spec.md
// の 5 節。
#ifndef HALO_UI_H
#define HALO_UI_H

#include <stdbool.h>

#include "halo_bridge.h"
#include "halo_button.h"
#include "halo_link.h"

typedef struct {
    uint8_t last_run; // 消灯から点灯に戻すときの halo_run の番号(初期値 1)
} halo_ui_t;

void halo_ui_init(halo_ui_t *ui);
// 操作 e を実行する。最初に halo_call で A 側の状態を読み直す。
// 成功したら true を返し、ESP32 / Pico W に知らせるイベントを *ev に入れる。
// 失敗(通信の失敗など)のときは false で、何も変えない。
bool halo_ui_handle(halo_ui_t *ui, halo_t *halo, halo_btn_event_t e, halo_bridge_event_t *ev);

#endif
