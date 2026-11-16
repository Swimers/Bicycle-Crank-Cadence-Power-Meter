cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED BOARD)
    set(BOARD cadence_sensor/nrf52832 CACHE STRING "Target board")
endif()
# Load ANT for both command-line and IDE builds. Cache the module path so
# subsequent IDE/pristine builds do not depend on the launching shell.
set(CADENCE_ANT_MODULE_DIR "D:/ncs/ant-v2.1.1/ant" CACHE PATH "sdk-ant v2.1.1 module")
if(DEFINED ENV{CADENCE_ANT_MODULE_DIR})
    set(CADENCE_ANT_MODULE_DIR "$ENV{CADENCE_ANT_MODULE_DIR}" CACHE PATH "sdk-ant v2.1.1 module" FORCE)
endif()
if(NOT EXISTS "${CADENCE_ANT_MODULE_DIR}/zephyr/module.yml")
    message(FATAL_ERROR "ANT add-on missing: set CADENCE_ANT_MODULE_DIR to sdk-ant v2.1.1")
endif()
list(APPEND ZEPHYR_EXTRA_MODULES "${CADENCE_ANT_MODULE_DIR}")
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
file(STRINGS "${ZEPHYR_NRF_MODULE_DIR}/VERSION" cadence_sdk_version LIMIT_COUNT 1)
if(NOT cadence_sdk_version STREQUAL "3.2.4")
    message(FATAL_ERROR "CadenceSensor requires NCS 3.2.4 with sdk-ant v2.1.1")
endif()

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
