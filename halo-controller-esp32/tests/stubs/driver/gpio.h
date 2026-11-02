// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// 構文の検査用の、ESP-IDF gpio の宣言の抜粋。
#pragma once
#include <stdint.h>
typedef int esp_err_t;
typedef struct {
    uint64_t pin_bit_mask;
    int mode;
    int pull_up_en;
    int pull_down_en;
    int intr_type;
} gpio_config_t;
#define GPIO_MODE_INPUT 1
#define GPIO_PULLUP_ENABLE 1
esp_err_t gpio_config(const gpio_config_t *cfg);
int gpio_get_level(int gpio);
