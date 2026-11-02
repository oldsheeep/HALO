#!/usr/bin/env bash
# ビルド対象ディレクトリを JSON 配列で標準出力に出す。
#
#   changed-dirs.sh all              ビルド可能な全ディレクトリ
#   changed-dirs.sh diff <base> <head>  base..head で差分のあるビルド可能ディレクトリ
#
# ビルド可能 = トップレベルの halo-*/ で、scripts/build.sh を持つもの。
# base が空または 0000... (初回 push) のときは全ディレクトリを返す。
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

all_dirs() {
    local d
    for d in halo-*/; do
        d=${d%/}
        [[ -f "$d/scripts/build.sh" ]] && echo "$d"
    done
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
        grep -qx "$d" <<<"$changed" && echo "$d"
    done | to_json
    ;;
*)
    echo "usage: $0 all | diff <base> <head>" >&2
    exit 2
    ;;
esac
