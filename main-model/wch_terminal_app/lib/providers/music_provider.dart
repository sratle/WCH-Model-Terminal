import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/cli/cli_engine.dart';
import '../core/cli/cli_commands.dart';
import '../core/state/music_state.dart';
import 'cli_provider.dart';

class MusicNotifier extends StateNotifier<MusicStatus> {
  final CliEngine _cli;
  Timer? _positionTimer;

  MusicNotifier(this._cli)
      : super(const MusicStatus(state: PlayerState.stopped));

  void _startPositionTick() {
    _stopPositionTick();
    _positionTimer = Timer.periodic(const Duration(seconds: 1), (_) {
      if (state.state == PlayerState.playing) {
        final newPos = (state.position ?? Duration.zero) + const Duration(seconds: 1);
        if (state.duration != null && newPos >= state.duration!) {
          _onTrackEnd();
        } else {
          state = state.copyWith(position: newPos);
        }
      }
    });
  }

  void _stopPositionTick() {
    _positionTimer?.cancel();
    _positionTimer = null;
  }

  void _onTrackEnd() {
    _stopPositionTick();
    if (state.hasNext) {
      playTrackAtIndex(state.currentIndex + 1);
    } else {
      state = state.copyWith(
        state: PlayerState.stopped,
        position: state.duration,
      );
    }
  }

  Future<void> playWithPlaylist(
      String file, List<MusicTrack> playlist, int index) async {
    state = state.copyWith(
      playlist: playlist,
      currentIndex: index,
      state: PlayerState.playing,
      position: Duration.zero,
      duration: null,
    );
    final resp = await _cli.execute(CliCommands.play(file));
    if (resp.isSuccess) {
      final parsed = MusicStatus.parsePlayResponse(resp.output);
      if (parsed != null) {
        state = state.copyWith(
          title: parsed.title ?? state.title,
          duration: parsed.duration,
          volume: parsed.volume,
        );
      }
    }
    _startPositionTick();
  }

  Future<void> playTrackAtIndex(int index) async {
    if (index < 0 || index >= state.playlist.length) return;
    final track = state.playlist[index];
    state = state.copyWith(
      currentIndex: index,
      state: PlayerState.playing,
      position: Duration.zero,
      duration: null,
    );
    final resp = await _cli.execute(CliCommands.play(track.name));
    if (resp.isSuccess) {
      final parsed = MusicStatus.parsePlayResponse(resp.output);
      if (parsed != null) {
        state = state.copyWith(
          title: parsed.title ?? state.title,
          duration: parsed.duration,
          volume: parsed.volume,
        );
      }
    }
    _startPositionTick();
  }

  Future<void> pause() async {
    await _cli.execute(CliCommands.pause());
    _stopPositionTick();
    state = state.copyWith(state: PlayerState.paused);
  }

  Future<void> resume() async {
    await _cli.execute(CliCommands.resume());
    state = state.copyWith(state: PlayerState.playing);
    _startPositionTick();
  }

  Future<void> setVolume(int level) async {
    state = state.copyWith(volume: level);
    await _cli.execute(CliCommands.vol(level));
  }

  void stop() {
    _stopPositionTick();
    state = const MusicStatus(state: PlayerState.stopped);
  }

  @override
  void dispose() {
    _stopPositionTick();
    super.dispose();
  }
}

final musicProvider =
    StateNotifierProvider<MusicNotifier, MusicStatus>((ref) {
  final cli = ref.watch(cliEngineProvider);
  return MusicNotifier(cli);
});
