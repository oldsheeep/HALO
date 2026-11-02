# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
cmake_minimum_required(VERSION 3.13)

# Pico SDK を、環境変数 PICO_SDK_PATH から取り込む
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
set(PICO_BOARD pico_w)

project(halo_controller_picow C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

# halo-link と halo-bridge(マスター側)を、リポジトリの相対パスで取り込む
set(HALO_LINK_DIR ${CMAKE_CURRENT_LIST_DIR}/../halo-link)
set(HALO_BRIDGE_DIR ${CMAKE_CURRENT_LIST_DIR}/../halo-bridge)

add_executable(halo_controller_picow
    src/main.c
    src/i2c_port.c
    ${HALO_LINK_DIR}/src/halo_link_frame.c
    ${HALO_LINK_DIR}/src/halo_link_master.c
    ${HALO_BRIDGE_DIR}/src/halo_bridge_master.c
)
target_include_directories(halo_controller_picow PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${HALO_LINK_DIR}/include
    ${HALO_BRIDGE_DIR}/include
)
target_link_libraries(halo_controller_picow pico_stdlib hardware_i2c)
pico_enable_stdio_usb(halo_controller_picow 1)
pico_enable_stdio_uart(halo_controller_picow 0)
pico_add_extra_outputs(halo_controller_picow)
