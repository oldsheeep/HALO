#!/usr/bin/env bash
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# ホストでのテスト。警告はエラー、サニタイザ有効。
#   bash halo-link/scripts/test.sh
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

cc -std=c99 -Wall -Wextra -Wpedantic -Werror -g -fsanitize=address,undefined \
    -fno-sanitize-recover=all -I"$DIR/include" \
    "$DIR"/src/*.c "$DIR/tests/test_halo_link.c" -o "$OUT/test_halo_link"
"$OUT/test_halo_link"
