import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/state/images_state.dart';
import '../providers/images_provider.dart';
import '../providers/ble_provider.dart';

class ImagesPage extends ConsumerStatefulWidget {
  const ImagesPage({super.key});

  @override
  ConsumerState<ImagesPage> createState() => _ImagesPageState();
}

class _ImagesPageState extends ConsumerState<ImagesPage> {
  @override
  void initState() {
    super.initState();
    Future.microtask(() {
      if (ref.read(isConnectedProvider)) {
        ref.read(imagesProvider.notifier).loadBmpList();
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    final state = ref.watch(imagesProvider);
    final notifier = ref.read(imagesProvider.notifier);

    return Column(
      children: [
        // Title bar
        _buildTitleBar(state, notifier),
        // Main content: list + preview
        Expanded(
          child: Row(
            children: [
              // Left: BMP file list (~35%)
              SizedBox(
                width: MediaQuery.of(context).size.width * 0.35,
                child: _buildFileList(state, notifier),
              ),
              // Right: preview area (~65%)
              Expanded(
                child: _buildPreview(state),
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _buildTitleBar(ImagesState state, ImagesNotifier notifier) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.surface,
        border: Border(bottom: BorderSide(color: Colors.grey.shade300)),
      ),
      child: Row(
        children: [
          Icon(Icons.image, color: Theme.of(context).colorScheme.primary, size: 20),
          const SizedBox(width: 8),
          const Text('BMP Browser', style: TextStyle(fontWeight: FontWeight.bold)),
          const Spacer(),
          if (state.files.isNotEmpty)
            Text('${state.files.length} files',
                style: TextStyle(color: Colors.grey.shade600, fontSize: 12)),
          const SizedBox(width: 8),
          IconButton(
            icon: const Icon(Icons.refresh, size: 18),
            onPressed: () => notifier.loadBmpList(),
            tooltip: 'Refresh',
          ),
        ],
      ),
    );
  }

  Widget _buildFileList(ImagesState state, ImagesNotifier notifier) {
    if (state.isLoading && state.files.isEmpty) {
      return const Center(child: CircularProgressIndicator());
    }

    if (state.files.isEmpty) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.image_not_supported, color: Colors.grey.shade400, size: 48),
            const SizedBox(height: 8),
            Text(
              state.error ?? 'No BMP files in \\BMP',
              style: TextStyle(color: Colors.grey.shade600),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 12),
            ElevatedButton.icon(
              onPressed: () => notifier.loadBmpList(),
              icon: const Icon(Icons.refresh, size: 16),
              label: const Text('Retry'),
            ),
          ],
        ),
      );
    }

    return Container(
      color: Colors.grey.shade50,
      child: ListView.builder(
        itemCount: state.files.length,
        itemBuilder: (context, index) {
          final file = state.files[index];
          final isSelected = index == state.selectedIndex;
          return ListTile(
            tileColor: isSelected ? Colors.blue.shade50 : null,
            selected: isSelected,
            leading: Icon(Icons.image, color: Colors.green.shade600, size: 20),
            title: Text(
              file.name,
              style: TextStyle(
                fontWeight: isSelected ? FontWeight.bold : FontWeight.normal,
                fontSize: 14,
              ),
              overflow: TextOverflow.ellipsis,
            ),
            subtitle: file.size != null
                ? Text(
                    _formatSize(file.size!),
                    style: TextStyle(color: Colors.grey.shade600, fontSize: 11),
                  )
                : null,
            onTap: () => notifier.loadBmpPreview(index),
          );
        },
      ),
    );
  }

  Widget _buildPreview(ImagesState state) {
    if (state.currentBmp == null) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            if (state.isLoading && state.selectedIndex >= 0)
              const CircularProgressIndicator()
            else ...[
              Icon(Icons.panorama, color: Colors.grey.shade300, size: 64),
              const SizedBox(height: 12),
              Text(
                'Select a BMP file to preview',
                style: TextStyle(color: Colors.grey.shade500),
              ),
            ],
            if (state.error != null && state.selectedIndex >= 0)
              Padding(
                padding: const EdgeInsets.only(top: 8),
                child: Text(state.error!,
                    style: TextStyle(color: Colors.red.shade400, fontSize: 12)),
              ),
          ],
        ),
      );
    }

    final bmp = state.currentBmp!;
    return Container(
      color: const Color(0xFF2C2C2C),
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // File name
          Text(
            bmp.name,
            style: const TextStyle(
              color: Colors.white,
              fontWeight: FontWeight.bold,
              fontSize: 16,
            ),
          ),
          const SizedBox(height: 8),
          // Info cards
          Wrap(
            spacing: 12,
            runSpacing: 8,
            children: [
              _infoChip('Size', _formatSize(bmp.fileSize)),
              if (bmp.width > 0)
                _infoChip('Dimensions', '${bmp.width} x ${bmp.height}'),
              if (bmp.bpp > 0) _infoChip('BPP', '${bmp.bpp}'),
            ],
          ),
          const SizedBox(height: 16),
          // Hex dump preview
          const Text(
            'Hex Dump (first 64 bytes)',
            style: TextStyle(color: Color(0xFFAAAAAA), fontSize: 12),
          ),
          const SizedBox(height: 4),
          Expanded(
            child: Container(
              width: double.infinity,
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: const Color(0xFF1E1E1E),
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: const Color(0xFF444444)),
              ),
              child: SingleChildScrollView(
                child: SelectableText(
                  bmp.hexPreview.isEmpty ? '(no data)' : bmp.hexPreview,
                  style: const TextStyle(
                    color: Color(0xFF88CC88),
                    fontFamily: 'monospace',
                    fontSize: 11,
                    height: 1.5,
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _infoChip(String label, String value) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
      decoration: BoxDecoration(
        color: const Color(0xFF3A3A3A),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text('$label: ',
              style: const TextStyle(color: Color(0xFF999999), fontSize: 12)),
          Text(value,
              style: const TextStyle(color: Colors.white, fontSize: 12, fontWeight: FontWeight.w500)),
        ],
      ),
    );
  }

  String _formatSize(int bytes) {
    if (bytes < 1024) return '$bytes B';
    if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(1)} KB';
    return '${(bytes / (1024 * 1024)).toStringAsFixed(1)} MB';
  }
}
