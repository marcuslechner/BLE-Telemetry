# BLE-Telemetry

BLE-Telemetry contains the ESP32 Bluetooth Low Energy gateway firmware and
the Flutter Android companion application for the BD robot.

```text
STM32N6 Cerebellum <-- UART / ESP Link --> ESP32 <-- BLE --> Android device
```

The ESP32 receives robot telemetry from the Cerebellum, exposes it over BLE,
and forwards app requests back to the STM32N6.

## Repository layout

- `firmware/esp32/` — ESP-IDF firmware.
- `apps/mobile/` — Flutter companion application targeting Android.
- `protocol/` — wire protocols, assigned identifiers, and interoperability
  test vectors shared by the firmware and app.

## Current status

The ESP32-S3 firmware receives and validates ESP Link UART frames and reports
the implemented battery message. The Flutter Android app scans for the standard
BLE Battery Service and displays its Battery Level value. The ESP32 GATT server
is the remaining link needed for an end-to-end reading.

The current STM32-to-ESP32 packet definition is in
[`protocol/esp_link_protocol.md`](protocol/esp_link_protocol.md).

## ESP32 firmware

Install and activate Espressif ESP-IDF v6.1, then select the ESP32-S3 target
and build:

```sh
cd firmware/esp32
idf.py set-target esp32s3
idf.py build
```

## Android application

After installing Flutter with Android tooling, run the application from
`apps/mobile/`:

```sh
cd apps/mobile
flutter pub get
flutter run
```

Commit the generated Flutter project, including `pubspec.lock`; local SDK
paths, tool caches, and build products are ignored.

## Protocol ownership

Documents and test vectors under `protocol/` are the canonical interface for
this repository. Protocol changes must also update the corresponding STM32
definitions in `Cerebellum_STM32N6` and should preserve byte-level test
vectors on both sides.
