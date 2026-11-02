// SK6812mini-e 1 線式ドライバ(CH32V003 / 48MHz / GPIO ビットバング)
#ifndef SK6812_H
#define SK6812_H

#include <stdint.h>

// データ線のポートとピン。変更は -DSK6812_PORT=GPIOD -DSK6812_PIN=2 などで。
#ifndef SK6812_PORT
#define SK6812_PORT GPIOC
#endif
#ifndef SK6812_PIN
#define SK6812_PIN 6
#endif

void sk6812_init(void);

// GRB 順の生バイト列(led_count * 3 バイト)を送り、リセット期間(>80us)を置く。
// 送信中は割り込みを止める。
void sk6812_send(const uint8_t *grb, uint16_t led_count);

#endif
