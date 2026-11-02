// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#include "sk6812.h"

#include "ch32fun.h"

#if FUNCONF_SYSTICK_USE_HCLK != 1 || FUNCONF_SYSTEM_CORE_CLOCK != 48000000
#error "sk6812.c は SysTick=HCLK, 48MHz 前提のタイミング定数を使う"
#endif

// SK6812mini-e のタイミング(48MHz = 1 クロック約 20.8ns)
//   周期 1.25us = 60 clk。T0H 0.3us ≒ 14 clk、T1H 0.6us ≒ 29 clk(許容 ±0.15us)
#define PERIOD_CLK 60
#define T0H_CLK 14
#define T1H_CLK 29

void sk6812_init(void) {
    int port_id = (((intptr_t)SK6812_PORT - (intptr_t)GPIOA) >> 10);
    RCC->APB2PCENR |= (RCC_APB2Periph_GPIOA << port_id);

    int off = SK6812_PIN * 4;
    SK6812_PORT->CFGLR =
        (SK6812_PORT->CFGLR & ~(0xf << off)) | ((GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << off);
    SK6812_PORT->BSHR = 1 << (16 + SK6812_PIN);
}

void sk6812_send(const uint8_t *grb, uint16_t led_count) {
    const uint32_t on = 1u << SK6812_PIN;
    const uint32_t off = 1u << (16 + SK6812_PIN);
    const uint8_t *end = grb + (uint32_t)led_count * 3;

    __disable_irq();
    while (grb != end) {
        uint8_t byte = *grb++;
        for (int i = 0; i < 8; i++) {
            uint32_t high = (byte & 0x80) ? T1H_CLK : T0H_CLK;
            uint32_t start = SysTick->CNT;
            SK6812_PORT->BSHR = on;
            while ((SysTick->CNT - start) < high) {
            }
            SK6812_PORT->BSHR = off;
            while ((SysTick->CNT - start) < PERIOD_CLK) {
            }
            byte <<= 1;
        }
    }
    __enable_irq();

    Delay_Us(100); // リセット(>80us)
}
