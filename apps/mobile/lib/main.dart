import 'package:flutter/material.dart';

import 'battery/battery_dashboard.dart';
import 'battery/reactive_ble_battery_telemetry.dart';

void main() {
  runApp(const BatteryApp());
}

class BatteryApp extends StatelessWidget {
  const BatteryApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'BD Battery',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xff22c55e),
          brightness: Brightness.dark,
        ),
        scaffoldBackgroundColor: const Color(0xff0b1220),
        useMaterial3: true,
      ),
      home: BatteryDashboard(telemetry: ReactiveBleBatteryTelemetry()),
    );
  }
}
