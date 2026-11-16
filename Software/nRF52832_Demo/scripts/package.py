"""Validate effective OTA configuration and merge the three SWD HEX images."""
import hashlib
import json
import sys
from pathlib import Path

from intelhex import IntelHex

build = Path(sys.argv[1])
required = {
    "mcuboot": ["CONFIG_BOOT_FIRMWARE_LOADER=y", "CONFIG_BOOT_VALIDATE_SLOT0=y",
                "CONFIG_BOOT_FIRMWARE_LOADER_NO_APPLICATION=y"],
    "ble_loader": ["CONFIG_MCUMGR=y", "CONFIG_ZCBOR=y", "CONFIG_MCUMGR_TRANSPORT_BT=y",
                   "CONFIG_MCUBOOT_APPLICATION_FIRMWARE_UPDATER=y",
                   "CONFIG_MCUMGR_SMP_COMMAND_STATUS_HOOKS=y", "CONFIG_FPROTECT=y"],
    "nRF52832_Demo": ["CONFIG_BOOTLOADER_MCUBOOT=y", "CONFIG_RETENTION_BOOT_MODE=y",
                     "CONFIG_ANT=y", "CONFIG_BT=y", "CONFIG_UART_BT=y", "CONFIG_I2C=y",
                     "CONFIG_ANT_TOTAL_CHANNELS_ALLOCATED=1"],
}
partitions = {"mcuboot": (0, 0x10000), "nRF52832_Demo": (0x10000, 0x3c000),
              "ble_loader": (0x4c000, 0x30000)}
merged = IntelHex()
defined = set()
report = {}
for name, settings in required.items():
    folder = build / name / "zephyr"
    config = (folder / ".config").read_text(encoding="utf-8").splitlines()
    for setting in settings:
        if setting not in config:
            raise RuntimeError(f"{name}: missing effective setting {setting}")
    if name != "nRF52832_Demo" and "CONFIG_ANT=y" in config:
        raise RuntimeError(f"{name}: ANT must only be enabled in the business image")
    values = dict(line.split("=", 1) for line in config if line.startswith("CONFIG_") and "=" in line)
    offset, capacity = partitions[name]
    if (int(values["CONFIG_FLASH_LOAD_OFFSET"], 0),
            int(values["CONFIG_FLASH_LOAD_SIZE"], 0)) != (offset, capacity):
        raise RuntimeError(f"{name}: unexpected Flash partition")
    hexfile = folder / ("zephyr.hex" if name == "mcuboot" else "zephyr.signed.hex")
    image = IntelHex(str(hexfile))
    if image.minaddr() != offset or image.maxaddr() >= offset + capacity:
        raise RuntimeError(f"{name}: HEX exceeds its Flash partition")
    for address in image.addresses():
        if address in defined and merged[address] != image[address]:
            raise RuntimeError(f"Conflicting HEX at 0x{address:x}")
        merged[address] = image[address]
        defined.add(address)
    report[name] = {"hex": str(hexfile.resolve()),
                    "sha256": hashlib.sha256(hexfile.read_bytes()).hexdigest(),
                    "segments": image.segments()}
merged.write_hex_file(str(build / "merged.hex"))
(build / "ota_artifacts.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print(f"Validated bootchain; merged SWD image: {build / 'merged.hex'}")
