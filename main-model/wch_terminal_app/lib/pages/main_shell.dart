import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'terminal_page.dart';
import 'file_browser_page.dart';
import 'ebook_page.dart';
import 'images_page.dart';
import 'device_scan_page.dart';
import '../providers/ble_provider.dart';

class MainShell extends ConsumerStatefulWidget {
  const MainShell({super.key});

  @override
  ConsumerState<MainShell> createState() => _MainShellState();
}

class _MainShellState extends ConsumerState<MainShell> {
  int _currentIndex = 1;

  final _pages = const [
    TerminalPage(),
    FileBrowserPage(),
    EBookPage(),
    ImagesPage(),
  ];

  @override
  Widget build(BuildContext context) {
    final isConnected = ref.watch(isConnectedProvider);

    return Scaffold(
      body: Column(
        children: [
          if (!isConnected)
            _DisconnectedBanner(
              onTap: () => Navigator.push(
                context,
                MaterialPageRoute(builder: (_) => const DeviceScanPage()),
              ),
            ),
          Expanded(
            child: IndexedStack(
              index: _currentIndex,
              children: _pages,
            ),
          ),
        ],
      ),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _currentIndex,
        onDestinationSelected: (i) => setState(() => _currentIndex = i),
        destinations: const [
          NavigationDestination(
            icon: Icon(Icons.terminal),
            label: '终端',
          ),
          NavigationDestination(
            icon: Icon(Icons.folder),
            label: '文件',
          ),
          NavigationDestination(
            icon: Icon(Icons.menu_book),
            label: '阅读',
          ),
          NavigationDestination(
            icon: Icon(Icons.image),
            label: '图片',
          ),
        ],
      ),
    );
  }
}

class _DisconnectedBanner extends StatelessWidget {
  final VoidCallback onTap;

  const _DisconnectedBanner({required this.onTap});

  @override
  Widget build(BuildContext context) {
    final topPadding = MediaQuery.of(context).padding.top;
    return GestureDetector(
      onTap: onTap,
      child: Container(
        width: double.infinity,
        color: Colors.orange.shade700,
        padding: EdgeInsets.only(left: 16, right: 16, top: topPadding + 8, bottom: 12),
        child: const Row(
          children: [
            Icon(Icons.bluetooth_disabled, color: Colors.white, size: 20),
            SizedBox(width: 8),
            Expanded(
              child: Text(
                '未连接设备，点击此处扫描并连接',
                style: TextStyle(color: Colors.white, fontSize: 14),
              ),
            ),
            Icon(Icons.chevron_right, color: Colors.white, size: 20),
          ],
        ),
      ),
    );
  }
}
