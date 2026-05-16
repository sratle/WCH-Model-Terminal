import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/state/file_state.dart';
import '../core/state/music_state.dart';
import '../providers/ble_provider.dart';
import '../providers/file_provider.dart';
import '../providers/music_provider.dart';
import '../utils/constants.dart';
import 'music_player_sheet.dart';
import 'text_editor_page.dart';

class FileBrowserPage extends ConsumerStatefulWidget {
  const FileBrowserPage({super.key});

  @override
  ConsumerState<FileBrowserPage> createState() => _FileBrowserPageState();
}

class _FileBrowserPageState extends ConsumerState<FileBrowserPage> {
  @override
  void initState() {
    super.initState();
    Future.microtask(() {
      if (ref.read(isConnectedProvider)) {
        ref.read(fileProvider.notifier).refresh();
      }
    });
  }

  IconData _fileIcon(FileItem item) {
    if (item.isDirectory) return Icons.folder;
    final ext = item.name.contains('.') ? item.name.split('.').last.toLowerCase() : '';
    switch (ext) {
      case 'wav':
        return Icons.music_note;
      case 'txt':
      case 'md':
      case 'json':
      case 'c':
      case 'h':
        return Icons.description;
      case 'bmp':
      case 'png':
      case 'jpg':
        return Icons.image;
      default:
        return Icons.insert_drive_file;
    }
  }

  Color _fileIconColor(FileItem item) {
    if (item.isDirectory) return Colors.amber.shade700;
    final ext = item.name.contains('.') ? item.name.split('.').last.toLowerCase() : '';
    switch (ext) {
      case 'wav':
        return Colors.deepPurple;
      case 'txt':
      case 'md':
        return Colors.blue;
      case 'json':
        return Colors.orange;
      case 'c':
      case 'h':
        return Colors.teal;
      case 'bmp':
      case 'png':
      case 'jpg':
        return Colors.green;
      default:
        return Colors.grey;
    }
  }

  String _formatSize(int bytes) {
    if (bytes < 1024) return '$bytes B';
    if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(1)} KB';
    return '${(bytes / (1024 * 1024)).toStringAsFixed(1)} MB';
  }

  void _showNeedConnection() {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('未连接设备，请先扫描并连接')),
    );
  }

  Future<void> _onItemTap(FileItem item) async {
    final isConnected = ref.read(isConnectedProvider);
    if (!isConnected) {
      _showNeedConnection();
      return;
    }

    if (item.isDirectory) {
      await ref
          .read(fileProvider.notifier)
          .enterDirectory(item.name);
      return;
    }

    final ext = item.name.contains('.') ? item.name.split('.').last.toLowerCase() : '';

    if (ext == 'wav') {
      if (mounted) {
        final fsState = ref.read(fileProvider);
        final dir = fsState.currentDir;
        final wavTracks = <MusicTrack>[];
        int wavIndex = -1;
        if (dir != null) {
          for (int i = 0; i < dir.items.length; i++) {
            final fi = dir.items[i];
            final fiExt = fi.name.contains('.')
                ? fi.name.split('.').last.toLowerCase()
                : '';
            if (fiExt == 'wav') {
              wavTracks.add(MusicTrack(
                name: fi.name,
              ));
              if (fi.name == item.name) {
                wavIndex = wavTracks.length - 1;
              }
            }
          }
        }
        showModalBottomSheet(
          context: context,
          isScrollControlled: true,
          useSafeArea: true,
          builder: (_) => MusicPlayerSheet(
            fileName: item.name,
            playlist: wavTracks,
            initialIndex: wavIndex,
          ),
        );
      }
      return;
    }

    if (['bmp', 'png', 'jpg'].contains(ext)) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('暂不支持打开图片文件 ${item.name}'),
            behavior: SnackBarBehavior.floating,
          ),
        );
      }
      return;
    }

    if (['txt', 'md', 'json', 'c', 'h'].contains(ext)) {
      if (item.size != null && item.size! > AppConstants.maxEditableFileSize) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text('文件过大（${_formatSize(item.size!)}），超过 20KB 限制，请在终端中查看'),
              behavior: SnackBarBehavior.floating,
              duration: const Duration(seconds: 3),
            ),
          );
        }
        return;
      }
      final content = await ref.read(fileProvider.notifier).getFileContent(item.name);
      if (content.length > AppConstants.maxEditableFileSize) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text('文件过大（${_formatSize(content.length)}），超过 20KB 限制，请在终端中查看'),
              behavior: SnackBarBehavior.floating,
              duration: const Duration(seconds: 3),
            ),
          );
        }
        return;
      }
      if (mounted) {
        Navigator.push(
          context,
          MaterialPageRoute(
            builder: (_) => TextEditorPage(
              fileName: item.name,
              initialContent: content,
            ),
          ),
        );
      }
      return;
    }

    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('暂不支持打开此类型文件 ${item.name}'),
          behavior: SnackBarBehavior.floating,
        ),
      );
    }
  }

  void _showContextMenu(FileItem item) {
    showModalBottomSheet(
      context: context,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(16)),
      ),
      builder: (context) {
        return SafeArea(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const SizedBox(height: 8),
              Container(
                width: 40,
                height: 4,
                decoration: BoxDecoration(
                  color: Colors.grey.shade400,
                  borderRadius: BorderRadius.circular(2),
                ),
              ),
              Padding(
                padding: const EdgeInsets.all(16),
                child: Row(
                  children: [
                    Icon(_fileIcon(item), color: _fileIconColor(item), size: 28),
                    const SizedBox(width: 12),
                    Expanded(
                      child: Text(
                        item.name,
                        style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 16),
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                  ],
                ),
              ),
              const Divider(height: 1),
              ListTile(
                leading: const Icon(Icons.info_outline),
                title: const Text('属性'),
                onTap: () {
                  Navigator.pop(context);
                  if (!ref.read(isConnectedProvider)) {
                    _showNeedConnection();
                    return;
                  }
                  ref.read(fileProvider.notifier).stat(item.name).then((statText) {
                    if (!mounted) return;
                    showDialog(
                      context: this.context,
                      builder: (_) => AlertDialog(
                        title: Text(item.name),
                        content: SelectableText(statText),
                        actions: [
                          TextButton(
                            onPressed: () => Navigator.pop(this.context),
                            child: const Text('确定'),
                          ),
                        ],
                      ),
                    );
                  });
                },
              ),
              ListTile(
                leading: const Icon(Icons.copy),
                title: const Text('复制'),
                onTap: () async {
                  Navigator.pop(context);
                  if (!ref.read(isConnectedProvider)) {
                    _showNeedConnection();
                    return;
                  }
                  final newName = await _showInputDialog('复制 ${item.name}', '新名称');
                  if (newName != null && newName.isNotEmpty) {
                    await ref.read(fileProvider.notifier).copy(item.name, newName);
                  }
                },
              ),
              if (!item.isDirectory)
                ListTile(
                  leading: const Icon(Icons.edit),
                  title: const Text('重命名'),
                  onTap: () async {
                    Navigator.pop(context);
                    if (!ref.read(isConnectedProvider)) {
                      _showNeedConnection();
                      return;
                    }
                    final newName = await _showInputDialog('重命名', '新名称');
                    if (newName != null && newName.isNotEmpty) {
                      await ref.read(fileProvider.notifier).rename(item.name, newName);
                    }
                  },
                ),
              ListTile(
                leading: Icon(Icons.delete, color: Theme.of(context).colorScheme.error),
                title: Text('删除', style: TextStyle(color: Theme.of(context).colorScheme.error)),
                onTap: () async {
                  Navigator.pop(context);
                  if (!ref.read(isConnectedProvider)) {
                    _showNeedConnection();
                    return;
                  }
                  final confirm = await showDialog<bool>(
                    context: this.context,
                    builder: (_) => AlertDialog(
                      title: const Text('确认删除'),
                      content: Text(item.isDirectory
                          ? '确定要删除文件夹 "${item.name}" 及其所有内容吗？'
                          : '确定要删除文件 "${item.name}" 吗？'),
                      actions: [
                        TextButton(
                          onPressed: () => Navigator.pop(this.context, false),
                          child: const Text('取消'),
                        ),
                        FilledButton(
                          onPressed: () => Navigator.pop(this.context, true),
                          style: FilledButton.styleFrom(
                            backgroundColor: Theme.of(this.context).colorScheme.error,
                          ),
                          child: const Text('删除'),
                        ),
                      ],
                    ),
                  );
                  if (confirm == true) {
                    await ref.read(fileProvider.notifier).delete(item);
                  }
                },
              ),
              const SizedBox(height: 8),
            ],
          ),
        );
      },
    );
  }

  Future<String?> _showInputDialog(String title, String hint) async {
    final controller = TextEditingController();
    return showDialog<String>(
      context: context,
      builder: (_) => AlertDialog(
        title: Text(title),
        content: TextField(
          controller: controller,
          decoration: InputDecoration(
            hintText: hint,
            border: const OutlineInputBorder(),
          ),
          autofocus: true,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('取消'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, controller.text.trim()),
            child: const Text('确定'),
          ),
        ],
      ),
    );
  }

  Widget _buildBreadcrumb(String path, bool isRoot) {
    final parts = path.split('\\').where((p) => p.isNotEmpty).toList();
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 2),
      child: SingleChildScrollView(
        scrollDirection: Axis.horizontal,
        child: Row(
          children: [
            IconButton(
              icon: Icon(Icons.arrow_upward, size: 20),
              tooltip: '上一级',
              onPressed: isRoot
                  ? null
                  : () async {
                      if (!ref.read(isConnectedProvider)) {
                        _showNeedConnection();
                        return;
                      }
                      await ref.read(fileProvider.notifier).goUp();
                    },
            ),
            const SizedBox(width: 4),
            InkWell(
              borderRadius: BorderRadius.circular(8),
              onTap: isRoot
                  ? null
                  : () {
                      if (!ref.read(isConnectedProvider)) {
                        _showNeedConnection();
                        return;
                      }
                      ref.read(fileProvider.notifier).navigateToPath('\\');
                    },
              child: Padding(
                padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                child: Icon(Icons.home, size: 18, color: isRoot ? Colors.grey : null),
              ),
            ),
            ...parts.asMap().entries.expand((entry) {
              final idx = entry.key;
              final part = entry.value;
              return [
                Icon(Icons.chevron_right, size: 16, color: Colors.grey.shade500),
                InkWell(
                  borderRadius: BorderRadius.circular(8),
                  onTap: idx == parts.length - 1
                      ? null
                      : () {
                          if (!ref.read(isConnectedProvider)) {
                            _showNeedConnection();
                            return;
                          }
                          final target = '\\${parts.sublist(0, idx + 1).join('\\')}';
                          ref.read(fileProvider.notifier).navigateToPath(target);
                        },
                  child: Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 4),
                    child: Text(
                      part,
                      style: TextStyle(
                        fontSize: 14,
                        fontWeight: idx == parts.length - 1 ? FontWeight.w600 : FontWeight.normal,
                      ),
                    ),
                  ),
                ),
              ];
            }),
          ],
        ),
      ),
    );
  }

  Widget _buildDeviceToggle(FileSystemState fsState) {
    final colorScheme = Theme.of(context).colorScheme;
    final isSd = fsState.currentDevice == StorageDevice.sd;

    return Container(
      height: 36,
      decoration: BoxDecoration(
        color: colorScheme.surfaceContainerHighest,
        borderRadius: BorderRadius.circular(18),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          _buildDeviceChip(
            label: 'TF卡',
            icon: Icons.sd_card,
            selected: isSd,
            onTap: fsState.isLoading
                ? null
                : () async {
                    if (isSd) return;
                    if (!ref.read(isConnectedProvider)) {
                      _showNeedConnection();
                      return;
                    }
                    final ok = await ref.read(fileProvider.notifier).switchDevice(StorageDevice.sd);
                    if (!ok && mounted) {
                      ScaffoldMessenger.of(context).showSnackBar(
                        SnackBar(
                          content: Text(fsState.error ?? '切换到TF卡失败'),
                          behavior: SnackBarBehavior.floating,
                        ),
                      );
                    }
                  },
          ),
          _buildDeviceChip(
            label: 'U盘',
            icon: Icons.usb,
            selected: !isSd,
            onTap: fsState.isLoading
                ? null
                : () async {
                    if (!isSd) return;
                    if (!ref.read(isConnectedProvider)) {
                      _showNeedConnection();
                      return;
                    }
                    final ok = await ref.read(fileProvider.notifier).switchDevice(StorageDevice.usb);
                    if (!ok && mounted) {
                      ScaffoldMessenger.of(context).showSnackBar(
                        SnackBar(
                          content: Text(fsState.error ?? '切换到U盘失败，设备可能不存在'),
                          behavior: SnackBarBehavior.floating,
                        ),
                      );
                    }
                  },
          ),
        ],
      ),
    );
  }

  Widget _buildDeviceChip({
    required String label,
    required IconData icon,
    required bool selected,
    required VoidCallback? onTap,
  }) {
    final colorScheme = Theme.of(context).colorScheme;
    return GestureDetector(
      onTap: onTap,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 200),
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
        decoration: BoxDecoration(
          color: selected ? colorScheme.primary : Colors.transparent,
          borderRadius: BorderRadius.circular(18),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              icon,
              size: 16,
              color: selected ? colorScheme.onPrimary : colorScheme.onSurfaceVariant,
            ),
            const SizedBox(width: 4),
            Text(
              label,
              style: TextStyle(
                fontSize: 13,
                fontWeight: FontWeight.w500,
                color: selected ? colorScheme.onPrimary : colorScheme.onSurfaceVariant,
              ),
            ),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final fsState = ref.watch(fileProvider);
    final musicStatus = ref.watch(musicProvider);
    final isMusicActive = musicStatus.state != PlayerState.stopped;
    final dir = fsState.currentDir;

    return Scaffold(
      appBar: AppBar(
        title: const Text('资源管理器'),
        actions: [
          _buildDeviceToggle(fsState),
          const SizedBox(width: 8),
          if (isMusicActive)
            IconButton(
              icon: const Icon(Icons.music_note),
              tooltip: '音乐播放器',
              onPressed: () {
                showModalBottomSheet(
                  context: context,
                  isScrollControlled: true,
                  useSafeArea: true,
                  builder: (_) => MusicPlayerSheet.resume(),
                );
              },
            ),
          PopupMenuButton<String>(
            icon: const Icon(Icons.add),
            onSelected: (value) async {
              if (!ref.read(isConnectedProvider)) {
                _showNeedConnection();
                return;
              }
              if (value == 'folder') {
                final name = await _showInputDialog('新建文件夹', '文件夹名称');
                if (name != null && name.isNotEmpty) {
                  await ref.read(fileProvider.notifier).createFolder(name);
                }
              } else if (value == 'file') {
                final name = await _showInputDialog('新建文件', '文件名称');
                if (name != null && name.isNotEmpty) {
                  await ref.read(fileProvider.notifier).createFile(name);
                }
              }
            },
            itemBuilder: (_) => const [
              PopupMenuItem(value: 'folder', child: Text('新建文件夹')),
              PopupMenuItem(value: 'file', child: Text('新建文件')),
            ],
          ),
          IconButton(
            icon: const Icon(Icons.refresh),
            tooltip: '刷新',
            onPressed: fsState.isLoading
                ? null
                : () {
                    if (!ref.read(isConnectedProvider)) {
                      _showNeedConnection();
                      return;
                    }
                    ref.read(fileProvider.notifier).refresh(force: true);
                  },
          ),
        ],
      ),
      body: _buildBody(fsState, dir, isMusicActive),
    );
  }

  Widget _buildBody(FileSystemState fsState, DirectoryNode? dir, bool isMusicActive) {
    if (fsState.isLoading && dir == null) {
      return const Center(child: CircularProgressIndicator());
    }

    if (fsState.error != null && dir == null) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.error_outline, size: 48, color: Colors.grey.shade400),
            const SizedBox(height: 16),
            Text(fsState.error!, style: const TextStyle(color: Colors.grey)),
            const SizedBox(height: 16),
            FilledButton.tonal(
              onPressed: () => ref.read(fileProvider.notifier).refresh(force: true),
              child: const Text('重试'),
            ),
          ],
        ),
      );
    }

    if (dir == null) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.folder_open, size: 64, color: Colors.grey.shade300),
            const SizedBox(height: 16),
            Text('空目录', style: TextStyle(color: Colors.grey.shade500, fontSize: 16)),
          ],
        ),
      );
    }

    final isRoot = dir.isRoot;
    final sortedItems = List<FileItem>.from(dir.items);
    sortedItems.sort((a, b) {
      if (a.isDirectory != b.isDirectory) return a.isDirectory ? -1 : 1;
      return a.name.toLowerCase().compareTo(b.name.toLowerCase());
    });

    final itemCount = sortedItems.length + (isRoot ? 0 : 1);

    return Column(
      children: [
        _buildBreadcrumb(dir.path, isRoot),
        if (fsState.isLoading)
          const LinearProgressIndicator(minHeight: 2),
        const Divider(height: 1),
        Expanded(
          child: itemCount == 0
              ? Center(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Icon(Icons.folder_open, size: 64, color: Colors.grey.shade300),
                      const SizedBox(height: 16),
                      Text('空目录', style: TextStyle(color: Colors.grey.shade500, fontSize: 16)),
                    ],
                  ),
                )
              : RefreshIndicator(
                  onRefresh: () => ref.read(fileProvider.notifier).refresh(force: true),
                  child: ListView.builder(
                    itemCount: itemCount,
                    itemBuilder: (context, index) {
                      if (!isRoot && index == 0) {
                        return ListTile(
                          leading: Icon(Icons.folder, color: Colors.amber.shade300),
                          title: const Text('..', style: TextStyle(color: Colors.grey)),
                          subtitle: const Text('上一级目录'),
                          onTap: () async {
                            if (!ref.read(isConnectedProvider)) {
                              _showNeedConnection();
                              return;
                            }
                            await ref.read(fileProvider.notifier).goUp();
                          },
                        );
                      }
                      final itemIndex = isRoot ? index : index - 1;
                      final item = sortedItems[itemIndex];
                      return _buildFileListItem(item);
                    },
                  ),
                ),
        ),
      ],
    );
  }

  Widget _buildFileListItem(FileItem item) {
    return ListTile(
      leading: Icon(_fileIcon(item), color: _fileIconColor(item)),
      title: Text(item.name, maxLines: 1, overflow: TextOverflow.ellipsis),
      subtitle: item.isDirectory
          ? null
          : item.size != null
              ? Text(_formatSize(item.size!))
              : null,
      trailing: IconButton(
        icon: const Icon(Icons.more_vert, size: 20),
        onPressed: () => _showContextMenu(item),
      ),
      onTap: () => _onItemTap(item),
    );
  }
}
