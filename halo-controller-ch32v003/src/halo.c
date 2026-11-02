// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// HALO CH32V003 ファームウェア。
// 起動時は AUTO(固定プリセット 3 パターンを順に表示、各 8 秒)。
// 通信(HALO Link、docs/halo-link-spec.md)が有効なら、B 側のコマンドで表示を切り替えられる。
#include "ch32fun.h"
#include "halo_config.h"
#include "halo_display.h"
#include "link_slave.h"
#include "sk6812.h"

#define FRAME_MS 16
#define FRAME_TICKS (FRAME_MS * (FUNCONF_SYSTEM_CORE_CLOCK / 1000))

static uint8_t frame[HALO_LED_COUNT * 3]; // GRB

int main(void) {
    SystemInit();
    sk6812_init();
    halo_display_init();
    link_slave_init();

    // フレームの周期は SysTick で計り、待っている間も通信を処理する。
    uint32_t t_ms = 0;
    uint32_t last = SysTick->CNT;
    for (;;) {
        link_slave_poll();
        if ((uint32_t)(SysTick->CNT - last) >= FRAME_TICKS) {
            last += FRAME_TICKS;
            halo_display_render(frame, t_ms);
            sk6812_send(frame, HALO_LED_COUNT);
            t_ms += FRAME_MS;
        }
    }
}
