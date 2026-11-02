// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// CH32V003 の I2C1 をスレーブとして動かし、halo-bridge のスレーブにつなぐ。
// PC1 = SDA、PC2 = SCL(どちらも 5V トレラント。オープンドレイン。プルアップは ESP32 / Pico W
// 側の 3.3V)。
// 割り込みで動く。**実機では未確認**(ホストのテストでは、ハードウェアの部分は試せない)。
#ifndef I2C_SLAVE_H
#define I2C_SLAVE_H

#include <stdint.h>

#include "halo_bridge.h"

// 7 ビットアドレス addr で待ち受ける。
void i2c_slave_init(halo_bridge_slave_t *bridge, uint8_t addr);

#endif
