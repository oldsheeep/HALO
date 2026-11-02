#!/usr/bin/env bash
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# ホストでのテスト(表示の状態管理 + halo-link スレーブとのつなぎ)。全 variant で行う。
#   bash halo-controller-ch32v003/tests/test.sh
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

for f in "$DIR"/variants/*.h; do
    v="$(basename "$f" .h)"
    echo "== test: $v =="
    cc -std=gnu11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
        -fno-sanitize-recover=all -I"$DIR/src" -I"$DIR/variants" -I"$DIR/../halo-link/include" \
        -DHALO_VARIANT_HEADER="\"$v.h\"" -DHALO_LINK_ENABLE=1 \
        "$DIR/tests/test_halo_slave.c" "$DIR/src/halo_display.c" "$DIR/src/link_slave.c" \
        "$DIR/../halo-link/src/halo_link_frame.c" "$DIR/../halo-link/src/halo_link_slave.c" \
        -o "$OUT/test_$v"
    "$OUT/test_$v"
done
