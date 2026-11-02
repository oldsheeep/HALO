// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// MCU 依存のポート。halo-link は UART を直接触らず、この 3〜4 個の関数だけを呼ぶ。
// 実装は A 側(スレーブ)・B 側(マスター)の各ディレクトリに置く。
#ifndef HALO_LINK_PORT_H
#define HALO_LINK_PORT_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    // len バイトを送信する。送れたバイト数を返す。
    size_t (*write)(void *ctx, const uint8_t *buf, size_t len);
    // 受信済みのバイトを最大 max バイト取り出す(待たない)。取り出したバイト数を返す。
    // 単線なので、自分が送ったバイトもエコーとして返ってくる(ライブラリが除去する)。
    size_t (*read)(void *ctx, uint8_t *buf, size_t max);
    // 単調増加の時刻[µs]。32 ビットで一周してよい。
    uint32_t (*now_us)(void *ctx);
    // 待ち時間のあいだに呼ばれる(任意。NULL 可)。RTOS の yield や省電力などに使う。
    void (*yield)(void *ctx);
    void *ctx;
} halo_link_port_t;

#endif
