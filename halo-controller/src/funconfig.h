// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#ifndef _FUNCONFIG_H
#define _FUNCONFIG_H

// 48MHz (HSI + PLL)。SysTick(HCLK)を now_us の時刻源にする。
#define FUNCONF_USE_HSI 1
#define FUNCONF_USE_PLL 1
#define FUNCONF_SYSTEM_CORE_CLOCK 48000000
#define FUNCONF_SYSTICK_USE_HCLK 1

// printf デバッグは使わない(UARTPRINTF は PD5 を使い、J4M6 では SWIO と同じピン。通信は PD6 の
// USART1 を使うので両立しない)
#define FUNCONF_USE_DEBUGPRINTF 0
#define FUNCONF_USE_UARTPRINTF 0

#endif
