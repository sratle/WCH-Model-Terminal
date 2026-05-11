import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/state/file_state.dart';
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
    final ext = item.name.split('.').last.toLowerCase();
    switch (ext) {
      case 'wav': return Icons.music_note;
      case 'txt':
      case 'md':
      case 'json':
      case 'c':
      case 'h': return Icons.description;
      case 'bmp':
      case 'png':
      case 'jpg': return Icons.image;
      default: return Icons.insert_drive_file;
    }
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
      await ref.read(fileProvider.notifier).enterDirectory(item.name);
      return;
    }

    final ext = item.name.split('.').last.toLowerCase();

    if (ext == 'wav') {
      if (mounted) {
        showModalBottomSheet(
          context: context,
          isScrollControlled: true,
          builder: (_) => MusicPlayerSheet(fileName: item.name),
        );
      }
      return;
    }

    if (['txt', 'md', 'json', 'c', 'h'].contains(ext)) {
      final content = await ref.read(fileProvider.notifier).getFileContent(item.name);
      if (content.length > AppConstants.maxEditableFileSize) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('文件过大，请在终端中编辑')),
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
        SnackBar(content: Text('无法打开 ${item.name}')),
      );
    }
  }

  void _showContextMenu(FileItem item) {
    showModalBottomSheet(
      context: context,
      builder: (context) {
        return SafeArea(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              ListTile(
                title: Text(item.name, style: const TextStyle(fontWeight: FontWeight.bold)),
              ),
              const Divider(),
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
                leading: const Icon(Icons.delete, color: Colors.red),
                title: const Text('删除', style: TextStyle(color: Colors.red)),
                onTap: () async {
                  Navigator.pop(context);
                  if (!ref.read(isConnectedProvider)) {
                    _showNeedConnection();
                    return;
                  }
                  final confirm = await showDialog<bool>(
                    context: context,
                    builder: (_) => AlertDialog(
                      title: const Text('确认删除'),
                      content: Text('确定要删除 ${item.name} 吗？'),
                      actions: [
                        TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('取消')),
                        TextButton(onPressed: () => Navigator.pop(context, true), child: const Text('删除')),
                      ],
                    ),
                  );
                  if (confirm == true) {
                    await ref.read(fileProvider.notifier).delete(item);
                  }
                },
              ),
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
          decoration: InputDecoration(hintText: hint),
          autofocus: true,
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context), child: const Text('取消')),
          TextButton(
            onPressed: () => Navigator.pop(context, controller.text.trim()),
            child: const Text('确定'),
          ),
        ],
      ),
    );
  }

  Widget _buildBreadcrumb(String path) {
    final parts = path.split('\\').where((p) => p.isNotEmpty).toList();
    return SingleChildScrollView(
      scrollDirection: Axis.horizontal,
      child: Row(
        children: [
          IconButton(
            icon: const Icon(Icons.arrow_back),
            onPressed: () async => ref.read(fileProvider.notifier).goUp(),
          ),
          ...parts.asMap().entries.expand((entry) {
            final idx = entry.key;
            final part = entry.value;
            return [
              TextButton(
                onPressed: () {
                  if (!ref.read(isConnectedProvider)) {
                    _showNeedConnection();
                    return;
                  }
                  final target = parts.sublist(0, idx + 1).join('\\');
                  ref.read(fileProvider.notifier).navigateToPath(target);
                },
                child: Text(part, style: const TextStyle(fontSize: 14)),
              ),
              if (idx < parts.length - 1) const Text('\\', style: TextStyle(color: Colors.grey)),
            ];
          }),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final fileState = ref.watch(fileProvider);

    return Scaffold(
      appBar: AppBar(
        title: const Text('资源管理器'),
        actions: [
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
            onPressed: () {
              if (!ref.read(isConnectedProvider)) {
                _showNeedConnection();
                return;
              }
              ref.read(fileProvider.notifier).refresh();
            },
          ),
        ],
      ),
      body: fileState.when(
        loading: () => const Center(child: CircularProgressIndicator()),
        error: (err, _) => Center(child: Text('错误: $err')),
        data: (dir) => Column(
          children: [
            _buildBreadcrumb(dir.path),
            const Divider(height: 1),
            Expanded(
              child: RefreshIndicator(
                onRefresh: () => ref.read(fileProvider.notifier).refresh(),
                child: ListView.builder(
                  itemCount: dir.items.length + 1,
                  itemBuilder: (context, index) {
                    if (index == 0) {
                      return ListTile(
                        leading: const Icon(Icons.folder, color: Colors.amber),
                        title: const Text('..'),
                        onTap: () async {
                        if (!ref.read(isConnectedProvider)) {
                          _showNeedConnection();
                          return;
                        }
                        await ref.read(fileProvider.notifier).goUp();
                      },
                      );
                    }
                    final item = dir.items[index - 1];
                    return ListTile(
                      leading: Icon(_fileIcon(item), color: item.isDirectory ? Colors.amber : null),
                      title: Text(item.name),
                      subtitle: item.size != null ? Text('${item.size} bytes') : null,
                      onTap: () => _onItemTap(item),
                      onLongPress: () => _showContextMenu(item),
                    );
                  },
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
