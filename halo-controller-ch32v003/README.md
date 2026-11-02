# halo-controller-ch32v003

CH32V003 で SK6812mini-e(アドレサブル LED)を制御するファームウェア。
HALO の種類(variant)ごとに変わる固定情報(LED の個数と偏角、固定プリセット)をビルド時に選べる。

現状は下地のみ: SK6812 ドライバと、固定プリセット 3 パターンを順に表示する点灯デモ。

## ツールチェーン選定

| 項目 | 選定 | 理由 |
| --- | --- | --- |
| フレームワーク | [ch32fun](https://github.com/cnlohr/ch32fun) | WCH 公式 SDK(MounRiver)を使わずオープンなツールだけで完結する。CH32V003 に最適化されており、ヘッダのみの軽量構成でフラッシュ 16KB に収まりやすい。`minichlink` で WCH-LinkE から書き込める。WS2812 系のビットバング例がある |
| コンパイラ | xPack `riscv-none-elf-gcc` 15.2.0-1 | CH32V003 は RV32EC(`-march=rv32ec -mabi=ilp32e`)。xPack は macOS(arm64/x64)・Linux 向けに同一バージョンの配布物があり、SHA256 で検証できる。ch32fun が対応するプレフィックスの一つ |
| SK6812 ドライバ | 自前(`src/sk6812.c`) | ch32fun の `ws2812b_simple.h` は WS2812B 向けのタイミングで、SK6812mini-e の許容に合わせて 48MHz のクロック数で持ちたい。SysTick(HCLK)で各ビットの時間を計る |

取得するバージョンは [scripts/setup.sh](scripts/setup.sh) の先頭で固定している(ch32fun はコミット、xPack はバージョン+SHA256)。

## ディレクトリ構成

```text
halo-controller-ch32v003/
├── Makefile            ch32fun を使うビルド定義(make VARIANT=<name> build)
├── scripts/
│   ├── setup.sh        ツールチェーンと ch32fun の取得(ローカル用)
│   ├── setup-ci.sh     CI 用(setup.sh + PATH 追加)
│   └── build.sh        全 variant をビルドして dist/ に出力
├── src/
│   ├── halo.c          点灯デモの main
│   ├── sk6812.[ch]     SK6812mini-e ドライバ
│   ├── halo_preset.h   プリセットのデータ形式(暫定)とレンダリング
│   ├── halo_config.h   variant の取り込みとビルド時検査
│   └── funconfig.h     ch32fun の設定(48MHz)
├── variants/           HALO の種類ごとの固定情報
│   ├── ring12.h        12 個等間隔のリング
│   ├── ring15.h        15 個等間隔のリング(単色の薄い青、薄い青↔青の回転)
│   └── ring8_ccw.h     8 個・反時計回り配線のリング
├── ext/ch32fun/        (取得物。コミットしない)
├── .toolchain/         (取得物。コミットしない)
└── dist/               (成果物。コミットしない)
```

## ビルド

リポジトリルートから(必要な取得物がなければ自動で `setup.sh` が走る)。

```bash
bash halo-controller-ch32v003/scripts/build.sh          # 全 variant
bash halo-controller-ch32v003/scripts/build.sh ring12   # 1 つだけ
```

成果物は `dist/halo-controller-ch32v003-<variant>.{elf,bin,hex,map}`。コミットしない。
取得だけ先に行う場合は `bash halo-controller-ch32v003/scripts/setup.sh`。

CI では `scripts/setup-ci.sh` → `scripts/build.sh` の順で呼ばれ、全 variant が `dist/` に出る。

## 書き込み(WCH-LinkE)

WCH-LinkE を CH32V003 の SWIO(PD1)・GND・電源に接続し、ch32fun の `minichlink` で書き込む。

```bash
cd halo-controller-ch32v003
make VARIANT=ring12 flash
```

`minichlink` は初回に ch32fun 内でビルドされる(macOS では `libusb` が必要: `brew install libusb`)。
実機への書き込みは、プロトコルやハードウェアへの影響を確認してから行うこと。

## 配線(データ線)

既定のデータ出力は **PC6**(`SK6812_PORT` / `SK6812_PIN`、`-D` で変更可)。
SK6812mini-e は 5V 駆動で入力のハイ閾値が 0.7×VDD(3.5V)のため、3.3V 出力の CH32V003 からは
レベルシフト、または LED 側を 3.3〜3.5V 付近で動かす等の対策が必要になることがある。

## 固定情報(variant)

variant は HALO の種類ごとに変わる、**ファームウェアに埋め込む固定情報**。

| 項目 | 内容 |
| --- | --- |
| `HALO_VARIANT_NAME` | variant 名(文字列) |
| `HALO_LED_COUNT` | SK6812 の個数 |
| `HALO_LED_ANGLES_DEG` | (任意) HALO の基準からの偏角[deg]を、LED が数珠つなぎになっている順に並べたカンマ区切りリスト。例 `0, 30, 60, ..., 330`。**省略すると `HALO_LED_COUNT` で等分した等間隔**(`i * 360 / 個数`)になる。等間隔でない・逆向きの配線のときだけ書く |
| `halo_fixed_presets[3]` | 固定プリセット 3 パターン |

`HALO_LED_COUNT` と偏角リストの要素数が違うと、`halo_config.h` の `_Static_assert` でビルドエラーになる。
固定プリセットが 3 つでない場合も同様。

偏角は一周を等速度でグラデーションさせるために使う。各 LED の色は、その LED の偏角に応じて
プリセットの色環から決まる(`halo_preset_color_at`)。

### 固定プリセットは変更できない

固定プリセットは `const` データとしてファームウェア本体(フラッシュのコード領域)に埋め込まれる。
実行時に書き換える経路は無く、書き換え可能プリセットの書き込み(別途 `docs/` で検討中)の対象外。
将来の書き換え可能プリセットはフラッシュの別領域に置き、書き込みで固定プリセットが消えない・
変わらない構成にする。

### プリセットのデータ形式(暫定)

`halo_preset_t`(16 バイト固定、リトルエンディアン)。書き込みプロトコルの形式が決まったら合わせる。

| オフセット | サイズ | 名前 | 内容 |
| --- | --- | --- | --- |
| 0 | 1 | `brightness` | 全体の明るさ 0..255 |
| 1 | 1 | `color_count` | 使う色数 1..4 |
| 2 | 2 | `period_ms` | 一周(360°)する時間[ms]。0 で静止 |
| 4 | 12 | `colors[4]` | RGB 各 1 バイト x 4。`color_count` 以降は未使用(0 埋め) |

色は色環(360°)に等間隔で並べ、隣同士を線形補間する(最後の色は最初の色へ戻る)。
`period_ms` が 0 でなければ、模様が偏角の増える向きへ等速度で回転する。

### プリセットの書き方(マクロ)

プリセットは構造体を直接書かず、`src/halo_preset.h` のマクロで書く。
すべてコンパイル時に計算される定数式なので、関数呼び出しや計算は実行時に残らない
(計算済みの値が `const` データとしてフラッシュに入る)。

| マクロ | 意味 |
| --- | --- |
| `HALO_RGB(r, g, b)` | 色。各 0〜255 |
| `HALO_RGBA(r, g, b, a)` | アルファ付き。`a` は 0〜255(255 = 100%)。色にあらかじめ掛けて暗くする |
| `HALO_HEX(0xRRGGBB)` | 16 進の色 |
| `HALO_HEXA(0xRRGGBBAA)` | 16 進のアルファ付きの色。例 `HALO_HEXA(0x4444BB88)` |
| `HALO_PRESET(period_ms, 色, 色, ...)` | プリセット。色は 1〜4 個。`period_ms` は一周(360°)回る時間で、`0` なら静止。全体の明るさは 100% |
| `HALO_PRESET_B(brightness, period_ms, 色, ...)` | 全体の明るさ(0〜255)も指定する |

```c
static const halo_preset_t halo_fixed_presets[] = {
    // 単色の薄い青 #4444BB88(静止)
    HALO_PRESET(0, HALO_HEXA(0x4444BB88)),
    // 薄い青 → 青 → 薄い青 が 6 秒で一周
    HALO_PRESET(6000, HALO_HEXA(0x4444BB88), HALO_HEX(0x0000FF)),
    // 暖色の静止(全体の明るさ 25%)
    HALO_PRESET_B(64, 0, HALO_HEX(0xFFB060)),
};
```

- 色は円周に等間隔で並び、最後の色が最初の色へ戻る。2 色なら「A → B → A」が一周になる。
- 色の個数が 0 個または 5 個以上だと、コンパイルエラーになる(`array is too large` と出る)。
- 100% の白は、LED 1 個あたり最大約 60mA。個数が多い HALO では全体の明るさ(`HALO_PRESET_B`)や電源容量に注意。

### サンプル

#### 等間隔の 15 個(`variants/ring15.h` そのまま)

偏角を省略すると、`HALO_LED_COUNT` で等分(15 個なら 24° 間隔)になる。

```c
#define HALO_VARIANT_NAME "ring15"
#define HALO_LED_COUNT 15

static const halo_preset_t halo_fixed_presets[] = {
    // 0: 単色の薄い青 #4444BB88(静止)
    HALO_PRESET(0, HALO_HEXA(0x4444BB88)),
    // 1: 薄い青 → 青 → 薄い青 が 6 秒で一周
    HALO_PRESET(6000, HALO_HEXA(0x4444BB88), HALO_HEX(0x0000FF)),
    // 2: 暖色の静止(全体の明るさ 25%)
    HALO_PRESET_B(64, 0, HALO_HEX(0xFFB060)),
};
```

#### 偏角を明示する(8 個・反時計回り配線、`variants/ring8_ccw.h`)

等間隔でない、または配線の向きが逆のときは、LED の接続順に偏角を並べる。個数と一致しないとビルドエラーになる。

```c
#define HALO_VARIANT_NAME "ring8_ccw"
#define HALO_LED_COUNT 8
#define HALO_LED_ANGLES_DEG 0, 315, 270, 225, 180, 135, 90, 45

static const halo_preset_t halo_fixed_presets[] = {
    // 虹色が 4 秒で一周(全体の明るさ 25%)
    HALO_PRESET_B(64, 4000, HALO_RGB(255, 0, 0), HALO_RGB(255, 255, 0), HALO_RGB(0, 255, 0),
                  HALO_RGB(0, 0, 255)),
    HALO_PRESET_B(64, 0, HALO_RGB(255, 120, 20)),
    HALO_PRESET_B(48, 8000, HALO_RGB(255, 0, 255), HALO_RGB(0, 255, 255)),
};
```

#### 色の書き方の対応

同じ色を別の書き方で書いた例。アルファは、色にあらかじめ掛けて暗くする。

```c
HALO_RGB(68, 68, 187)         // (68, 68, 187)
HALO_HEX(0x4444BB)            // 同上
HALO_RGBA(68, 68, 187, 136)   // アルファ 136(0x88、約 53%)を掛けて (36, 36, 100)
HALO_HEXA(0x4444BB88)         // 同上
```

#### 使い方(ビルドから確認まで)

```bash
# 1. variants/ring15.h を編集する(新しい variant なら作る)
# 2. ビルド(個数・色の個数の誤りはここでエラーになる)
bash halo-controller-ch32v003/scripts/build.sh ring15
# 3. 書き込み(WCH-LinkE 接続時)
cd halo-controller-ch32v003 && make VARIANT=ring15 flash
```

### variant の追加方法

1. `variants/<name>.h` を作る(`ring12.h` をコピーして編集)。
2. `HALO_VARIANT_NAME`・`HALO_LED_COUNT`・`halo_fixed_presets`(上のマクロで 3 つ)を定義する。等間隔でなければ `HALO_LED_ANGLES_DEG` も定義する。
3. `bash halo-controller-ch32v003/scripts/build.sh <name>` でビルドして確認する。
   個数と偏角の不一致はここでエラーになる。

`build.sh` は引数なしで `variants/*.h` をすべてビルドするので、追加した variant は CI でもビルドされる。
