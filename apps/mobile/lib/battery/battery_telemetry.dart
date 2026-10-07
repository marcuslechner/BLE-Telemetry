enum BatteryConnectionPhase {
  idle,
  requestingPermission,
  scanning,
  connecting,
  connected,
  permissionDenied,
  unavailable,
  error,
}

class BatteryTelemetrySnapshot {
  const BatteryTelemetrySnapshot({
    required this.phase,
    required this.message,
    this.percent,
    this.deviceName,
  });

  const BatteryTelemetrySnapshot.idle()
    : this(
        phase: BatteryConnectionPhase.idle,
        message: 'Ready to find the robot',
      );

  final BatteryConnectionPhase phase;
  final String message;
  final int? percent;
  final String? deviceName;

  bool get isBusy => switch (phase) {
    BatteryConnectionPhase.requestingPermission ||
    BatteryConnectionPhase.scanning ||
    BatteryConnectionPhase.connecting => true,
    _ => false,
  };

  bool get canRetry => switch (phase) {
    BatteryConnectionPhase.permissionDenied ||
    BatteryConnectionPhase.unavailable ||
    BatteryConnectionPhase.error => true,
    _ => false,
  };
}

abstract interface class BatteryTelemetry {
  BatteryTelemetrySnapshot get current;

  Stream<BatteryTelemetrySnapshot> get snapshots;

  Future<void> start();

  Future<void> dispose();
}
