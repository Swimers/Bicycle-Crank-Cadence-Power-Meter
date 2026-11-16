# Use the same sysbuild entry point for the CLI and nRF Connect IDE.
find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sysbuild LANGUAGES)

# Always refresh and validate the complete image after all three images build.
add_custom_target(cadence_package ALL
    COMMAND "${Python3_EXECUTABLE}" "${APP_DIR}/scripts/package.py" "${CMAKE_BINARY_DIR}"
    DEPENDS nRF52832_Demo mcuboot ble_loader
    COMMENT "Validating ANT/BLE/OTA images and merging HEX"
    VERBATIM
)
