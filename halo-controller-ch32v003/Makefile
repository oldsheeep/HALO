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

ADDITIONAL_C_FILES += src/sk6812.c src/halo_display.c

# HALO Link(単線半二重通信のスレーブ)。無効にするとき: make HALO_LINK_ENABLE=0 ...
# 有効なとき USART1 が PD6 の 1 ピン(リマップ 2 + 半二重)を使う。halo-link は ../halo-link から取り込む(変更しない)。
HALO_LINK_ENABLE ?= 1
EXTRA_CFLAGS += -DHALO_LINK_ENABLE=$(HALO_LINK_ENABLE)
ifneq ($(HALO_LINK_ENABLE),0)
EXTRA_CFLAGS += -I../halo-link/include
ADDITIONAL_C_FILES += src/link_uart.c src/link_slave.c \
	../halo-link/src/halo_link_frame.c ../halo-link/src/halo_link_slave.c
endif

include ext/ch32fun/ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean
