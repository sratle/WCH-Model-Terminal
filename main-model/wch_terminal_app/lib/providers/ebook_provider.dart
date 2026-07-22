import 'dart:async';
import 'dart:convert';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/cli/cli_engine.dart';
import '../core/cli/cli_commands.dart';
import '../core/state/ebook_state.dart';
import 'cli_provider.dart';

class EBookNotifier extends StateNotifier<EBookState> {
  final CliEngine _cli;

  EBookNotifier(this._cli) : super(const EBookState());

  /// Load book list from \BOOK directory.
  /// Sends "cd BOOK" then "ls", parses .txt/.md/.json files.
  Future<void> loadBookList() async {
    state = state.copyWith(isLoading: true, error: null);
    try {
      final cdResp = await _cli.execute(CliCommands.cd('BOOK'));
      if (!cdResp.isSuccess || cdResp.output.contains('failed')) {
        state = state.copyWith(
          isLoading: false,
          error: 'BOOK 目录不存在或无法进入',
        );
        return;
      }

      final lsResp =
          await _cli.execute(CliCommands.ls(), timeout: const Duration(seconds: 15));
      final books = BookItem.parseLsOutput(lsResp.output);

      state = state.copyWith(
        books: books,
        isLoading: false,
        selectedIndex: -1,
        chunk: '',
        pageStarts: const [0],
        currentPage: 0,
      );
    } catch (e) {
      state = state.copyWith(isLoading: false, error: e.toString());
    }
  }

  /// Open a book by index: reset pagination and fetch the first chunk.
  Future<void> openBook(int index) async {
    if (index < 0 || index >= state.books.length) return;

    state = state.copyWith(
      isLoading: true,
      selectedIndex: index,
      error: null,
      chunk: '',
      chunkOff: 0,
      chunkEof: false,
      pageStarts: const [0],
      currentPage: 0,
    );

    await _requestChunk(0);
  }

  /// Request the chunk at byte offset [off] from Core via `read`.
  Future<void> _requestChunk(int off) async {
    if (state.selectedIndex < 0 || state.selectedIndex >= state.books.length) {
      return;
    }
    final book = state.books[state.selectedIndex];

    state = state.copyWith(isLoading: true);
    try {
      final resp = await _cli.execute(
        CliCommands.read(book.name, off, EBookState.chunkSize),
        timeout: const Duration(seconds: 15),
      );

      if (!resp.isSuccess) {
        state = state.copyWith(isLoading: false, error: '读取文件失败');
        return;
      }

      final text = resp.output;
      state = state.copyWith(
        chunk: text,
        chunkOff: off,
        chunkEof: text.length < EBookState.chunkSize,
        isLoading: false,
      );
      _registerNextPage();
    } catch (e) {
      state = state.copyWith(isLoading: false, error: e.toString());
    }
  }

  /// After a page is displayable, record where the NEXT page begins.
  /// The offset is a BYTE offset (what `read` expects), computed from the
  /// UTF-8 byte length actually consumed by the current page, so multi-byte
  /// (e.g. Chinese) text paginates correctly instead of drifting.
  void _registerNextPage() {
    final consumed = utf8.encode(state.pageText).length;
    final next = state.chunkOff + consumed;
    final chunkEndByte = state.chunkOff + utf8.encode(state.chunk).length;

    // The book continues unless the file ended within this chunk and the
    // current page already consumed all of it.
    final continues = !state.chunkEof || next < chunkEndByte;
    if (continues && next > state.chunkOff &&
        state.currentPage + 1 >= state.pageCount) {
      state = state.copyWith(
        pageStarts: [...state.pageStarts, next],
      );
    }
  }

  /// Per-page fetch model: every page turn fetches [pageStart, pageStart +
  /// chunkSize) — one page plus a boundary margin, never the whole file.
  Future<void> _ensureChunk() async {
    final ps = state.pageStarts[state.currentPage];
    if (ps != state.chunkOff) {
      await _requestChunk(ps);
    }
  }

  /// Manually re-fetch the current page's content from Core (refresh button).
  Future<void> refreshPage() async {
    if (state.selectedIndex < 0 || state.isLoading) return;
    final off = state.currentPage < state.pageStarts.length
        ? state.pageStarts[state.currentPage]
        : state.chunkOff;
    await _requestChunk(off);
  }

  void nextPage() {
    if (state.currentPage + 1 < state.pageCount && !state.isLoading) {
      state = state.copyWith(currentPage: state.currentPage + 1);
      _ensureChunk();
    }
  }

  void prevPage() {
    if (state.currentPage > 0 && !state.isLoading) {
      state = state.copyWith(currentPage: state.currentPage - 1);
      _ensureChunk();
    }
  }

  void cycleTheme() {
    final next = EbookTheme.values[(state.theme.index + 1) % EbookTheme.values.length];
    state = state.copyWith(theme: next);
  }

  void changeFontSize(double delta) {
    final newSize = (state.fontSize + delta).clamp(12.0, 28.0);
    state = state.copyWith(fontSize: newSize);
  }
}

final ebookProvider =
    StateNotifierProvider<EBookNotifier, EBookState>((ref) {
  final cli = ref.watch(cliEngineProvider);
  return EBookNotifier(cli);
});
