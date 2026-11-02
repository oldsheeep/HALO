#!/usr/bin/env bash
# base...head で差分のあるファイルから、PR に付ける scope を1行ずつ出す。
#   halo-<scope>/ 配下 → <scope>、それ以外(ルート直下や .github など) → repo
#
# usage: pr-scopes.sh <base> <head>
set -euo pipefail
[[ $# -eq 2 ]] || { echo "usage: $0 <base> <head>" >&2; exit 2; }

git diff --name-only "$1...$2" | cut -d/ -f1 | sort -u | while read -r d; do
    case "$d" in
    halo-*) echo "${d#halo-}" ;;
    *) echo repo ;;
    esac
done | sort -u
