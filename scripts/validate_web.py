#!/usr/bin/env python3
"""Validate the packaged GitHub Pages artifact before deployment."""

from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath


REQUIRED_SITE_FILES = (
    "index.html",
    "manifest.json",
    "LICENSE.txt",
    "THIRD_PARTY_NOTICES.md",
)


def fail(message: str) -> None:
    raise SystemExit(f"Web package validation failed: {message}")


def validate_relative_path(raw_path: object) -> PurePosixPath:
    if not isinstance(raw_path, str) or not raw_path:
        fail("every firmware part must have a non-empty string path")

    path = PurePosixPath(raw_path)
    if path.is_absolute() or ".." in path.parts:
        fail(f"firmware path must stay inside the site artifact: {raw_path!r}")
    return path


def validate_site(site_root: Path) -> None:
    if not site_root.is_dir():
        fail(f"site directory does not exist: {site_root}")

    missing_site_files = [
        name for name in REQUIRED_SITE_FILES if not (site_root / name).is_file()
    ]
    if missing_site_files:
        fail(f"missing required files: {', '.join(missing_site_files)}")

    manifest_path = site_root / "manifest.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        fail(f"cannot read manifest.json: {error}")

    builds = manifest.get("builds")
    if not isinstance(builds, list) or not builds:
        fail("manifest must contain at least one build")

    seen_families: set[str] = set()
    for build_number, build in enumerate(builds, start=1):
        if not isinstance(build, dict):
            fail(f"build {build_number} must be an object")

        family = build.get("chipFamily")
        if not isinstance(family, str) or not family:
            fail(f"build {build_number} has no chipFamily")
        if family in seen_families:
            fail(f"duplicate chipFamily: {family}")
        seen_families.add(family)

        parts = build.get("parts")
        if not isinstance(parts, list) or not parts:
            fail(f"build {family} has no firmware parts")

        seen_offsets: set[int] = set()
        for part in parts:
            if not isinstance(part, dict):
                fail(f"build {family} contains an invalid firmware part")

            relative_path = validate_relative_path(part.get("path"))
            firmware_file = site_root.joinpath(*relative_path.parts)
            if not firmware_file.is_file() or firmware_file.stat().st_size == 0:
                fail(f"missing or empty firmware file: {relative_path}")

            offset = part.get("offset")
            if not isinstance(offset, int) or offset < 0:
                fail(f"invalid offset for {relative_path}: {offset!r}")
            if offset in seen_offsets:
                fail(f"duplicate offset in build {family}: {offset}")
            seen_offsets.add(offset)

    index_html = (site_root / "index.html").read_text(encoding="utf-8")
    if 'manifest="manifest.json"' not in index_html:
        fail("installer does not reference the packaged manifest.json relatively")

    print(
        f"Validated {len(builds)} builds and "
        f"{sum(len(build['parts']) for build in builds)} firmware parts in {site_root}"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("site_root", nargs="?", type=Path, default=Path("dist"))
    args = parser.parse_args()
    validate_site(args.site_root.resolve())


if __name__ == "__main__":
    main()
