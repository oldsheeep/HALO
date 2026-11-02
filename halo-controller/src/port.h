// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// halo-link のポート実装(CH32V003 の USART1 + SysTick)。
#ifndef PORT_H
#define PORT_H

#include "halo_link_port.h"

// USART1 を初期化し(PD5 = TX、PD6 = RX、115200 8N1)、ポートを返す。
halo_link_port_t port_init(void);

#endif
