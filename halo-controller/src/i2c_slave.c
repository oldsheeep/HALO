// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
#include "i2c_slave.h"

#include "ch32fun.h"

// 受信は、レジスタ番号 + 最大 16 バイト(REQ)。送信は、最大 16 バイト(RESP)。
static halo_bridge_slave_t *g_bridge;
static uint8_t rx[1 + HALO_BRIDGE_FRAME_LEN + 3];
static uint8_t rx_len;
static uint8_t tx[HALO_BRIDGE_FRAME_LEN];
static uint8_t tx_len, tx_pos;

void i2c_slave_init(halo_bridge_slave_t *bridge, uint8_t addr) {
    g_bridge = bridge;
    rx_len = tx_len = tx_pos = 0;

    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;
    RCC->APB1PCENR |= RCC_APB1Periph_I2C1;

    // PC1 = SDA、PC2 = SCL: 複合機能のオープンドレイン(10MHz)
    GPIOC->CFGLR = (GPIOC->CFGLR & ~((0xFu << (4 * 1)) | (0xFu << (4 * 2)))) | (0xDu << (4 * 1)) |
                   (0xDu << (4 * 2));

    // I2C1 をリセットして、初期化する
    RCC->APB1PRSTR |= RCC_APB1Periph_I2C1;
    RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;
    I2C1->CTLR1 |= I2C_CTLR1_SWRST;
    I2C1->CTLR1 &= ~I2C_CTLR1_SWRST;

    // 周辺クロックの周波数。ch32fun の I2C スレーブの例と同じ値(2 MHz で割った値)にしている
    I2C1->CTLR2 = (uint16_t)((FUNCONF_SYSTEM_CORE_CLOCK / 2000000) & I2C_CTLR2_FREQ);
    I2C1->CTLR2 |= I2C_CTLR2_ITBUFEN | I2C_CTLR2_ITEVTEN | I2C_CTLR2_ITERREN;

    // スレーブとしてのバスのクロックは、マスター(ESP32 / Pico W)が決める。ここは、100kHz
    // の値を入れておく
    I2C1->CKCFGR = (uint16_t)((FUNCONF_SYSTEM_CORE_CLOCK / (2 * 100000)) & I2C_CKCFGR_CCR);

    I2C1->OADDR1 = (uint16_t)(addr << 1);
    I2C1->OADDR2 = 0;

    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_SetPriority(I2C1_EV_IRQn, 2 << 4);
    NVIC_EnableIRQ(I2C1_ER_IRQn);
    NVIC_SetPriority(I2C1_ER_IRQn, 2 << 4);

    I2C1->CTLR1 |= I2C_CTLR1_PE;
    I2C1->CTLR1 |= I2C_CTLR1_ACK;
}

void I2C1_EV_IRQHandler(void) __attribute__((interrupt));
void I2C1_EV_IRQHandler(void) {
    uint16_t star1 = I2C1->STAR1;
    uint16_t star2 = I2C1->STAR2; // STAR1 のあとに STAR2 を読むと、ADDR が消える

    if (star1 & I2C_STAR1_ADDR) {    // アドレスが一致した
        if (star2 & I2C_STAR2_TRA) { // マスターが読む(送信側)
            if (rx_len > 0) {        // 書き(レジスタ番号)のあとの、繰り返しスタート
                halo_bridge_slave_on_write(g_bridge, rx, rx_len);
                rx_len = 0;
            }
            tx_len = (uint8_t)halo_bridge_slave_on_read(g_bridge, tx, sizeof tx);
            tx_pos = 0;
        } else { // マスターが書く
            rx_len = 0;
        }
    }

    if (star1 & I2C_STAR1_RXNE) { // 1 バイト受信した
        uint8_t b = (uint8_t)I2C1->DATAR;
        if (rx_len < sizeof rx)
            rx[rx_len++] = b;
    }

    if (star1 & I2C_STAR1_TXE) { // 次の 1 バイトを送る
        I2C1->DATAR = (tx_pos < tx_len) ? tx[tx_pos++] : 0xFF;
    }

    if (star1 & I2C_STAR1_STOPF) { // STOP: 書き込みが終わった
        I2C1->CTLR1 &=
            (uint16_t)~I2C_CTLR1_STOP; // STOPF を消す(STAR1 を読んだあとの CTLR1 への書き込み)
        if (rx_len > 0) {
            halo_bridge_slave_on_write(g_bridge, rx, rx_len);
            rx_len = 0;
        }
    }
}

void I2C1_ER_IRQHandler(void) __attribute__((interrupt));
void I2C1_ER_IRQHandler(void) {
    uint16_t star1 = I2C1->STAR1;
    // バスエラー、アービトレーション・ロスト、ACK 失敗は、フラグを消して、次の通信に備える
    if (star1 & (I2C_STAR1_BERR | I2C_STAR1_ARLO | I2C_STAR1_AF | I2C_STAR1_OVR))
        I2C1->STAR1 &= (uint16_t)~(I2C_STAR1_BERR | I2C_STAR1_ARLO | I2C_STAR1_AF | I2C_STAR1_OVR);
    rx_len = 0;
}
