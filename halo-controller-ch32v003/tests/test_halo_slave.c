// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// 表示の状態管理と、halo-link スレーブとのつなぎ(link_slave.c)のホストテスト。
// link_uart_*(USART1)は模擬に置き換える。要求フレームを流し込み、応答と表示の状態を確かめる。
#include <stdio.h>
#include <string.h>

#include "halo_display.h"
#include "halo_link.h"
#include "link_slave.h"

static int failures;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                    \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

// ---- 模擬ポート ----
static uint8_t rx[256], tx[256];
static size_t rx_len, rx_pos, tx_len;
static uint32_t clock_us;

void link_uart_init(void) {}
size_t link_uart_write(void *ctx, const uint8_t *buf, size_t len) {
    (void)ctx;
    memcpy(tx + tx_len, buf, len);
    tx_len += len;
    return len;
}
size_t link_uart_read(void *ctx, uint8_t *buf, size_t max) {
    (void)ctx;
    size_t n = rx_len - rx_pos < max ? rx_len - rx_pos : max;
    memcpy(buf, rx + rx_pos, n);
    rx_pos += n;
    return n;
}
uint32_t link_uart_now_us(void *ctx) {
    (void)ctx;
    return clock_us += 100;
}

// 要求を 1 つ送り、応答フレームを返す(応答が無ければ false)。
static uint8_t seq;
static bool call(uint8_t cmd, uint8_t flags, const uint8_t *payload, size_t plen,
                 halo_link_frame_t *res) {
    halo_link_frame_t req;
    memset(&req, 0, sizeof req);
    req.sof = HALO_LINK_SOF_REQ;
    req.seq = ++seq;
    req.cmd = cmd;
    req.flags_status = flags;
    if (plen)
        memcpy(req.payload, payload, plen);
    rx_len = rx_pos = tx_len = 0;
    halo_link_frame_encode(&req, rx);
    rx_len = HALO_LINK_FRAME_LEN;
    link_slave_poll();
    if (tx_len != HALO_LINK_FRAME_LEN)
        return false;
    CHECK(halo_link_frame_decode(tx, res));
    CHECK(res->sof == HALO_LINK_SOF_RES && res->seq == seq && res->cmd == (cmd | 0x80));
    return true;
}

static uint8_t state_mode(void) {
    uint8_t m, p, b;
    halo_display_get(&m, &p, &b);
    return m;
}

// halo_config.h(halo_preset.h)と halo_link.h は halo_rgb_t が衝突するので、variant の値は
// halo_display の関数で読む。
#define LED_COUNT halo_display_led_count()
#define PRESET_COUNT halo_display_preset_count()

int main(void) {
    halo_display_init();
    link_slave_init();
    halo_link_frame_t r;
    uint8_t grb[256 * 3], ref[256 * 3];
    const size_t glen = (size_t)LED_COUNT * 3;

    // 起動時は AUTO。通信が無くても動く(render は単体で呼べる)
    CHECK(state_mode() == HALO_DISPLAY_AUTO);
    halo_display_render(ref, 0);

    // HELLO: LED 個数・プリセット数は variant の値
    CHECK(call(HALO_CMD_HELLO, 0, NULL, 0, &r));
    CHECK(r.flags_status == HALO_STATUS_OK && r.payload[0] == HALO_LINK_PROTO_VERSION);
    CHECK((r.payload[4] | (r.payload[5] << 8)) == LED_COUNT);
    CHECK(r.payload[6] == PRESET_COUNT);

    // GET_ANGLES: 偏角は variant の値
    uint8_t ang[2] = {0, 2};
    CHECK(call(HALO_CMD_GET_ANGLES, 0, ang, 2, &r) && r.flags_status == HALO_STATUS_OK);
    CHECK((r.payload[2] | (r.payload[3] << 8)) == halo_display_led_angle(0));
    CHECK((r.payload[4] | (r.payload[5] << 8)) == halo_display_led_angle(1));

    // APPLY_PRESET: 範囲外は BAD_ARG で状態は変わらない
    uint8_t id = PRESET_COUNT;
    CHECK(call(HALO_CMD_APPLY_PRESET, 0, &id, 1, &r) && r.flags_status == HALO_STATUS_BAD_ARG);
    CHECK(state_mode() == HALO_DISPLAY_AUTO);
    // PRESET
    id = 1;
    CHECK(call(HALO_CMD_APPLY_PRESET, 0, &id, 1, &r) && r.flags_status == HALO_STATUS_OK);
    CHECK(r.payload[0] == HALO_MODE_PRESET && r.payload[1] == 1 && r.payload[2] == 255);
    halo_display_render(grb, 0);
    // 消灯
    id = HALO_PRESET_OFF;
    CHECK(call(HALO_CMD_APPLY_PRESET, 0, &id, 1, &r) && r.payload[0] == HALO_MODE_OFF);
    halo_display_render(grb, 0);
    for (size_t i = 0; i < glen; i++)
        CHECK(grb[i] == 0);
    // AUTO に戻る(AUTO の描画は最初と同じ)
    id = HALO_PRESET_AUTO;
    CHECK(call(HALO_CMD_APPLY_PRESET, 0, &id, 1, &r) && r.payload[0] == HALO_MODE_AUTO);
    halo_display_render(grb, 0);
    CHECK(memcmp(grb, ref, glen) == 0);

    // 直接制御: SET_LEDS / FILL はステージングだけで、SHOW まで表示は変わらない
    uint8_t leds[10] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
    CHECK(call(HALO_CMD_SET_LEDS, 3, leds, 10, &r) && r.flags_status == HALO_STATUS_OK);
    CHECK(state_mode() == HALO_DISPLAY_AUTO);
    CHECK(call(HALO_CMD_SHOW, 0, NULL, 0, &r) && r.payload[0] == HALO_MODE_DIRECT);
    halo_display_render(grb, 0);
    CHECK(grb[0] == 20 && grb[1] == 10 && grb[2] == 30); // GRB 順: LED0 = (10,20,30)
    CHECK(grb[3] == 50 && grb[4] == 40 && grb[5] == 60);
    CHECK(grb[9] == 0 && grb[10] == 0 && grb[11] == 0); // LED3 は未設定

    // SHOW 後のステージング更新は、次の SHOW まで表示に出ない
    uint8_t fill[3] = {200, 100, 50};
    CHECK(call(HALO_CMD_FILL, 0, fill, 3, &r) && r.flags_status == HALO_STATUS_OK);
    halo_display_render(grb, 0);
    CHECK(grb[0] == 20 && grb[1] == 10);
    CHECK(call(HALO_CMD_SHOW, 0, NULL, 0, &r));
    halo_display_render(grb, 0);
    for (int i = 0; i < LED_COUNT; i++)
        CHECK(grb[i * 3] == 100 && grb[i * 3 + 1] == 200 && grb[i * 3 + 2] == 50);

    // 全体の明るさ: 半分。0 で全消灯。255 に戻すと元の値
    uint8_t b = 128;
    CHECK(call(HALO_CMD_SET_BRIGHTNESS, 0, &b, 1, &r) && r.flags_status == HALO_STATUS_OK);
    halo_display_render(grb, 0);
    CHECK(grb[0] == 100 * 128 / 255 && grb[1] == 200 * 128 / 255);
    b = 0;
    CHECK(call(HALO_CMD_SET_BRIGHTNESS, 0, &b, 1, &r));
    halo_display_render(grb, 0);
    for (size_t i = 0; i < glen; i++)
        CHECK(grb[i] == 0);
    b = 255;
    CHECK(call(HALO_CMD_SET_BRIGHTNESS, 0, &b, 1, &r));
    CHECK(call(HALO_CMD_GET_STATE, 0, NULL, 0, &r) && r.payload[2] == 255);

    // 範囲外の LED は BAD_ARG(状態は変わらない)
    uint8_t bad[10] = {LED_COUNT};
    CHECK(call(HALO_CMD_SET_LEDS, 3, bad, 10, &r) && r.flags_status == HALO_STATUS_BAD_ARG);

    // 末尾の端数: 個数を超える分は切り詰め、隣の LED を壊さない
    uint8_t tail[10] = {LED_COUNT - 1, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    CHECK(call(HALO_CMD_SET_LEDS, 3, tail, 10, &r) && r.flags_status == HALO_STATUS_OK);

    // 予約コマンドは UNSUPPORTED
    CHECK(call(0x40, 0, NULL, 0, &r) && r.flags_status == HALO_STATUS_UNSUPPORTED);

    // AUTO は時間で固定プリセットを順に切り替える
    halo_display_apply_preset(HALO_DISPLAY_ID_AUTO);
    halo_display_render(ref, 0);
    halo_display_render(grb, HALO_DISPLAY_AUTO_SHOW_MS * PRESET_COUNT); // 一周して同じ
    CHECK(memcmp(grb, ref, glen) == 0);

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
