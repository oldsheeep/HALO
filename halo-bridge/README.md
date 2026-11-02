# halo-bridge

ESP32 / Raspberry Pi Pico W(I2C マスター)と、B 側の CH32V003(I2C スレーブ)をつなぐライブラリ。
仕様は [docs/halo-bridge-spec.md](../docs/halo-bridge-spec.md)。

```text
ESP32 / Pico W ── I2C ── B 側 CH32V003 ── 単線 (HALO Link) ── A 側 CH32V003
 (halo-link の halo_t)    (halo-bridge のスレーブ)
```

- ESP32 / Pico W は、`halo-link` の `halo_new`・`halo_run`・`halo_set_led` などを、**ポートだけ I2C に差し替えて**そのまま使える。ESP32 / Pico W は、ジャックの通信線につながらない。
- B 側は、要求フレーム(HALO Link と同じ 16 バイト)の SOF と CRC を検査して、単線へ流し、A 側の応答を返す。ボタンなどの B 側のローカルな変化は、イベントと INT 線で知らせる。
- 純粋な C99。動的メモリ確保・グローバル状態なし。MCU 依存は、I2C のポート(マスター側)と、I2C のイベントの接続(スレーブ側)に切り出してある。
- [halo-link](../halo-link/) に依存する(`halo_link.h` と、スレーブ側が使うフレーム・パーサ)。ビルド対象ではない(`scripts/build.sh` が無いので、release には載らない)。

**状態: ホストのテストのみ。実機(CH32V003 の I2C スレーブ、ESP32、Pico W)では未確認。**

## 構成

```text
halo-bridge/
├── include/halo_bridge.h        公開 API(レジスタ、マスター、スレーブ)
├── src/
│   ├── halo_bridge_master.c     ESP32 / Pico W 側(I2C から halo-link のポートを作る)
│   └── halo_bridge_slave.c      B 側(I2C のレジスタ → 単線の中継)
├── tests/test_halo_bridge.c     ホストのテスト(模擬の I2C と単線)
└── scripts/test.sh
```

使う側は、`include/` と `../halo-link/include` をインクルードパスに加える。マスターだけなら `halo_bridge_master.c` と、`halo-link` のマスター(`halo_link_frame.c`・`halo_link_master.c`)をリンクする。スレーブ(B 側)は、`halo_bridge_slave.c` と、`halo_link_frame.c` をリンクする。

## マスター(ESP32 / Pico W)

I2C のポートを、使っているプラットフォームの関数で与える(`halo_bridge_i2c_t`)。

```c
// 例: ESP-IDF(i2c_master)。write / write_read は、成功したら 0 を返す
static int my_write(void *ctx, uint8_t addr, const uint8_t *buf, size_t len);
static int my_write_read(void *ctx, uint8_t addr, const uint8_t *w, size_t wl, uint8_t *r, size_t rl);
static uint32_t my_now_us(void *ctx);   // esp_timer_get_time() など
static void my_yield(void *ctx);        // vTaskDelay(1) など(任意)

halo_bridge_i2c_t i2c = {my_write, my_write_read, my_now_us, my_yield, &my_bus};
halo_bridge_master_t bridge;
halo_bridge_master_init(&bridge, &i2c, HALO_BRIDGE_DEFAULT_ADDR);

halo_link_port_t port = halo_bridge_master_port(&bridge);
halo_t halo;
halo_new(&halo, &port);      // 以降は halo-link のとおり
if (!halo.res.ok) return;
halo_run(&halo, 2);          // 固定プリセットの 2 番目
```

- `halo_bridge_read_status`・`halo_bridge_read_event` で、B 側の状態とイベント(ボタンによる変化)を読める。INT 線(アクティブ L)を割り込みに使ってもよい。
- I2C のプルアップは、**ESP32 / Pico W 側の 3.3V へ**(4.7kΩ)。B 側(5V 駆動)には付けない。GND は共通にする。

## スレーブ(B 側の CH32V003)

```c
halo_bridge_slave_t bridge;
halo_bridge_slave_init(&bridge, &link_port, int_pin_set, NULL);  // link_port: A 側とつながる単線のポート

// I2C のスレーブの割り込みから(ハードウェアに依存する部分は halo-controller に置く)
//   STOP のとき:   halo_bridge_slave_on_write(&bridge, rx_bytes, rx_len);
//   読みの開始:    n = halo_bridge_slave_on_read(&bridge, tx_buf, sizeof tx_buf);

for (;;) {
    halo_bridge_slave_poll(&bridge);   // メインループで頻繁に呼ぶ
}
```

- ボタンなどの B 側のローカルな処理が、単線を使うとき: `halo_bridge_slave_hold(&bridge, true)` → `halo_bridge_slave_link_busy()` が false になるのを待つ → 単線を使う → `halo_bridge_slave_hold(&bridge, false)`。結果は、`halo_bridge_slave_post_event()` で ESP32 / Pico W に知らせる。

## テスト

```bash
bash halo-bridge/scripts/test.sh
```

警告をエラーにし、AddressSanitizer / UndefinedBehaviorSanitizer を有効にしてビルドする。模擬の I2C と単線で、**ESP32 / Pico W の `halo_t` から、ブリッジ、A 側まで**をつなぎ、次を確認する。

- レジスタ(INFO、STATUS、未定義のレジスタ、読み出し専用への書き込み)
- 通常の流れ(`halo_new` → `halo_run` → `halo_set_led` → `halo_run(4)`)
- A 側の応答が来ないときの TIMEOUT と、そこからの回復
- `BUSY` 中の要求の拒否、不正なフレーム(CRC、SOF、長さ)を単線へ流さないこと、単線に残った古い応答の破棄
- EVENT と INT 線、`hold`、ブリッジ自身の 15 ms のタイムアウト
