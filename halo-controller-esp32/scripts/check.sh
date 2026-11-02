#!/usr/bin/env bash
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# ESP-IDF が無い環境での、構文の検査(スタブのヘッダで、-fsyntax-only)。ビルドの代わりにはならない。
#   bash halo-controller-esp32/scripts/check.sh
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
for f in "$DIR"/main/main.c "$DIR"/main/i2c_port.c; do
    cc -std=gnu11 -Wall -Wextra -Werror -fsyntax-only \
        -I"$DIR/tests/stubs" -I"$DIR/main" -I"$DIR/../halo-link/include" -I"$DIR/../halo-bridge/include" "$f"
done
echo "ok: 構文の検査を通過(ビルドは idf.py build で行う)"
