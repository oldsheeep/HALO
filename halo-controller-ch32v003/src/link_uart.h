// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// HALO Link のポート(CH32V003 の USART1)。halo_link_port_t の write / read / now_us の実体。
//   TX = PD5、RX = PD6(USART1 の既定ピン)。115200 bps 8N1。
//   RX は DMA1 チャンネル 5 の循環バッファで受けるので、SK6812 の送信中(割り込み禁止)でも
//   取りこぼさない。単線(ダイオード方式)なので、自分の送信も RX に返る(ライブラリが除去する)。
#ifndef LINK_UART_H
#define LINK_UART_H

#include <stddef.h>
#include <stdint.h>

#ifndef HALO_LINK_ENABLE
#define HALO_LINK_ENABLE 1
#endif

#define LINK_UART_BAUD 115200
#define LINK_UART_RX_BUF_LEN 64 // DMA 循環バッファ。16 バイトのフレーム 4 個分

void link_uart_init(void);

// halo_link_port_t の各関数(ctx は使わない)
size_t link_uart_write(void *ctx, const uint8_t *buf, size_t len);
size_t link_uart_read(void *ctx, uint8_t *buf, size_t max);
// 単調増加の時刻[µs](32 ビットで一周する)。89 秒に 1 回以上呼ぶこと(SysTick が一周するため)。
uint32_t link_uart_now_us(void *ctx);

#endif
