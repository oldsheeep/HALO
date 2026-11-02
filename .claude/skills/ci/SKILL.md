---
name: ci
description: HALO の CI/CD(GitHub Actions と pre-commit)の仕組みと、ディレクトリを追加・ビルドを足すときの規約。develop→main の自動 PR、lint/format、ディレクトリごとのビルドと release の動き、build.sh の書き方を説明する。新しい halo-* ディレクトリを作る、ビルドが通らない、release の中身を確認する、lint/format の挙動を変えるときに使う。
---

# CI/CD

## 全体像

| 仕組み | ファイル | いつ | 何をする |
| --- | --- | --- | --- |
| pre-commit | `.pre-commit-config.yaml` | コミット時(ステージ済みファイルのみ) | linter / formatter を **auto fix** |
| Lint & Format | `.github/workflows/lint-format.yml` | PR のみ | 差分ファイルを検査(修正はしない)。error はブロック、warn は警告のみ |
| Auto PR to main | `.github/workflows/auto-pr-to-main.yml` | develop への push | develop → main の PR を作成・更新し、`scope:<scope>` label を付ける |
| Auto PR for hotfix | `.github/workflows/auto-pr-hotfix.yml` | `hotfix/**` への push | main 向けの PR を作成・更新し、`hotfix` と `scope:<scope>` label を付ける |
| Sync main to develop | `.github/workflows/sync-main-to-develop.yml` | main への push | develop に無い main のコミット(hotfix)があれば、main → develop の PR を作る |
| Test scripts | `.github/workflows/test-scripts.yml` | `scripts/` `halo-*/deps.txt` を変える PR | `scripts/test-changed-dirs.sh` を実行 |
| Build & Release | `.github/workflows/release.yml` | main への push | 差分のあるディレクトリだけビルドし release を切る |

## 初回セットアップ

```bash
pip install pre-commit   # または brew install pre-commit
pre-commit install       # clone / worktree ごとではなくリポジトリ単位で1回
```

リポジトリ設定(GitHub):

- Settings → Actions → General → 「Allow GitHub Actions to create and approve pull requests」を有効にする。
- 自動作成した PR でも lint-format を走らせるため、PAT を secret `PR_CREATE_TOKEN` に登録する(`GITHUB_TOKEN` で作った PR は他のワークフローを起動しない)。未設定でも PR は作られるが、検査は走らない。
- Settings → General → 「Automatically delete head branches」を有効にする。
- develop → main、hotfix → main、main → develop の PR は **merge commit** でマージする(squash すると develop が main と食い違い続け、同期 PR が毎回できる)。
- label `scope:*` `hotfix` `hotfix-backport` は Actions が自動で作る。

## pre-commit の使い方

- auto fix したフックはファイルを書き換えてコミットを失敗させる。内容を確認して `git add` し、もう一度コミットする。
- 全体にかけたいとき: `pre-commit run --all-files`
- 対応言語を増やすときは `.pre-commit-config.yaml` にフックを足す。PR で warn 扱いにするフックは `lint-format.yml` の `WARN_HOOKS` にも書く。

## ビルドの規約

`halo-*` ディレクトリが **`scripts/build.sh` を持っていればビルド対象**になる(`scripts/changed-dirs.sh` が判定する)。

- `halo-<name>/scripts/build.sh`: リポジトリのルートから `bash halo-<name>/scripts/build.sh` で呼ばれる。成果物を `halo-<name>/dist/` に出力する。
- `halo-<name>/scripts/setup-ci.sh`(任意): ツールチェーンのインストール。`build.sh` の前に CI だけで実行される。
- `dist/` はコミットしない(`.gitignore` 済み)。
- CI は `dist/` に `LICENSE` と `THIRD_PARTY_NOTICES.md` を加えて `halo-<name>.zip` にし、release のアセットにする。
- 自作のソースには SPDX ヘッダが必要(`spdx-header` フック。無いと CI が失敗する)。

`build.sh` が無いディレクトリはビルドされない(release にも載らない)。

## 依存の宣言(deps.txt)

他のディレクトリのソースをビルド時に取り込むディレクトリは、`halo-<name>/deps.txt` に依存先を書く。依存先が変更されたとき、そのディレクトリも再ビルド(release)の対象になる。

```text
# halo-link を ../halo-link から取り込む
halo-link
```

- 1 行 1 ディレクトリ名。`#` 以降と空行は無視する。
- 依存先は `scripts/build.sh` を持たなくてよい(`halo-link` はビルド対象ではない)。
- 直接の依存だけを見る(依存の依存は辿らない)。必要なら、依存元にも書く。
- 疎結合の方針により、書いてよい依存先は共有ライブラリの `halo-link` だけ。デバイスのディレクトリ同士は書かない。
- 現在あるのは `halo-controller-ch32v003/deps.txt` のみ。`halo-controller/` `halo-controller-esp32/` `halo-controller-picow/` は、そのディレクトリを作る PR で `deps.txt`(中身は `halo-link`)を足す。
- `all` モード・初回 push は deps.txt に関係なく全ビルド対象を返す。

## release の動き

1. main への push で、前回との差分があるビルド対象ディレクトリ(と、差分のあるディレクトリに `deps.txt` で依存するもの)だけをビルドする。
2. 差分が無いビルド対象ディレクトリの `halo-<name>.zip` は、直前の release からコピーして同じ release に載せる。
3. ビルド対象の差分が1つも無い(docs のみなど)ときは release を切らない。
4. タグは `release-<UTC日時>-<短縮sha>`。本文に「今回ビルド」と「引き継ぎ」を書く。

手動で全ディレクトリをビルドしたいときは Actions → Build & Release → Run workflow で `all` を有効にする。

## ローカルでの確認

```bash
scripts/changed-dirs.sh all                   # ビルド対象の全ディレクトリ
scripts/changed-dirs.sh diff origin/main HEAD # 差分のあるビルド対象(依存元を含む)
scripts/test-changed-dirs.sh                  # changed-dirs.sh のテスト(一時 git リポジトリで実行)
```

`changed-dirs.sh` を変えたら `test-changed-dirs.sh` にケースを足して実行する。
