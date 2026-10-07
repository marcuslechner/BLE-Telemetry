# Flutter Android application

This directory will contain the Flutter companion application targeting
Android.

After installing Flutter and the Android SDK, generate the project here:

```sh
flutter create --platforms=android --org ca.lechnology --project-name ble_telemetry .
```

The initial app should scan for the BLE-Telemetry gateway, connect, display
connection diagnostics, and decode the first battery state-of-charge value.
Define the BLE GATT contract under `protocol/` before implementing service and
characteristic UUIDs in either the app or firmware.
