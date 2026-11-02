// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#pragma once
#include <stdio.h>
#define ESP_LOGI(tag, ...)                                                                         \
    do {                                                                                           \
        (void)(tag);                                                                               \
        printf(__VA_ARGS__);                                                                       \
    } while (0)
#define ESP_LOGW(tag, ...)                                                                         \
    do {                                                                                           \
        (void)(tag);                                                                               \
        printf(__VA_ARGS__);                                                                       \
    } while (0)
#define ESP_LOGE(tag, ...)                                                                         \
    do {                                                                                           \
        (void)(tag);                                                                               \
        printf(__VA_ARGS__);                                                                       \
    } while (0)
