import 'dart:typed_data';

import 'frame_model.dart';
import 'frame_builder.dart';
import 'frame_parser.dart';
import 'protocol_constants.dart';

class ProtocolHandler {
  final FrameParser _parser = FrameParser();
  int _txSeq = 0;
  int maxPayloadSize = 12; // Default for MTU=23 (23-11=12)

  ProtocolHandler() {
    _parser.onError = (err) {
      // TODO: log error
    };
  }

  List<List<int>> encodeCliData(String text) {
    // Ensure command ends with \r\n for CLI compatibility
    var cmd = text;
    if (!cmd.endsWith('\r\n')) {
      if (cmd.endsWith('\n')) {
        cmd = cmd.substring(0, cmd.length - 1) + '\r\n';
      } else if (cmd.endsWith('\r')) {
        cmd = cmd + '\n';
      } else {
        cmd = cmd + '\r\n';
      }
    }
    final payload = Uint8List.fromList(cmd.codeUnits);
    final frames = FrameBuilder.buildFrames(
      type: ProtocolConstants.msgTypeCliData,
      payload: payload.toList(),
      startSeq: _txSeq,
      maxPayloadSize: maxPayloadSize,
      dir: 0,
    );
    _txSeq = (_txSeq + frames.length) & 0xFF;
    return frames.map((f) => f.toBytes()).toList();
  }

  FrameModel? decodeFrame(List<int> bytes) {
    final frame = FrameModel.fromBytes(bytes);
    if (frame == null) return null;
    return _parser.feed(frame);
  }

  List<int> encodeHeartbeat() {
    return FrameModel(
      type: ProtocolConstants.msgTypeHeartbeat,
      flags: 0x03, // SOF + EOF
      seq: 0,
      len: 0,
      payload: [],
    ).toBytes();
  }
}
