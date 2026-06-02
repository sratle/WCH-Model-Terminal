enum PlayerState { stopped, playing, paused }

enum PlayMode { singleLoop, sequential, singlePlay }

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
  final PlayMode playMode;
  final bool speakerOn;

  const MusicStatus({
    required this.state,
    this.title,
    this.position,
    this.duration,
    this.volume,
    this.playlist = const [],
    this.currentIndex = -1,
    this.playMode = PlayMode.sequential,
    this.speakerOn = false,
  });

  bool get hasPlaylist => playlist.length > 1 && currentIndex >= 0;
  bool get hasPrev => hasPlaylist && currentIndex > 0;
  bool get hasNext => hasPlaylist && currentIndex < playlist.length - 1;

  static PlayerState _parseState(String text) {
    final m = RegExp(r'State:\s*(\w+)').firstMatch(text);
    if (m == null) return PlayerState.stopped;
    switch (m.group(1)) {
      case 'Playing': return PlayerState.playing;
      case 'Paused':  return PlayerState.paused;
      default:        return PlayerState.stopped;
    }
  }

  static String? _parseTrack(String text) {
    final m = RegExp(r'Track:\s*(.+)').firstMatch(text);
    final val = m?.group(1)?.trim();
    if (val == null || val == '(none)') return null;
    return val;
  }

  static Duration? _parseTime(String text) {
    final m = RegExp(r'Time:\s*(\d{2}):(\d{2})').firstMatch(text);
    if (m == null) return null;
    return Duration(minutes: int.parse(m.group(1)!), seconds: int.parse(m.group(2)!));
  }

  static int? _parseVolume(String text) {
    final m = RegExp(r'Volume:\s*(\d+)').firstMatch(text);
    return m != null ? int.parse(m.group(1)!) : null;
  }

  static bool? _parseSpeaker(String text) {
    final m = RegExp(r'Speaker:\s*L=(\w+)\s+R=(\w+)').firstMatch(text);
    if (m == null) return null;
    return m.group(1) == 'ON' || m.group(2) == 'ON';
  }

  static MusicStatus parsePlaystResponse(String text) {
    return MusicStatus(
      state: _parseState(text),
      title: _parseTrack(text),
      position: _parseTime(text),
      volume: _parseVolume(text),
      speakerOn: _parseSpeaker(text) ?? false,
    );
  }

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
    PlayMode? playMode,
    bool? speakerOn,
  }) {
    return MusicStatus(
      state: state ?? this.state,
      title: title ?? this.title,
      position: position ?? this.position,
      duration: duration ?? this.duration,
      volume: volume ?? this.volume,
      playlist: playlist ?? this.playlist,
      currentIndex: currentIndex ?? this.currentIndex,
      playMode: playMode ?? this.playMode,
      speakerOn: speakerOn ?? this.speakerOn,
    );
  }
}
