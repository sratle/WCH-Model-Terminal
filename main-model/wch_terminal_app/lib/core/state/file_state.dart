enum StorageDevice { sd, usb }

class FileItem {
  final String name;
  final bool isDirectory;
  final int? size;

  const FileItem(
      {required this.name,
      required this.isDirectory,
      this.size});

  static FileItem? parseLsLine(String line) {
    final cleanLine = line.replaceAll('\r', '').trim();
    if (cleanLine.isEmpty) return null;

    if (cleanLine.contains('[DIR]')) {
      final afterDir = cleanLine.split('[DIR]').last.trim();
      if (afterDir.isEmpty) return null;
      return FileItem(name: afterDir, isDirectory: true);
    }

    if (cleanLine.contains('[FILE]')) {
      final afterFile = cleanLine.split('[FILE]').last.trim();
      if (afterFile.isEmpty) return null;
      final sizeMatch = RegExp(r'\((\d+)\s+bytes\)$').firstMatch(afterFile);
      if (sizeMatch == null) return null;
      final sizeStr = sizeMatch.group(0)!;
      final name = afterFile.substring(0, afterFile.length - sizeStr.length).trim();
      if (name.isEmpty) return null;
      return FileItem(
        name: name,
        isDirectory: false,
        size: int.parse(sizeMatch.group(1)!),
      );
    }

    return null;
  }

  static List<FileItem> parseLsOutput(String text) {
    final items = <FileItem>[];
    for (final line in text.split(RegExp(r'\r?\n'))) {
      final item = parseLsLine(line);
      if (item != null) items.add(item);
    }
    return items;
  }
}

class DirectoryNode {
  final String path;
  final List<FileItem> items;

  const DirectoryNode({required this.path, required this.items});

  String? get parentPath {
    final idx = path.lastIndexOf('\\');
    if (idx <= 0) return null;
    return path.substring(0, idx);
  }

  bool get isRoot {
    final cleaned = path.replaceAll('\\', '').trim();
    return cleaned.isEmpty;
  }
}

class FileSystemState {
  final StorageDevice currentDevice;
  final String currentPath;
  final Map<String, DirectoryNode> cache;
  final bool isLoading;
  final String? error;

  const FileSystemState({
    this.currentDevice = StorageDevice.usb,
    this.currentPath = '\\',
    this.cache = const {},
    this.isLoading = false,
    this.error,
  });

  DirectoryNode? get currentDir => cache[currentPath];

  FileSystemState copyWith({
    StorageDevice? currentDevice,
    String? currentPath,
    Map<String, DirectoryNode>? cache,
    bool? isLoading,
    String? error,
  }) {
    return FileSystemState(
      currentDevice: currentDevice ?? this.currentDevice,
      currentPath: currentPath ?? this.currentPath,
      cache: cache ?? this.cache,
      isLoading: isLoading ?? this.isLoading,
      error: error,
    );
  }
}
