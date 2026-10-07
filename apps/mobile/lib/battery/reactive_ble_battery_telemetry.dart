import 'dart:async';

import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:permission_handler/permission_handler.dart';

import 'battery_telemetry.dart';

/// Reads the Bluetooth SIG Battery Service exposed by the ESP32 gateway.
final class ReactiveBleBatteryTelemetry implements BatteryTelemetry {
  ReactiveBleBatteryTelemetry({FlutterReactiveBle? ble})
    : _ble = ble ?? FlutterReactiveBle();

  static final Uuid batteryServiceId = Uuid.parse('180f');
  static final Uuid batteryLevelId = Uuid.parse('2a19');
  static const String gatewayName = 'BLE-Telemetry';

  final FlutterReactiveBle _ble;
  final StreamController<BatteryTelemetrySnapshot> _snapshots =
      StreamController<BatteryTelemetrySnapshot>.broadcast();

  StreamSubscription<DiscoveredDevice>? _scanSubscription;
  StreamSubscription<ConnectionStateUpdate>? _connectionSubscription;
  StreamSubscription<List<int>>? _valueSubscription;
  Timer? _scanTimer;
  int _run = 0;
  bool _disposed = false;
  bool _readingBattery = false;

  @override
  BatteryTelemetrySnapshot current = const BatteryTelemetrySnapshot.idle();

  @override
  Stream<BatteryTelemetrySnapshot> get snapshots => _snapshots.stream;

  @override
  Future<void> start() async {
    if (_disposed) {
      return;
    }

    final int run = ++_run;
    await _stopOperations();
    _readingBattery = false;
    _emit(
      const BatteryTelemetrySnapshot(
        phase: BatteryConnectionPhase.requestingPermission,
        message: 'Checking Bluetooth access…',
      ),
    );

    final Map<Permission, PermissionStatus> permissions = await <Permission>[
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
    ].request();

    if (!_isCurrent(run)) {
      return;
    }
    if (permissions.values.any(
      (PermissionStatus status) => !status.isGranted,
    )) {
      _emit(
        const BatteryTelemetrySnapshot(
          phase: BatteryConnectionPhase.permissionDenied,
          message:
              'Bluetooth permission is required. Enable it in Android '
              'settings, then retry.',
        ),
      );
      return;
    }

    BleStatus status;
    try {
      status = await _ble.statusStream
          .firstWhere((BleStatus value) => value != BleStatus.unknown)
          .timeout(const Duration(seconds: 5));
    } on Object {
      _fail('Could not determine Bluetooth status. Tap retry.');
      return;
    }

    if (!_isCurrent(run)) {
      return;
    }
    if (status != BleStatus.ready) {
      _emit(
        BatteryTelemetrySnapshot(
          phase: status == BleStatus.unauthorized
              ? BatteryConnectionPhase.permissionDenied
              : BatteryConnectionPhase.unavailable,
          message: _statusMessage(status),
        ),
      );
      return;
    }

    _beginScan(run);
  }

  void _beginScan(int run) {
    _emit(
      const BatteryTelemetrySnapshot(
        phase: BatteryConnectionPhase.scanning,
        message: 'Looking for the BLE-Telemetry gateway…',
      ),
    );

    _scanTimer = Timer(const Duration(seconds: 12), () {
      if (_isCurrent(run) && current.phase == BatteryConnectionPhase.scanning) {
        unawaited(_scanSubscription?.cancel());
        _fail('No robot found. Make sure the ESP32 is powered, then retry.');
      }
    });

    _scanSubscription = _ble
        .scanForDevices(
          withServices: <Uuid>[batteryServiceId],
          scanMode: ScanMode.lowLatency,
        )
        .listen(
          (DiscoveredDevice device) {
            if (_isCurrent(run) &&
                device.name.trim() == gatewayName &&
                current.phase == BatteryConnectionPhase.scanning) {
              unawaited(_connect(device, run));
            }
          },
          onError: (Object error) {
            if (_isCurrent(run)) {
              _fail('Bluetooth scan failed. Tap retry.');
            }
          },
        );
  }

  Future<void> _connect(DiscoveredDevice device, int run) async {
    final String deviceName = device.name.trim().isEmpty
        ? 'BLE-Telemetry gateway'
        : device.name.trim();
    _scanTimer?.cancel();
    _emit(
      BatteryTelemetrySnapshot(
        phase: BatteryConnectionPhase.connecting,
        message: 'Connecting to $deviceName…',
        deviceName: deviceName,
      ),
    );
    await _scanSubscription?.cancel();
    _scanSubscription = null;

    if (!_isCurrent(run)) {
      return;
    }

    _connectionSubscription = _ble
        .connectToAdvertisingDevice(
          id: device.id,
          withServices: <Uuid>[batteryServiceId],
          prescanDuration: const Duration(seconds: 3),
          servicesWithCharacteristicsToDiscover: <Uuid, List<Uuid>>{
            batteryServiceId: <Uuid>[batteryLevelId],
          },
          connectionTimeout: const Duration(seconds: 8),
        )
        .listen(
          (ConnectionStateUpdate update) {
            if (!_isCurrent(run)) {
              return;
            }
            switch (update.connectionState) {
              case DeviceConnectionState.connecting:
                break;
              case DeviceConnectionState.connected:
                if (!_readingBattery) {
                  _readingBattery = true;
                  unawaited(_readBattery(device, deviceName, run));
                }
              case DeviceConnectionState.disconnecting:
                break;
              case DeviceConnectionState.disconnected:
                _fail('Connection lost. Tap retry to reconnect.');
            }
          },
          onError: (Object error) {
            if (_isCurrent(run)) {
              _fail('Could not connect to the robot. Tap retry.');
            }
          },
        );
  }

  Future<void> _readBattery(
    DiscoveredDevice device,
    String deviceName,
    int run,
  ) async {
    final QualifiedCharacteristic characteristic = QualifiedCharacteristic(
      serviceId: batteryServiceId,
      characteristicId: batteryLevelId,
      deviceId: device.id,
    );

    _emit(
      BatteryTelemetrySnapshot(
        phase: BatteryConnectionPhase.connected,
        message: 'Waiting for battery data…',
        deviceName: deviceName,
      ),
    );

    _valueSubscription = _ble
        .subscribeToCharacteristic(characteristic)
        .listen(
          (List<int> value) => _acceptValue(value, deviceName, run),
          onError: (Object error) {
            if (_isCurrent(run)) {
              _fail('Battery notifications stopped. Tap retry.');
            }
          },
        );

    try {
      final List<int> value = await _ble.readCharacteristic(characteristic);
      _acceptValue(value, deviceName, run);
    } on Object {
      if (_isCurrent(run) && current.percent == null) {
        _fail('The gateway battery value could not be read. Tap retry.');
      }
    }
  }

  void _acceptValue(List<int> value, String deviceName, int run) {
    if (!_isCurrent(run)) {
      return;
    }

    try {
      final int percent = decodeBatteryLevel(value);
      _emit(
        BatteryTelemetrySnapshot(
          phase: BatteryConnectionPhase.connected,
          message: 'Live battery state of charge',
          percent: percent,
          deviceName: deviceName,
        ),
      );
    } on FormatException {
      _fail('The gateway sent an invalid battery value.');
    }
  }

  static int decodeBatteryLevel(List<int> value) {
    if (value.length != 1 || value.single < 0 || value.single > 100) {
      throw const FormatException(
        'Battery level must be one byte from 0 to 100.',
      );
    }
    return value.single;
  }

  String _statusMessage(BleStatus status) => switch (status) {
    BleStatus.poweredOff => 'Bluetooth is off. Turn it on, then retry.',
    BleStatus.unauthorized =>
      'Bluetooth permission is required. Enable it in Android settings, '
          'then retry.',
    BleStatus.unsupported =>
      'This Android device does not support Bluetooth LE.',
    BleStatus.locationServicesDisabled =>
      'Location services must be on for BLE scans on this Android version.',
    _ => 'Bluetooth is not ready. Tap retry.',
  };

  bool _isCurrent(int run) => !_disposed && run == _run;

  void _fail(String message) {
    _emit(
      BatteryTelemetrySnapshot(
        phase: BatteryConnectionPhase.error,
        message: message,
      ),
    );
  }

  void _emit(BatteryTelemetrySnapshot snapshot) {
    if (_disposed) {
      return;
    }
    current = snapshot;
    _snapshots.add(snapshot);
  }

  Future<void> _stopOperations() async {
    _scanTimer?.cancel();
    _scanTimer = null;
    await _scanSubscription?.cancel();
    await _valueSubscription?.cancel();
    await _connectionSubscription?.cancel();
    _scanSubscription = null;
    _valueSubscription = null;
    _connectionSubscription = null;
  }

  @override
  Future<void> dispose() async {
    if (_disposed) {
      return;
    }
    _disposed = true;
    ++_run;
    await _stopOperations();
    await _snapshots.close();
  }
}
