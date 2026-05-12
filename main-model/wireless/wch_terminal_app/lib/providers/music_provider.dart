import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/cli/cli_engine.dart';
import '../core/cli/cli_commands.dart';
import '../core/state/music_state.dart';
import 'cli_provider.dart';

class MusicNotifier extends StateNotifier<MusicStatus> {
  final CliEngine _cli;
  Timer? _pollTimer;

  MusicNotifier(this._cli) : super(const MusicStatus(state: PlayerState.stopped));

  void startPolling() {
    stopPolling();
    _pollTimer = Timer.periodic(const Duration(seconds: 2), (_) => refresh());
  }

  void stopPolling() {
    _pollTimer?.cancel();
    _pollTimer = null;
  }

  Future<void> refresh() async {
    final resp = await _cli.execute(CliCommands.playst());
    if (resp.isSuccess) {
      final status = MusicStatus.parsePlayst(resp.output);
      if (status != null) state = status;
    }
  }

  Future<void> play(String file) async {
    await _cli.execute(CliCommands.play(file));
    await refresh();
  }

  Future<void> pause() async {
    await _cli.execute(CliCommands.pause());
    state = state.copyWith(state: PlayerState.paused);
  }

  Future<void> resume() async {
    await _cli.execute(CliCommands.resume());
    state = state.copyWith(state: PlayerState.playing);
  }

  Future<void> setVolume(int level) async {
    await _cli.execute(CliCommands.vol(level));
  }

  @override
  void dispose() {
    stopPolling();
    super.dispose();
  }
}

final musicProvider = StateNotifierProvider<MusicNotifier, MusicStatus>((ref) {
  final cli = ref.watch(cliEngineProvider);
  return MusicNotifier(cli);
});

extension MusicStatusCopy on MusicStatus {
  MusicStatus copyWith({
    PlayerState? state,
    String? title,
    String? filePath,
    Duration? position,
    int? volume,
  }) {
    return MusicStatus(
      state: state ?? this.state,
      title: title ?? this.title,
      filePath: filePath ?? this.filePath,
      position: position ?? this.position,
      volume: volume ?? this.volume,
    );
  }
}
