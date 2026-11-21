list(APPEND BOARD_ROOT "${APP_DIR}")
list(APPEND DTS_ROOT "${APP_DIR}")
list(REMOVE_DUPLICATES DTS_ROOT)
set(DTS_ROOT "${DTS_ROOT}" CACHE STRING "Device tree roots for all images" FORCE)
find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(sysbuild LANGUAGES)

add_custom_target(cadence_package ALL
    COMMAND "${Python3_EXECUTABLE}" "${APP_DIR}/scripts/package.py"
            "${CMAKE_BINARY_DIR}" "${DEFAULT_IMAGE}"
    DEPENDS ${DEFAULT_IMAGE} mcuboot ble_loader
    COMMENT "Validating BLE/OTA images and merging HEX"
    VERBATIM
)
