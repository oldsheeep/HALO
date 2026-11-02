# halo-controller

HALO の B 側(マスター)の CH32V003 版。A 側(`halo-controller-ch32v003`)へ単線半二重通信(HALO Link)で接続し、
`halo-link` のマスター API(`halo_new` / `halo_call` / `halo_set_led` / `halo_run`)で点灯を操作する。

現状は疎通確認のデモ。**実機・A 側が無い環境でビルドまで確認したもので、実機での疎通は未確認。**

- 通信仕様・回路: [docs/halo-link-spec.md](../docs/halo-link-spec.md)(3 節: ダイオード方式の単線回路、115200 bps 8N1)
- ライブラリ: [halo-link](../halo-link/README.md)。このディレクトリはソースを `halo-link` だけから取り込む(`include/` と `halo_link_frame.c` / `halo_link_master.c`)。A 側のディレクトリには依存しない

## 配線

| ピン | 機能 |
| --- | --- |
| PD5 | USART1 TX(ダイオードのカソードへ) |
| PD6 | USART1 RX(信号線へ直接) |

信号線は外付けのプルアップで H にする。回路図・部品は仕様書の 3.1 を参照(U5: 実機での L レベル・立ち上がりは未確認)。
自分の送信は RX にエコーされるが、`halo-link` が除去する。ピンはデータシートでの確認が必要(仕様書 U の未決事項)。

## ディレクトリ構成

```text
halo-controller/
├── Makefile           ch32fun を使うビルド定義
├── scripts/
│   ├── setup.sh       ツールチェーンと ch32fun の取得(ローカル用。バージョン固定)
│   ├── setup-ci.sh    CI 用(setup.sh + PATH 追加)
│   └── build.sh       ビルドして dist/ に出力
├── src/
│   ├── halo_controller.c  デモの main
│   ├── port.[ch]          halo_link_port_t の実装(USART1 + SysTick)
│   └── funconfig.h        ch32fun の設定(48MHz)
├── ext/ch32fun/       (取得物。コミットしない)
├── .toolchain/        (取得物。コミットしない)
└── dist/              (成果物。コミットしない)
```

ポートの実装: 受信は割り込みなしのポーリングで、write 中も 1 バイトごとに吸い出してリングバッファ(64 バイト)へ入れる。
`now_us` は SysTick(48MHz)の差分を µs に積算して作る(89 秒より短い間隔で呼ばれる前提。通信のたびに追いつく)。

## ビルド

リポジトリルートから(取得物が無ければ自動で `setup.sh` が走る)。

```bash
bash halo-controller/scripts/build.sh
```

成果物は `halo-controller/dist/halo-controller.{elf,bin,hex,map}`。サイズ: FLASH 約 3.3 KB / 16 KB、RAM 約 312 B / 2 KB。

## 書き込み

WCH-LinkE を接続して(実機への書き込みは事前に確認すること):

```bash
cd halo-controller && make PREFIX=riscv-none-elf flash
```

`PATH` に `.toolchain/bin` を通しておく(`scripts/build.sh` を一度実行すれば取得される)。

## デモの動作

1. 起動時に `halo_new`。失敗したら 500 ms ごとに `halo_call` で再試行する
2. 接続できたら `res.status` の LED 個数・固定プリセット数を使い、次を繰り返す
    1. `halo_run(1..preset_count)` で固定プリセットを 3 秒ずつ順に実行
    2. `halo_set_led` で虹色を書き、`halo_run(preset_count + 1)` で表示。色相を 10° ずつずらして 1 周(36 回、60 ms 間隔)
    3. `halo_run(0)` で消灯(1.5 秒)
3. API が失敗(`halo.res.ok == false`)したら、そのループを抜けて 500 ms 待ち、`halo_call` で再接続してから再開する

LED が 48 個を超える場合、虹色は先頭の 48 個までを書く(RAM 節約のための上限 `MAX_LEDS`)。
