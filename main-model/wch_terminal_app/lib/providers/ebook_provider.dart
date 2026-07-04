import 'dart:async';
import 'dart:math';
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
      // Navigate to BOOK directory
      final cdResp = await _cli.execute(CliCommands.cd('BOOK'));
      if (!cdResp.isSuccess || cdResp.output.contains('failed')) {
        state = state.copyWith(
          isLoading: false,
          error: 'BOOK 目录不存在或无法进入',
        );
        return;
      }

      // List files
      final lsResp =
          await _cli.execute(CliCommands.ls(), timeout: const Duration(seconds: 15));
      final books = BookItem.parseLsOutput(lsResp.output);

      state = state.copyWith(
        books: books,
        isLoading: false,
        selectedIndex: -1,
        content: '',
        currentPage: 0,
        totalPages: 0,
      );
    } catch (e) {
      state = state.copyWith(isLoading: false, error: e.toString());
    }
  }

  /// Open a book by name: execute "cat" and paginate.
  Future<void> openBook(int index) async {
    if (index < 0 || index >= state.books.length) return;
    final book = state.books[index];

    state = state.copyWith(
      isLoading: true,
      selectedIndex: index,
      error: null,
      content: '',
      currentPage: 0,
      totalPages: 0,
    );

    try {
      final resp = await _cli.execute(
        CliCommands.cat(book.name),
        timeout: const Duration(seconds: 15),
      );

      if (!resp.isSuccess) {
        state = state.copyWith(isLoading: false, error: '读取文件失败');
        return;
      }

      final content = resp.output;
      final totalPages = max(1, (content.length / EBookState.charsPerPage).ceil());

      state = state.copyWith(
        content: content,
        currentPage: 0,
        totalPages: totalPages,
        isLoading: false,
      );
    } catch (e) {
      state = state.copyWith(isLoading: false, error: e.toString());
    }
  }

  void nextPage() {
    if (state.currentPage < state.totalPages - 1) {
      state = state.copyWith(currentPage: state.currentPage + 1);
    }
  }

  void prevPage() {
    if (state.currentPage > 0) {
      state = state.copyWith(currentPage: state.currentPage - 1);
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
