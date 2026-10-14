# Adding a chip family (not implemented in v1)

v1 ships ESP32, ESP32-S3, and ESP32-C3 only, because browser flashing
protocols differ fundamentally by chip family — there is no single
"flash anything" browser API:

| Family | Protocol | Browser API |
| --- | --- | --- |
| ESP32 / ESP8266 | esptool serial bootloader protocol | Web Serial (via [ESP Web Tools](https://github.com/esphome/esp-web-tools)) |
| RP2040 | UF2 — drag a file onto a mass-storage volume exposed in BOOTSEL mode | File System Access API / mass storage, not Web Serial |
| STM32 | USB DFU class | WebUSB + the DFU protocol, not Web Serial |

Adding a family means adding a new packaging/flashing backend, not
extending the existing one. The seams to extend:

- **Firmware build**: add a PlatformIO environment in `platformio.ini`
  (or a separate build system entirely, e.g. the Pico SDK/CMake for
  RP2040) targeting the same `src/main.cpp` logic — the captive-portal
  provisioning flow and the gateway HTTP contract (`/v1/responses` with
  a bearer device token) are chip-agnostic; only the networking/TLS/NVS
  primitives (`WiFiClientSecure`, `HTTPClient`, `Preferences`) are
  ESP32-Arduino-specific and would need a per-platform equivalent.
- **Packaging**: `scripts/package_web.py`'s `BUILDS` dict maps a
  PlatformIO environment to an ESP Web Tools `chipFamily` and
  bootloader offset — this is ESP-specific. A new family needs its own
  packaging step (e.g. producing a `.uf2` file for RP2040, or a `.dfu`
  file for STM32) and its own entry point in `site/index.html` (ESP Web
  Tools' `<esp-web-install-button>` only understands the ESP manifest
  format; a different family needs a different installer widget).
- **CA embedding**: `scripts/embed_gateway_ca.py` and the
  `gateway_root_ca.h` pattern are TLS-stack-specific
  (`WiFiClientSecure::setCACert`). A non-Arduino-ESP32 platform would
  need the equivalent call for whatever TLS stack it uses (commonly
  mbedTLS directly).

None of this is implemented today. If you want to add RP2040 or STM32
support, start with the packaging/flashing backend — that's the part
with no existing code to reuse, not the firmware logic.
