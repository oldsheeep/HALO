// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#include "link_uart.h"

#if HALO_LINK_ENABLE

#include "ch32fun.h"

#if FUNCONF_USE_DEBUGPRINTF || FUNCONF_USE_UARTPRINTF
#error "printf デバッグが PD5(USART1 TX)を使う。通信を有効にするなら funconfig.h で無効にする"
#endif
#if FUNCONF_SYSTICK_USE_HCLK != 1 || FUNCONF_SYSTEM_CORE_CLOCK != 48000000
#error "link_uart.c は SysTick=HCLK, 48MHz 前提(now_us)"
#endif

#define TICKS_PER_US (FUNCONF_SYSTEM_CORE_CLOCK / 1000000)

static volatile uint8_t rx_buf[LINK_UART_RX_BUF_LEN];
static uint8_t rx_tail;

void link_uart_init(void) {
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1;
    RCC->AHBPCENR |= RCC_AHBPeriph_DMA1;

    // PD5 = TX(複合機能の push-pull。ダイオードのカソードへ)、PD6 = RX(プルアップ入力)
    GPIOD->CFGLR = (GPIOD->CFGLR & ~((0xf << (5 * 4)) | (0xf << (6 * 4)))) |
                   ((GPIO_Speed_10MHz | GPIO_CNF_OUT_PP_AF) << (5 * 4)) |
                   (GPIO_CNF_IN_PUPD << (6 * 4));
    GPIOD->BSHR = 1 << 6; // プルアップ

    // 115200 8N1
    USART1->CTLR1 = 0;
    USART1->CTLR2 = 0;
    USART1->CTLR3 = USART_CTLR3_DMAR;
    USART1->BRR = (FUNCONF_SYSTEM_CORE_CLOCK + LINK_UART_BAUD / 2) / LINK_UART_BAUD;

    // RX: USART1 → DMA1 チャンネル 5 → rx_buf(循環)
    DMA1_Channel5->CFGR = 0;
    DMA1_Channel5->PADDR = (uint32_t)&USART1->DATAR;
    DMA1_Channel5->MADDR = (uint32_t)rx_buf;
    DMA1_Channel5->CNTR = LINK_UART_RX_BUF_LEN;
    DMA1_Channel5->CFGR = DMA_CFGR1_MINC | DMA_CFGR1_CIRC | DMA_CFGR1_EN;
    rx_tail = 0;

    USART1->CTLR1 = USART_CTLR1_UE | USART_CTLR1_TE | USART_CTLR1_RE;
}

size_t link_uart_write(void *ctx, const uint8_t *buf, size_t len) {
    (void)ctx;
    for (size_t i = 0; i < len; i++) {
        while (!(USART1->STATR & USART_STATR_TXE)) {
        }
        USART1->DATAR = buf[i];
    }
    while (!(USART1->STATR & USART_STATR_TC)) {
    }
    return len;
}

size_t link_uart_read(void *ctx, uint8_t *buf, size_t max) {
    (void)ctx;
    // CNTR は書き込むたびに減る(LINK_UART_RX_BUF_LEN → 1 → 折り返し)。head = 次の書き込み位置
    uint8_t head = (uint8_t)(LINK_UART_RX_BUF_LEN - DMA1_Channel5->CNTR);
    size_t n = 0;
    while (n < max && rx_tail != head) {
        buf[n++] = rx_buf[rx_tail];
        if (++rx_tail == LINK_UART_RX_BUF_LEN)
            rx_tail = 0;
    }
    return n;
}

uint32_t link_uart_now_us(void *ctx) {
    (void)ctx;
    // SysTick(32 ビット、48MHz)を µs に直す。一周(約 89 秒)の継ぎ目で値が飛ばないよう、
    // 前回からの差分を積算する。
    static uint32_t last_cnt, rem_ticks, us;
    uint32_t cnt = SysTick->CNT;
    rem_ticks += cnt - last_cnt;
    last_cnt = cnt;
    us += rem_ticks / TICKS_PER_US;
    rem_ticks %= TICKS_PER_US;
    return us;
}

#endif // HALO_LINK_ENABLE
