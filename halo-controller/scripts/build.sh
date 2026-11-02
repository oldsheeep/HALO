#!/usr/bin/env bash
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# ビルドして、成果物を dist/ に出す。
#   bash halo-controller/scripts/build.sh
# 出力: dist/halo-controller.{elf,bin,hex,map}
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

# 取得物が無ければ(ローカル初回など)取得する。
if [ ! -d ext/ch32fun/ch32fun ] || { [ ! -x .toolchain/bin/riscv-none-elf-gcc ] && ! command -v riscv-none-elf-gcc >/dev/null 2>&1; }; then
    bash scripts/setup.sh
fi
if [ -x .toolchain/bin/riscv-none-elf-gcc ]; then
    export PATH="$DIR/.toolchain/bin:$PATH"
fi

rm -rf dist
mkdir -p dist

make --no-print-directory PREFIX=riscv-none-elf clean
make --no-print-directory PREFIX=riscv-none-elf build
for ext in elf bin hex map; do
    cp "src/halo_controller.$ext" "dist/halo-controller.$ext"
done
make --no-print-directory PREFIX=riscv-none-elf clean

echo "== dist =="
ls -l dist
