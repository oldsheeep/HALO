# halo-controller

HALO の B 側(左の基板)の CH32V003J4M6 のファームウェア。

- A 側(`halo-controller-ch32v003`)へ、単線半二重通信(HALO Link)でつなぐ**マスター**
- ESP32 / Pico W からの **I2C を、単線へ中継**する(ブリッジ。`halo-bridge`)
- **ボタン**(PC4): 単押し = 次のプリセット、長押し = 消灯 ⇔ 点灯、2 回押し = プリセット 1

**状態: ビルドと、ホストのテスト(ボタンの判定、操作 → A 側への命令)まで。実機での動作は未確認**。特に、CH32V003 の I2C スレーブ(`src/i2c_slave.c`)は、実機で確認していない。

仕様:
[HALO Link](../docs/halo-link-spec.md)(単線)、
[HALO Bridge](../docs/halo-bridge-spec.md)(I2C)、
[HALO Button](../docs/halo-button-spec.md)(ボタン)。

## ピン(CH32V003J4M6、SOP8)

| ピン | 機能 |
| --- | --- |
| pin 1 PD6 | HALO Link(USART1 リマップ 2 の TX、半二重、オープンドレイン)。**プルアップ(+5V へ 10kΩ)は、2 枚のどちらかに 1 つ**。回路は [halo-pcb-ch32v003](../halo-pcb-ch32v003/) |
| pin 5 PC1 | I2C の SDA(ESP32 / Pico W へ。プルアップは ESP32 / Pico W 側の 3.3V) |
| pin 6 PC2 | I2C の SCL(同上) |
| pin 3 PA2 | INT(オープンドレイン、アクティブ L。応答の準備ができたときと、ボタンの変化があったとき) |
| pin 7 PC4 | ボタン(入力、内部プルアップ。スイッチを GND との間に入れる。押すと L) |
| pin 8 PD1 | 書き込み(SWIO)。**PD4 / PD5 と同じピンなので、PD5 などを触らない** |

## ディレクトリ構成

```text
halo-controller/
├── Makefile             ch32fun を使うビルド定義
├── deps.txt             取り込む他ディレクトリ(halo-link、halo-bridge。CI の再ビルド対象になる)
├── scripts/
│   ├── setup.sh         ツールチェーンと ch32fun の取得(ローカル用。バージョン固定)
│   ├── setup-ci.sh      CI 用(setup.sh + PATH 追加)
│   ├── build.sh         ビルドして dist/ に出力
│   └── test.sh          ホストのテスト
├── src/
│   ├── halo_controller.c  main(ピンの設定、ブリッジ、ボタン、ESP32 / Pico W との共存)
│   ├── halo_button.[ch]   ボタンの判定(MCU に依存しない)
│   ├── halo_ui.[ch]       ボタンの操作 → A 側への命令(MCU に依存しない)
│   ├── i2c_slave.[ch]     CH32V003 の I2C1 スレーブ(割り込み)→ halo-bridge
│   ├── port.[ch]          halo_link_port_t の実装(USART1 半二重 + SysTick)
│   ├── halo_critical.h    halo-bridge の競合区間を、割り込みの禁止・許可で囲む
│   └── funconfig.h        ch32fun の設定(48MHz)
├── tests/                 ホストのテスト
├── ext/ch32fun/           (取得物。コミットしない)
├── .toolchain/            (取得物。コミットしない)
└── dist/                  (成果物。コミットしない)
```

## ふるまい

起動すると、A 側へ `halo_new` で接続を試す(A 側が無くても進む)。以降は、メインループで次を繰り返す。

1. `halo_bridge_slave_poll`: ESP32 / Pico W の要求を、単線へ中継する
2. ボタンを 1 ms ごとに読んで、判定する(20 ms のデバウンス、長押し 700 ms、2 回押しの間隔 300 ms)
3. ボタンの操作があったら、ブリッジの送信を止めて(`hold`)、`halo_call` で A 側の状態を読み直し、`halo_run` を実行して、再開する。結果は `post_event` で ESP32 / Pico W に知らせる

ボタンの操作と動作の詳細は、[仕様書](../docs/halo-button-spec.md)。時間は、ビルドで変えられる(`-DHALO_BUTTON_LONG_MS=…`、`-DHALO_BUTTON_DOUBLE_MS=…`、`-DHALO_BUTTON_DEBOUNCE_MS=…`)。I2C のアドレスは `-DHALO_I2C_ADDR=…`(既定 `0x32`)。

## ビルド

リポジトリルートから(取得物が無ければ自動で `setup.sh` が走る)。

```bash
bash halo-controller/scripts/build.sh
```

成果物は `halo-controller/dist/halo-controller.{elf,bin,hex,map}`。サイズ: FLASH 約 4.7 KB / 16 KB、RAM 約 330 B / 2 KB。

## テスト

```bash
bash halo-controller/scripts/test.sh
```

警告をエラーにし、AddressSanitizer / UBSan を有効にする。確認する内容:

- **ボタンの判定**: 単押し、2 回押し、長押し(2 回目が長押しの場合を含む)、窓を過ぎた 2 回目、3 回続けた押下、デバウンス、チャタリング、起動時に押されていた場合、時刻の一周
- **操作 → A 側への命令**: プリセットの循環、消灯と点灯(直前のプリセットに戻る)、消灯中の単押し、2 回押し、ESP32 / Pico W が先に変えた状態を読み直すこと、カスタム点灯からの動作、通信の失敗と回復

## 書き込み

WCH-LinkE を、SWIO(pin 8)と GND につないで(実機への書き込みは、事前に確認すること):

```bash
cd halo-controller && make PREFIX=riscv-none-elf flash
```

`PATH` に `.toolchain/bin` を通しておく(`scripts/build.sh` を一度実行すれば取得される)。
