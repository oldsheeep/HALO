# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 sheeplab
# halo-link と halo-bridge(マスター側)を、リポジトリの相対パスで取り込む。
set(HALO_LINK_DIR ${CMAKE_CURRENT_LIST_DIR}/../../halo-link)
set(HALO_BRIDGE_DIR ${CMAKE_CURRENT_LIST_DIR}/../../halo-bridge)

idf_component_register(
    SRCS
        "main.c"
        "i2c_port.c"
        "${HALO_LINK_DIR}/src/halo_link_frame.c"
        "${HALO_LINK_DIR}/src/halo_link_master.c"
        "${HALO_BRIDGE_DIR}/src/halo_bridge_master.c"
    INCLUDE_DIRS
        "."
        "${HALO_LINK_DIR}/include"
        "${HALO_BRIDGE_DIR}/include"
    REQUIRES driver esp_timer esp_rom
)
