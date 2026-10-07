# BLE GATT Protocol

This document defines the Bluetooth Low Energy interface between the ESP32
gateway and the Android companion app.

## Advertising

The ESP32 advertises with the local name `BLE-Telemetry` and includes the
Bluetooth SIG Battery Service UUID (`0x180F`) in its advertised service list.
Advertising the service UUID lets the app use a service-filtered scan. Because
the standard Battery Service is also used by other devices, the app then
selects the result whose advertised local name is exactly `BLE-Telemetry`.

## Battery Service

The initial interface uses the standard Bluetooth SIG Battery Service rather
than a custom service.

| Item | UUID | Properties | Value |
|---|---|---|---|
| Battery Service | `0x180F` | — | — |
| Battery Level | `0x2A19` | Read, Notify | One `uint8` percentage from 0 to 100 |

The ESP32 updates Battery Level whenever it receives a valid ESP Link
`INFO/BATTERY_SOC` frame. While a phone is connected, it sends a notification
when the value changes. The current value must also be available through a GATT
read immediately after connection.

The current STM32 value is a hardcoded 78 percent placeholder, so an end-to-end
test is expected to display `78%` until real battery measurement is added.
