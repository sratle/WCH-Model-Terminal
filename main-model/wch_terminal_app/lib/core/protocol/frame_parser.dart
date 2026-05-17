import 'dart:async';
import 'frame_model.dart';
import 'protocol_constants.dart';

class FrameParser {
  final List<int> _buffer = [];
  int? _expectedSeq;
  Timer? _timeoutTimer;
  int? _sofFlags;
  int? _sofType;

  void Function(String)? onError;

  FrameModel? feed(FrameModel frame) {
    if (frame.isSof) {
      if (_expectedSeq != null && frame.type != ProtocolConstants.msgTypeCliData) {
        return null;
      }
      _buffer.clear();
      _buffer.addAll(frame.payload);
      _expectedSeq = (frame.seq + 1) & 0xFF;
      _sofFlags = frame.flags;
      _sofType = frame.type;
      _startTimeout();
      if (frame.isEof) {
        _cancelTimeout();
        _expectedSeq = null;
        return _buildFrame(_sofType!, _sofFlags!, frame.seq, _buffer);
      }
      return null;
    }

    if (_expectedSeq != null && frame.type != ProtocolConstants.msgTypeCliData) {
      return null;
    }

    if (_expectedSeq == null) {
      onError?.call('Unexpected frame without SOF');
      return null;
    }

    if (frame.seq != _expectedSeq) {
      onError?.call('SEQ mismatch: expected $_expectedSeq, got ${frame.seq}');
      _buffer.clear();
      _expectedSeq = null;
      _sofFlags = null;
      _sofType = null;
      _cancelTimeout();
      return null;
    }

    _buffer.addAll(frame.payload);
    _expectedSeq = (_expectedSeq! + 1) & 0xFF;
    _startTimeout();

    if (frame.isEof) {
      _cancelTimeout();
      final flags = _sofFlags! | FrameFlags.eof;
      final result = _buildFrame(_sofType!, flags, frame.seq, _buffer);
      _expectedSeq = null;
      _sofFlags = null;
      _sofType = null;
      return result;
    }

    return null;
  }

  FrameModel _buildFrame(int type, int flags, int seq, List<int> payload) {
    return FrameModel(
      type: type,
      flags: flags,
      seq: seq,
      len: payload.length,
      payload: List.unmodifiable(payload),
    );
  }

  void _startTimeout() {
    _cancelTimeout();
    _timeoutTimer = Timer(const Duration(milliseconds: 2000), () {
      onError?.call('Frame reassembly timeout');
      _buffer.clear();
      _expectedSeq = null;
      _sofFlags = null;
      _sofType = null;
    });
  }

  void _cancelTimeout() {
    _timeoutTimer?.cancel();
    _timeoutTimer = null;
  }

  void reset() {
    _buffer.clear();
    _expectedSeq = null;
    _sofFlags = null;
    _sofType = null;
    _cancelTimeout();
  }
}
