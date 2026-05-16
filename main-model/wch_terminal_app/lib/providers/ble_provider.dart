import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/ble/ble_manager.dart';

final bleManagerProvider = Provider<BleManager>((ref) {
  final manager = BleManager();
  ref.onDispose(() => manager.dispose());
  return manager;
});

final connectionStateProvider = StreamProvider<BluetoothConnectionState>((ref) {
  final ble = ref.watch(bleManagerProvider);
  return ble.connectionStream;
});

final isConnectedProvider = Provider<bool>((ref) {
  final state = ref.watch(connectionStateProvider);
  return state.valueOrNull == BluetoothConnectionState.connected;
});
