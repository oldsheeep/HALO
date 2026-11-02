#!/usr/bin/env bash
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# 全 variant(または指定した variant)をビルドし、成果物を dist/ に出す。
#   bash halo-controller-ch32v003/scripts/build.sh            # variants/*.h すべて
#   bash halo-controller-ch32v003/scripts/build.sh ring12     # 指定のみ
# 出力: dist/halo-controller-ch32v003-<variant>.{elf,bin,hex,map}
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

if [ "$#" -gt 0 ]; then
    variants=("$@")
else
    variants=()
    for f in variants/*.h; do
        variants+=("$(basename "$f" .h)")
    done
fi

rm -rf dist
mkdir -p dist

for v in "${variants[@]}"; do
    if [ ! -f "variants/$v.h" ]; then
        echo "variant が見つかりません: variants/$v.h" >&2
        exit 1
    fi
    echo "== build: $v =="
    make --no-print-directory PREFIX=riscv-none-elf VARIANT="$v" clean
    make --no-print-directory PREFIX=riscv-none-elf VARIANT="$v" build
    for ext in elf bin hex map; do
        cp "src/halo.$ext" "dist/halo-controller-ch32v003-$v.$ext"
    done
    make --no-print-directory PREFIX=riscv-none-elf VARIANT="$v" clean
done

echo "== dist =="
ls -l dist
