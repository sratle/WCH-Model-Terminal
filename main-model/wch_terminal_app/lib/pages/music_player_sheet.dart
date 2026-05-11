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

class _MusicPlayerSheetState extends ConsumerState<MusicPlayerSheet> {
  Timer? _pollTimer;

  @override
  void initState() {
    super.initState();
    Future.microtask(() async {
      final music = ref.read(musicProvider.notifier);
      await music.play(widget.fileName);
      music.startPolling();
    });
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    ref.read(musicProvider.notifier).stopPolling();
    super.dispose();
  }

  String _formatDuration(Duration? d) {
    if (d == null) return '--:--';
    final m = d.inMinutes.toString().padLeft(2, '0');
    final s = (d.inSeconds % 60).toString().padLeft(2, '0');
    return '$m:$s';
  }

  @override
  Widget build(BuildContext context) {
    final status = ref.watch(musicProvider);

    return DraggableScrollableSheet(
      initialChildSize: 0.85,
      minChildSize: 0.5,
      maxChildSize: 0.95,
      expand: false,
      builder: (context, scrollController) {
        return Container(
          decoration: BoxDecoration(
            color: Theme.of(context).colorScheme.surface,
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
              AppBar(
                backgroundColor: Colors.transparent,
                elevation: 0,
                leading: IconButton(
                  icon: const Icon(Icons.keyboard_arrow_down),
                  onPressed: () => Navigator.pop(context),
                ),
                title: const Text('音乐播放'),
                centerTitle: true,
              ),
              Expanded(
                child: Padding(
                  padding: const EdgeInsets.all(24),
                  child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      Container(
                        width: 180,
                        height: 180,
                        decoration: BoxDecoration(
                          color: Theme.of(context).colorScheme.primaryContainer,
                          borderRadius: BorderRadius.circular(16),
                        ),
                        child: const Icon(Icons.music_note, size: 80),
                      ),
                      const SizedBox(height: 32),
                      Text(
                        status.title ?? widget.fileName,
                        style: Theme.of(context).textTheme.headlineSmall,
                        textAlign: TextAlign.center,
                      ),
                      const SizedBox(height: 8),
                      Text(
                        widget.fileName,
                        style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                          color: Colors.grey,
                        ),
                      ),
                      const SizedBox(height: 32),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          Text(_formatDuration(status.position)),
                          const SizedBox(width: 12),
                          Expanded(
                            child: Slider(
                              value: status.position?.inSeconds.toDouble() ?? 0,
                              max: 300, // Unknown duration, placeholder
                              onChanged: (_) {},
                            ),
                          ),
                          const SizedBox(width: 12),
                          const Text('--:--'),
                        ],
                      ),
                      const SizedBox(height: 24),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          IconButton(
                            iconSize: 48,
                            icon: Icon(
                              status.state == PlayerState.playing
                                  ? Icons.pause_circle_filled
                                  : Icons.play_circle_filled,
                            ),
                            onPressed: () async {
                              final notifier = ref.read(musicProvider.notifier);
                              if (status.state == PlayerState.playing) {
                                await notifier.pause();
                              } else {
                                await notifier.resume();
                              }
                            },
                          ),
                        ],
                      ),
                      const SizedBox(height: 32),
                      Row(
                        children: [
                          const Icon(Icons.volume_mute),
                          Expanded(
                            child: Slider(
                              value: (status.volume ?? 80).toDouble(),
                              min: 0,
                              max: 100,
                              divisions: 100,
                              label: '${status.volume ?? 80}',
                              onChanged: (v) {
                                ref.read(musicProvider.notifier).setVolume(v.toInt());
                              },
                            ),
                          ),
                          const Icon(Icons.volume_up),
                        ],
                      ),
                    ],
                  ),
                ),
              ),
            ],
          ),
        );
      },
    );
  }
}
