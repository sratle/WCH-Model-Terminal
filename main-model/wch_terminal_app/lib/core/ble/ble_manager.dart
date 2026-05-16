import 'dart:async';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../../utils/constants.dart';

class BleManager {
  BluetoothDevice? _connectedDevice;
  BluetoothCharacteristic? _rxChar; // Write
  BluetoothCharacteristic? _txChar; // Notify

  StreamSubscription<BluetoothConnectionState>? _connSub;

  final _connectionController = StreamController<BluetoothConnectionState>.broadcast();
  Stream<BluetoothConnectionState> get connectionStream => _connectionController.stream;

  final _dataController = StreamController<List<int>>.broadcast();
  Stream<List<int>> get dataStream => _dataController.stream;

  bool get isConnected => _connectedDevice != null && (_connectedDevice!.isConnected);

  BluetoothDevice? get connectedDevice => _connectedDevice;

  int _negotiatedMtu = 23;
  int get negotiatedMtu => _negotiatedMtu;

  // Auto reconnect
  bool _autoReconnect = true;
  int _reconnectAttempts = 0;
  static const int _maxReconnectAttempts = 3;
  static const Duration _reconnectInterval = Duration(seconds: 3);
  Timer? _reconnectTimer;

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
    _cancelReconnect();
    _connectedDevice = device;

    await device.connect(autoConnect: false, mtu: null);

    _connSub?.cancel();
    _connSub = device.connectionState.listen((state) {
      _connectionController.add(state);
      if (state == BluetoothConnectionState.disconnected) {
        _rxChar = null;
        _txChar = null;
        _tryAutoReconnect();
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
      _negotiatedMtu = await device.requestMtu(AppConstants.targetMtu);
    } catch (e) {
      _negotiatedMtu = 23;
    }

    // Reset reconnect counter on successful connection
    _reconnectAttempts = 0;
  }

  void _tryAutoReconnect() {
    if (!_autoReconnect || _connectedDevice == null) return;
    if (_reconnectAttempts >= _maxReconnectAttempts) {
      _connectedDevice = null;
      return;
    }
    _reconnectAttempts++;
    _reconnectTimer = Timer(_reconnectInterval, () async {
      if (_connectedDevice != null) {
        try {
          await connect(_connectedDevice!);
        } catch (e) {
          _tryAutoReconnect();
        }
      }
    });
  }

  void _cancelReconnect() {
    _reconnectTimer?.cancel();
    _reconnectTimer = null;
  }

  Future<void> disconnect() async {
    _autoReconnect = false;
    _cancelReconnect();
    _connSub?.cancel();
    _connSub = null;
    try {
      await _txChar?.setNotifyValue(false);
    } catch (_) {}
    if (_connectedDevice != null) {
      await _connectedDevice!.disconnect();
      _connectedDevice = null;
    }
    _rxChar = null;
    _txChar = null;
    _negotiatedMtu = 23;
  }

  Future<void> write(List<int> data, {bool withoutResponse = false}) async {
    if (_rxChar == null) throw Exception('RX characteristic not found');
    await _rxChar!.write(data, withoutResponse: withoutResponse);
  }

  void dispose() {
    _cancelReconnect();
    _connSub?.cancel();
    _connectionController.close();
    _dataController.close();
  }
}
