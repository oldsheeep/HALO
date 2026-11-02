#!/usr/bin/env bash
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# ツールチェーン(xPack riscv-none-elf-gcc)と ch32fun を、固定バージョンで取得する。
# 何度実行しても同じ結果になる(取得済みならスキップ)。
#   bash halo-controller/scripts/setup.sh
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# --- 固定するバージョン ---
CH32FUN_REPO="https://github.com/cnlohr/ch32fun"
CH32FUN_COMMIT="b28d285460ce052a179dba8f4a22c6f73ff8e328"

XPACK_VERSION="15.2.0-1"
XPACK_BASE="https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/download/v${XPACK_VERSION}"
# --- ここまで ---

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | cut -d' ' -f1
    else
        shasum -a 256 "$1" | cut -d' ' -f1
    fi
}

setup_ch32fun() {
    local dest="$DIR/ext/ch32fun"
    if [ ! -d "$dest/.git" ]; then
        mkdir -p "$DIR/ext"
        git clone --quiet "$CH32FUN_REPO" "$dest"
    fi
    if [ "$(git -C "$dest" rev-parse HEAD)" != "$CH32FUN_COMMIT" ]; then
        git -C "$dest" fetch --quiet origin
        git -C "$dest" checkout --quiet "$CH32FUN_COMMIT"
    fi
    echo "ch32fun: $CH32FUN_COMMIT"
}

setup_toolchain() {
    local dest="$DIR/.toolchain"
    if [ -x "$dest/bin/riscv-none-elf-gcc" ]; then
        echo "toolchain: $("$dest/bin/riscv-none-elf-gcc" -dumpversion) (取得済み)"
        return
    fi

    local platform sha
    case "$(uname -s)-$(uname -m)" in
    Darwin-arm64)
        platform=darwin-arm64
        sha=6588e8351455fad8aca37551f0e5a5543f3346bfa9a837cf03cbd3bdd4989f8f
        ;;
    Darwin-x86_64)
        platform=darwin-x64
        sha=98e83f097b10163869dabffd58389ac8e4eb41bae0f67124569158655be593ea
        ;;
    Linux-x86_64)
        platform=linux-x64
        sha=aaaa8060c914851a3e5ee1ba82cc3d6f80972f90638a05c6e823a37557a33758
        ;;
    *)
        echo "未対応の環境: $(uname -s)-$(uname -m)。riscv-none-elf-gcc を自分で PATH に入れてください" >&2
        exit 1
        ;;
    esac

    local name="xpack-riscv-none-elf-gcc-${XPACK_VERSION}-${platform}.tar.gz"
    tmp="$(mktemp -d)"
    trap 'rm -rf "${tmp:-}"' EXIT

    echo "toolchain: ${name} を取得中..."
    curl -fsSL -o "$tmp/$name" "${XPACK_BASE}/${name}"
    if [ "$(sha256_of "$tmp/$name")" != "$sha" ]; then
        echo "SHA256 が一致しません: $name" >&2
        exit 1
    fi
    mkdir -p "$tmp/x"
    tar -xzf "$tmp/$name" -C "$tmp/x" --strip-components=1
    rm -rf "$dest"
    mv "$tmp/x" "$dest"
    echo "toolchain: $("$dest/bin/riscv-none-elf-gcc" -dumpversion)"
}

setup_ch32fun
setup_toolchain
