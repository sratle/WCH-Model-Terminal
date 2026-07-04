import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/cli/cli_engine.dart';
import '../core/cli/cli_commands.dart';
import '../core/state/images_state.dart';
import 'cli_provider.dart';

class ImagesNotifier extends StateNotifier<ImagesState> {
  final CliEngine _cli;

  ImagesNotifier(this._cli) : super(const ImagesState());

  /// Load BMP file list via "bmp ls".
  Future<void> loadBmpList() async {
    state = state.copyWith(isLoading: true, error: null);
    try {
      final resp = await _cli.execute(
        CliCommands.bmpLs(),
        timeout: const Duration(seconds: 15),
      );

      if (!resp.isSuccess) {
        state = state.copyWith(isLoading: false, error: 'bmp ls 命令执行失败');
        return;
      }

      if (resp.output.contains('not found')) {
        state = state.copyWith(
          isLoading: false,
          error: 'BMP 目录不存在',
          files: [],
        );
        return;
      }

      final files = BmpItem.parseLsOutput(resp.output);
      state = state.copyWith(
        files: files,
        isLoading: false,
        selectedIndex: -1,
        currentBmp: null,
      );
    } catch (e) {
      state = state.copyWith(isLoading: false, error: e.toString());
    }
  }

  /// Load BMP preview via "bmp get".
  /// Parses the header info and hex dump from the output.
  Future<void> loadBmpPreview(int index) async {
    if (index < 0 || index >= state.files.length) return;
    final file = state.files[index];

    state = state.copyWith(
      isLoading: true,
      selectedIndex: index,
      error: null,
      currentBmp: null,
    );

    try {
      // Strip extension for bmp get command (it appends .BMP automatically)
      final nameNoExt = file.name.contains('.')
          ? file.name.substring(0, file.name.lastIndexOf('.'))
          : file.name;

      final resp = await _cli.execute(
        CliCommands.bmpGet(nameNoExt),
        timeout: const Duration(seconds: 30),
      );

      if (!resp.isSuccess) {
        state = state.copyWith(isLoading: false, error: 'bmp get 命令执行失败');
        return;
      }

      final info = BmpInfo.parseHeader(resp.output);
      if (info == null) {
        state = state.copyWith(
          isLoading: false,
          error: '无法解析 BMP 信息',
        );
        return;
      }

      state = state.copyWith(
        currentBmp: info,
        isLoading: false,
      );
    } catch (e) {
      state = state.copyWith(isLoading: false, error: e.toString());
    }
  }
}

final imagesProvider =
    StateNotifierProvider<ImagesNotifier, ImagesState>((ref) {
  final cli = ref.watch(cliEngineProvider);
  return ImagesNotifier(cli);
});
