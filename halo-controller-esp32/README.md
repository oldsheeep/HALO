# halo-controller-esp32

HALO の ESP32 側。B 側の CH32V003(`halo-controller`)に **I2C** でつなぎ、`halo-link` のマスター API で A 側を操作する。
ESP32 は、ジャックの通信線にはつながらない(B 側が、単線へ中継する)。

**状態: 構文の検査のみ。ESP-IDF でのビルドと、実機での動作は未確認。**

- 仕様: [docs/halo-bridge-spec.md](../docs/halo-bridge-spec.md)(I2C)、[docs/halo-link-spec.md](../docs/halo-link-spec.md)(`halo_t` の API)
- ライブラリ: [halo-link](../halo-link/)、[halo-bridge](../halo-bridge/)(マスター側のソースだけを使う)

## 配線

| ESP32 | 機能 | 備考 |
| --- | --- | --- |
| GPIO 21 | I2C の SDA | B 側の PC1(pin 5)へ。**4.7kΩ で 3.3V へプルアップ** |
| GPIO 22 | I2C の SCL | B 側の PC2(pin 6)へ。**4.7kΩ で 3.3V へプルアップ** |
| GPIO 4 | INT(入力) | B 側の PA2(pin 3、アクティブ L)へ。10kΩ で 3.3V へプルアップ |
| GND | GND | B 側の GND と**共通にする** |

- ピンは、`-DHALO_I2C_SDA_GPIO=…`、`-DHALO_I2C_SCL_GPIO=…`、`-DHALO_INT_GPIO=…` で変えられる(ESP32 の種類によっては、GPIO 21 / 22 が使えない)。
- I2C のプルアップは、ESP32 側の 3.3V に付ける。B 側(5V 駆動)には付けない(B 側は L に引くだけなので、ESP32 に 5V は出ない)。

## ビルド

ESP-IDF v5.2 以降(`i2c_master` の API を使う)。

```bash
cd halo-controller-esp32
idf.py set-target esp32      # 使う ESP32 の種類に合わせる
idf.py build flash monitor
```

ESP-IDF が無い環境での構文の検査:

```bash
bash halo-controller-esp32/scripts/check.sh
```

CI の `scripts/build.sh` は、まだ無い(CI に ESP-IDF の導入が要るため)。`halo-*/scripts/build.sh` が無いディレクトリは、CI のビルド対象にならない。

## ディレクトリ構成

```text
halo-controller-esp32/
├── CMakeLists.txt         ESP-IDF のプロジェクト
├── sdkconfig.defaults
├── main/
│   ├── main.c             デモ(接続 → 3 秒ごとにプリセットを順に実行 → INT でイベントを読む)
│   ├── i2c_port.[ch]      halo-bridge の I2C ポート(ESP-IDF の i2c_master)
│   └── CMakeLists.txt     halo-link / halo-bridge のソースを、相対パスで取り込む
├── scripts/check.sh       構文の検査
└── tests/stubs/           check.sh 用の、ESP-IDF の宣言の抜粋
```

## デモの動作

1. 起動時に `halo_new` で接続する(失敗したら 500 ms ごとに再試行)
2. 3 秒ごとに、固定プリセットを順に実行する(`halo_run`)。失敗したら `halo_call` で再接続を試す
3. INT 線が L になったら、STATUS の `EVENT` を確かめて、イベント(ボタンによる変化)を読み、`halo_call` で状態を読み直す

BLE(モバイル端末からの操作)は、この上に足す。
