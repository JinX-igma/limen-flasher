# Limen ESP32 Codex Flasher

A reusable firmware and browser flasher that connects ESP32-family boards to the OpenAI Responses API. It supports classic ESP32, ESP32-S3, and ESP32-C3 boards, stores runtime credentials in NVS, verifies TLS, and publishes an ESP Web Tools installer through GitHub Pages.

The model runs in OpenAI's cloud. The ESP32 is a secure HTTPS client; it does not run the Codex model locally.

## What is reused

- [ESP Web Tools](https://github.com/esphome/esp-web-tools) provides the Apache-2.0 browser/Web Serial installer.
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) handles request and response JSON.
- The TLS trust-anchor pattern and GlobalSign root certificate were cross-checked against the MIT-licensed [ESPAI](https://github.com/enkei0x/espai) project. ESPAI itself uses Chat Completions, so this firmware implements the Responses API directly.

## Browser installation

Open the repository's GitHub Pages URL in desktop Chrome or Edge, connect the board, and click the install button. The installer detects the chip family and selects the correct build.

After the first boot:

1. Join `Limen-Codex-XXXXXX` with password `limen-codex`.
2. Open `http://192.168.4.1`.
3. Enter Wi-Fi credentials, an OpenAI API key, and a model ID.
4. Open a serial monitor at 115200 baud and enter prompts one line at a time.

The default model is `gpt-5.3-codex`. Use `/model MODEL_ID` to change it.

## Command-line build and flash

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html), then:

```sh
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

## Serial commands

| Command | Effect |
| --- | --- |
| any text | Send the line to `POST /v1/responses` |
| `/status` | Show firmware, chip, Wi-Fi, masked-key, and model status |
| `/setup` | Start the local configuration portal |
| `/model ID` | Save a different model ID |
| `/clear yes` | Erase stored credentials and restart |
| `/help` | Show commands |

## Security boundary

This design is suitable for personal prototypes and controlled lab devices. An API key stored in ESP32 NVS can be extracted by an attacker with physical access, and the temporary setup portal is local HTTP protected by a fixed WPA2 password. Use a dedicated OpenAI project key, restrict its permissions, and set a low project spend limit. Production or distributed devices should call a backend proxy that authenticates devices and keeps the OpenAI key server-side.

The embedded GlobalSign root certificate expires on 2028-01-28. A scheduled GitHub Action verifies the live `api.openai.com` certificate chain weekly so maintainers receive an early failure if OpenAI's chain changes.

## API behavior

Requests use `POST https://api.openai.com/v1/responses`, `store: false`, low reasoning effort, and a 512-token output cap. The parser scans every message/content item for `output_text` rather than assuming a fixed position in the response array.

## License

MIT. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
