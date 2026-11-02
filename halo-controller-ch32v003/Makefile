# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# ch32fun を使ったビルド。通常は scripts/build.sh 経由で呼ぶ。
#   make VARIANT=ring12 build
#   make VARIANT=ring12 flash   (WCH-LinkE 接続時。minichlink を使う)

all : build

TARGET := src/halo
TARGET_MCU ?= CH32V003

VARIANT ?= ring12
EXTRA_CFLAGS += -Isrc -Ivariants -DHALO_VARIANT_HEADER='"$(VARIANT).h"' -Werror

ADDITIONAL_C_FILES += src/sk6812.c

include ext/ch32fun/ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean
