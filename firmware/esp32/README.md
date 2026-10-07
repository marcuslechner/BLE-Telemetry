# ESP32 firmware

This directory is an ESP-IDF v6.1 project for the BLE-Telemetry gateway,
targeting the ESP32-S3-WROOM-1 module.

The connected project board reports 8 MB quad flash and 8 MB embedded octal
PSRAM. These settings are captured in `sdkconfig.defaults`.

Responsibilities:

- Receive and validate framed telemetry from the STM32N6 over UART.
- Publish selected telemetry through a versioned BLE GATT service.
- Forward requests from the companion application to the STM32N6.
- Track CRC failures, sequence gaps, UART overruns, and BLE delivery drops.

The UART framing contract is defined in
[`../../protocol/esp_link_protocol.md`](../../protocol/esp_link_protocol.md).
The firmware advertises as `BLE-Telemetry` with the standard Battery Service
(`0x180F`). Its Battery Level characteristic (`0x2A19`) supports reads and
notifications. A valid ESP Link battery state-of-charge frame updates that
characteristic, and subscribed phones are notified whenever the value changes.

## Hardware

The default UART1 mapping uses the ESP32-S3's native UART pins:

| STM32N6 | ESP32-S3-WROOM-1 | Direction |
|---|---|---|
| `PD8` / `USART3_TX` | `GPIO18` / UART1 RX | STM32 to ESP32 |
| `PD9` / `USART3_RX` | `GPIO17` / UART1 TX | ESP32 to STM32 |
| `GND` | `GND` | common reference |

The TX and RX pins and baud rate are configurable under
`menuconfig -> BLE-Telemetry gateway`. Confirm that the carrier board exposes
these module pins before wiring it. Both sides must use 3.3 V logic.

## Configure and build

Install and activate ESP-IDF v6.1, then run:

```sh
cd firmware/esp32
idf.py set-target esp32s3
idf.py menuconfig       # optional: change UART pins or baud rate
idf.py build
idf.py -p <PORT> flash monitor
```

With the STM32 connected, the current firmware should report a battery frame
once per second:

```text
I (...) ble_telemetry: battery=78% seq=0
```

After the NimBLE host synchronizes, it also reports:

```text
I (...) ble_battery: advertising as BLE-Telemetry with Battery Service 0x180F
```

## Host parser tests

The ESP Link decoder has no ESP-IDF dependencies, so its golden-vector tests
can run with the host compiler:

```sh
cc -std=c11 -Wall -Wextra -Werror \
  -I main main/esp_link.c tests/test_esp_link.c \
  -o /tmp/test_esp_link
/tmp/test_esp_link
```
