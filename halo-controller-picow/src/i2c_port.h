// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// Pico SDK(hardware_i2c)で、halo-bridge の I2C ポート(halo_bridge_i2c_t)を作る。
#ifndef I2C_PORT_H
#define I2C_PORT_H

#include "halo_bridge.h"

// I2C0 を、100 kHz のマスターとして初期化し、ポートを返す。sda / scl は GPIO 番号。
// 3.3V へのプルアップ(4.7kΩ)が、外付けで必要(内部プルアップも有効にするが、弱い)。
halo_bridge_i2c_t pico_i2c_port_init(unsigned sda, unsigned scl);

#endif
