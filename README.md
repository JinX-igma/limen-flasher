# Limen Flasher

A reusable firmware and browser flasher for ESP32-family boards that talk
to an LLM through [`limen-gateway`](https://github.com/JinX-igma/limen-gateway)
— never directly to OpenAI, and never with a real API key on the device.
Supports classic ESP32, ESP32-S3, and ESP32-C3, stores only a revocable
per-device token in NVS, verifies TLS against your gateway's own
certificate, and publishes an ESP Web Tools installer through GitHub Pages.

The model runs behind your gateway (in OpenAI's cloud, or via a local
ChatGPT-subscription Codex CLI — the gateway operator's choice). The
ESP32 is a secure HTTPS client to that gateway; it does not run any
model locally and never sees an upstream API key.

## Why a gateway, not a direct API key

A key embedded in firmware you've shipped to a device is a credential
you can no longer fully protect — extracted once, it's billable compute
with no further exploit needed. `limen-gateway` holds the real
credential; this device only ever holds a token the gateway operator
can revoke in one request, with no effect on any other device or on
the upstream account itself.

## What is reused

- [ESP Web Tools](https://github.com/esphome/esp-web-tools) provides the Apache-2.0 browser/Web Serial installer.
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) handles request and response JSON.

## Browser installation

Open the repository's GitHub Pages URL in desktop Chrome or Edge, connect the board, and click the install button. The installer detects the chip family and selects the correct build.

**The published Pages build is a demo placeholder**: CI embeds a
throwaway, 1-day self-signed certificate so the firmware compiles, but
it does not trust any real gateway. To flash a device that actually
works, build locally after running `scripts/embed_gateway_ca.py`
against your own gateway's CA certificate (see "Command-line build and
flash" below).

After the first boot:

1. Join `Limen-Codex-XXXXXX` with password `limen-codex`.
2. Open `http://192.168.4.1`.
3. Enter Wi-Fi credentials, the gateway URL (e.g. `https://gateway.local:8443`), and the device token your gateway operator minted for you (`python3 scripts/manage-devices.py mint` on the gateway, or its admin API).
4. Open a serial monitor at 115200 baud and enter prompts one line at a time.

## Command-line build and flash

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html), then embed your gateway's CA before building:

```sh
python3 scripts/embed_gateway_ca.py /path/to/your/gateway/ca.crt
pio run
PORT=/dev/cu.usbserial-XXXX ENVIRONMENT=esp32dev ./scripts/flash.sh
pio device monitor -p /dev/cu.usbserial-XXXX -b 115200
```

Choose one environment:

| Chip family | PlatformIO environment |
| --- | --- |
| ESP32 | `esp32dev` |
| ESP32-S3 | `esp32-s3-devkitc-1` |
| ESP32-C3 | `esp32-c3-devkitm-1` |

Only the ESP32 family is supported today — RP2040 (UF2) and STM32 (DFU)
use fundamentally different browser flashing protocols; see
[`docs/adding-a-chip-family.md`](docs/adding-a-chip-family.md) for the
extension seam if you want to add one.

## Serial commands

| Command | Effect |
| --- | --- |
| any text | Send the line to the gateway's `POST /v1/responses` |
| `/status` | Show firmware, chip, Wi-Fi, masked-token, and gateway status |
| `/setup` | Start the local configuration portal |
| `/clear yes` | Erase stored credentials and restart |
| `/help` | Show commands |

## Security boundary

This design is suitable for personal prototypes, lab devices, and small
fleets behind a self-hosted gateway. The device token stored in ESP32
NVS can still be extracted by an attacker with physical access — but
unlike a raw API key, it is scoped to one device and revocable by the
gateway operator without touching any other device or the upstream
credential. The temporary setup portal is still local HTTP protected by
a fixed WPA2 password (`limen-codex`); this is an accepted residual
risk for a same-room setup flow, not something this project tries to
harden further.

TLS is pinned per-deployment to your gateway's own CA
(`scripts/embed_gateway_ca.py`), not to a single global root — each
operator trusts their own gateway, not a shared upstream host.

## License

MIT. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
