cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED BOARD)
    set(BOARD cadence_sensor/nrf52832 CACHE STRING "Target board")
endif()
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
file(STRINGS "${ZEPHYR_NRF_MODULE_DIR}/VERSION" cadence_sdk_version LIMIT_COUNT 1)
if(NOT cadence_sdk_version STREQUAL "3.2.4")
    message(FATAL_ERROR "CadenceSensor requires NCS 3.2.4")
endif()

project(nRF52832_CadanceSensor)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_sources(app PRIVATE
    src/main.c
    src/app/ble_csc.c
    src/app/cadence_estimator.c
    src/app/sc7a20h_app.c
    src/drivers/sc7a20h.c
)
