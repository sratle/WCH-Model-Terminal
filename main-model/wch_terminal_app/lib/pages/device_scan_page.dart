import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../providers/ble_provider.dart';

class DeviceScanPage extends ConsumerStatefulWidget {
  const DeviceScanPage({super.key});

  @override
  ConsumerState<DeviceScanPage> createState() => _DeviceScanPageState();
}

class _DeviceScanPageState extends ConsumerState<DeviceScanPage> {
  bool _isScanning = false;

  @override
  void initState() {
    super.initState();
    _startScan();
  }

  void _startScan() {
    final ble = ref.read(bleManagerProvider);
    ble.startScan();
    setState(() => _isScanning = true);
  }

  void _stopScan() {
    final ble = ref.read(bleManagerProvider);
    ble.stopScan();
    setState(() => _isScanning = false);
  }

  Future<void> _connect(BluetoothDevice device) async {
    _stopScan();
    final ble = ref.read(bleManagerProvider);
    try {
      await ble.connect(device);
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('连接失败: $e')),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final scanResults = FlutterBluePlus.scanResults;

    return Scaffold(
      appBar: AppBar(
        title: const Text('扫描设备'),
        actions: [
          if (_isScanning)
            const Padding(
              padding: EdgeInsets.all(16.0),
              child: SizedBox(
                width: 20,
                height: 20,
                child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
              ),
            )
          else
            IconButton(
              icon: const Icon(Icons.refresh),
              onPressed: _startScan,
            ),
        ],
      ),
      body: StreamBuilder<List<ScanResult>>(
        stream: scanResults,
        builder: (context, snapshot) {
          final results = snapshot.data ?? [];
          if (results.isEmpty) {
            return const Center(child: Text('未发现设备，请下拉刷新'));
          }

          return ListView.builder(
            itemCount: results.length,
            itemBuilder: (context, index) {
              final r = results[index];
              final name = r.device.advName.isNotEmpty
                  ? r.device.advName
                  : r.device.remoteId.str;
              return ListTile(
                leading: const Icon(Icons.bluetooth),
                title: Text(name),
                subtitle: Text('RSSI: ${r.rssi} dBm'),
                trailing: ElevatedButton(
                  onPressed: () => _connect(r.device),
                  child: const Text('连接'),
                ),
              );
            },
          );
        },
      ),
    );
  }

  @override
  void dispose() {
    _stopScan();
    super.dispose();
  }
}
