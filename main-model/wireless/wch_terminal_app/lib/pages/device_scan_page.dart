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
  String? _connectingDeviceId;

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
    setState(() => _connectingDeviceId = device.remoteId.str);
    final ble = ref.read(bleManagerProvider);
    try {
      await ble.connect(device);
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('连接成功')),
        );
        Navigator.pop(context);
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('连接失败: $e')),
        );
      }
    } finally {
      if (mounted) {
        setState(() => _connectingDeviceId = null);
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
          final allResults = snapshot.data ?? [];
          // Filter: name contains "WCH" or already has a known name
          final results = allResults.where((r) {
            final name = r.device.advName;
            return name.isNotEmpty && name.toUpperCase().contains('WCH');
          }).toList();
          if (results.isEmpty) {
            return RefreshIndicator(
              onRefresh: () async {
                _startScan();
                await Future.delayed(const Duration(seconds: 2));
              },
              child: const Center(
                child: SingleChildScrollView(
                  physics: AlwaysScrollableScrollPhysics(),
                  child: Padding(
                    padding: EdgeInsets.only(top: 100),
                    child: Text('未发现设备，请下拉刷新'),
                  ),
                ),
              ),
            );
          }

          return RefreshIndicator(
            onRefresh: () async {
              _startScan();
              await Future.delayed(const Duration(seconds: 2));
            },
            child: ListView.builder(
              physics: const AlwaysScrollableScrollPhysics(),
              itemCount: results.length,
              itemBuilder: (context, index) {
                final r = results[index];
                final name = r.device.advName.isNotEmpty
                    ? r.device.advName
                    : r.device.remoteId.str;
                final isConnecting = _connectingDeviceId == r.device.remoteId.str;
                return ListTile(
                  leading: const Icon(Icons.bluetooth),
                  title: Text(name),
                  subtitle: Text('RSSI: ${r.rssi} dBm'),
                  trailing: ElevatedButton(
                    onPressed: isConnecting ? null : () => _connect(r.device),
                    child: isConnecting
                        ? const SizedBox(
                            width: 16,
                            height: 16,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          )
                        : const Text('连接'),
                  ),
                );
              },
            ),
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
