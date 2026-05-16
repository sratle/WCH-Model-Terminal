enum PlayerState { stopped, playing, paused }

class MusicTrack {
  final String name;
  const MusicTrack({required this.name});
}

class MusicStatus {
  final PlayerState state;
  final String? title;
  final Duration? position;
  final Duration? duration;
  final int? volume;
  final List<MusicTrack> playlist;
  final int currentIndex;

  const MusicStatus({
    required this.state,
    this.title,
    this.position,
    this.duration,
    this.volume,
    this.playlist = const [],
    this.currentIndex = -1,
  });

  bool get hasPlaylist => playlist.length > 1 && currentIndex >= 0;
  bool get hasPrev => hasPlaylist && currentIndex > 0;
  bool get hasNext => hasPlaylist && currentIndex < playlist.length - 1;

  static MusicStatus? parsePlayResponse(String text) {
    String? track;
    Duration? duration;
    int? volume;

    final trackMatch = RegExp(r'Playing:\s*(.+)').firstMatch(text);
    if (trackMatch != null) {
      track = trackMatch.group(1)?.trim();
    }

    final durMatch = RegExp(r'Duration:\s*(\d{2}):(\d{2})').firstMatch(text);
    if (durMatch != null) {
      final min = int.parse(durMatch.group(1)!);
      final sec = int.parse(durMatch.group(2)!);
      duration = Duration(minutes: min, seconds: sec);
    }

    final volMatch = RegExp(r'Volume:\s*(\d+)').firstMatch(text);
    if (volMatch != null) {
      volume = int.parse(volMatch.group(1)!);
    }

    if (track == null && duration == null && volume == null) return null;

    return MusicStatus(
      state: PlayerState.playing,
      title: track,
      duration: duration,
      volume: volume,
    );
  }

  MusicStatus copyWith({
    PlayerState? state,
    String? title,
    Duration? position,
    Duration? duration,
    int? volume,
    List<MusicTrack>? playlist,
    int? currentIndex,
  }) {
    return MusicStatus(
      state: state ?? this.state,
      title: title ?? this.title,
      position: position ?? this.position,
      duration: duration ?? this.duration,
      volume: volume ?? this.volume,
      playlist: playlist ?? this.playlist,
      currentIndex: currentIndex ?? this.currentIndex,
    );
  }
}
