import 'dart:async';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../../utils/constants.dart';

class BleManager {
  BluetoothDevice? _connectedDevice;
  BluetoothCharacteristic? _rxChar; // Write
  BluetoothCharacteristic? _txChar; // Notify

  final _connectionController = StreamController<BluetoothConnectionState>.broadcast();
  Stream<BluetoothConnectionState> get connectionStream => _connectionController.stream;

  final _dataController = StreamController<List<int>>.broadcast();
  Stream<List<int>> get dataStream => _dataController.stream;

  bool get isConnected => _connectedDevice != null &&
      (_connectedDevice!.isConnected);

  BluetoothDevice? get connectedDevice => _connectedDevice;

  Stream<List<ScanResult>> startScan({Duration? timeout}) {
    FlutterBluePlus.startScan(
      withServices: [Guid(AppConstants.bleServiceUuid)],
      timeout: timeout ?? const Duration(seconds: 10),
    );
    return FlutterBluePlus.scanResults;
  }

  void stopScan() {
    FlutterBluePlus.stopScan();
  }

  Future<void> connect(BluetoothDevice device) async {
    _connectedDevice = device;

    await device.connect(autoConnect: false, mtu: null);

    device.connectionState.listen((state) {
      _connectionController.add(state);
      if (state == BluetoothConnectionState.disconnected) {
        _rxChar = null;
        _txChar = null;
      }
    });

    // Discover services
    final services = await device.discoverServices();
    for (final service in services) {
      if (service.uuid.str.toLowerCase() == AppConstants.bleServiceUuid) {
        for (final char in service.characteristics) {
          final uuid = char.uuid.str.toLowerCase();
          if (uuid == AppConstants.bleRxCharUuid) {
            _rxChar = char;
          } else if (uuid == AppConstants.bleTxCharUuid) {
            _txChar = char;
            await char.setNotifyValue(true);
            char.onValueReceived.listen((value) {
              _dataController.add(value);
            });
          }
        }
      }
    }

    // Request MTU
    try {
      await device.requestMtu(AppConstants.targetMtu);
    } catch (e) {
      // Fallback handled by framework
    }
  }

  Future<void> disconnect() async {
    if (_connectedDevice != null) {
      await _connectedDevice!.disconnect();
      _connectedDevice = null;
      _rxChar = null;
      _txChar = null;
    }
  }

  Future<void> write(List<int> data, {bool withoutResponse = false}) async {
    if (_rxChar == null) throw Exception('RX characteristic not found');
    await _rxChar!.write(data, withoutResponse: withoutResponse);
  }

  void dispose() {
    _connectionController.close();
    _dataController.close();
  }
}
