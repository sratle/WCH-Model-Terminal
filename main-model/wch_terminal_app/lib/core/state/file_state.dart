class FileItem {
  final String name;
  final bool isDirectory;
  final int? size;

  FileItem({required this.name, required this.isDirectory, this.size});

  static FileItem? parseLsLine(String line) {
    final dirMatch = RegExp(r'^\s+\[DIR\]\s+(.+)$').firstMatch(line);
    if (dirMatch != null) {
      return FileItem(name: dirMatch.group(1)!.trim(), isDirectory: true);
    }

    final fileMatch = RegExp(r'^\s+\[FILE\]\s+(.+?)\s+\((\d+) bytes\)\s*$')
        .firstMatch(line);
    if (fileMatch != null) {
      return FileItem(
        name: fileMatch.group(1)!.trim(),
        isDirectory: false,
        size: int.parse(fileMatch.group(2)!),
      );
    }
    return null;
  }

  static List<FileItem> parseLsOutput(String text) {
    final items = <FileItem>[];
    for (final line in text.split('\n')) {
      final item = parseLsLine(line);
      if (item != null) items.add(item);
    }
    return items;
  }
}

class DirectoryNode {
  final String path;
  final List<FileItem> items;
  final String? parentPath;

  DirectoryNode({required this.path, required this.items, this.parentPath});
}
