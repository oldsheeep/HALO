// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// HALO Bridge: ESP32 / Pico W(I2C マスター)と、B 側の CH32V003(I2C スレーブ)をつなぐライブラリ。
// 仕様は docs/halo-bridge-spec.md。
//   - マスター(ESP32 / Pico W): I2C から halo-link のポート(halo_link_port_t)を作る。
//     halo-link の halo_new / halo_run などを、そのまま I2C 越しに使える。
//   - スレーブ(B 側): I2C の書き込み・読み出しを受けて、HALO Link のフレームを A 側へ中継する。
// 純粋な C99。動的メモリ確保・グローバル状態なし。halo-link(halo_link.h)に依存する。
#ifndef HALO_BRIDGE_H
#define HALO_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "halo_link.h"

#ifdef __cplusplus
extern "C" {
#endif

// スレーブの状態を、I2C の割り込み(on_write / on_read)と、メインループ(poll
// など)の両方が書き換える。
// 割り込みと競合する短い区間を、これで囲む。割り込みを止められる環境では、割り込みを禁止・許可する
// 関数に定義し直す(ホストのテストや、割り込みを使わない環境では、何もしなくてよい)。
#ifndef HALO_BRIDGE_CRITICAL_ENTER
#define HALO_BRIDGE_CRITICAL_ENTER() ((void)0)
#define HALO_BRIDGE_CRITICAL_EXIT() ((void)0)
#endif

#define HALO_BRIDGE_PROTO_VERSION 1
#define HALO_BRIDGE_DEFAULT_ADDR 0x32 // I2C の 7 ビットアドレス
#define HALO_BRIDGE_FRAME_LEN HALO_LINK_FRAME_LEN
// 単線で A 側の応答を待つ時間。halo-link のマスターの応答待ち(20 ms)より短くする。
#define HALO_BRIDGE_LINK_TIMEOUT_US 15000

// レジスタ番号
typedef enum {
    HALO_BRIDGE_REG_INFO = 0x00,   // 読み 4 バイト: [ブリッジ版, 機能, 0, 0]
    HALO_BRIDGE_REG_STATUS = 0x01, // 読み 1 バイト
    HALO_BRIDGE_REG_REQ = 0x02,    // 書き 16 バイト(要求フレーム)
    HALO_BRIDGE_REG_RESP = 0x03,   // 読み 16 バイト(応答フレーム)
    HALO_BRIDGE_REG_EVENT = 0x04,  // 読み 4 バイト: [種類, 引数, 通番, 0]
} halo_bridge_reg_t;

// STATUS のビット
#define HALO_BRIDGE_ST_BUSY 0x01     // 要求を送って、応答を待っている(または順番を待っている)
#define HALO_BRIDGE_ST_READY 0x02    // 応答が RESP にある
#define HALO_BRIDGE_ST_TIMEOUT 0x04  // 時間内に有効な応答が来なかった
#define HALO_BRIDGE_ST_EVENT 0x08    // B 側のローカル操作(ボタン)による変化がある
#define HALO_BRIDGE_ST_REJECTED 0x10 // 直前の書き込みを受け付けなかった

// INFO の機能ビット
#define HALO_BRIDGE_CAP_INT 0x01 // INT 線あり

// EVENT の種類
typedef enum {
    HALO_BRIDGE_EVT_NONE = 0,
    HALO_BRIDGE_EVT_PRESET = 1, // プリセットを切り替えた。arg = halo_run の番号
    HALO_BRIDGE_EVT_OFF = 2,    // 消灯した
    HALO_BRIDGE_EVT_ON = 3,     // 点灯した。arg = 直前のプリセットの番号
} halo_bridge_evt_t;

typedef struct {
    uint8_t type; // halo_bridge_evt_t
    uint8_t arg;
    uint8_t seq; // イベントのたびに 1 ずつ増える
} halo_bridge_event_t;

// ---- マスター(ESP32 / Pico W) ----------------------------------------------------------

// I2C のポート。実装は、ESP-IDF / Arduino / Pico SDK などの側に置く。0 を返せば成功、負なら失敗。
typedef struct {
    // addr(7 ビット)へ len バイトを書く(STOP まで)。
    int (*write)(void *ctx, uint8_t addr, const uint8_t *buf, size_t len);
    // wlen バイトを書き(STOP なし)、繰り返しスタートで rlen バイトを読む。
    int (*write_read)(void *ctx, uint8_t addr, const uint8_t *wbuf, size_t wlen, uint8_t *rbuf,
                      size_t rlen);
    // 単調増加の時刻[µs]
    uint32_t (*now_us)(void *ctx);
    // 待ち時間に呼ばれる(任意。NULL 可)
    void (*yield)(void *ctx);
    void *ctx;
} halo_bridge_i2c_t;

typedef struct {
    halo_bridge_i2c_t i2c;
    uint8_t addr;
    uint8_t tx[HALO_BRIDGE_FRAME_LEN];
    size_t tx_len;
    uint8_t rx[HALO_BRIDGE_FRAME_LEN];
    size_t rx_len, rx_pos;
    bool pending; // 要求を出して、応答を待っている
} halo_bridge_master_t;

void halo_bridge_master_init(halo_bridge_master_t *m, const halo_bridge_i2c_t *i2c, uint8_t addr);
// halo-link のポート。halo_new(&halo, &port) などに、そのまま渡せる。m
// は、使っている間、生かしておく。
halo_link_port_t halo_bridge_master_port(halo_bridge_master_t *m);

// 0 を返せば成功
int halo_bridge_read_info(halo_bridge_master_t *m, uint8_t out[4]);
int halo_bridge_read_status(halo_bridge_master_t *m, uint8_t *status);
int halo_bridge_read_event(halo_bridge_master_t *m, halo_bridge_event_t *ev);

// ---- スレーブ(B 側の CH32V003) ----------------------------------------------------------

typedef struct {
    halo_link_port_t link;                     // A 側とつながる単線のポート
    void (*int_set)(void *ctx, bool asserted); // INT 線(任意。NULL 可)。asserted = L にする
    void *int_ctx;

    // 以下は内部用
    uint8_t reg;
    uint8_t status;
    uint8_t req[HALO_BRIDGE_FRAME_LEN];
    uint8_t resp[HALO_BRIDGE_FRAME_LEN];
    halo_bridge_event_t ev;
    uint8_t state; // 0 = 待機、1 = 送る順番を待つ、2 = 応答を待つ
    uint32_t t0;
    bool hold;
    bool int_asserted;
    halo_link_parser_t parser;
} halo_bridge_slave_t;

void halo_bridge_slave_init(halo_bridge_slave_t *s, const halo_link_port_t *link,
                            void (*int_set)(void *ctx, bool asserted), void *int_ctx);
// I2C の書き込みが終わったとき(STOP)に、バイト列(レジスタ番号 + データ)を渡す。
void halo_bridge_slave_on_write(halo_bridge_slave_t *s, const uint8_t *buf, size_t len);
// I2C の読み出しが始まるときに呼ぶ。現在のレジスタの内容を out に書き、書いたバイト数を返す。
// 読み出されたものとして、READY / TIMEOUT / EVENT / REJECTED を消す(最後まで読まれる前提)。
size_t halo_bridge_slave_on_read(halo_bridge_slave_t *s, uint8_t *out, size_t max);
// メインループから、頻繁に呼ぶ。単線の送受信を、ブロックせずに進める。
void halo_bridge_slave_poll(halo_bridge_slave_t *s);
// B 側のローカルな処理(ボタンなど)が単線を使うとき、true にして、新しい送信を止める。
// 送信中のもの(link_busy)が終わるのを待ってから、単線を使い、終わったら false に戻す。
void halo_bridge_slave_hold(halo_bridge_slave_t *s, bool hold);
bool halo_bridge_slave_link_busy(const halo_bridge_slave_t *s);
// B 側のローカルな変化を、ESP32 / Pico W に知らせる。
void halo_bridge_slave_post_event(halo_bridge_slave_t *s, halo_bridge_evt_t type, uint8_t arg);

#ifdef __cplusplus
}
#endif

#endif
