cmake_minimum_required(VERSION 3.20.0)
# Match the ANT add-on used by the existing nRF52832 project. Override the
# path through the environment or CMake cache when installed elsewhere.
set(CADENCE_ANT_MODULE_DIR "D:/ncs/ant-v2.1.1/ant" CACHE PATH "sdk-ant v2.1.1 module")
if(DEFINED ENV{CADENCE_ANT_MODULE_DIR})
  set(CADENCE_ANT_MODULE_DIR "$ENV{CADENCE_ANT_MODULE_DIR}" CACHE PATH "sdk-ant v2.1.1 module" FORCE)
endif()
if(NOT EXISTS "${CADENCE_ANT_MODULE_DIR}/zephyr/module.yml")
  message(FATAL_ERROR "ANT add-on missing: set CADENCE_ANT_MODULE_DIR to the sdk-ant v2.1.1 ant directory")
endif()
list(APPEND ZEPHYR_EXTRA_MODULES "${CADENCE_ANT_MODULE_DIR}")
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
file(STRINGS "${ZEPHYR_NRF_MODULE_DIR}/VERSION" ncs_version LIMIT_COUNT 1)
if(NOT ncs_version STREQUAL "3.2.4")
  message(FATAL_ERROR "This project requires NCS 3.2.4 with sdk-ant v2.1.1")
endif()

project(nRF52832_BCCPM_V1.1)

target_sources(app PRIVATE src/main.c)
