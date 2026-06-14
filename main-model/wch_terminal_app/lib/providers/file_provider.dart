import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/cli/cli_engine.dart';
import '../core/cli/cli_commands.dart';
import '../core/state/file_state.dart';
import 'cli_provider.dart';

class FileNotifier extends StateNotifier<FileSystemState> {
  final CliEngine _cli;
  StreamSubscription<String>? _cwdSub;

  FileNotifier(this._cli)
      : super(const FileSystemState(currentPath: '\\')) {
    // Listen for CWD notifications pushed by Core (when another client
    // changes directory). This keeps the App's path in sync without polling.
    _cwdSub = _cli.cwdStream.listen(_onCwdNotify);
  }

  void _onCwdNotify(String path) {
    if (path != state.currentPath) {
      // Another client changed the directory on Core.
      // Update our local path and refresh the file list.
      state = state.copyWith(currentPath: path, isLoading: true);
      _fetchAndCache();
    }
  }

  @override
  void dispose() {
    _cwdSub?.cancel();
    super.dispose();
  }

  Future<bool> _cd(String path) async {
    final resp = await _cli.execute(CliCommands.cd(path));
    if (!resp.isSuccess) return false;
    final output = resp.output.trim();
    if (output.contains('failed')) return false;
    return true;
  }

  Future<DirectoryNode?> _ls() async {
    final lsResp = await _cli.execute(CliCommands.ls(),
        timeout: const Duration(seconds: 15));
    final output = lsResp.output;
    final items = FileItem.parseLsOutput(output);

    // Extract path from "ls" header line: "--- \PATH ---"
    // This avoids a separate "pwd" round-trip.
    String path = state.currentPath;
    final headerMatch =
        RegExp(r'^---\s*(.+?)\s*---', multiLine: true).firstMatch(output);
    if (headerMatch != null) {
      path = headerMatch.group(1)!.replaceAll('\r', '').trim();
    }

    return DirectoryNode(path: path, items: items);
  }

  Future<void> _fetchAndCache() async {
    state = state.copyWith(isLoading: true, error: null);
    try {
      final dir = await _ls();
      if (dir != null) {
        final newCache = Map<String, DirectoryNode>.from(state.cache);
        newCache[dir.path] = dir;
        state = state.copyWith(
          currentPath: dir.path,
          cache: newCache,
          isLoading: false,
        );
      }
    } catch (e) {
      state = state.copyWith(isLoading: false, error: e.toString());
    }
  }

  Future<void> refresh({bool force = false}) async {
    if (!force && state.cache.containsKey(state.currentPath)) {
      final newCache = Map<String, DirectoryNode>.from(state.cache);
      newCache.remove(state.currentPath);
      state = state.copyWith(cache: newCache);
    }
    await _fetchAndCache();
  }

  Future<bool> enterDirectory(String dirName) async {
    state = state.copyWith(isLoading: true, error: null);
    final ok = await _cd(dirName);
    if (!ok) {
      state = state.copyWith(isLoading: false);
      return false;
    }
    await _fetchAndCache();
    return true;
  }

  Future<bool> goUp() async {
    state = state.copyWith(isLoading: true, error: null);
    final ok = await _cd('..');
    if (!ok) {
      state = state.copyWith(isLoading: false);
      return false;
    }
    await _fetchAndCache();
    return true;
  }

  Future<bool> navigateToPath(String path) async {
    state = state.copyWith(isLoading: true, error: null);
    final ok = await _cd(path);
    if (!ok) {
      state = state.copyWith(isLoading: false);
      return false;
    }
    if (state.cache.containsKey(path)) {
      // Use ls header to get the actual path instead of a separate pwd call
      final dir = await _ls();
      if (dir != null) {
        state = state.copyWith(currentPath: dir.path, isLoading: false);
      } else {
        state = state.copyWith(isLoading: false);
      }
      return true;
    }
    await _fetchAndCache();
    return true;
  }

  Future<bool> switchDevice(StorageDevice device) async {
    if (device == state.currentDevice) return true;
    state = state.copyWith(isLoading: true, error: null);
    final cmd = device == StorageDevice.usb ? 'usb' : 'sd';
    final resp = await _cli.execute(CliCommands.deviceSwitch(cmd),
        timeout: const Duration(seconds: 15));
    if (!resp.isSuccess) {
      state = state.copyWith(isLoading: false, error: '切换设备失败');
      return false;
    }
    final output = resp.output;
    if (output.contains('mount failed') ||
        output.contains('not connected') ||
        output.contains('not initialized') ||
        output.contains('communication test failed')) {
      state = state.copyWith(isLoading: false, error: '设备不存在或挂载失败');
      return false;
    }
    final newCache = Map<String, DirectoryNode>.from(state.cache);
    newCache.clear();
    state = state.copyWith(
      currentDevice: device,
      cache: newCache,
      currentPath: '\\',
    );
    await _fetchAndCache();
    return true;
  }

  Future<String> queryDevice() async {
    final resp = await _cli.execute(CliCommands.device());
    return resp.output.trim();
  }

  Future<void> createFolder(String name) async {
    await _cli.execute(CliCommands.mkdir(name));
    await refresh(force: true);
  }

  Future<void> createFile(String name) async {
    await _cli.execute(CliCommands.touch(name));
    await refresh(force: true);
  }

  Future<void> delete(FileItem item) async {
    if (item.isDirectory) {
      await _cli.execute(CliCommands.rmRf(item.name));
    } else {
      await _cli.execute(CliCommands.rm(item.name));
    }
    await refresh(force: true);
  }

  Future<void> copy(String src, String dst) async {
    await _cli.execute(CliCommands.cp(src, dst));
    await refresh(force: true);
  }

  Future<void> rename(String oldName, String newName) async {
    await _cli.execute(CliCommands.mv(oldName, newName));
    await refresh(force: true);
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
}

final fileProvider =
    StateNotifierProvider<FileNotifier, FileSystemState>((ref) {
  final cli = ref.watch(cliEngineProvider);
  return FileNotifier(cli);
});
