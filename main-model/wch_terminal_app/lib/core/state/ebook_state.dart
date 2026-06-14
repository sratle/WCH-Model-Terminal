// EBook reader state model.
// Browses BOOK directory for .txt/.md/.json files, reads via "cat",
// supports pagination.

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
  final String content;
  final int currentPage;
  final int totalPages;
  final bool isLoading;
  final String? error;
  final EbookTheme theme;
  final double fontSize;

  const EBookState({
    this.books = const [],
    this.selectedIndex = -1,
    this.content = '',
    this.currentPage = 0,
    this.totalPages = 0,
    this.isLoading = false,
    this.error,
    this.theme = EbookTheme.sepia,
    this.fontSize = 16.0,
  });

  /// Characters per page (approximate, tuned for mobile screen).
  static const int charsPerPage = 800;

  EBookState copyWith({
    List<BookItem>? books,
    int? selectedIndex,
    String? content,
    int? currentPage,
    int? totalPages,
    bool? isLoading,
    String? error,
    EbookTheme? theme,
    double? fontSize,
  }) {
    return EBookState(
      books: books ?? this.books,
      selectedIndex: selectedIndex ?? this.selectedIndex,
      content: content ?? this.content,
      currentPage: currentPage ?? this.currentPage,
      totalPages: totalPages ?? this.totalPages,
      isLoading: isLoading ?? this.isLoading,
      error: error,
      theme: theme ?? this.theme,
      fontSize: fontSize ?? this.fontSize,
    );
  }

  /// Get the text for the current page.
  String get pageText {
    if (content.isEmpty || totalPages == 0) return '';
    final start = currentPage * charsPerPage;
    if (start >= content.length) return '';
    final end = (start + charsPerPage > content.length)
        ? content.length
        : start + charsPerPage;
    return content.substring(start, end);
  }
}
