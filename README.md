# D Flux ESP32 Acquisition Firmware

Firmware for the ESP32-based probe used by the D Flux application to capture
CO₂ concentration, ambient temperature, and relative humidity while controlling
auxiliary hardware (servo-driven flux chamber) and streaming data via Bluetooth
Low Energy (BLE).

This repository contains the PlatformIO project that pairs with the
[D Flux client](https://github.com/le0o0oo/d-flux) built with Tauri 2 +
Vue 3. The client discovers the probe over BLE, starts measurement sessions,
adds GPS metadata, performs analysis, and exports datasets.

## Table of Contents

1. [Hardware Overview](#hardware-overview)
2. [Firmware Features](#firmware-features)
3. [BLE Service & Protocol](#ble-service--protocol)
4. [Build & Flash](#build--flash)
5. [Configuration & Calibration](#configuration--calibration)
6. [Troubleshooting](#troubleshooting)
7. [Project Structure](#project-structure)
8. [Companion Application](#companion-application)
9. [License](#license)

## Hardware Overview

- **MCU**: ESP32-DevKitC (or compatible) running the Arduino framework.
- **Sensor**: Adafruit SCD30 (CO₂, temperature, humidity) wired over I²C.
- **Actuator**: Servo connected to `SERVO_PIN` (GPIO 12 by default) for driving
  a flux chamber lid or pump.
- **Status LED**: Simple connection on `LED_PIN` (GPIO 2) for connection state.

## Firmware Features

### Data pipeline

- Initializes the SCD30 sensor, checks readiness, and continuously polls for new
  measurements.
- Streams readings as line-based `DATA` packets when the measurement session is
  active and a BLE central is connected.

### Bluetooth Low Energy

- Uses [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) for a lean BLE
  stack.
- Advertises manufacturer data with probe metadata (`ID`, `ORG`, `FW`) to help
  the client filter compatible devices before connecting.
- Exposes a single custom service with RX/TX characteristics for command/control
  (details below).

### Command handling

- Accepts text-based commands to start/stop acquisition, read/write calibration
  offsets, trigger servo motion, and manage BLE connections.
- Persists configurable calibration options (e.g., sensor calibration multiplier) in NVS via
  the `Preferences` API so they survive power cycles.

### Servo control

- Uses the ESP32 LEDC peripheral to drive a servo with a dedicated PWM channel.
- Spins the servo when acquisition starts and stops it when acquisition ends
  (hooks are in `commands.cpp`).

### Status indication

- LED heartbeat shows pairing state: slow blinking while advertising and OFF when connected.

## BLE Service & Protocol

| Item              | UUID / Description                                                      |
| ----------------- | ----------------------------------------------------------------------- |
| Service UUID      | `DB594551-159C-4DA8-B59E-1C98587348E1`                                  |
| RX Characteristic | `7B6B12CD-CA54-46A6-B3F4-3A848A3ED00B` (WRITE, WRITE_NR) – App → ESP32  |
| TX Characteristic | `907BAC5D-92ED-4D90-905E-A3A7B9899F21` (READ, NOTIFY) – ESP32 → App     |
| Manufacturer data | `companyId=0xB71E` + `ID=<DEVICE_ID>;ORG=<DEVICE_ORG>;FW=<DEVICE_FW>`    |

### Command list

| Command                 | Direction | Description |
| ----------------------- | --------- | ----------- |
| `WHOIS`                 | App → ESP32 | Returns probe metadata (ID/ORG/FW). |
| `START_ACQUISITION`     | App → ESP32 | Enables servo spin and begins sensor streaming. |
| `STOP_ACQUISITION`      | App → ESP32 | Stops servo and pauses streaming. |
| `GET_ACQUISITION_STATE` | App → ESP32 | Returns `0`/`1` depending on active session. |
| `DISCONNECT`            | App → ESP32 | Forces current BLE link to close. |
| `GET_SETTINGS`          | App → ESP32 | Reads stored settings (offset, multiplier). |
| `SET_SETTINGS key=val;` | App → ESP32 | Updates persisted settings. Multiple `key=value;` pairs allowed. |
| `SET_HW_CALIBRATION_REF <value>` | App → ESP32 | Forces SCD30 calibration reference (400-2000 ppm) and confirms. |
| `GET_HW_CALIBRATION_REF`| App → ESP32 | Returns current forced calibration reference from the sensor. |

### Data frames

ESP32 sends newline-terminated frames shaped as `HEADER PAYLOAD\n`.

Examples:

```
DATA CO2=400.12;TMP=24.50;HUM=40.10
WHOIS ID=ESP32_01;ORG=INGV;FW=1.0
```

## Build & Flash

### Prerequisites

- [PlatformIO CLI](https://platformio.org/install/cli) or VS Code + PlatformIO
  extension.
- Python 3.9+ (installed automatically with PlatformIO).
- USB cable and access to the ESP32 serial port.

### Dependencies

Declared in `platformio.ini`:

- `adafruit/Adafruit SCD30@^1.0.11`
- `h2zero/NimBLE-Arduino@^2.3.7`

### Typical workflow

```bash
# 1. Install libraries and build
pio run

# 2. Flash firmware (set --upload-port if needed)
pio run --target upload

# 3. Open serial monitor at 115200 baud
pio device monitor
```

If you prefer VS Code, open the folder with the PlatformIO extension and use the
"Build", "Upload", and "Monitor" tasks.

> **Clangd tip**: If IntelliSense loses track of includes, regenerate the
> compilation database and restart clangd:
> ```bash
> pio run --target compiledb
> ```

## Configuration & Calibration

- **Device identity**: Update `deviceName`, `DEVICE_ID`, and `DEVICE_ORG` in `src/config.cpp` to match your deployment so the client can
  display meaningful metadata.
- **Persistent settings**: `offset` and `multiplier` can be changed directly from
  the D Flux app’s device settings UI; the firmware stores the values in NVS and
  echoes them in `SETTINGS` frames. Manual `SET_SETTINGS offset=12.5;...` commands
  remain available for debugging or third-party tools.
- **Hardware calibration**: Use the companion app’s calibration panel (preferred)
  or send `SET_HW_CALIBRATION_REF <ppm>` with a known CO₂ reference between 400
  and 2000 ppm. The firmware applies `Adafruit_SCD30::forceRecalibrationWithReference`
  and returns the new value.
- **Servo tuning**: `SERVO_PIN`, PWM frequency, resolution, and pulse widths are
  defined in `include/config.h` / `src/servo.cpp`. Adjust to match your actuator.

## Troubleshooting

| Symptom | Possible fix |
| ------- | ------------- |
| Serial log shows `Failed to find SCD30` | Check I²C wiring (SDA/SCL), ensure sensor is powered, and confirm the address (0x61). |
| BLE client cannot find the probe | Verify advertising is running (`BLE Advertising started` in logs), ensure no lingering bonds (the firmware deletes all on boot), and confirm device is powered. |
| No `DATA` frames are received | Confirm `START_ACQUISITION` was issued, check that the SCD30 has fresh data (`dataReady()`), and verify connection is active. |
| Servo spins unexpectedly | Inspect command log; `START_ACQUISITION` automatically attaches the servo. Modify `commands.cpp` if you need decoupled control. |

## Project Structure

```
├── include/        # Public headers (config, BLE, sensor, servo)
├── src/
│   ├── bluetooth.cpp   # Advertising + GATT setup
│   ├── commands.cpp    # Command parsing and routing
│   ├── config.cpp      # Device metadata + settings catalog
│   ├── main.cpp        # Entry point, loop, LED heartbeat
│   ├── sensor.cpp      # SCD30 setup and sampling
│   └── servo.cpp       # Servo helpers (start/stop)
├── platformio.ini  # Target, libs, and build flags
└── README.md
```

## Companion Application

Pair this firmware with the D Flux Tauri/Vue application:

- BLE scanning, GPS enrichment, CSV export, regression analysis, and mapping are
  all handled on the desktop/mobile side.
- Ensure the app’s BLE UUIDs match the constants in `include/config.h`. If you
  change UUIDs here, update the client accordingly.
- See the client README for installation and usage instructions.

## License

MIT License. See [LICENSE](LICENSE) for details.