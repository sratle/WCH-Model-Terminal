import 'dart:async';
import 'frame_model.dart';

class FrameParser {
  final List<int> _buffer = [];
  int? _expectedSeq;
  Timer? _timeoutTimer;

  void Function(String)? onError;

  FrameModel? feed(FrameModel frame) {
    // Check SOF
    if (frame.isSof) {
      _buffer.clear();
      _buffer.addAll(frame.payload);
      _expectedSeq = (frame.seq + 1) & 0xFF;
      _startTimeout();
      if (frame.isEof) {
        _cancelTimeout();
        return _buildFrame(frame.type, frame.flags, frame.seq, _buffer);
      }
      return null;
    }

    // Middle or end frame
    if (_expectedSeq == null) {
      onError?.call('Unexpected frame without SOF');
      return null;
    }

    if (frame.seq != _expectedSeq) {
      onError?.call('SEQ mismatch: expected $_expectedSeq, got ${frame.seq}');
      _buffer.clear();
      _expectedSeq = null;
      _cancelTimeout();
      return null;
    }

    _buffer.addAll(frame.payload);
    _expectedSeq = (_expectedSeq! + 1) & 0xFF;
    _startTimeout();

    if (frame.isEof) {
      _cancelTimeout();
      // Reconstruct: use first frame's type/flags but with EOF set
      return _buildFrame(frame.type, frame.flags | 0x02, frame.seq, _buffer);
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
    });
  }

  void _cancelTimeout() {
    _timeoutTimer?.cancel();
    _timeoutTimer = null;
  }

  void reset() {
    _buffer.clear();
    _expectedSeq = null;
    _cancelTimeout();
  }
}
