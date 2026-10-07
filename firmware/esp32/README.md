# ESP32 firmware

This directory is an ESP-IDF project for the BLE-Telemetry gateway.

Planned responsibilities:

- Receive and validate framed telemetry from the STM32N6 over UART.
- Publish selected telemetry through a versioned BLE GATT service.
- Forward requests from the companion application to the STM32N6.
- Track CRC failures, sequence gaps, UART overruns, and BLE delivery drops.

The UART framing contract is defined in
[`../../protocol/esp_link_protocol.md`](../../protocol/esp_link_protocol.md).
Pin assignments and the ESP-IDF target remain intentionally unset until the
ESP32 module and board are selected.
