cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED BOARD)
    set(BOARD cadence_sensor/nrf52832 CACHE STRING "Target board")
endif()
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nRF52832_Demo)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_sources(app PRIVATE
    src/main.c
    src/app/ant_app.c
    src/app/bluetooth_app.c
    src/app/ota_entry.c
    src/app/sc7a20h_app.c
    src/drivers/sc7a20h.c
)
