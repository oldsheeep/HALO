// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// 構文の検査用の、Pico SDK の宣言の抜粋。実際のヘッダは Pico SDK が持つ。
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint64_t t;
} absolute_time_t;
void stdio_init_all(void);
void sleep_ms(uint32_t ms);
void busy_wait_us(uint32_t us);
absolute_time_t get_absolute_time(void);
absolute_time_t delayed_by_ms(absolute_time_t t, uint32_t ms);
int64_t absolute_time_diff_us(absolute_time_t from, absolute_time_t to);
