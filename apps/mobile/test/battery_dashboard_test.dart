import 'dart:async';

import 'package:ble_telemetry/battery/battery_dashboard.dart';
import 'package:ble_telemetry/battery/battery_telemetry.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('shows the live battery state of charge', (
    WidgetTester tester,
  ) async {
    final _FakeBatteryTelemetry telemetry = _FakeBatteryTelemetry();

    await tester.pumpWidget(
      MaterialApp(home: BatteryDashboard(telemetry: telemetry)),
    );
    expect(telemetry.started, isTrue);

    telemetry.emit(
      const BatteryTelemetrySnapshot(
        phase: BatteryConnectionPhase.connected,
        message: 'Live battery state of charge',
        percent: 78,
        deviceName: 'BLE-Telemetry',
      ),
    );
    await tester.pump();

    expect(find.text('78%'), findsOneWidget);
    expect(find.text('Connected'), findsOneWidget);
    expect(find.text('BLE-Telemetry'), findsOneWidget);
  });
}

final class _FakeBatteryTelemetry implements BatteryTelemetry {
  final StreamController<BatteryTelemetrySnapshot> _controller =
      StreamController<BatteryTelemetrySnapshot>.broadcast();

  bool started = false;

  @override
  BatteryTelemetrySnapshot current = const BatteryTelemetrySnapshot.idle();

  @override
  Stream<BatteryTelemetrySnapshot> get snapshots => _controller.stream;

  void emit(BatteryTelemetrySnapshot snapshot) {
    current = snapshot;
    _controller.add(snapshot);
  }

  @override
  Future<void> start() async {
    started = true;
  }

  @override
  Future<void> dispose() => _controller.close();
}
