cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
# Run after Zephyr has loaded DT/Kconfig, before its signing commands are built.
set(CMAKE_PROJECT_Zephyr-Kernel_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/sign-loader-dts.cmake)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cadence_ble_loader)
target_sources(app PRIVATE src/main.c)
