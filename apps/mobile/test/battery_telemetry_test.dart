import 'package:ble_telemetry/battery/reactive_ble_battery_telemetry.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('decodeBatteryLevel', () {
    test('decodes a valid state of charge', () {
      expect(ReactiveBleBatteryTelemetry.decodeBatteryLevel(<int>[78]), 78);
    });

    test('rejects values outside the GATT Battery Level contract', () {
      expect(
        () => ReactiveBleBatteryTelemetry.decodeBatteryLevel(<int>[101]),
        throwsFormatException,
      );
      expect(
        () => ReactiveBleBatteryTelemetry.decodeBatteryLevel(<int>[78, 0]),
        throwsFormatException,
      );
    });
  });
}
