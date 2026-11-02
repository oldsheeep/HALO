// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// halo-link のポート実装(CH32V003 の USART1 + SysTick)。
#ifndef PORT_H
#define PORT_H

#include "halo_link_port.h"

// USART1 を初期化し(PD6 の 1 ピン、リマップ 2 + 半二重、オープンドレイン、115200
// 8N1)、ポートを返す。 信号線には外付けのプルアップ(3.3V へ)が必要。
halo_link_port_t port_init(void);

#endif
