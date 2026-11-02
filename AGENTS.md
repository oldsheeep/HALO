# HALO

CH32V003 で SK6812mini-e(アドレサブルLED)を制御するプログラムを管理するリポジトリ。
加えて、ESP32 / Raspberry Pi Pico W から BLE 経由でモバイル端末と通信し、ライティングのプリセットの書き込み・呼び出しを行う。

このファイルは AI エージェント共通の前提ドキュメント。Claude Code 固有の運用ルール(ブランチ命名規則など)は [CLAUDE.md](CLAUDE.md) を参照。

## リポジトリ構成(monorepo)

デバイス・役割ごとにトップレベルのディレクトリを分ける。1ディレクトリ = 1機能(= CI でビルドする単位)。

| ディレクトリ | 役割 |
| --- | --- |
| `halo-controller-ch32v003/` | CH32V003 上で SK6812mini-e を制御するファームウェア |
| `halo-link/` | 単線半二重通信のライブラリ(A・B 側で共有。仕様は [docs/halo-link-spec.md](docs/halo-link-spec.md)) |
| `halo-bridge/` | I2C ブリッジのライブラリ(ESP32 / Pico W ⇔ B 側の CH32V003。仕様は [docs/halo-bridge-spec.md](docs/halo-bridge-spec.md)) |
| `halo-controller/` | B 側(マスター)の CH32V003 版。A 側へプリセット実行・LED 直接制御・情報要求を送る |
| `halo-controller-esp32/` | ESP32 側。B 側へ I2C でつなぎ、A 側を操作する(ビルド・実機は未確認) |
| `halo-controller-picow/` | Pico W 側。B 側へ I2C でつなぎ、A 側を操作する(ビルド・実機は未確認) |
| `halo-pcb-ch32v003/` | CH32V003J4M6 のコントローラ基板の回路図(KiCad) |

今後追加予定(追加時はこの表を更新する):

- モバイル端末向けアプリ
- 図面データ(基板・筐体など)
- プロジェクトロゴ・アイコン
- その他

## 管理対象

プログラムに加えて、以下もこのリポジトリで管理する。

- 各種図面データ
- プロジェクトロゴ(アイコン)
- その他(今後追加)

## ライセンス

- 自作のソース(`.c` `.h` `.sh` `Makefile`)の先頭に SPDX ヘッダを付ける(ヘッダが無いと pre-commit / CI が失敗する)。

    ```text
    SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
    Copyright (c) 2026 sheeplab
    ```

    行コメントの記号は言語に合わせる(C/C++ は `//`、シェル・Makefile は `#`)。シェバン行がある場合はその次の行に置く。
- 全体は [LICENSE](LICENSE)(PolyForm Noncommercial 1.0.0)。プロジェクトロゴ・アイコンは All rights reserved で、`assets/logo/` に置く。
- 取り込んだ第三者コードをビルド成果物に含める場合は、[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) に入手元と全文を追記する。

## 設計方針

- **疎結合**: 各ディレクトリ(デバイス)は互いのソースを直接参照しない。デバイス間の情報伝達はプロトコル(BLE の GATT 定義、シリアル/I2C などのバイト列仕様)だけを契約とし、仕様はドキュメントとして明文化する。
- 共有したい定義(プリセットのデータ形式、コマンド仕様など)は、特定デバイスのディレクトリ内に閉じ込めず、仕様書として切り出す。実装側はそれに従う。
- 1つのディレクトリの変更が他のディレクトリのビルドを壊さないようにする。

## ブランチ運用とCI

- 通常の開発は `develop` ブランチ(およびそこから切った作業ブランチ)で行う。
- `main` へのマージで GitHub Actions が走り、**変更のあったディレクトリ(機能)のみ**ビルドして書き込み用の成果物を作る。
  - 対象は `halo-*/scripts/build.sh` を持つディレクトリ。差分のあるものだけビルドして release を切り、差分の無いものは前回 release の成果物を引き継ぐ。
  - `halo-link` など他ディレクトリを取り込むときは `halo-<name>/deps.txt` に依存先を書く。依存先が変わると、そのディレクトリも再ビルドされる(`ci` skill)。
- `develop` への push で `develop` → `main` の PR が自動作成される。
- 緊急時は `hotfix/*` を main から切り、main へ直接 PR する(自動作成)。マージ後は main → develop の同期 PR が自動作成される(`hotfix` skill)。
- コミット時は pre-commit が差分に linter / formatter を auto fix し、PR では CI が検査する。詳細は `ci` skill。
- 作業は git worktree 上で行う(メインのチェックアウトでは作業しない)。
- PR には、対象の機能を示す label(`scope:<scope>`)を付ける(手順は `pr` skill)。
- ブランチ命名規則・worktree の詳細ルールは [CLAUDE.md](CLAUDE.md) に定義する。

## 反復作業の扱い

何度も繰り返す操作(ビルド、書き込み、図面の書き出し、リリース準備など)は、その都度コマンドを打つのではなく次の形で整備する。

- 手順を `scripts/` 配下のスクリプトにする(ディレクトリ固有のものは各ディレクトリ内の `scripts/`)。
- エージェント向けの手順書は `.claude/skills/<name>/SKILL.md`(skill)に書く。スクリプトの使い方・前提・注意点を書く。
- 新しいスクリプトを追加したら、対応する skill も同時に更新する。

## エージェント向けの注意

- 変更は、依頼された機能のディレクトリ内に収める。他デバイスのディレクトリに手を入れる必要が出たら、先にその理由を伝える。
- ハードウェアへの書き込み・実機操作を伴う作業は、実行前にユーザーへ確認する。
- バイナリ(ビルド成果物、`.hex` / `.bin` など)はコミットしない。成果物は Actions で生成する。
