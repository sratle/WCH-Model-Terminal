import 'dart:async';
import 'dart:convert';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../ble/ble_manager.dart';
import '../protocol/protocol_handler.dart';
import '../protocol/protocol_constants.dart';
import '../protocol/frame_model.dart';
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

  // CWD change notification stream: Core pushes [CWD] <path> when
  // directory changes (via cd/device command from any client).
  final _cwdController = StreamController<String>.broadcast();
  Stream<String> get cwdStream => _cwdController.stream;

  StreamSubscription? _dataSub;
  StreamSubscription? _connSub;

  // Heartbeat
  Timer? _heartbeatTimer;
  int _heartbeatMissed = 0;
  static const int _maxHeartbeatMissed = 3;
  static const Duration _heartbeatInterval = Duration(seconds: 5);

  CliEngine({required BleManager ble}) : _ble = ble {
    _dataSub = _ble.dataStream.listen(_onData);
    _connSub = _ble.connectionStream.listen((state) {
      if (state == BluetoothConnectionState.connected) {
        _startHeartbeat();
        _updateMtu();
      } else {
        _stopHeartbeat();
      }
    });
    _protocol.onTimeoutFrame = _onTimeoutFrame;
    if (_ble.isConnected) {
      _startHeartbeat();
      _updateMtu();
    }
  }

  Future<CliResponse> execute(String command, {Duration? timeout}) async {
    final completer = Completer<CliResponse>();
    final pending = _PendingCommand(command, completer);
    _pending.add(pending);

    final frames = _protocol.encodeCliData(command);
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

  /// Check text for [CWD] notifications from Core.
  /// Returns the text with [CWD] lines removed.
  String _checkCwdNotifications(String text) {
    final lines = text.split('\n');
    final remaining = <String>[];
    for (final line in lines) {
      final trimmed = line.trim();
      if (trimmed.startsWith('[CWD] ')) {
        final path = trimmed.substring(6).trim();
        if (path.isNotEmpty) {
          print('[CLI] CWD notification: $path');
          _cwdController.add(path);
        }
      } else {
        remaining.add(line);
      }
    }
    return remaining.join('\n');
  }

  void _onData(List<int> data) {
    final message = _protocol.decodeFrame(data);
    if (message == null) return;

    if (message.type == ProtocolConstants.msgTypeHeartbeat && message.isDirUp) {
      _heartbeatMissed = 0;
      return;
    }

    if (message.type == ProtocolConstants.msgTypeCliData && message.isDirUp) {
      final text = utf8.decode(message.payload, allowMalformed: true);
      // Extract [CWD] notifications before processing
      final cleanedText = _checkCwdNotifications(text);
      print('[CLI] data: type=CLI isEof=${message.isEof} payloadLen=${message.payload.length} textLen=${text.length} pending=${_pending.length}');

      if (_pending.isNotEmpty) {
        final pending = _pending.first;
        pending.buffer.write(cleanedText);

        if (message.isEof) {
          _pending.removeAt(0);
          final total = pending.buffer.length;
          print('[CLI] command complete: "${pending.command}" totalLen=$total');
          pending.completer.complete(
            CliResponse.success(pending.command, pending.buffer.toString()),
          );
        }
      } else {
        if (cleanedText.isNotEmpty) {
          _outputController.add(cleanedText);
        }
      }
    }
  }

  void _onTimeoutFrame(FrameModel frame) {
    if (frame.type == ProtocolConstants.msgTypeCliData && frame.isDirUp) {
      final text = utf8.decode(frame.payload, allowMalformed: true);
      final cleanedText = _checkCwdNotifications(text);
      print('[CLI] timeout frame: payloadLen=${frame.payload.length} pending=${_pending.length}');
      if (_pending.isNotEmpty) {
        final pending = _pending.first;
        pending.buffer.write(cleanedText);
        _pending.removeAt(0);
        print('[CLI] timeout deliver: "${pending.command}" totalLen=${pending.buffer.length}');
        pending.completer.complete(
          CliResponse.success(pending.command, pending.buffer.toString()),
        );
      } else {
        if (cleanedText.isNotEmpty) {
          _outputController.add(cleanedText);
        }
      }
    }
  }

  void _startHeartbeat() {
    _stopHeartbeat();
    _heartbeatMissed = 0;
    _heartbeatTimer = Timer.periodic(_heartbeatInterval, (_) async {
      try {
        final frame = _protocol.encodeHeartbeat();
        await _ble.write(frame, withoutResponse: true);
        _heartbeatMissed++;
        if (_heartbeatMissed > _maxHeartbeatMissed) {
          // Link abnormal, trigger disconnect (auto-reconnect will kick in)
          await _ble.disconnect();
        }
      } catch (e) {
        // Write failed, likely disconnected
      }
    });
  }

  void _stopHeartbeat() {
    _heartbeatTimer?.cancel();
    _heartbeatTimer = null;
    _heartbeatMissed = 0;
  }

  void _updateMtu() {
    final mtu = _ble.negotiatedMtu;
    // Per protocol doc: maxPayload = mtu - 11
    final maxPayload = mtu > 11 ? mtu - 11 : 12;
    _protocol.maxPayloadSize = maxPayload;
  }

  Future<void> sendHeartbeat() async {
    final frame = _protocol.encodeHeartbeat();
    await _ble.write(frame, withoutResponse: true);
  }

  void dispose() {
    _stopHeartbeat();
    _connSub?.cancel();
    _dataSub?.cancel();
    _outputController.close();
    _cwdController.close();
  }
}
