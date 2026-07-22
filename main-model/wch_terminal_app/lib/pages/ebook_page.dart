import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/state/ebook_state.dart';
import '../providers/ebook_provider.dart';
import '../providers/ble_provider.dart';

class EBookPage extends ConsumerStatefulWidget {
  const EBookPage({super.key});

  @override
  ConsumerState<EBookPage> createState() => _EBookPageState();
}

class _EBookPageState extends ConsumerState<EBookPage> {
  @override
  void initState() {
    super.initState();
    Future.microtask(() {
      if (ref.read(isConnectedProvider)) {
        ref.read(ebookProvider.notifier).loadBookList();
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    final state = ref.watch(ebookProvider);
    final notifier = ref.read(ebookProvider.notifier);
    final theme = state.theme;

    // Theme colors
    final bgColor = _themeBg(theme);
    final textColor = _themeText(theme);
    final listBg = _themeListBg(theme);
    final listSel = _themeListSel(theme);
    final listAlt = _themeListAlt(theme);
    final accent = _themeAccent(theme);

    // Phones in portrait are too narrow for a side-by-side list; give the
    // reader the whole width and open the list on demand instead.
    final isWide = MediaQuery.of(context).size.width >= 600;

    return Container(
      color: bgColor,
      child: Column(
        children: [
          // Title bar (shows a book-list button when the list is off-screen)
          _buildTitleBar(state, notifier, accent, textColor, isWide, listBg,
              listSel, listAlt),
          // Main content
          Expanded(
            child: isWide
                // Tablet / landscape: side-by-side list + reader
                ? Row(
                    children: [
                      SizedBox(
                        width: MediaQuery.of(context).size.width * 0.32,
                        child: _buildBookList(state, notifier, listBg, listSel,
                            listAlt, textColor),
                      ),
                      Expanded(
                        child: _buildReaderInteractive(
                            state, notifier, bgColor, textColor, accent),
                      ),
                    ],
                  )
                // Phone (portrait): full-width reader, swipe to turn pages
                : _buildReaderInteractive(
                    state, notifier, bgColor, textColor, accent),
          ),
          // Bottom toolbar
          _buildToolbar(state, notifier, accent, textColor),
        ],
      ),
    );
  }

  Widget _buildTitleBar(EBookState state, EBookNotifier notifier, Color accent,
      Color textColor, bool isWide, Color listBg, Color listSel,
      Color listAlt) {
    final themeName = state.theme == EbookTheme.dark
        ? 'Dark'
        : state.theme == EbookTheme.sepia
            ? 'Sepia'
            : 'Light';
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      color: Colors.black12,
      child: Row(
        children: [
          if (!isWide)
            IconButton(
              icon: Icon(Icons.menu, color: accent, size: 20),
              tooltip: 'Book list',
              padding: EdgeInsets.zero,
              constraints: const BoxConstraints(),
              onPressed: () => _openBookListSheet(
                  notifier, listBg, listSel, listAlt, textColor),
            )
          else
            Icon(Icons.menu_book, color: accent, size: 20),
          const SizedBox(width: 8),
          Text('EBook Reader', style: TextStyle(color: textColor, fontWeight: FontWeight.bold)),
          const Spacer(),
          if (state.books.isNotEmpty)
            Text('${state.books.length} books',
                style: TextStyle(color: textColor.withValues(alpha: 0.6), fontSize: 12)),
          const SizedBox(width: 12),
          Text(themeName,
              style: TextStyle(color: accent, fontSize: 12)),
        ],
      ),
    );
  }

  /// Reader wrapped with horizontal-swipe page turning — the primary
  /// navigation gesture on phones (toolbar chevrons remain as a fallback).
  Widget _buildReaderInteractive(EBookState state, EBookNotifier notifier,
      Color bgColor, Color textColor, Color accent) {
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onHorizontalDragEnd: (details) {
        if (state.isLoading) return;
        final v = details.primaryVelocity ?? 0;
        if (v < -100 && state.hasNextPage) {
          notifier.nextPage();
        } else if (v > 100 && state.currentPage > 0) {
          notifier.prevPage();
        }
      },
      child: _buildReader(state, bgColor, textColor, accent),
    );
  }

  /// Book list as a modal sheet (phones). Uses a Consumer so it reflects
  /// loading/selection changes while open, and closes after a book is picked.
  void _openBookListSheet(EBookNotifier notifier, Color listBg, Color listSel,
      Color listAlt, Color textColor) {
    showModalBottomSheet(
      context: context,
      backgroundColor: listBg,
      isScrollControlled: true,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(16)),
      ),
      builder: (ctx) {
        return SafeArea(
          child: SizedBox(
            height: MediaQuery.of(ctx).size.height * 0.6,
            child: Column(
              children: [
                Padding(
                  padding: const EdgeInsets.symmetric(vertical: 8),
                  child: Container(
                    width: 40,
                    height: 4,
                    decoration: BoxDecoration(
                      color: textColor.withValues(alpha: 0.3),
                      borderRadius: BorderRadius.circular(2),
                    ),
                  ),
                ),
                Expanded(
                  child: Consumer(
                    builder: (c, ref2, _) {
                      final s = ref2.watch(ebookProvider);
                      return _buildBookList(s, notifier, listBg, listSel,
                          listAlt, textColor,
                          onOpened: () => Navigator.of(ctx).maybePop());
                    },
                  ),
                ),
              ],
            ),
          ),
        );
      },
    );
  }

  Widget _buildBookList(EBookState state, EBookNotifier notifier, Color listBg,
      Color listSel, Color listAlt, Color textColor, {VoidCallback? onOpened}) {
    return Container(
      color: listBg,
      child: state.isLoading && state.books.isEmpty
          ? const Center(child: CircularProgressIndicator())
          : state.books.isEmpty
              ? Center(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Icon(Icons.folder_off, color: textColor.withValues(alpha: 0.4), size: 48),
                      const SizedBox(height: 8),
                      Text(
                        state.error ?? 'No books in \\BOOK',
                        style: TextStyle(color: textColor.withValues(alpha: 0.6)),
                        textAlign: TextAlign.center,
                      ),
                      const SizedBox(height: 12),
                      ElevatedButton.icon(
                        onPressed: () => notifier.loadBookList(),
                        icon: const Icon(Icons.refresh, size: 16),
                        label: const Text('Retry'),
                      ),
                    ],
                  ),
                )
              : ListView.builder(
                  itemCount: state.books.length + 1, // +1 for refresh item
                  itemBuilder: (context, index) {
                    if (index == state.books.length) {
                      return ListTile(
                        leading: Icon(Icons.refresh, color: textColor.withValues(alpha: 0.5)),
                        title: Text('Refresh',
                            style: TextStyle(color: textColor.withValues(alpha: 0.5))),
                        onTap: () => notifier.loadBookList(),
                      );
                    }
                    final book = state.books[index];
                    final isSelected = index == state.selectedIndex;
                    final ext = book.name.contains('.')
                        ? book.name.split('.').last.toUpperCase()
                        : '';
                    return ListTile(
                      tileColor: isSelected
                          ? listSel
                          : (index.isEven ? null : listAlt),
                      leading: Icon(
                        ext == 'MD'
                            ? Icons.article
                            : ext == 'JSON'
                                ? Icons.data_object
                                : Icons.text_snippet,
                        color: textColor.withValues(alpha: 0.7),
                        size: 20,
                      ),
                      title: Text(
                        book.name,
                        style: TextStyle(
                          color: textColor,
                          fontWeight: isSelected ? FontWeight.bold : FontWeight.normal,
                          fontSize: 14,
                        ),
                        overflow: TextOverflow.ellipsis,
                      ),
                      subtitle: book.size != null
                          ? Text(
                              _formatSize(book.size!),
                              style: TextStyle(
                                  color: textColor.withValues(alpha: 0.5), fontSize: 11),
                            )
                          : null,
                      onTap: () {
                        notifier.openBook(index);
                        onOpened?.call();
                      },
                    );
                  },
                ),
    );
  }

  Widget _buildReader(
      EBookState state, Color bgColor, Color textColor, Color accent) {
    if (state.chunk.isEmpty) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.auto_stories, color: textColor.withValues(alpha: 0.3), size: 64),
            const SizedBox(height: 12),
            Text(
              state.selectedIndex >= 0 && state.isLoading
                  ? 'Loading...'
                  : 'Select a book to read',
              style: TextStyle(color: textColor.withValues(alpha: 0.5)),
            ),
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

    return Padding(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Book title
          if (state.selectedIndex >= 0 && state.selectedIndex < state.books.length)
            Padding(
              padding: const EdgeInsets.only(bottom: 8),
              child: Text(
                state.books[state.selectedIndex].name,
                style: TextStyle(
                  color: accent,
                  fontWeight: FontWeight.bold,
                  fontSize: state.fontSize + 2,
                ),
              ),
            ),
          // Content
          Expanded(
            child: SingleChildScrollView(
              child: SelectableText(
                state.pageText,
                style: TextStyle(
                  color: textColor,
                  fontSize: state.fontSize,
                  height: 1.6,
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildToolbar(
      EBookState state, EBookNotifier notifier, Color accent, Color textColor) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
      decoration: BoxDecoration(
        color: Colors.black12,
        border: Border(top: BorderSide(color: Colors.black26)),
      ),
      child: Row(
        children: [
          // Font size controls
          IconButton(
            icon: const Icon(Icons.text_decrease, size: 18),
            onPressed: () => notifier.changeFontSize(-2),
            tooltip: 'Smaller font',
          ),
          Text('${state.fontSize.toInt()}',
              style: TextStyle(color: textColor, fontSize: 12)),
          IconButton(
            icon: const Icon(Icons.text_increase, size: 18),
            onPressed: () => notifier.changeFontSize(2),
            tooltip: 'Larger font',
          ),
          const SizedBox(width: 16),
          // Theme toggle
          IconButton(
            icon: const Icon(Icons.palette, size: 18),
            onPressed: () => notifier.cycleTheme(),
            tooltip: 'Cycle theme',
          ),
          const Spacer(),
          // Page navigation
          IconButton(
            icon: const Icon(Icons.chevron_left),
            onPressed: state.currentPage > 0 && !state.isLoading
                ? () => notifier.prevPage()
                : null,
          ),
          Text(
            state.selectedIndex >= 0
                ? '${state.currentPage + 1} / ${state.pageCount}${state.hasNextPage ? "+" : ""}'
                : '--',
            style: TextStyle(color: textColor, fontSize: 13),
          ),
          IconButton(
            icon: const Icon(Icons.chevron_right),
            onPressed: state.hasNextPage && !state.isLoading
                ? () => notifier.nextPage()
                : null,
          ),
        ],
      ),
    );
  }

  String _formatSize(int bytes) {
    if (bytes < 1024) return '$bytes B';
    if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(1)} KB';
    return '${(bytes / (1024 * 1024)).toStringAsFixed(1)} MB';
  }

  // Theme color helpers
  Color _themeBg(EbookTheme t) {
    switch (t) {
      case EbookTheme.dark:
        return const Color(0xFF1A1A2E);
      case EbookTheme.sepia:
        return const Color(0xFFF5F0E8);
      case EbookTheme.light:
        return Colors.white;
    }
  }

  Color _themeText(EbookTheme t) {
    switch (t) {
      case EbookTheme.dark:
        return const Color(0xFFE0E0E0);
      case EbookTheme.sepia:
        return const Color(0xFF3E2C1C);
      case EbookTheme.light:
        return const Color(0xFF212121);
    }
  }

  Color _themeListBg(EbookTheme t) {
    switch (t) {
      case EbookTheme.dark:
        return const Color(0xFF16213E);
      case EbookTheme.sepia:
        return const Color(0xFFEDE5D5);
      case EbookTheme.light:
        return const Color(0xFFF5F5F5);
    }
  }

  Color _themeListSel(EbookTheme t) {
    switch (t) {
      case EbookTheme.dark:
        return const Color(0xFF0F3460);
      case EbookTheme.sepia:
        return const Color(0xFFD4C4A8);
      case EbookTheme.light:
        return const Color(0xFFE3F2FD);
    }
  }

  Color _themeListAlt(EbookTheme t) {
    switch (t) {
      case EbookTheme.dark:
        return const Color(0xFF192240);
      case EbookTheme.sepia:
        return const Color(0xFFF0E8D8);
      case EbookTheme.light:
        return const Color(0xFFFAFAFA);
    }
  }

  Color _themeAccent(EbookTheme t) {
    switch (t) {
      case EbookTheme.dark:
        return const Color(0xFF7EC8C8);
      case EbookTheme.sepia:
        return const Color(0xFF8B6914);
      case EbookTheme.light:
        return const Color(0xFF1565C0);
    }
  }
}
