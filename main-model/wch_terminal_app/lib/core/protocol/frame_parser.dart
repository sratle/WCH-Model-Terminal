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
  void Function(FrameModel)? onTimeoutFrame;

  FrameModel? feed(FrameModel frame) {
    final flagStr = 'S${frame.isSof ? 1 : 0}E${frame.isEof ? 1 : 0}D${frame.isDirUp ? 1 : 0}';
    print('[FP] recv: type=0x${frame.type.toRadixString(16)} flags=0x${frame.flags.toRadixString(16)}($flagStr) seq=${frame.seq} len=${frame.payload.length} expectSeq=$_expectedSeq bufLen=${_buffer.length}');

    if (frame.isSof) {
      if (_expectedSeq != null && frame.type != ProtocolConstants.msgTypeCliData) {
        print('[FP] ignore non-CLI SOF during reassembly');
        return null;
      }
      if (_expectedSeq != null && frame.type == ProtocolConstants.msgTypeCliData) {
        _deliverTimeoutFrame();
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
        print('[FP] single-frame complete: ${_buffer.length} bytes');
        return _buildFrame(_sofType!, _sofFlags!, frame.seq, _buffer);
      }
      return null;
    }

    if (_expectedSeq != null && frame.type != ProtocolConstants.msgTypeCliData) {
      print('[FP] ignore non-CLI frame during reassembly');
      return null;
    }

    if (_expectedSeq == null) {
      onError?.call('Unexpected frame without SOF: seq=${frame.seq} flags=0x${frame.flags.toRadixString(16)}');
      return null;
    }

    if (frame.seq != _expectedSeq) {
      onError?.call('SEQ mismatch: expected $_expectedSeq, got ${frame.seq}');
      _deliverTimeoutFrame();
      return null;
    }

    _buffer.addAll(frame.payload);
    _expectedSeq = (_expectedSeq! + 1) & 0xFF;
    _startTimeout();

    if (frame.isEof) {
      _cancelTimeout();
      final flags = _sofFlags! | FrameFlags.eof;
      final result = _buildFrame(_sofType!, flags, frame.seq, _buffer);
      print('[FP] multi-frame complete: ${_buffer.length} bytes');
      _expectedSeq = null;
      _sofFlags = null;
      _sofType = null;
      return result;
    }

    return null;
  }

  void _deliverTimeoutFrame() {
    if (_buffer.isNotEmpty && _sofType != null && _sofFlags != null) {
      final flags = _sofFlags! | FrameFlags.eof;
      final result = _buildFrame(_sofType!, flags, 0, _buffer);
      print('[FP] timeout deliver: ${_buffer.length} bytes (EOF was missing)');
      onTimeoutFrame?.call(result);
    }
    _buffer.clear();
    _expectedSeq = null;
    _sofFlags = null;
    _sofType = null;
    _cancelTimeout();
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
    _timeoutTimer = Timer(const Duration(milliseconds: 1500), () {
      _deliverTimeoutFrame();
      onError?.call('Frame reassembly timeout');
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
