#!/usr/bin/env python3
"""Package PlatformIO binaries for ESP Web Tools without rebuilding them."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path


BUILDS = {
    "esp32dev": ("ESP32", 0x1000),
    "esp32-s3-devkitc-1": ("ESP32-S3", 0x0000),
    "esp32-c3-devkitm-1": ("ESP32-C3", 0x0000),
}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", default="dev")
    parser.add_argument("--output", type=Path, default=Path("dist"))
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    output = (root / args.output).resolve()
    firmware_output = output / "firmware"
    if output.exists():
        shutil.rmtree(output)
    shutil.copytree(root / "site", output)
    firmware_output.mkdir(parents=True, exist_ok=True)

    platformio_homes = [root / ".platformio-core", Path.home() / ".platformio"]
    boot_app_candidates = []
    for platformio_home in platformio_homes:
        boot_app_candidates.extend(
            platformio_home.glob("packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin")
        )
    if not boot_app_candidates:
        raise SystemExit("boot_app0.bin not found; run `pio run` first")
    boot_app = boot_app_candidates[0]

    builds = []
    for environment, (family, bootloader_offset) in BUILDS.items():
        build_dir = root / ".pio" / "build" / environment
        required = {
            "bootloader.bin": build_dir / "bootloader.bin",
            "partitions.bin": build_dir / "partitions.bin",
            "boot_app0.bin": boot_app,
            "firmware.bin": build_dir / "firmware.bin",
        }
        missing = [str(path) for path in required.values() if not path.is_file()]
        if missing:
            raise SystemExit(f"Missing build artifacts for {environment}: {', '.join(missing)}")
        env_output = firmware_output / environment
        env_output.mkdir()
        for name, source in required.items():
            shutil.copy2(source, env_output / name)
        prefix = f"firmware/{environment}"
        builds.append(
            {
                "chipFamily": family,
                "parts": [
                    {"path": f"{prefix}/bootloader.bin", "offset": bootloader_offset},
                    {"path": f"{prefix}/partitions.bin", "offset": 0x8000},
                    {"path": f"{prefix}/boot_app0.bin", "offset": 0xE000},
                    {"path": f"{prefix}/firmware.bin", "offset": 0x10000},
                ],
            }
        )

    manifest = {
        "name": "Limen ESP32 Codex",
        "version": args.version,
        "new_install_prompt_erase": True,
        "builds": builds,
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
