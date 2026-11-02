// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#pragma once
#include <stdbool.h>
#define GPIO_IN 0
#define GPIO_FUNC_I2C 3
void gpio_init(unsigned gpio);
void gpio_set_dir(unsigned gpio, bool out);
void gpio_pull_up(unsigned gpio);
void gpio_set_function(unsigned gpio, unsigned fn);
bool gpio_get(unsigned gpio);
