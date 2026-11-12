# Install script for directory: D:/ncs/v3.4.1/zephyr

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/Zephyr-Kernel")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/ncs/toolchains/4f5b6ad6dd/opt/zephyr-sdk/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-objdump.exe")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/zephyr/arch/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/zephyr/lib/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/zephyr/soc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/zephyr/boards/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/zephyr/subsys/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/zephyr/drivers/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/nrf/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/mcuboot/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/mbedtls/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/trusted-firmware-m/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/cjson/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/azure-sdk-for-c/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/cirrus-logic/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/openthread/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/nrf_wifi/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/hal_nordic/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/memfault-firmware-sdk/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/hostap/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/canopennode/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/chre/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/cmsis/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/cmsis-dsp/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/cmsis-nn/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/cmsis_6/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/dhara/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/fatfs/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/hal_st/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/hal_tdk/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/hal_wurthelektronik/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/liblc3/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/libmetal/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/libsbc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/littlefs/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/loramac-node/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/lvgl/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/mipi-sys-t/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/nanopb/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/open-amp/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/percepio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/picolibc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/segger/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/uoscore-uedhoc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/zcbor/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/nrfxlib/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/nrf_hw_models/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/modules/connectedhomeip/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/zephyr/kernel/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/zephyr/cmake/flash/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/zephyr/cmake/usage/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/zephyr/cmake/reports/cmake_install.cmake")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo/zephyr/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
