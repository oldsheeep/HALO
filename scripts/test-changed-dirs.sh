#!/usr/bin/env bash
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# scripts/changed-dirs.sh の diff / all モードのテスト。
# 一時的な git リポジトリに scripts/changed-dirs.sh をコピーして確認する。
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)/changed-dirs.sh
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

fail=0
check() { # <名前> <期待> <実際>
    if [[ "$2" == "$3" ]]; then
        echo "ok   $1"
    else
        echo "FAIL $1: expected $2 but got $3"
        fail=1
    fi
}

# <name> を作る: テスト用リポジトリを初期化して $tmp/<name> に移動する
new_repo() {
    local r=$tmp/$1
    mkdir -p "$r/scripts"
    cp "$src" "$r/scripts/changed-dirs.sh"
    cd "$r"
    git init -q
    git config user.email t@example.com
    git config user.name t
    git config commit.gpgsign false
}
mk_buildable() { mkdir -p "$1/scripts" && echo "# build" >"$1/scripts/build.sh" && echo x >"$1/src.txt"; }
commit() { git add -A && git commit -qm "$1"; }
run_diff() { bash scripts/changed-dirs.sh diff "$1" HEAD; }

# 共通の初期状態: halo-link(ビルド対象外)、halo-a(halo-link に依存)、halo-b(依存なし)
setup_basic() {
    new_repo "$1"
    mkdir -p halo-link
    echo x >halo-link/lib.c
    mk_buildable halo-a
    mk_buildable halo-b
    printf '# comment\n\nhalo-link\n' >halo-a/deps.txt
    echo base >README.md
    commit base
    base=$(git rev-parse HEAD)
}

setup_basic a_dep_only
echo y >halo-link/lib.c && commit c
check "(a) 依存先だけ変更" '["halo-a"]' "$(run_diff "$base")"

setup_basic b_src_only
echo y >halo-a/src.txt && commit c
check "(b) 依存元だけ変更" '["halo-a"]' "$(run_diff "$base")"

setup_basic c_both
echo y >halo-a/src.txt && echo y >halo-link/lib.c && commit c
check "(c) 両方変更(重複しない)" '["halo-a"]' "$(run_diff "$base")"

setup_basic d_none
echo y >README.md && commit c
check "(d) どれも変更なし" '[]' "$(run_diff "$base")"

setup_basic e_nodeps
git rm -q halo-a/deps.txt && commit nodeps
base=$(git rev-parse HEAD)
echo y >halo-link/lib.c && echo y >halo-b/src.txt && commit c
check "(e) deps.txt が無いディレクトリは自身の変更のみ" '["halo-b"]' "$(run_diff "$base")"

new_repo f_zero
echo x >README.md
commit base
base=$(git rev-parse HEAD)
echo y >README.md && commit c
check "(f) halo-* が 0 件の diff" '[]' "$(run_diff "$base")"
check "(f) halo-* が 0 件の all" '[]' "$(bash scripts/changed-dirs.sh all)"

setup_basic g_multi
printf 'halo-b\r\n  halo-link/  # 末尾コメント\n' >halo-a/deps.txt
echo y >halo-b/src.txt && commit c
check "(g) 複数依存・空白/CRLF/末尾スラッシュ" '["halo-a","halo-b"]' "$(run_diff "$base")"

setup_basic h_first
check "(h) 初回 push(base が 0)は全ディレクトリ" '["halo-a","halo-b"]' \
    "$(run_diff 0000000000000000000000000000000000000000)"
check "(h) all モード" '["halo-a","halo-b"]' "$(bash scripts/changed-dirs.sh all)"

exit "$fail"
