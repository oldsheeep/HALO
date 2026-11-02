---
name: hotfix
description: develop を経由せず main に即時反映する hotfix ブランチの手順。緊急でビルド(release)が必要なとき、CI の修正を main にすぐ反映したいときに使う。hotfix ブランチの切り方、自動で作られる main 向け PR、マージ後に main → develop へ戻す同期 PR、コンフリクト時の解消方法を説明する。
---

# hotfix

通常の流れ(`feature/*` → `develop` → `main`)を待てないときだけ使う。

- 緊急でビルド(release)が必要になった
- CI / Actions の修正を main にすぐ反映したい

## 流れ

1. **main から** worktree を切る。ブランチ名は `hotfix/<scope>-<summary>`(例: `hotfix/controller-ch32v003-fix-led-timing`、`hotfix/repo-fix-release-workflow`)。

    ```bash
    git fetch origin
    git worktree add -b hotfix/<scope>-<summary> ../HALO-wt/<name> origin/main
    ```

2. 修正してコミットし、push する。
3. `auto-pr-hotfix.yml` が **main 向けの PR を自動で作る**(label `hotfix` と `scope:<scope>`)。手で作る必要はない。
4. レビュー(`lint-format` が走る)→ main へ **merge commit** でマージ。`release.yml` が走り、差分のあるディレクトリがビルドされる。
5. main への push で `sync-main-to-develop.yml` が、**main → develop の同期 PR** を自動で作る(label `hotfix-backport`)。
6. 同期 PR を **merge commit** でマージして、hotfix を develop に取り込む。
7. hotfix ブランチと worktree を削除する(`pr` skill のマージ後の手順と同じ)。

同期 PR を放置すると、次の develop → main で hotfix の修正が巻き戻る恐れがある。hotfix をマージしたら、同期 PR を先に片付ける。

## 同期 PR がコンフリクトしたとき

develop 側が hotfix と同じ箇所を変更していると、GitHub 上でマージできない。ローカルで解消する。

```bash
git fetch origin
git worktree add -b sync/main-to-develop ../HALO-wt/sync-main-to-develop origin/develop
cd ../HALO-wt/sync-main-to-develop
git merge origin/main        # コンフリクトを解消してコミット
git push -u origin sync/main-to-develop
gh pr create --base develop --head sync/main-to-develop --label hotfix-backport --title "Sync: main → develop (hotfix)"
```

解消では、hotfix の修正内容を残しつつ develop の新しい変更も活かす。元の同期 PR(head が `main`)は閉じる。

## 注意

- hotfix は例外。通常の変更に使わない。
- main への PR を自動で作るには secret `PR_CREATE_TOKEN` が必要(`ci` skill 参照)。無い場合は PR が作られるが `lint-format` が走らない。
- develop → main、hotfix → main、main → develop は、いずれも **merge commit** を使う(squash / rebase しない)。
