# BLE-Telemetry

BLE-Telemetry contains the ESP32 Bluetooth Low Energy gateway firmware and
the Apple companion application for the BD robot.

```text
STM32N6 Cerebellum <-- UART / ESP Link --> ESP32 <-- BLE --> iPhone or iPad
```

The ESP32 receives robot telemetry from the Cerebellum, exposes it over BLE,
and forwards app requests back to the STM32N6.

## Repository layout

- `firmware/esp32/` — ESP-IDF firmware.
- `apps/ios/` — Swift/SwiftUI companion application.
- `protocol/` — wire protocols, assigned identifiers, and interoperability
  test vectors shared by the firmware and app.

## Current status

The repository has its initial project structure. The ESP-IDF application
builds to a minimal startup log once ESP-IDF is installed. BLE services,
UART reception, and the iOS project are not implemented yet.

The current STM32-to-ESP32 packet definition is in
[`protocol/esp_link_protocol.md`](protocol/esp_link_protocol.md).

## ESP32 firmware

Install and activate Espressif ESP-IDF, then select the exact chip used by the
board and build:

```sh
cd firmware/esp32
idf.py set-target <esp32-target>
idf.py build
```

Do not choose a target until the ESP32 board/module has been selected.

## iOS application

Create the Xcode SwiftUI application in `apps/ios/` using `BLETelemetry` as
the product name. The Xcode project should be committed, while user-specific
workspace state and build products are ignored.

## Protocol ownership

Documents and test vectors under `protocol/` are the canonical interface for
this repository. Protocol changes must also update the corresponding STM32
definitions in `Cerebellum_STM32N6` and should preserve byte-level test
vectors on both sides.
