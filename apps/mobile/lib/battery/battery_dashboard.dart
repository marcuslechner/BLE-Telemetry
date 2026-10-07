import 'dart:async';

import 'package:flutter/material.dart';

import 'battery_telemetry.dart';

class BatteryDashboard extends StatefulWidget {
  const BatteryDashboard({required this.telemetry, super.key});

  final BatteryTelemetry telemetry;

  @override
  State<BatteryDashboard> createState() => _BatteryDashboardState();
}

class _BatteryDashboardState extends State<BatteryDashboard> {
  late BatteryTelemetrySnapshot _snapshot;
  late final StreamSubscription<BatteryTelemetrySnapshot> _subscription;

  @override
  void initState() {
    super.initState();
    _snapshot = widget.telemetry.current;
    _subscription = widget.telemetry.snapshots.listen((
      BatteryTelemetrySnapshot snapshot,
    ) {
      if (mounted) {
        setState(() => _snapshot = snapshot);
      }
    });
    unawaited(widget.telemetry.start());
  }

  @override
  void dispose() {
    unawaited(_subscription.cancel());
    unawaited(widget.telemetry.dispose());
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final int? percent = _snapshot.percent;
    final Color batteryColor = _batteryColor(percent);

    return Scaffold(
      appBar: AppBar(
        title: const Text('BD Battery'),
        backgroundColor: Colors.transparent,
      ),
      body: SafeArea(
        child: Center(
          child: SingleChildScrollView(
            padding: const EdgeInsets.all(24),
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 420),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: <Widget>[
                  _StatusPill(snapshot: _snapshot),
                  const SizedBox(height: 32),
                  Semantics(
                    label: percent == null
                        ? 'Battery level unavailable'
                        : 'Battery level $percent percent',
                    child: SizedBox.square(
                      dimension: 220,
                      child: Stack(
                        alignment: Alignment.center,
                        children: <Widget>[
                          SizedBox.square(
                            dimension: 210,
                            child: CircularProgressIndicator(
                              value: percent == null
                                  ? (_snapshot.isBusy ? null : 0)
                                  : percent / 100,
                              strokeWidth: 15,
                              strokeCap: StrokeCap.round,
                              color: batteryColor,
                              backgroundColor: Colors.white12,
                            ),
                          ),
                          Column(
                            mainAxisSize: MainAxisSize.min,
                            children: <Widget>[
                              Icon(
                                _batteryIcon(percent),
                                size: 48,
                                color: batteryColor,
                              ),
                              const SizedBox(height: 8),
                              Text(
                                percent == null ? '--%' : '$percent%',
                                style: Theme.of(context).textTheme.displayMedium
                                    ?.copyWith(fontWeight: FontWeight.w700),
                              ),
                            ],
                          ),
                        ],
                      ),
                    ),
                  ),
                  const SizedBox(height: 32),
                  Text(
                    _snapshot.message,
                    textAlign: TextAlign.center,
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                  if (_snapshot.deviceName case final String name) ...<Widget>[
                    const SizedBox(height: 8),
                    Text(
                      name,
                      style: Theme.of(context).textTheme.bodyMedium
                          ?.copyWith(color: Colors.white60),
                    ),
                  ],
                  if (_snapshot.canRetry) ...<Widget>[
                    const SizedBox(height: 24),
                    FilledButton.icon(
                      onPressed: () => unawaited(widget.telemetry.start()),
                      icon: const Icon(Icons.refresh),
                      label: const Text('Retry'),
                    ),
                  ],
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }

  Color _batteryColor(int? percent) {
    if (percent == null) {
      return const Color(0xff60a5fa);
    }
    if (percent <= 20) {
      return const Color(0xffef4444);
    }
    if (percent <= 40) {
      return const Color(0xfff59e0b);
    }
    return const Color(0xff22c55e);
  }

  IconData _batteryIcon(int? percent) {
    if (percent == null) {
      return Icons.battery_unknown;
    }
    if (percent <= 20) {
      return Icons.battery_1_bar;
    }
    if (percent <= 50) {
      return Icons.battery_3_bar;
    }
    if (percent <= 80) {
      return Icons.battery_5_bar;
    }
    return Icons.battery_full;
  }
}

class _StatusPill extends StatelessWidget {
  const _StatusPill({required this.snapshot});

  final BatteryTelemetrySnapshot snapshot;

  @override
  Widget build(BuildContext context) {
    final (String label, Color color) = switch (snapshot.phase) {
      BatteryConnectionPhase.connected => (
        'Connected',
        const Color(0xff22c55e),
      ),
      BatteryConnectionPhase.error ||
      BatteryConnectionPhase.permissionDenied ||
      BatteryConnectionPhase.unavailable => (
        'Disconnected',
        const Color(0xffef4444),
      ),
      BatteryConnectionPhase.idle => ('Idle', const Color(0xff94a3b8)),
      _ => ('Connecting', const Color(0xff60a5fa)),
    };

    return DecoratedBox(
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.14),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: color.withValues(alpha: 0.45)),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 8),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            Container(
              width: 9,
              height: 9,
              decoration: BoxDecoration(color: color, shape: BoxShape.circle),
            ),
            const SizedBox(width: 8),
            Text(label),
          ],
        ),
      ),
    );
  }
}
