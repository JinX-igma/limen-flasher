#!/usr/bin/env bash
set -euo pipefail

environment="${ENVIRONMENT:-esp32dev}"
port="${PORT:-}"

if ! command -v pio >/dev/null 2>&1; then
  echo "PlatformIO is required: https://platformio.org/install/cli" >&2
  exit 1
fi

if [[ -z "${port}" ]]; then
  port="$(pio device list --json-output | python3 -c 'import json,sys; ports=json.load(sys.stdin); candidates=[p["port"] for p in ports if "Bluetooth" not in p["port"] and "debug-console" not in p["port"]]; print(candidates[0] if len(candidates)==1 else "")')"
fi

if [[ -z "${port}" ]]; then
  echo "Could not select one serial port. Set PORT=/dev/your-port." >&2
  exit 2
fi

echo "Building ${environment} and flashing ${port}"
pio run -e "${environment}" -t upload --upload-port "${port}"
echo "Flash complete. Open the monitor with: pio device monitor -p '${port}' -b 115200"
