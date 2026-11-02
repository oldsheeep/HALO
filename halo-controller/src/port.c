// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// halo-link のポート実装。単線(ダイオード方式、仕様 3.1)なので、自分の送信も RX に返る。
// 受信は割り込みを使わずポーリングし、write 中も 1 バイトごとに吸い出して取りこぼしを防ぐ。
#include "port.h"

#include "ch32fun.h"

#if FUNCONF_SYSTICK_USE_HCLK != 1 || FUNCONF_SYSTEM_CORE_CLOCK != 48000000
#error "port.c の now_us は SysTick=HCLK, 48MHz 前提"
#endif

#define BAUD 115200u
#define TICKS_PER_US (FUNCONF_SYSTEM_CORE_CLOCK / 1000000u)

#define STATR_ORE 0x0008u
#define STATR_RXNE 0x0020u
#define STATR_TXE 0x0080u

#define RX_BUF_LEN 64 // 2 の冪(16 バイトのフレーム + エコー数個分)
static volatile uint8_t rx_buf[RX_BUF_LEN];
static uint8_t rx_head, rx_tail;

// USART の受信データを RAM のリングバッファへ移す。
static void rx_drain(void) {
    for (;;) {
        uint32_t st = USART1->STATR;
        if (!(st & (STATR_RXNE | STATR_ORE)))
            return;
        // ORE は STATR → DATAR の順に読むと消える。バイトは 1 つ読み出す。
        uint8_t b = (uint8_t)USART1->DATAR;
        if ((uint8_t)(rx_head - rx_tail) < RX_BUF_LEN) // 溢れたら捨てる(ライブラリが再送で回復)
            rx_buf[rx_head++ & (RX_BUF_LEN - 1)] = b;
    }
}

static size_t port_write(void *ctx, const uint8_t *buf, size_t len) {
    (void)ctx;
    for (size_t i = 0; i < len; i++) {
        while (!(USART1->STATR & STATR_TXE))
            rx_drain();
        USART1->DATAR = buf[i];
        rx_drain();
    }
    return len;
}

static size_t port_read(void *ctx, uint8_t *buf, size_t max) {
    (void)ctx;
    rx_drain();
    size_t n = 0;
    while (n < max && rx_tail != rx_head)
        buf[n++] = rx_buf[rx_tail++ & (RX_BUF_LEN - 1)];
    return n;
}

// SysTick(48MHz、32 ビット)の差分を µs に直して積算する。89 秒より短い間隔で呼ばれる前提
// (ライブラリは待ち中に頻繁に呼ぶ。デモの待機中は呼ばれないが、通信のたびに追いつく)。
static uint32_t port_now_us(void *ctx) {
    static uint32_t last_cnt, us, rem;
    (void)ctx;
    uint32_t cnt = SysTick->CNT;
    uint32_t d = (cnt - last_cnt) + rem;
    last_cnt = cnt;
    us += d / TICKS_PER_US;
    rem = d % TICKS_PER_US;
    return us;
}

static void port_yield(void *ctx) {
    (void)ctx;
    rx_drain();
}

halo_link_port_t port_init(void) {
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1 | RCC_APB2Periph_AFIO;

    // PD5 = USART1_TX(複用プッシュプル 10MHz)、PD6 = USART1_RX(プルアップ入力)。
    // ダイオード方式なので TX は H のとき信号線に影響しない。線の H は外付けのプルアップが作る。
    GPIOD->CFGLR &= ~((0xFu << (4 * 5)) | (0xFu << (4 * 6)));
    GPIOD->CFGLR |= (0x9u << (4 * 5)) | (0x8u << (4 * 6));
    GPIOD->BSHR = 1u << 6;

    USART1->BRR = (FUNCONF_SYSTEM_CORE_CLOCK + BAUD / 2) / BAUD;
    USART1->CTLR2 = 0;
    USART1->CTLR3 = 0;
    USART1->CTLR1 = USART_CTLR1_TE | USART_CTLR1_RE | USART_CTLR1_UE; // 8N1
    (void)USART1->STATR;
    (void)USART1->DATAR;

    halo_link_port_t p = {port_write, port_read, port_now_us, port_yield, 0};
    return p;
}
