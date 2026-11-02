# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# ch32fun を使ったビルド。通常は scripts/build.sh 経由で呼ぶ。
#   make build
#   make flash   (WCH-LinkE 接続時。minichlink を使う)

all : build

TARGET := src/halo_controller
TARGET_MCU ?= CH32V003

LINK_DIR := ../halo-link
BRIDGE_DIR := ../halo-bridge
# halo_critical.h: halo-bridge の競合区間を、割り込みの禁止・許可で囲む(全ソースに入れる)
EXTRA_CFLAGS += -Isrc -I$(LINK_DIR)/include -I$(BRIDGE_DIR)/include -include src/halo_critical.h -Werror

ADDITIONAL_C_FILES += src/port.c src/halo_button.c src/halo_ui.c src/i2c_slave.c
ADDITIONAL_C_FILES += $(LINK_DIR)/src/halo_link_frame.c $(LINK_DIR)/src/halo_link_master.c
ADDITIONAL_C_FILES += $(BRIDGE_DIR)/src/halo_bridge_slave.c

include ext/ch32fun/ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean
