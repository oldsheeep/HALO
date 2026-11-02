// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// halo-link のスレーブを halo_display につなぐ(ops の実装)。
#ifndef LINK_SLAVE_H
#define LINK_SLAVE_H

#include "link_uart.h"

#if HALO_LINK_ENABLE
// ポートを初期化してスレーブを用意する。halo_display_init() の後に呼ぶ。
void link_slave_init(void);
// 受信済みのバイトを処理して応答する。メインループから頻繁に呼ぶ。
void link_slave_poll(void);
#else
static inline void link_slave_init(void) {}
static inline void link_slave_poll(void) {}
#endif

#endif
