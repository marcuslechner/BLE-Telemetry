# Protocols

This directory owns the contracts between the three independently built
components:

1. `esp_link_protocol.md` defines UART frames exchanged by the STM32N6 and
   ESP32.
2. A future BLE GATT specification will define services and characteristics
   exchanged by the ESP32 and companion app.
3. Machine-readable constants and golden byte vectors should be added here
   once the protocols stabilize.

A protocol change is complete only when its specification, implementations,
and interoperability tests agree.
