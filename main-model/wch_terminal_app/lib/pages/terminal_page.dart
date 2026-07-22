import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../providers/ble_provider.dart';
import '../providers/cli_provider.dart';
import '../providers/terminal_provider.dart';
import '../utils/constants.dart';

class TerminalPage extends ConsumerStatefulWidget {
  const TerminalPage({super.key});

  @override
  ConsumerState<TerminalPage> createState() => _TerminalPageState();
}

class _TerminalPageState extends ConsumerState<TerminalPage> {
  final _controller = TextEditingController();
  final _scrollController = ScrollController();
  final _focusNode = FocusNode();
  final List<String> _history = [];
  int _historyIndex = -1;
  final List<String> _output = [];

  @override
  void initState() {
    super.initState();
  }

  @override
  void dispose() {
    _controller.dispose();
    _scrollController.dispose();
    _focusNode.dispose();
    super.dispose();
  }

  bool _isSending = false;

  /// Trim output to max lines to prevent unbounded memory growth.
  void _trimOutput() {
    const maxLines = AppConstants.terminalMaxLines;
    if (_output.length > maxLines) {
      _output.removeRange(0, _output.length - maxLines);
    }
  }

  /// Remove ANSI/VT100 escape sequences and stray CR so they don't render as
  /// stray boxes/garbage in the monospace view.
  String _stripAnsi(String input) {
    var s = input;
    // CSI sequences: ESC [ ... <final letter>  (colors, cursor moves, erase)
    s = s.replaceAll(RegExp(r'\x1B\[[0-9;?]*[A-Za-z]'), '');
    // OSC sequences: ESC ] ... BEL
    s = s.replaceAll(RegExp(r'\x1B\][^\x07]*\x07'), '');
    // Lone ESC + single char (e.g. ESC c reset)
    s = s.replaceAll(RegExp(r'\x1B.'), '');
    s = s.replaceAll('\r', '');
    return s;
  }

  /// Append Core output to the terminal, honoring clear-screen sequences
  /// (`clear` emits \x1B[2J\x1B[H) and stripping other escape codes.
  void _appendOutput(String rawText) {
    var text = rawText;
    // Clear-screen: ESC[2J / ESC[<n>J or form-feed. Discard everything up to
    // and including the last clear marker, then wipe the view.
    final clearRe = RegExp(r'\x1B\[[0-3]?J|\x0C');
    final matches = clearRe.allMatches(text).toList();
    if (matches.isNotEmpty) {
      _output.clear();
      text = text.substring(matches.last.end);
    }
    final cleaned = _stripAnsi(text);
    if (cleaned.isEmpty) return;
    _output.addAll(cleaned.split('\n'));
    _trimOutput();
  }

  Future<void> _sendCommand() async {
    final cmd = _controller.text.trim();
    if (cmd.isEmpty) return;

    final isConnected = ref.read(isConnectedProvider);
    if (!isConnected) {
      setState(() => _output.add('> $cmd'));
      setState(() => _output.add('Error: 未连接设备，请先扫描并连接'));
      _controller.clear();
      _scrollToBottom();
      return;
    }

    setState(() {
      _isSending = true;
      _output.add('> $cmd');
      _history.add(cmd);
      _historyIndex = _history.length;
    });
    _controller.clear();
    _scrollToBottom();

    final cli = ref.read(cliEngineProvider);
    final resp = await cli.execute(cmd);

    setState(() {
      _isSending = false;
      if (resp.isSuccess) {
        _appendOutput(resp.output);
      } else {
        _output.add('Error: ${resp.error}');
      }
      _trimOutput();
    });
    _scrollToBottom();
    // Keep the keyboard up for the next command instead of dismissing it.
    _focusNode.requestFocus();
  }

  void _scrollToBottom() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_scrollController.hasClients) {
        _scrollController.jumpTo(_scrollController.position.maxScrollExtent);
      }
    });
  }

  void _clear() => setState(() => _output.clear());

  void _historyUp() {
    if (_history.isEmpty) return;
    if (_historyIndex > 0) _historyIndex--;
    _controller.text = _history[_historyIndex];
  }

  void _historyDown() {
    if (_history.isEmpty) return;
    if (_historyIndex < _history.length - 1) {
      _historyIndex++;
      _controller.text = _history[_historyIndex];
    } else {
      _historyIndex = _history.length;
      _controller.clear();
    }
  }

  @override
  Widget build(BuildContext context) {
    ref.listen(terminalOutputProvider, (_, asyncValue) {
      asyncValue.whenData((text) {
        setState(() {
          _appendOutput(text);
        });
        _scrollToBottom();
      });
    });

    return Scaffold(
      appBar: AppBar(
        title: const Text('终端'),
        actions: [
          IconButton(icon: const Icon(Icons.clear), onPressed: _clear),
        ],
      ),
      body: Column(
        children: [
          Expanded(
            child: ListView.builder(
              controller: _scrollController,
              itemCount: _output.length,
              itemBuilder: (context, index) {
                final line = _output[index];
                return Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 1),
                  child: SelectableText(
                    line,
                    style: const TextStyle(fontFamily: 'monospace', fontSize: 13),
                  ),
                );
              },
            ),
          ),
          const Divider(height: 1),
          Padding(
            padding: const EdgeInsets.all(8),
            child: Row(
              children: [
                IconButton(
                  icon: const Icon(Icons.arrow_upward),
                  onPressed: _historyUp,
                ),
                IconButton(
                  icon: const Icon(Icons.arrow_downward),
                  onPressed: _historyDown,
                ),
                Expanded(
                  child: TextField(
                    controller: _controller,
                    focusNode: _focusNode,
                    decoration: const InputDecoration(
                      hintText: '输入命令...',
                      border: OutlineInputBorder(),
                      contentPadding: EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                    ),
                    style: const TextStyle(fontFamily: 'monospace'),
                    onSubmitted: (_) => _sendCommand(),
                  ),
                ),
                const SizedBox(width: 8),
                ElevatedButton(
                  onPressed: _isSending ? null : _sendCommand,
                  child: _isSending
                      ? const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
                        )
                      : const Text('发送'),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
