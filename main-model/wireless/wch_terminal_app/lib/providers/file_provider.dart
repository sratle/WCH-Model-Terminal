import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/cli/cli_engine.dart';
import '../core/cli/cli_commands.dart';
import '../core/state/file_state.dart';
import 'cli_provider.dart';

class FileNotifier extends StateNotifier<AsyncValue<DirectoryNode>> {
  final CliEngine _cli;

  FileNotifier(this._cli) : super(const AsyncValue.loading());

  Future<void> refresh() async {
    state = const AsyncValue.loading();
    try {
      final pwdResp = await _cli.execute(CliCommands.pwd());
      final path = pwdResp.output.trim();

      final lsResp = await _cli.execute(CliCommands.ls());
      final items = FileItem.parseLsOutput(lsResp.output);

      state = AsyncValue.data(DirectoryNode(
        path: path,
        items: items,
        parentPath: _getParentPath(path),
      ));
    } catch (e, st) {
      state = AsyncValue.error(e, st);
    }
  }

  Future<void> enterDirectory(String dirName) async {
    await _cli.execute(CliCommands.cd(dirName));
    await refresh();
  }

  Future<void> goUp() async {
    await _cli.execute(CliCommands.cd('..'));
    await refresh();
  }

  Future<void> navigateToPath(String path) async {
    await _cli.execute(CliCommands.cd(path));
    await refresh();
  }

  Future<void> createFolder(String name) async {
    await _cli.execute(CliCommands.mkdir(name));
    await refresh();
  }

  Future<void> createFile(String name) async {
    await _cli.execute(CliCommands.touch(name));
    await refresh();
  }

  Future<void> delete(FileItem item) async {
    if (item.isDirectory) {
      await _cli.execute(CliCommands.rmRf(item.name));
    } else {
      await _cli.execute(CliCommands.rm(item.name));
    }
    await refresh();
  }

  Future<void> copy(String src, String dst) async {
    await _cli.execute(CliCommands.cp(src, dst));
    await refresh();
  }

  Future<void> rename(String oldName, String newName) async {
    await _cli.execute(CliCommands.mv(oldName, newName));
    await refresh();
  }

  Future<void> writeFile(String name, String content) async {
    final lines = content.split('\n');
    for (int i = 0; i < lines.length; i++) {
      final line = lines[i];
      final escaped = line.replaceAll('"', '\\"');
      final cmd = i == 0
          ? CliCommands.echoOverwrite(escaped, name)
          : CliCommands.echoAppend(escaped, name);
      await _cli.execute(cmd);
    }
  }

  Future<String> getFileContent(String name) async {
    final resp = await _cli.execute(CliCommands.cat(name));
    return resp.output;
  }

  Future<String> stat(String name) async {
    final resp = await _cli.execute(CliCommands.stat(name));
    return resp.output;
  }

  String? _getParentPath(String path) {
    final idx = path.lastIndexOf('\\');
    if (idx <= 0) return null;
    return path.substring(0, idx);
  }
}

final fileProvider = StateNotifierProvider<FileNotifier, AsyncValue<DirectoryNode>>((ref) {
  final cli = ref.watch(cliEngineProvider);
  return FileNotifier(cli);
});
