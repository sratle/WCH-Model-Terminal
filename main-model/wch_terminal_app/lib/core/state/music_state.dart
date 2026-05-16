enum PlayerState { stopped, playing, paused }

class MusicStatus {
  final PlayerState state;
  final String? title;
  final String? filePath;
  final Duration? position;
  final int? volume;

  const MusicStatus({
    required this.state,
    this.title,
    this.filePath,
    this.position,
    this.volume,
  });

  static MusicStatus? parsePlayst(String text) {
    String? track;
    Duration? pos;

    final trackMatch = RegExp(r'Track:\s*(.+)').firstMatch(text);
    if (trackMatch != null) {
      track = trackMatch.group(1)?.trim();
      if (track == '(none)') track = null;
    }

    final timeMatch = RegExp(r'Time:\s*(\d{2}):(\d{2})').firstMatch(text);
    if (timeMatch != null) {
      final min = int.parse(timeMatch.group(1)!);
      final sec = int.parse(timeMatch.group(2)!);
      pos = Duration(minutes: min, seconds: sec);
    }

    final state = track != null && track.isNotEmpty
        ? PlayerState.playing
        : PlayerState.stopped;

    return MusicStatus(state: state, title: track, position: pos);
  }
}
