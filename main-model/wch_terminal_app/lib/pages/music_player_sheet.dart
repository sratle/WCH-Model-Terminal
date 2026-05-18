import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../providers/music_provider.dart';
import '../core/state/music_state.dart';

class MusicPlayerSheet extends ConsumerStatefulWidget {
  final String fileName;
  final List<MusicTrack>? playlist;
  final int initialIndex;
  final bool autoPlay;

  const MusicPlayerSheet({
    super.key,
    required this.fileName,
    this.playlist,
    this.initialIndex = -1,
    this.autoPlay = true,
  });

  factory MusicPlayerSheet.resume({Key? key}) {
    return MusicPlayerSheet(
      key: key,
      fileName: '',
      autoPlay: false,
    );
  }

  @override
  ConsumerState<MusicPlayerSheet> createState() => _MusicPlayerSheetState();
}

class _MusicPlayerSheetState extends ConsumerState<MusicPlayerSheet>
    with SingleTickerProviderStateMixin {
  bool _hasStarted = false;
  late AnimationController _discAnimController;
  bool _showPlaylist = false;

  @override
  void initState() {
    super.initState();
    _discAnimController = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 8),
    );
    Future.microtask(() async {
      final music = ref.read(musicProvider.notifier);
      if (widget.autoPlay) {
        final playlist = widget.playlist ?? const <MusicTrack>[];
        final index = widget.initialIndex;
        if (playlist.isNotEmpty && index >= 0) {
          await music.playWithPlaylist(widget.fileName, playlist, index);
        } else {
          await music.playWithPlaylist(widget.fileName, const [], -1);
        }
      }
      if (mounted) setState(() => _hasStarted = true);
    });
  }

  @override
  void dispose() {
    _discAnimController.dispose();
    super.dispose();
  }

  String _formatDuration(Duration? d) {
    if (d == null) return '00:00';
    final m = d.inMinutes.toString().padLeft(2, '0');
    final s = (d.inSeconds % 60).toString().padLeft(2, '0');
    return '$m:$s';
  }

  String _displayName(String fileName) {
    final dotIdx = fileName.lastIndexOf('.');
    if (dotIdx > 0) return fileName.substring(0, dotIdx);
    return fileName;
  }

  @override
  Widget build(BuildContext context) {
    final status = ref.watch(musicProvider);
    final colorScheme = Theme.of(context).colorScheme;
    final isPlaying = status.state == PlayerState.playing;
    final playlist = status.playlist;
    final currentIndex = status.currentIndex;

    if (isPlaying && !_discAnimController.isAnimating) {
      _discAnimController.repeat();
    } else if (!isPlaying && _discAnimController.isAnimating) {
      _discAnimController.stop();
    }

    ref.listen(musicProvider, (prev, next) {
      if (_hasStarted && next.state == PlayerState.stopped && prev?.state != PlayerState.stopped) {
        if (!next.hasNext) {
          Navigator.of(context).pop();
        }
      }
    });

    final currentFileName = status.title ?? (widget.fileName.isNotEmpty ? widget.fileName : '');
    final displayTitle = _displayName(currentFileName);

    return DraggableScrollableSheet(
      initialChildSize: 0.92,
      minChildSize: 0.5,
      maxChildSize: 0.95,
      expand: false,
      builder: (context, scrollController) {
        return Container(
          decoration: BoxDecoration(
            gradient: LinearGradient(
              begin: Alignment.topCenter,
              end: Alignment.bottomCenter,
              colors: [
                colorScheme.primaryContainer.withValues(alpha: 0.3),
                colorScheme.surface,
              ],
            ),
            borderRadius: const BorderRadius.vertical(top: Radius.circular(20)),
          ),
          child: Column(
            children: [
              const SizedBox(height: 8),
              Container(
                width: 40,
                height: 4,
                decoration: BoxDecoration(
                  color: Colors.grey.shade400,
                  borderRadius: BorderRadius.circular(2),
                ),
              ),
              Padding(
                padding: const EdgeInsets.symmetric(horizontal: 8),
                child: Row(
                  children: [
                    IconButton(
                      icon: const Icon(Icons.keyboard_arrow_down),
                      onPressed: () => Navigator.pop(context),
                    ),
                    Expanded(
                      child: Text(
                        '正在播放',
                        textAlign: TextAlign.center,
                        style: TextStyle(
                          color: colorScheme.onSurfaceVariant,
                          fontWeight: FontWeight.w500,
                        ),
                      ),
                    ),
                    if (playlist.length > 1)
                      IconButton(
                        icon: Icon(
                          _showPlaylist ? Icons.queue_music : Icons.queue_music_outlined,
                        ),
                        tooltip: '播放列表',
                        onPressed: () => setState(() => _showPlaylist = !_showPlaylist),
                      )
                    else
                      const SizedBox(width: 48),
                  ],
                ),
              ),
              Expanded(
                child: _showPlaylist && playlist.length > 1
                    ? _buildPlaylistView(playlist, currentIndex, colorScheme)
                    : SingleChildScrollView(
                        controller: scrollController,
                        child: Padding(
                          padding: const EdgeInsets.symmetric(horizontal: 32),
                          child: Column(
                            children: [
                              const SizedBox(height: 24),
                              _buildDisc(colorScheme, isPlaying),
                              const SizedBox(height: 36),
                              Text(
                                displayTitle,
                                style: Theme.of(context)
                                    .textTheme
                                    .headlineSmall
                                    ?.copyWith(fontWeight: FontWeight.bold),
                                textAlign: TextAlign.center,
                                maxLines: 2,
                                overflow: TextOverflow.ellipsis,
                              ),
                              const SizedBox(height: 4),
                              Text(
                                currentFileName,
                                style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                                      color: colorScheme.onSurfaceVariant,
                                    ),
                                textAlign: TextAlign.center,
                              ),
                              const SizedBox(height: 32),
                              _buildProgressSection(status, colorScheme),
                              const SizedBox(height: 16),
                              _buildControls(status, colorScheme, isPlaying),
                              const SizedBox(height: 28),
                              _buildVolumeSection(status, colorScheme),
                              const SizedBox(height: 24),
                            ],
                          ),
                        ),
                      ),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _buildPlaylistView(
      List<MusicTrack> playlist, int currentIndex, ColorScheme colorScheme) {
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
          child: Row(
            children: [
              Text(
                '播放列表',
                style: Theme.of(context).textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.bold,
                    ),
              ),
              const Spacer(),
              Text(
                '${currentIndex + 1}/${playlist.length}',
                style: TextStyle(
                  color: colorScheme.onSurfaceVariant,
                  fontSize: 13,
                ),
              ),
            ],
          ),
        ),
        const Divider(height: 1),
        Expanded(
          child: ListView.builder(
            itemCount: playlist.length,
            itemBuilder: (context, index) {
              final track = playlist[index];
              final isCurrent = index == currentIndex;
              return ListTile(
                leading: Icon(
                  isCurrent ? Icons.music_note : Icons.audiotrack,
                  color: isCurrent ? colorScheme.primary : null,
                ),
                title: Text(
                  _displayName(track.name),
                  style: TextStyle(
                    fontWeight: isCurrent ? FontWeight.bold : FontWeight.normal,
                    color: isCurrent ? colorScheme.primary : null,
                  ),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
                subtitle: Text(
                  track.name,
                  style: TextStyle(fontSize: 12, color: colorScheme.onSurfaceVariant),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
                trailing: isCurrent
                    ? SizedBox(
                        width: 20,
                        height: 20,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          color: colorScheme.primary,
                        ),
                      )
                    : null,
                onTap: () {
                  ref.read(musicProvider.notifier).playTrackAtIndex(index);
                },
              );
            },
          ),
        ),
      ],
    );
  }

  Widget _buildDisc(ColorScheme colorScheme, bool isPlaying) {
    return AnimatedBuilder(
      animation: _discAnimController,
      builder: (context, child) {
        return Transform.rotate(
          angle: _discAnimController.value * 2 * 3.14159,
          child: child,
        );
      },
      child: Container(
        width: 200,
        height: 200,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          gradient: RadialGradient(
            colors: [
              colorScheme.primaryContainer,
              colorScheme.primary.withValues(alpha: 0.3),
              colorScheme.primaryContainer.withValues(alpha: 0.6),
            ],
            stops: const [0.0, 0.5, 1.0],
          ),
          boxShadow: [
            BoxShadow(
              color: colorScheme.primary.withValues(alpha: 0.3),
              blurRadius: 24,
              spreadRadius: 2,
            ),
          ],
        ),
        child: Center(
          child: Container(
            width: 72,
            height: 72,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: colorScheme.surface,
              boxShadow: [
                BoxShadow(
                  color: Colors.black.withValues(alpha: 0.15),
                  blurRadius: 8,
                  spreadRadius: 1,
                ),
              ],
            ),
            child: Icon(
              Icons.music_note,
              size: 36,
              color: colorScheme.primary,
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildProgressSection(MusicStatus status, ColorScheme colorScheme) {
    final positionSec = status.position?.inSeconds.toDouble() ?? 0.0;
    final durationSec = status.duration?.inSeconds.toDouble() ?? 0.0;
    final maxVal = durationSec > 0 ? durationSec : (positionSec > 300 ? positionSec + 30 : 300.0);

    return Column(
      children: [
        SliderTheme(
          data: SliderThemeData(
            trackHeight: 4,
            thumbShape: const RoundSliderThumbShape(enabledThumbRadius: 6),
            overlayShape: const RoundSliderOverlayShape(overlayRadius: 14),
            activeTrackColor: colorScheme.primary,
            inactiveTrackColor: colorScheme.primaryContainer,
            thumbColor: colorScheme.primary,
          ),
          child: Slider(
            value: positionSec.clamp(0.0, maxVal).toDouble(),
            max: maxVal,
            onChanged: null,
          ),
        ),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 8),
          child: Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text(
                _formatDuration(status.position),
                style: TextStyle(
                  color: colorScheme.onSurfaceVariant,
                  fontSize: 12,
                ),
              ),
              Text(
                _formatDuration(status.duration),
                style: TextStyle(
                  color: colorScheme.onSurfaceVariant,
                  fontSize: 12,
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _buildControls(MusicStatus status, ColorScheme colorScheme, bool isPlaying) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        SizedBox(
          width: 40,
          height: 40,
          child: IconButton(
            icon: Icon(_playModeIcon(status.playMode), size: 20),
            padding: EdgeInsets.zero,
            style: IconButton.styleFrom(
              foregroundColor: colorScheme.onSurfaceVariant,
            ),
            onPressed: () {
              final modes = PlayMode.values;
              final nextIndex = (modes.indexOf(status.playMode) + 1) % modes.length;
              ref.read(musicProvider.notifier).setPlayMode(modes[nextIndex]);
            },
          ),
        ),
        const SizedBox(width: 4),
        SizedBox(
          width: 48,
          height: 48,
          child: IconButton.filledTonal(
            iconSize: 24,
            padding: EdgeInsets.zero,
            icon: Icon(
              Icons.skip_previous,
              color: status.hasPrev ? null : colorScheme.onSurface.withValues(alpha: 0.3),
            ),
            onPressed: status.hasPrev
                ? () => ref.read(musicProvider.notifier).playTrackAtIndex(status.currentIndex - 1)
                : null,
          ),
        ),
        const SizedBox(width: 16),
        Container(
          width: 56,
          height: 56,
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            color: colorScheme.primary,
            boxShadow: [
              BoxShadow(
                color: colorScheme.primary.withValues(alpha: 0.4),
                blurRadius: 12,
                spreadRadius: 2,
              ),
            ],
          ),
          child: IconButton(
            iconSize: 36,
            padding: EdgeInsets.zero,
            icon: Icon(
              isPlaying ? Icons.pause : Icons.play_arrow,
              color: colorScheme.onPrimary,
            ),
            onPressed: () async {
              final notifier = ref.read(musicProvider.notifier);
              if (isPlaying) {
                await notifier.pause();
              } else {
                await notifier.resume();
              }
            },
          ),
        ),
        const SizedBox(width: 16),
        SizedBox(
          width: 48,
          height: 48,
          child: IconButton.filledTonal(
            iconSize: 24,
            padding: EdgeInsets.zero,
            icon: Icon(
              Icons.skip_next,
              color: status.hasNext ? null : colorScheme.onSurface.withValues(alpha: 0.3),
            ),
            onPressed: status.hasNext
                ? () => ref.read(musicProvider.notifier).playTrackAtIndex(status.currentIndex + 1)
                : null,
          ),
        ),
        const SizedBox(width: 4),
        SizedBox(
          width: 40,
          height: 40,
          child: IconButton(
            icon: const Icon(Icons.playlist_play, size: 20),
            padding: EdgeInsets.zero,
            style: IconButton.styleFrom(
              foregroundColor: status.playlist.length > 1
                  ? colorScheme.onSurfaceVariant
                  : colorScheme.onSurface.withValues(alpha: 0.3),
            ),
            onPressed: status.playlist.length > 1
                ? () => setState(() => _showPlaylist = !_showPlaylist)
                : null,
          ),
        ),
      ],
    );
  }

  IconData _playModeIcon(PlayMode mode) {
    switch (mode) {
      case PlayMode.singleLoop:
        return Icons.repeat_one;
      case PlayMode.sequential:
        return Icons.repeat;
      case PlayMode.singlePlay:
        return Icons.looks_one;
    }
  }

  Widget _buildVolumeSection(MusicStatus status, ColorScheme colorScheme) {
    final volume = (status.volume ?? 60).toDouble();

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      decoration: BoxDecoration(
        color: colorScheme.surfaceContainerHighest.withValues(alpha: 0.5),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Row(
        children: [
          Icon(
            volume == 0 ? Icons.volume_off : Icons.volume_up,
            size: 20,
            color: colorScheme.onSurfaceVariant,
          ),
          const SizedBox(width: 4),
          _volumeButton(Icons.remove, colorScheme, () {
            final newVol = (volume - 5).clamp(0, 100).toInt();
            ref.read(musicProvider.notifier).setVolume(newVol);
          }),
          Expanded(
            child: SliderTheme(
              data: SliderThemeData(
                trackHeight: 3,
                thumbShape: const RoundSliderThumbShape(enabledThumbRadius: 5),
                activeTrackColor: colorScheme.primary,
                inactiveTrackColor: colorScheme.primaryContainer,
                thumbColor: colorScheme.primary,
              ),
              child: Slider(
                value: volume,
                min: 0,
                max: 100,
                divisions: 20,
                onChanged: (v) {
                  ref.read(musicProvider.notifier).setVolume(v.toInt());
                },
              ),
            ),
          ),
          _volumeButton(Icons.add, colorScheme, () {
            final newVol = (volume + 5).clamp(0, 100).toInt();
            ref.read(musicProvider.notifier).setVolume(newVol);
          }),
          const SizedBox(width: 4),
          SizedBox(
            width: 32,
            child: Text(
              '${volume.toInt()}',
              textAlign: TextAlign.center,
              style: TextStyle(
                color: colorScheme.onSurfaceVariant,
                fontSize: 12,
                fontWeight: FontWeight.w500,
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _volumeButton(IconData icon, ColorScheme colorScheme, VoidCallback onPressed) {
    return SizedBox(
      width: 32,
      height: 32,
      child: IconButton(
        icon: Icon(icon, size: 18),
        padding: EdgeInsets.zero,
        style: IconButton.styleFrom(
          tapTargetSize: MaterialTapTargetSize.shrinkWrap,
          foregroundColor: colorScheme.onSurfaceVariant,
        ),
        onPressed: onPressed,
      ),
    );
  }
}
