#!/usr/bin/env bash
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# ホストでのテスト。警告はエラー、サニタイザ有効。halo-link のソースも使う。
#   bash halo-bridge/scripts/test.sh
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LINK="$DIR/../halo-link"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

cc -std=c99 -Wall -Wextra -Wpedantic -Werror -g -fsanitize=address,undefined \
    -fno-sanitize-recover=all -I"$DIR/include" -I"$LINK/include" \
    "$DIR"/src/*.c "$LINK/src/halo_link_frame.c" "$LINK/src/halo_link_master.c" \
    "$LINK/src/halo_link_slave.c" "$DIR/tests/test_halo_bridge.c" -o "$OUT/test_halo_bridge"
"$OUT/test_halo_bridge"
