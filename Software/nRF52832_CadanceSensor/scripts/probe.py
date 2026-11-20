"""Explicit fireDAP operations for CadenceSensor development and validation.

Run using the NCS Python (pyocd and intelhex are bundled).
"""
import argparse
import hashlib
import json
from pathlib import Path

from intelhex import IntelHex
from pyocd.core.helpers import ConnectHelper
from pyocd.flash.file_programmer import FileProgrammer


def digest(data):
    return hashlib.sha256(bytes(data)).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("backup", "flash", "inspect", "enter-loader", "reset", "verify-app"))
    parser.add_argument("--probe", default="0123456789abcdef")
    parser.add_argument("--output")
    parser.add_argument("--hex")
    parser.add_argument("--bin")
    args = parser.parse_args()
    if args.command == "backup" and not args.output:
        parser.error("backup requires --output")
    if args.command == "flash" and not args.hex:
        parser.error("flash requires --hex")
    if args.command == "verify-app" and not args.bin:
        parser.error("verify-app requires --bin")
    with ConnectHelper.session_with_chosen_probe(
            unique_id=args.probe, target_override="nrf52832", frequency=1000000,
            options={"auto_unlock": False}) as session:
        target = session.target
        if args.command == "flash":
            FileProgrammer(session, chip_erase="sector", trust_crc=False).program(args.hex)
            expected = IntelHex(args.hex)
            target.halt()
            for start, end in expected.segments():
                actual = bytes(target.read_memory_block8(start, end - start))
                if actual != bytes(expected.tobinarray(start=start, end=end - 1)):
                    raise RuntimeError(f"Readback mismatch at 0x{start:x}")
            print("FLASH full HEX readback matched", flush=True)
            target.reset()
        elif args.command == "verify-app":
            expected = Path(args.bin).read_bytes()
            if not 32 <= len(expected) <= 0x3C000:
                raise ValueError("Business image size outside slot0 limits")
            target.halt()
            try:
                actual = bytes(target.read_memory_block8(0x10000, len(expected)))
                if actual != expected:
                    raise RuntimeError("Application BIN readback mismatch")
                print(f"VERIFY APP matched {len(actual)} bytes sha256={digest(actual)}")
            finally:
                target.resume()
        elif args.command == "backup":
            folder = Path(args.output)
            folder.mkdir(parents=True, exist_ok=True)
            target.halt()
            try:
                for name, start, size in (("flash", 0, 0x80000), ("uicr", 0x10001000, 0x1000)):
                    data = bytes(target.read_memory_block8(start, size))
                    (folder / (name + ".bin")).write_bytes(data)
                    print(name, len(data), digest(data), flush=True)
            finally:
                target.resume()
        elif args.command == "inspect":
            target.halt()
            try:
                report = {"pc": hex(target.read_core_register("pc")),
                          "vtor": hex(target.read32(0xE000ED08)),
                          "gpregret": hex(target.read32(0x4000051C)),
                          "bprot": [hex(target.read32(x)) for x in
                                    (0x40000600, 0x40000604, 0x40000610, 0x40000614)]}
                for name, start, size in (("boot", 0, 0x10000),
                                          ("application", 0x10000, 0x3c000),
                                          ("loader", 0x4c000, 0x30000),
                                          ("storage", 0x7c000, 0x4000)):
                    report[name] = digest(target.read_memory_block8(start, size))
                print(json.dumps(report, indent=2), flush=True)
            finally:
                target.resume()
        elif args.command == "enter-loader":
            target.halt()
            target.write32(0x4000051C, 1)
            target.reset()
        else:
            target.reset()


if __name__ == "__main__":
    main()
