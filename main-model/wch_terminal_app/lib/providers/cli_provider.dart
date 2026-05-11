import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/ble/ble_manager.dart';
import '../core/cli/cli_engine.dart';
import 'ble_provider.dart';

final cliEngineProvider = Provider<CliEngine>((ref) {
  final ble = ref.watch(bleManagerProvider);
  final engine = CliEngine(ble: ble);
  ref.onDispose(() => engine.dispose());
  return engine;
});
