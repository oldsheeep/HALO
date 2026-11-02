# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# ch32fun を使ったビルド。通常は scripts/build.sh 経由で呼ぶ。
#   make build
#   make flash   (WCH-LinkE 接続時。minichlink を使う)

all : build

TARGET := src/halo_controller
TARGET_MCU ?= CH32V003

LINK_DIR := ../halo-link
EXTRA_CFLAGS += -Isrc -I$(LINK_DIR)/include -Werror

ADDITIONAL_C_FILES += src/port.c $(LINK_DIR)/src/halo_link_frame.c $(LINK_DIR)/src/halo_link_master.c

include ext/ch32fun/ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean
