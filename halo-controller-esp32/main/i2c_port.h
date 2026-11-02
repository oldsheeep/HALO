// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// ESP-IDF(v5.2 以降の i2c_master)で、halo-bridge の I2C ポート(halo_bridge_i2c_t)を作る。
#ifndef I2C_PORT_H
#define I2C_PORT_H

#include "halo_bridge.h"

// I2C マスターを初期化し(100 kHz)、ポートを返す。失敗したら write / write_read が NULL
// のポートを返す。 sda / scl は GPIO 番号。ESP32 / Pico W 側の 3.3V
// へのプルアップ(4.7kΩ)が、外付けで必要。
halo_bridge_i2c_t esp_i2c_port_init(int sda, int scl, uint8_t addr);

#endif
