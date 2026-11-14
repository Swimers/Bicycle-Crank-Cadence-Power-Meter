cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nRF52832_Demo)

target_sources(app PRIVATE
    src/main.c
    src/bluetooth_app.c
    src/drivers/sc7a20h.c
    src/sc7a20h_app.c
)
