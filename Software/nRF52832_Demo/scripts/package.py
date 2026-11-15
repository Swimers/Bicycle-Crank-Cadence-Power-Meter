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
    "nRF52832_Demo": ["CONFIG_BOOTLOADER_MCUBOOT=y", "CONFIG_RETENTION_BOOT_MODE=y"],
}
merged = IntelHex()
defined = set()
report = {}
for name, settings in required.items():
    folder = build / name / "zephyr"
    config = (folder / ".config").read_text(encoding="utf-8").splitlines()
    for setting in settings:
        if setting not in config:
            raise RuntimeError(f"{name}: missing effective setting {setting}")
    hexfile = folder / ("zephyr.hex" if name == "mcuboot" else "zephyr.signed.hex")
    image = IntelHex(str(hexfile))
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
