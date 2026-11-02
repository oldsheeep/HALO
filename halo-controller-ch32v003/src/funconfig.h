#ifndef _FUNCONFIG_H
#define _FUNCONFIG_H

// 48MHz (HSI + PLL)。SK6812 のタイミングは SysTick(HCLK)で計るので HCLK 駆動にする。
#define FUNCONF_USE_HSI 1
#define FUNCONF_USE_PLL 1
#define FUNCONF_SYSTEM_CORE_CLOCK 48000000
#define FUNCONF_SYSTICK_USE_HCLK 1

// printf デバッグは使わない(コードサイズ節約)
#define FUNCONF_USE_DEBUGPRINTF 0
#define FUNCONF_USE_UARTPRINTF 0

#endif
