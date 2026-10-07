# iOS application

This directory will contain the Swift/SwiftUI companion application.

Create the app in Xcode with:

- Product name: `BLETelemetry`
- Interface: SwiftUI
- Language: Swift

The initial app should scan for the BLE-Telemetry gateway, connect, display
connection diagnostics, and decode the first battery state-of-charge value.
Define the BLE GATT contract under `protocol/` before implementing service and
characteristic UUIDs in either the app or firmware.
