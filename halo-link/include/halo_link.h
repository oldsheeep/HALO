// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// HALO Link: 単線半二重通信のライブラリ。仕様は docs/halo-link-spec.md。
//   - フレーム(16 バイト固定長)・CRC・受信パーサ  … 全員が使う
//   - マスター(B 側): halo_t   … halo_new / halo_call / halo_set_led / halo_run
//   - スレーブ(A 側): halo_link_slave_t … 受信したコマンドごとにハンドラを呼ぶ
// 純粋な C99。動的メモリ確保・グローバル状態なし。
#ifndef HALO_LINK_H
#define HALO_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "halo_link_port.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- 定数 -------------------------------------------------------------------------------

#define HALO_LINK_PROTO_VERSION 1

#define HALO_LINK_FRAME_LEN 16
#define HALO_LINK_PAYLOAD_LEN 10
#define HALO_LINK_SOF_REQ 0xA5 // 要求
#define HALO_LINK_SOF_RES 0x5A // 応答

#define HALO_LINK_BYTE_TIMEOUT_US 2000      // バイト間がこれ以上空いたら途中のフレームを捨てる
#define HALO_LINK_RESPONSE_TIMEOUT_US 20000 // マスターの応答待ち
#define HALO_LINK_MAX_TRIES 4               // 初回 + 再送 3 回
#define HALO_LINK_MIN_GAP_US 1000           // 前の通信からこれ以上空けて送る

typedef enum {
    HALO_CMD_PING = 0x01,
    HALO_CMD_HELLO = 0x02,
    HALO_CMD_GET_ANGLES = 0x03,
    HALO_CMD_GET_STATE = 0x04,
    HALO_CMD_APPLY_PRESET = 0x10,
    HALO_CMD_SET_BRIGHTNESS = 0x11,
    HALO_CMD_SET_LEDS = 0x20,
    HALO_CMD_FILL = 0x21,
    HALO_CMD_SHOW = 0x22,
} halo_cmd_t;

// APPLY_PRESET の特別な id
#define HALO_PRESET_OFF 0xFE  // 消灯
#define HALO_PRESET_AUTO 0xFF // 現在のデモ動作に戻す

// 応答の STATUS
typedef enum {
    HALO_STATUS_OK = 0x00,
    HALO_STATUS_BAD_CMD = 0x01,
    HALO_STATUS_BAD_ARG = 0x02,
    HALO_STATUS_BUSY = 0x03,
    HALO_STATUS_UNSUPPORTED = 0x04,
    HALO_STATUS_INTERNAL = 0x05,
} halo_status_t;

// GET_STATE の mode
typedef enum {
    HALO_MODE_AUTO = 0,
    HALO_MODE_PRESET = 1,
    HALO_MODE_DIRECT = 2,
    HALO_MODE_OFF = 3,
} halo_mode_t;

typedef struct {
    uint8_t r, g, b;
} halo_rgb_t;

// ---- フレーム -----------------------------------------------------------------------------

typedef struct {
    uint8_t sof;
    uint8_t seq;
    uint8_t cmd;
    uint8_t flags_status; // 要求: FLAGS、応答: STATUS
    uint8_t payload[HALO_LINK_PAYLOAD_LEN];
} halo_link_frame_t;

// CRC-16/CCITT-FALSE(多項式 0x1021、初期値 0xFFFF)
uint16_t halo_link_crc16(const uint8_t *data, size_t len);

// フレームを 16 バイトに組み立てる(CRC を付ける)。
void halo_link_frame_encode(const halo_link_frame_t *f, uint8_t out[HALO_LINK_FRAME_LEN]);
// 16 バイトを解析する。CRC が合わなければ false。SOF の種類は見ない。
bool halo_link_frame_decode(const uint8_t in[HALO_LINK_FRAME_LEN], halo_link_frame_t *out);

// 受信パーサ。バイトを 1 つずつ渡し、フレームが 1 つ揃ったら true を返す。
//   - 期待する SOF 以外のバイト(エコーなど)は、フレームの先頭では捨てる
//   - CRC が合わなければ、先頭の 1 バイトを捨てて次の SOF 候補から再走査する
//   - バイト間が HALO_LINK_BYTE_TIMEOUT_US 以上空いたら、途中のフレームを捨てる
typedef struct {
    uint8_t buf[HALO_LINK_FRAME_LEN];
    uint8_t len;
    uint8_t sof;
    uint32_t last_us;
} halo_link_parser_t;

void halo_link_parser_init(halo_link_parser_t *p, uint8_t expect_sof);
bool halo_link_parser_feed(halo_link_parser_t *p, uint8_t byte, uint32_t now_us,
                           halo_link_frame_t *out);

// ---- マスター(B 側) -----------------------------------------------------------------------
// 使い方:
//   halo_t halo;
//   halo_new(&halo, &port);              // 生成時に halo_call() を 1 回行う
//   if (!halo.res.ok) return;
//   uint16_t count = halo.res.status.led_count;
//   halo_set_led(&halo, 0, colors, 15);  // カスタム点灯を書く(表示は変わらない)
//   halo_run(&halo, 4);                  // カスタムを表示(0 = 消灯、1〜3 = 固定プリセット)
// 各 API は ok(bool)を返し、同じ結果を halo.res にも入れる。

typedef enum {
    HALO_ERR_NONE = 0,
    HALO_ERR_TIMEOUT,       // 再送しても有効な応答が来なかった
    HALO_ERR_DEVICE_STATUS, // A が OK 以外の STATUS を返した(device_status)
    HALO_ERR_BAD_ARG,       // 引数が範囲外(通信しない)
    HALO_ERR_NOT_READY,     // 接続できていない(通信しない)
    HALO_ERR_VERSION,       // A のプロトコル版が新しい
} halo_err_kind_t;

typedef struct {
    halo_err_kind_t kind;
    uint8_t device_status; // kind == HALO_ERR_DEVICE_STATUS のとき
    uint8_t index;         // set_led で、最初に失敗した通信の先頭の LED 番号
} halo_error_t;

typedef struct {
    uint16_t led_count;
    uint8_t preset_count;
    uint8_t proto_ver;
    uint8_t fw_version[3];
    uint8_t variant_id;
    uint8_t mode; // halo_mode_t
    uint8_t preset_id;
    uint8_t brightness;
    bool valid;      // 1 度でも halo_call() に成功した
    bool fresh;      // 最新の状態か(失敗すると false。次の call/run の成功で true)
    uint32_t age_ms; // 状態を最後に更新してから、直近の API 呼び出しの終わりまでの時間
} halo_state_t;

typedef struct {
    bool ok;
    halo_error_t error;
    halo_state_t status;
} halo_res_t;

typedef struct {
    halo_res_t res;
    // 以下は内部用
    halo_link_port_t port;
    halo_link_parser_t parser;
    uint8_t seq;
    bool has_last;
    uint32_t last_us;   // 最後に通信した時刻
    uint32_t status_us; // 状態を最後に更新した時刻
} halo_t;

// 初期化して halo_call() を 1 回行う。戻り値は ok。
bool halo_new(halo_t *h, const halo_link_port_t *port);
// 状態を更新する(HELLO + GET_STATE)。
bool halo_call(halo_t *h);
// LED を start から n 個、colors[] の色に設定する(A のステージングに書く)。表示は変わらない。
bool halo_set_led(halo_t *h, uint8_t start, const halo_rgb_t *colors, uint8_t n);
// 全 LED を同じ色にする(ステージングに書く)。
bool halo_set_fill(halo_t *h, halo_rgb_t color);
// 全体の明るさ(0〜255)。
bool halo_set_brightness(halo_t *h, uint8_t value);
// 点灯を実行する。0 = 消灯、1〜preset_count = 固定プリセット、preset_count + 1 = カスタム。
bool halo_run(halo_t *h, uint8_t n);
// 状態を最後に更新してからの経過時間[ms](halo.res.status.age_ms は API の終わりの値)。
uint32_t halo_status_age_ms(halo_t *h);

// ---- スレーブ(A 側) -----------------------------------------------------------------------

typedef struct {
    uint8_t fw_version[3];
    uint16_t led_count;
    uint8_t preset_count;
    uint8_t variant_id;
} halo_link_info_t;

typedef struct {
    uint8_t mode;
    uint8_t preset_id;
    uint8_t brightness;
} halo_link_state_t;

// 受信したコマンドに対応するハンドラ。NULL のものは UNSUPPORTED を返す。
// ハンドラは halo_status_t を返す。ここで時間のかかる処理をしない。
typedef struct {
    void *ctx;
    void (*get_info)(void *ctx, halo_link_info_t *out);
    void (*get_state)(void *ctx, halo_link_state_t *out);
    uint16_t (*get_angle)(void *ctx, uint8_t index);
    halo_status_t (*apply_preset)(void *ctx, uint8_t id); // id: HALO_PRESET_OFF / AUTO も含む
    halo_status_t (*set_brightness)(void *ctx, uint8_t value);
    // start から n 個(1〜3)。LED 個数を超える分は呼び出し前に切り詰め済み。
    halo_status_t (*set_leds)(void *ctx, uint8_t start, uint8_t n, const halo_rgb_t *rgb);
    halo_status_t (*fill)(void *ctx, halo_rgb_t rgb);
    halo_status_t (*show)(void *ctx);
} halo_link_slave_ops_t;

typedef struct {
    halo_link_port_t port;
    halo_link_slave_ops_t ops;
    halo_link_parser_t parser;
} halo_link_slave_t;

void halo_link_slave_init(halo_link_slave_t *s, const halo_link_port_t *port,
                          const halo_link_slave_ops_t *ops);
// 受信済みのバイトを処理し、要求ごとに応答を送る。メインループから頻繁に呼ぶ。
void halo_link_slave_poll(halo_link_slave_t *s);

#ifdef __cplusplus
}
#endif

#endif
