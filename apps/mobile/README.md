# Flutter Android application

This directory contains the Flutter companion application targeting Android.
The first screen automatically scans for the robot gateway and shows its live
battery state of charge.

Run it on a physical Android device with Bluetooth enabled:

```sh
flutter pub get
flutter run
```

The app requests the Android nearby-device permission, scans for the standard
BLE Battery Service (`0x180F`), connects to the first advertising gateway, and
reads/subscribes to Battery Level (`0x2A19`). See
[`../../protocol/ble_gatt_protocol.md`](../../protocol/ble_gatt_protocol.md).

Run the checks with:

```sh
flutter analyze
flutter test
```
