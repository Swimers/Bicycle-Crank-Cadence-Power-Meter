cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nRF52840_Kit_ANT)

target_sources(app PRIVATE src/main.c src/ant_decode.c)
target_sources_ifdef(CONFIG_ANT app PRIVATE src/ant_receiver.c)
