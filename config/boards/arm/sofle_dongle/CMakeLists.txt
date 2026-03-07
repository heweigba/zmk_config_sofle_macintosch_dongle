# Copyright (c) 2023 ZitaoTech
# SPDX-License-Identifier: MIT

if(CONFIG_BOARD_SOFLE_DONGLE_RIGHT)
    zephyr_library_sources(custom_driver/keyboard_backlight.c)
    zephyr_library_include_directories(${ZEPHYR_BASE}/drivers)
    zephyr_library_include_directories(../../../include)
    zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
endif()

if(CONFIG_BOARD_SOFLE_DONGLE_LEFT)
    zephyr_library_sources(custom_driver/keyboard_backlight.c)
    zephyr_library_include_directories(${ZEPHYR_BASE}/drivers)
    zephyr_library_include_directories(../../../include)
    zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
endif()

zephyr_library()
zephyr_library_sources(bb_wheel_to_arrow.c)
zephyr_library_sources(bb_btn_to_arrow.c)
zephyr_library_include_directories(../../../include)
    zephyr_library_include_directories(${ZEPHYR_BASE}/drivers)
    zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
