import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../providers/ble_provider.dart';
import '../providers/cli_provider.dart';
import '../providers/terminal_provider.dart';

class TerminalPage extends ConsumerStatefulWidget {
  const TerminalPage({super.key});

  @override
  ConsumerState<TerminalPage> createState() => _TerminalPageState();
}

class _TerminalPageState extends ConsumerState<TerminalPage> {
  final _controller = TextEditingController();
  final _scrollController = ScrollController();
  final List<String> _history = [];
  int _historyIndex = -1;
  final List<String> _output = [];

  @override
  void initState() {
    super.initState();
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
      _output.add('> $cmd');
      _history.add(cmd);
      _historyIndex = _history.length;
    });
    _controller.clear();
    _scrollToBottom();

    final cli = ref.read(cliEngineProvider);
    final resp = await cli.execute(cmd);

    setState(() {
      if (resp.isSuccess) {
        _output.addAll(resp.output.split('\n'));
      } else {
        _output.add('Error: ${resp.error}');
      }
    });
    _scrollToBottom();
  }

  void _scrollToBottom() {
    Future.delayed(const Duration(milliseconds: 100), () {
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
        setState(() => _output.addAll(text.split('\n')));
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
                  onPressed: _sendCommand,
                  child: const Text('发送'),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
