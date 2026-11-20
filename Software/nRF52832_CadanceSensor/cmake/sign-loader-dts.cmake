# NCS 3.2.4's loader signer assumes Partition Manager is enabled. This project
# hook runs after DT/Kconfig are available, before signing commands are built.
# Newer Zephyr/NCS versions already support DTS signing without this adapter.
if(NOT PROJECT_VERSION VERSION_LESS "4.3.0" OR CONFIG_PARTITION_MANAGER_ENABLED
   OR NOT CONFIG_MCUBOOT_APPLICATION_FIRMWARE_UPDATER)
  return()
endif()

dt_chosen(loader_partition PROPERTY "zephyr,code-partition")
dt_prop(loader_slot_size PATH "${loader_partition}" PROPERTY "reg" INDEX 1 REQUIRED)
if(NOT loader_slot_size OR NOT CONFIG_ROM_START_OFFSET)
  message(FATAL_ERROR "Loader partition size or MCUboot header size is missing")
endif()

# Zephyr's loader binary already contains the reserved header at the partition
# start. Do not use --pad-header: that would shift the vector table a second time.
set(imgtool_sign_sysbuild
    --slot-size ${loader_slot_size} --header-size ${CONFIG_ROM_START_OFFSET}
    CACHE STRING "DTS loader signing arguments" FORCE)
message(STATUS "DTS loader signing: slot=${loader_slot_size}, header=${CONFIG_ROM_START_OFFSET}")
