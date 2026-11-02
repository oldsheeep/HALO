@AGENTS.md

# Claude Code 向けルール

プロジェクトの概要・構成・設計方針は上の `AGENTS.md` を正とする。ここには Claude Code 固有の運用ルールと、AGENTS.md から参照されている規約(ブランチ命名規則、worktree)を書く。

## ブランチ命名規則

どの機能を開発しているかがブランチ名だけで分かることを最優先にする。

```text
<type>/<scope>-<summary>
```

- `<type>`: 作業の種類
  - `feature` 新機能 / `fix` 不具合修正 / `refactor` 挙動を変えない整理
  - `docs` ドキュメント・仕様書 / `design` 図面・ロゴ・アイコン
  - `ci` GitHub Actions・スクリプト / `chore` その他
  - `hotfix` develop を経由せず main に即時反映する緊急修正(下記)
- `<scope>`: 対象の機能 = トップレベルディレクトリ名から `halo-` を除いたもの。複数にまたがる場合は主となるものを選ぶ。リポジトリ全体なら `repo`。
  - 例: `halo-controller-ch32v003` → `controller-ch32v003`、`halo-controller` → `controller`
- `<summary>`: 内容を表す短い英語の kebab-case(小文字・数字・ハイフンのみ)

例:

- `feature/controller-ch32v003-sk6812-driver`
- `feature/controller-ble-preset-write`
- `fix/controller-ch32v003-reset-timing`
- `docs/repo-ble-gatt-spec`
- `ci/repo-path-filtered-build`
- `hotfix/repo-fix-release-workflow`

運用:

- 作業ブランチは `develop` から切り、`develop` へマージする。`main` への直接コミットはしない。
- 例外: `hotfix/*` は **main から**切り、main へ直接 PR する(push で PR が自動作成される)。マージ後は main → develop の同期 PR を必ずマージする。手順は `hotfix` skill。
- `main` へのマージはビルド(Actions)が走るため、ユーザーの指示があるときだけ行う。
- 1ブランチ = 1機能(1スコープ)。複数ディレクトリを横断する変更は、仕様(`docs`)と各実装に分けるのが望ましい。
- 新しいトップレベルディレクトリを追加したら、`AGENTS.md` の構成表を更新し、必要ならここの scope 例も更新する。ビルド対象にするなら `scripts/build.sh` を置く(`ci` skill)。

## worktree での作業

- 作業は **必ず git worktree 上で行う**。メインのチェックアウト(リポジトリ直下)では作業しない。
- 1ブランチ = 1 worktree。ブランチ名は上記の命名規則に従う。
- PR がマージされたら、ブランチ(リモート・ローカル)と worktree を削除する。`develop` / `main` は対象外。手順は `pr` skill を参照。

## コミット

- メッセージは `<type>(<scope>): <内容>` 形式を基本とする(例: `feat(controller-ch32v003): SK6812の送信タイミングを調整`)。
- 日本語・英語どちらでもよいが、1リポジトリ内では揃える。

## Skills

繰り返し行う作業の手順は、`.claude/skills/<name>/SKILL.md` に skill として置く。必要なときだけ読み込まれるので、CLAUDE.md には手順を書かない。

| skill | 使う場面 |
| --- | --- |
| `pr` | PR を作る・更新する(ベースブランチ、`scope:<scope>` label、本文の書き方) |
| `hotfix` | develop を経由せず main に即時反映する(緊急ビルド、CI 修正)。main → develop への戻し方 |
| `ci` | pre-commit・Actions・ビルド/release の仕組み、`build.sh` の規約、新しい `halo-*` ディレクトリを足すとき |

- 反復する操作は `scripts/` にスクリプト化し、使い方・前提・注意点をその操作の skill に書く(詳細は AGENTS.md の「反復作業の扱い」)。
- 該当する skill がある作業は、始める前にその skill を読む。
- スクリプトを追加・変更したら、同じコミットで skill も更新する。新しい skill を作ったら上の表に追記する。

## 作業の進め方

- 実機への書き込みや、デバイス間プロトコル(GATT 定義・プリセット形式)の変更は、実行・変更前にユーザーへ確認する。
- プロトコルを変更するときは、仕様書を先に更新してから各デバイスの実装を合わせる(疎結合を保つため)。
