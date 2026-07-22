// EBook reader state model — PAGED BYTE-STREAM.
// Browses BOOK directory for .txt/.md/.json files, reads via the Core CLI
// `read <file> <off> <len>` command in fixed-size chunks, and keeps a
// page-offset stack so books of any size use flat memory.

class BookItem {
  final String name;
  final int? size;

  const BookItem({required this.name, this.size});

  /// Parse a single line from `ls` output (same format as FileItem).
  static BookItem? parseLsLine(String line) {
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

      // Filter: only text-readable files
      final ext = name.contains('.') ? name.split('.').last.toLowerCase() : '';
      if (ext != 'txt' && ext != 'md' && ext != 'json') return null;

      return BookItem(name: name, size: int.parse(sizeMatch.group(1)!));
    }
    return null;
  }

  static List<BookItem> parseLsOutput(String text) {
    final items = <BookItem>[];
    for (final line in text.split(RegExp(r'\r?\n'))) {
      final item = parseLsLine(line);
      if (item != null) items.add(item);
    }
    return items;
  }
}

enum EbookTheme { dark, sepia, light }

class EBookState {
  final List<BookItem> books;
  final int selectedIndex;

  /// Loaded chunk (text of [chunkOff, chunkOff + chunk.length) in the file)
  final String chunk;
  final int chunkOff;
  final bool chunkEof;

  /// Byte offset of each discovered page start (pageStarts[0] == 0).
  /// Grows lazily as the user reads forward.
  final List<int> pageStarts;
  final int currentPage;

  final bool isLoading;
  final String? error;
  final EbookTheme theme;
  final double fontSize;

  const EBookState({
    this.books = const [],
    this.selectedIndex = -1,
    this.chunk = '',
    this.chunkOff = 0,
    this.chunkEof = false,
    this.pageStarts = const [0],
    this.currentPage = 0,
    this.isLoading = false,
    this.error,
    this.theme = EbookTheme.sepia,
    this.fontSize = 16.0,
  });

  /// Characters per page (approximate, tuned for mobile screen).
  static const int charsPerPage = 800;

  /// Bytes per Core `read` request: one page (800 chars) plus a margin
  /// covering the boundary word/line.  Per-page fetch — never the whole file.
  static const int chunkSize = 1024;

  /// Number of pages discovered so far.
  int get pageCount => pageStarts.length;

  bool get hasBook => selectedIndex >= 0;

  /// More content may exist beyond the known pages.
  bool get hasNextPage => currentPage + 1 < pageCount || !chunkEof;

  EBookState copyWith({
    List<BookItem>? books,
    int? selectedIndex,
    String? chunk,
    int? chunkOff,
    bool? chunkEof,
    List<int>? pageStarts,
    int? currentPage,
    bool? isLoading,
    String? error,
    EbookTheme? theme,
    double? fontSize,
  }) {
    return EBookState(
      books: books ?? this.books,
      selectedIndex: selectedIndex ?? this.selectedIndex,
      chunk: chunk ?? this.chunk,
      chunkOff: chunkOff ?? this.chunkOff,
      chunkEof: chunkEof ?? this.chunkEof,
      pageStarts: pageStarts ?? this.pageStarts,
      currentPage: currentPage ?? this.currentPage,
      isLoading: isLoading ?? this.isLoading,
      error: error,
      theme: theme ?? this.theme,
      fontSize: fontSize ?? this.fontSize,
    );
  }

  /// Get the text for the current page (from the loaded chunk).
  /// The chunk always begins exactly at this page's byte offset
  /// (chunkOff == pageStarts[currentPage]), so the slice starts at 0.
  String get pageText {
    if (chunk.isEmpty || pageStarts.isEmpty) return '';
    final start = pageStarts[currentPage] - chunkOff;
    if (start < 0 || start >= chunk.length) return '';
    final end = (start + charsPerPage > chunk.length)
        ? chunk.length
        : start + charsPerPage;
    var pt = chunk.substring(start, end);
    // When the chunk was truncated mid-file, its last character may be a
    // partial UTF-8 sequence decoded as U+FFFD. Drop trailing replacement
    // chars so the next page's byte offset (computed from utf8.encode of this
    // page) lands exactly on a character boundary — essential for CJK text.
    if (!chunkEof) {
      while (pt.isNotEmpty && pt.codeUnitAt(pt.length - 1) == 0xFFFD) {
        pt = pt.substring(0, pt.length - 1);
      }
    }
    return pt;
  }
}
