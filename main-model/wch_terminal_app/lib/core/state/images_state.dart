// Images browser state model.
// Lists BMP files via "bmp ls", fetches via "bmp get" hex dump.

class BmpItem {
  final String name;
  final int? size;

  const BmpItem({required this.name, this.size});

  /// Parse a single line from `bmp ls` output.
  /// Format: "  [FILE] name.BMP  (1234 bytes)"
  static BmpItem? parseLsLine(String line) {
    final clean = line.replaceAll('\r', '').trim();
    if (clean.isEmpty || clean.contains('[DIR]')) return null;

    if (clean.contains('[FILE]')) {
      final afterFile = clean.split('[FILE]').last.trim();
      if (afterFile.isEmpty) return null;
      final sizeMatch = RegExp(r'\((\d+)\s+bytes\)$').firstMatch(afterFile);
      if (sizeMatch == null) return null;
      final sizeStr = sizeMatch.group(0)!;
      final name =
          afterFile.substring(0, afterFile.length - sizeStr.length).trim();
      if (name.isEmpty) return null;
      return BmpItem(name: name, size: int.parse(sizeMatch.group(1)!));
    }
    return null;
  }

  static List<BmpItem> parseLsOutput(String text) {
    final items = <BmpItem>[];
    for (final line in text.split(RegExp(r'\r?\n'))) {
      final item = parseLsLine(line);
      if (item != null) items.add(item);
    }
    return items;
  }
}

class BmpInfo {
  final String name;
  final int fileSize;
  final int width;
  final int height;
  final int bpp;
  final String hexPreview; // First 128 bytes as hex string for display

  const BmpInfo({
    required this.name,
    required this.fileSize,
    required this.width,
    required this.height,
    required this.bpp,
    this.hexPreview = '',
  });

  /// Parse `bmp get` output header line.
  /// Format: "BMP: name  size=1234 bytes"
  static BmpInfo? parseHeader(String output) {
    final match =
        RegExp(r'BMP:\s*(\S+)\s+size=(\d+)\s+bytes').firstMatch(output);
    if (match == null) return null;

    final name = match.group(1)!;
    final size = int.parse(match.group(2)!);

    // Try to parse BMP header from hex dump lines
    int width = 0, height = 0, bpp = 0;
    final lines = output.split(RegExp(r'\r?\n'));
    // Collect hex bytes from the dump (format: "XX XX XX ..." per line)
    final hexBytes = <int>[];
    for (final line in lines) {
      final trimmed = line.trim();
      // Skip header/info lines
      if (trimmed.startsWith('BMP:') || trimmed.isEmpty) continue;
      // Try to parse hex bytes
      final bytes = RegExp(r'([0-9A-Fa-f]{2})').allMatches(trimmed);
      if (bytes.isNotEmpty && bytes.length >= 8) {
        for (final b in bytes) {
          if (hexBytes.length < 64) {
            hexBytes.add(int.parse(b.group(1)!, radix: 16));
          }
        }
      }
    }

    // BMP info header: offset 18=width(4), 22=height(4), 28=bpp(2)
    if (hexBytes.length >= 30) {
      width = hexBytes[18] |
          (hexBytes[19] << 8) |
          (hexBytes[20] << 16) |
          (hexBytes[21] << 24);
      height = hexBytes[22] |
          (hexBytes[23] << 8) |
          (hexBytes[24] << 16) |
          (hexBytes[25] << 24);
      bpp = hexBytes[28] | (hexBytes[29] << 8);
    }

    // Build hex preview string
    final preview = hexBytes
        .take(64)
        .map((b) => b.toRadixString(16).padLeft(2, '0').toUpperCase())
        .join(' ');

    return BmpInfo(
      name: name,
      fileSize: size,
      width: width,
      height: height,
      bpp: bpp,
      hexPreview: preview,
    );
  }
}

class ImagesState {
  final List<BmpItem> files;
  final int selectedIndex;
  final BmpInfo? currentBmp;
  final bool isLoading;
  final String? error;

  const ImagesState({
    this.files = const [],
    this.selectedIndex = -1,
    this.currentBmp,
    this.isLoading = false,
    this.error,
  });

  ImagesState copyWith({
    List<BmpItem>? files,
    int? selectedIndex,
    BmpInfo? currentBmp,
    bool? isLoading,
    String? error,
  }) {
    return ImagesState(
      files: files ?? this.files,
      selectedIndex: selectedIndex ?? this.selectedIndex,
      currentBmp: currentBmp ?? this.currentBmp,
      isLoading: isLoading ?? this.isLoading,
      error: error,
    );
  }
}
