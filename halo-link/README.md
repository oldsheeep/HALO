# halo-link

HALO の単線半二重通信(HALO Link)のライブラリ。マスター(B 側)とスレーブ(A 側 = CH32V003 の HALO ファームウェア)で共有する。
仕様は [docs/halo-link-spec.md](../docs/halo-link-spec.md)。

- 純粋な C99。動的メモリ確保なし、グローバル状態なし。MCU 依存の部分は「ポート」に切り出してある
- フレーム(16 バイト固定長)・CRC・受信パーサ・マスター・スレーブを含む
- ビルド対象ではない(`scripts/build.sh` が無いので、release には載らない)。使う側のディレクトリがソースを取り込む

## 構成

```text
halo-link/
├── include/
│   ├── halo_link.h        公開 API
│   └── halo_link_port.h   MCU 依存のポート
├── src/
│   ├── halo_link_frame.c  組立・解析・CRC・受信パーサ
│   ├── halo_link_master.c マスター(halo_t)
│   └── halo_link_slave.c  スレーブ
├── tests/test_halo_link.c ホストでのテスト(模擬バス)
└── scripts/test.sh
```

使う側は、`include/` をインクルードパスに加え、`src/*.c` をビルドに加える(マスターだけなら `halo_link_slave.c` は不要、スレーブだけなら `halo_link_master.c` は不要)。

## ポートの実装

UART への接続は、使う側が次の関数で与える(`halo_link_port.h`)。

```c
static size_t my_write(void *ctx, const uint8_t *buf, size_t len);  // 送信
static size_t my_read(void *ctx, uint8_t *buf, size_t max);         // 受信済みを取り出す(待たない)
static uint32_t my_now_us(void *ctx);                               // 単調増加の時刻[µs]
static void my_yield(void *ctx);                                    // 待ち時間に呼ばれる(任意。NULL 可)

halo_link_port_t port = {my_write, my_read, my_now_us, my_yield, &my_uart};
```

- 単線なので、自分の送信が `read` にエコーとして返ってくる。ライブラリが除去するので、ポートでは何もしなくてよい。
- 回路と UART の設定(115200 bps、8N1)は仕様書の 3 節を参照。

## マスター(B 側)

```c
halo_t halo;
halo_new(&halo, &port);                    // 生成時に halo_call() を 1 回行う
if (!halo.res.ok) return;                  // 直近の API の成否。API を呼ぶたびに更新される

uint16_t count  = halo.res.status.led_count;     // LED の個数
uint8_t  preset = halo.res.status.preset_count;  // 固定プリセットの数

halo_rgb_t colors[15] = { /* ... */ };
halo_set_led(&halo, 0, colors, 15);        // カスタム点灯を書く(表示はまだ変わらない)
halo_run(&halo, preset + 1);               // カスタムを表示(3 つなら 4)
halo_run(&halo, 2);                        // 固定プリセットの 2 番目
halo_run(&halo, 0);                        // 消灯

halo_call(&halo);                          // 状態を更新する
```

| 関数 | 内容 |
| --- | --- |
| `halo_new` / `halo_call` | 接続確認と状態の取得(`HELLO` + `GET_STATE`) |
| `halo_set_led(h, start, colors, n)` | LED を `start` から `n` 個設定する。3 個ずつの分割はライブラリが行う |
| `halo_set_fill(h, color)` | 全 LED を同じ色にする |
| `halo_set_brightness(h, value)` | 全体の明るさ(0〜255。保存されない) |
| `halo_run(h, n)` | `0` = 消灯、`1〜preset_count` = 固定プリセット、`preset_count + 1` = カスタム |

- 各 API は `ok` を返し、同じ結果を `halo.res` に入れる。`res.error.kind` に失敗の理由、`res.status` に A の状態のキャッシュが入る(失敗すると `res.status.fresh` が `false` になる)。詳しくは仕様書の 7.0.1。
- `halo_set_led` は最初の失敗で中断し、`res.error.index` に失敗した位置を入れる。やり直すときは最初から書き直してから `halo_run` を呼ぶ。
- 1 つの `halo_t` は単一のスレッド(またはメインループ)から使う。
- LED の番号は 8 ビット(255 まで)。

## スレーブ(A 側)

```c
halo_link_slave_ops_t ops = {
    .ctx = &my_device,
    .get_info = my_get_info,         // LED 個数、固定プリセット数、ファーム版
    .get_state = my_get_state,       // mode, preset_id, brightness
    .get_angle = my_get_angle,       // LED の偏角
    .apply_preset = my_apply_preset, // id: 0〜、HALO_PRESET_OFF、HALO_PRESET_AUTO
    .set_brightness = my_set_brightness,
    .set_leds = my_set_leds,         // ステージングに書く
    .fill = my_fill,
    .show = my_show,                 // ステージングを表示に反映する
};
halo_link_slave_t slave;
halo_link_slave_init(&slave, &port, &ops);

for (;;) {
    halo_link_slave_poll(&slave);  // 受信済みを処理して応答する。頻繁に呼ぶ
    /* LED の更新など */
}
```

- ハンドラは `halo_status_t` を返す。時間のかかる処理をしない。
- 割り込みを止める処理(SK6812 の送信など)の間に受信が欠けないよう、`read` は DMA の循環バッファなどから取り出す(仕様書の 4.3)。
- 予約したコマンド(`0x40`〜`0x4F`)には `UNSUPPORTED` を返す。

## テスト

```bash
bash halo-link/scripts/test.sh
```

警告をエラーにし、AddressSanitizer / UndefinedBehaviorSanitizer を有効にしてビルドする。
模擬バス(単線・エコーあり)でマスターとスレーブをつなぎ、次を確認する。

- CRC(既知のベクタ)、フレームの組立・解析、パーサの再走査・タイムアウト
- 通常の流れ(`new` → `run` → `set_led` → `run(4)`)と、端数の LED を壊さないこと
- 範囲外の引数(通信しない)、接続できていない状態
- タイムアウト(再送 4 回)、失敗時に値が残って `fresh` だけが `false` になること
- `set_led` の途中での失敗(失敗位置)、A の STATUS エラー、壊れた応答からの回復、プロトコル版の不一致
- スレーブ単体(予約コマンド、未知のコマンド、引数の検査)
