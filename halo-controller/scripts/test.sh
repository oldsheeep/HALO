#!/usr/bin/env bash
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# ホストでのテスト(ボタンの判定、操作 → A 側への命令)。警告はエラー、サニタイザ有効。
#   bash halo-controller/scripts/test.sh
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LINK="$DIR/../halo-link"
BRIDGE="$DIR/../halo-bridge"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

cc -std=c99 -Wall -Wextra -Wpedantic -Werror -g -fsanitize=address,undefined \
    -fno-sanitize-recover=all -I"$DIR/src" -I"$LINK/include" -I"$BRIDGE/include" \
    "$DIR/src/halo_button.c" "$DIR/src/halo_ui.c" \
    "$LINK/src/halo_link_frame.c" "$LINK/src/halo_link_master.c" "$LINK/src/halo_link_slave.c" \
    "$DIR/tests/test_halo_controller.c" -o "$OUT/test_halo_controller"
"$OUT/test_halo_controller"
