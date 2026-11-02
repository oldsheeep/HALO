#!/usr/bin/env bash
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# ビルド対象ディレクトリを JSON 配列で標準出力に出す。
#
#   changed-dirs.sh all              ビルド可能な全ディレクトリ
#   changed-dirs.sh diff <base> <head>  base..head で差分のあるビルド可能ディレクトリ
#                                       (差分のあるディレクトリに依存するものを含む)
#
# ビルド可能 = トップレベルの halo-*/ で、scripts/build.sh を持つもの。
# 依存 = <dir>/deps.txt に書いたディレクトリ名(1 行 1 つ、# 始まりと空行は無視)。
#   依存先は scripts/build.sh を持たなくてよい(halo-link など)。直接の依存のみ扱う。
# base が空または 0000... (初回 push) のときは全ディレクトリを返す。
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

# set -e / pipefail 下で、該当なしでも終了コード 0 にするため、&& ではなく if を使う
all_dirs() {
    local d
    shopt -s nullglob
    for d in halo-*/; do
        d=${d%/}
        if [[ -f "$d/scripts/build.sh" ]]; then echo "$d"; fi
    done
}

# deps.txt の依存先を 1 行ずつ出す(コメント・空行・前後の空白・末尾の / を除く)
deps_of() {
    local line
    while read -r line || [[ -n "$line" ]]; do
        line=${line%%#*}
        line=${line%"${line##*[![:space:]]}"}
        line=${line%/}
        if [[ -n "$line" ]]; then echo "$line"; fi
    done <"$1/deps.txt"
}

to_json() {
    local first=1 d
    printf '['
    while read -r d; do
        [[ -z "$d" ]] && continue
        [[ $first -eq 0 ]] && printf ','
        printf '"%s"' "$d"
        first=0
    done
    printf ']\n'
}

case "${1:-}" in
all)
    all_dirs | to_json
    ;;
diff)
    base=${2:-}
    head=${3:-HEAD}
    if [[ -z "$base" || "$base" =~ ^0+$ ]]; then
        all_dirs | to_json
        exit 0
    fi
    changed=$(git diff --name-only "$base" "$head" | cut -d/ -f1 | sort -u)
    all_dirs | while read -r d; do
        if grep -qx "$d" <<<"$changed"; then
            echo "$d"
        elif [[ -f "$d/deps.txt" ]] && deps_of "$d" | grep -qxFf <(echo "$changed"); then
            echo "$d"
        fi
    done | to_json
    ;;
*)
    echo "usage: $0 all | diff <base> <head>" >&2
    exit 2
    ;;
esac
