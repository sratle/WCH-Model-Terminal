import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../providers/music_provider.dart';
import '../core/state/music_state.dart';

class MusicPlayerSheet extends ConsumerStatefulWidget {
  final String fileName;

  const MusicPlayerSheet({super.key, required this.fileName});

  @override
  ConsumerState<MusicPlayerSheet> createState() => _MusicPlayerSheetState();
}

class _MusicPlayerSheetState extends ConsumerState<MusicPlayerSheet>
    with SingleTickerProviderStateMixin {
  bool _hasStarted = false;
  late AnimationController _discAnimController;

  @override
  void initState() {
    super.initState();
    _discAnimController = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 8),
    );
    Future.microtask(() async {
      final music = ref.read(musicProvider.notifier);
      await music.play(widget.fileName);
      music.startPolling();
      if (mounted) setState(() => _hasStarted = true);
    });
  }

  @override
  void dispose() {
    _discAnimController.dispose();
    ref.read(musicProvider.notifier).stopPolling();
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

    if (isPlaying && !_discAnimController.isAnimating) {
      _discAnimController.repeat();
    } else if (!isPlaying && _discAnimController.isAnimating) {
      _discAnimController.stop();
    }

    ref.listen(musicProvider, (_, next) {
      if (_hasStarted && next.state == PlayerState.stopped && mounted) {
        Navigator.of(context).pop();
      }
    });

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
                    const SizedBox(width: 48),
                  ],
                ),
              ),
              Expanded(
                child: SingleChildScrollView(
                  controller: scrollController,
                  child: Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 32),
                    child: Column(
                      children: [
                        const SizedBox(height: 24),
                        _buildDisc(colorScheme, isPlaying),
                        const SizedBox(height: 36),
                        Text(
                          status.title ?? _displayName(widget.fileName),
                          style: Theme.of(context).textTheme.headlineSmall?.copyWith(
                                fontWeight: FontWeight.bold,
                              ),
                          textAlign: TextAlign.center,
                          maxLines: 2,
                          overflow: TextOverflow.ellipsis,
                        ),
                        const SizedBox(height: 4),
                        Text(
                          widget.fileName,
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
    final positionSec = status.position?.inSeconds.toDouble() ?? 0;

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
            value: positionSec,
            max: positionSec > 300 ? positionSec + 30 : 300,
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
                '--:--',
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
          width: 56,
          height: 56,
          child: IconButton.filledTonal(
            iconSize: 28,
            icon: const Icon(Icons.replay_10),
            onPressed: () {},
          ),
        ),
        const SizedBox(width: 20),
        Container(
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
            iconSize: 40,
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
        const SizedBox(width: 20),
        SizedBox(
          width: 56,
          height: 56,
          child: IconButton.filledTonal(
            iconSize: 28,
            icon: const Icon(Icons.forward_10),
            onPressed: () {},
          ),
        ),
      ],
    );
  }

  Widget _buildVolumeSection(MusicStatus status, ColorScheme colorScheme) {
    final volume = (status.volume ?? 80).toDouble();

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      decoration: BoxDecoration(
        color: colorScheme.surfaceContainerHighest.withValues(alpha: 0.5),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Row(
        children: [
          Icon(
            volume == 0 ? Icons.volume_off : Icons.volume_mute,
            size: 20,
            color: colorScheme.onSurfaceVariant,
          ),
          const SizedBox(width: 8),
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
          const SizedBox(width: 8),
          Icon(
            Icons.volume_up,
            size: 20,
            color: colorScheme.onSurfaceVariant,
          ),
          const SizedBox(width: 8),
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
}
