---
name: pr
description: HALO リポジトリでプルリクエスト(PR)を作る・更新するときの手順とルール。ベースブランチ、scope の label 付け、PR 本文の書き方、プロトコル変更時の扱いを定める。PR を作成する、PR の label やタイトルを決める、「PR 出して」と言われたときに使う。
---

# PR 作成ルール

## 前提

- 作業は git worktree 上で行い、ブランチ名は CLAUDE.md の命名規則(`<type>/<scope>-<summary>`)に従っていること。
- PR 本文は `.github/pull_request_template.md` に従う。

## ベースブランチ

- 基本は `develop`。
- `main` 向けの PR は、ユーザーが指示したときだけ作る(マージで Actions が走り、変更のあったディレクトリがビルドされるため)。
- `hotfix/*` ブランチはベースが `main`(push で PR が自動作成される。手順は `hotfix` skill)。

## label(scope)

どの部分に関する PR かを GitHub の label で示す。

- label 名は `scope:<scope>`(例: `scope:controller-ch32v003`、`scope:controller`、`scope:repo`)。
- `<scope>` はブランチ名と同じ。トップレベルディレクトリ名から `halo-` を除いたもの。リポジトリ全体なら `repo`。
- 複数ディレクトリにまたがる場合は該当する scope をすべて付ける。ただしブランチは1スコープに分けるのが望ましい。
- 対応する label が無ければ作成する(新しいスコープ = ディレクトリを追加したときも合わせて作る)。

```bash
gh label create "scope:<scope>" --description "<scope> に関する変更" --force
gh pr create --base develop --label "scope:<scope>" --title "<タイトル>" --body-file <本文ファイル>
```

## 本文に書くこと

- 変更内容
- 動作確認の方法(実機で確認したか、ビルドのみか)
- 関連する仕様書の更新有無

## プロトコル変更を含む場合

GATT 定義・プリセット形式などデバイス間のプロトコルを変更する PR は、仕様書の更新を同じ PR に含める。

## マージ後

PR がマージされたら、作業ブランチと worktree を削除する。`develop` / `main` は削除しない。

- リモートブランチ: マージ時に削除する(`gh pr merge --delete-branch`、または GitHub の「Automatically delete head branches」設定)。
- worktree とローカルブランチ: マージ済みであることを確認してから削除する。

```bash
git worktree remove <worktree のパス>
git branch -d <ブランチ名>
```
