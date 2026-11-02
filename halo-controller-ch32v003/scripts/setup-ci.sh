#!/usr/bin/env bash
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# CI 用: setup.sh と同じ内容で取得し、ツールチェーンを後続ステップの PATH に追加する。
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

bash "$DIR/scripts/setup.sh"

if [ -n "${GITHUB_PATH:-}" ]; then
    echo "$DIR/.toolchain/bin" >>"$GITHUB_PATH"
fi
