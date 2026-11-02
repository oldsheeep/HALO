#!/usr/bin/env bash
# CI 用: setup.sh と同じ内容で取得し、ツールチェーンを後続ステップの PATH に追加する。
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

bash "$DIR/scripts/setup.sh"

if [ -n "${GITHUB_PATH:-}" ]; then
    echo "$DIR/.toolchain/bin" >>"$GITHUB_PATH"
fi
