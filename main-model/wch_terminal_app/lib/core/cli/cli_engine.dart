import 'dart:async';
import 'dart:convert';
import '../ble/ble_manager.dart';
import '../protocol/protocol_handler.dart';
import '../protocol/protocol_constants.dart';
import 'cli_response.dart';

class _PendingCommand {
  final String command;
  final Completer<CliResponse> completer;
  final StringBuffer buffer = StringBuffer();
  final DateTime startTime;

  _PendingCommand(this.command, this.completer)
      : startTime = DateTime.now();
}

class CliEngine {
  final BleManager _ble;
  final ProtocolHandler _protocol = ProtocolHandler();

  final List<_PendingCommand> _pending = [];

  final _outputController = StreamController<String>.broadcast();
  Stream<String> get outputStream => _outputController.stream;

  StreamSubscription? _dataSub;

  CliEngine({required BleManager ble}) : _ble = ble {
    _dataSub = _ble.dataStream.listen(_onData);
  }

  Future<CliResponse> execute(String command, {Duration? timeout}) async {
    final completer = Completer<CliResponse>();
    final pending = _PendingCommand(command, completer);
    _pending.add(pending);

    final frames = _protocol.encodeCliData('$command\n');
    for (final frame in frames) {
      await _ble.write(frame, withoutResponse: false);
    }

    // Timeout
    final effectiveTimeout = timeout ?? const Duration(seconds: 5);
    Timer(effectiveTimeout, () {
      if (!completer.isCompleted && _pending.contains(pending)) {
        _pending.remove(pending);
        completer.complete(
          CliResponse.error(command, 'Command timeout'),
        );
      }
    });

    return completer.future;
  }

  void _onData(List<int> data) {
    final message = _protocol.decodeFrame(data);
    if (message == null) return;

    if (message.type == ProtocolConstants.msgTypeCliData && message.isDirUp) {
      final text = utf8.decode(message.payload, allowMalformed: true);

      if (_pending.isNotEmpty) {
        final pending = _pending.first;
        pending.buffer.write(text);

        if (message.isEof) {
          _pending.removeAt(0);
          pending.completer.complete(
            CliResponse.success(pending.command, pending.buffer.toString()),
          );
        }
      } else {
        _outputController.add(text);
      }
    }
  }

  Future<void> sendHeartbeat() async {
    final frame = _protocol.encodeHeartbeat();
    await _ble.write(frame, withoutResponse: true);
  }

  void dispose() {
    _dataSub?.cancel();
    _outputController.close();
  }
}
