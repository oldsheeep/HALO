// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#ifndef _FUNCONFIG_H
#define _FUNCONFIG_H

// 48MHz (HSI + PLL)。SysTick(HCLK)を now_us の時刻源にする。
#define FUNCONF_USE_HSI 1
#define FUNCONF_USE_PLL 1
#define FUNCONF_SYSTEM_CORE_CLOCK 48000000
#define FUNCONF_SYSTICK_USE_HCLK 1

// printf デバッグは使わない(PD5 を USART1 TX に使うので、DEBUGPRINTF/UARTPRINTF とは両立しない)
#define FUNCONF_USE_DEBUGPRINTF 0
#define FUNCONF_USE_UARTPRINTF 0

#endif
