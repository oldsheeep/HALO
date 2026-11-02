// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct i2c_inst i2c_inst_t;
extern i2c_inst_t *const i2c0;
unsigned i2c_init(i2c_inst_t *i2c, unsigned baudrate);
int i2c_write_timeout_us(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, bool nostop,
                         unsigned timeout_us);
int i2c_read_timeout_us(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop,
                        unsigned timeout_us);
