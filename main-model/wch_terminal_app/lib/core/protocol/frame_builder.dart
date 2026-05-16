import 'frame_model.dart';

class FrameBuilder {
  static List<FrameModel> buildFrames({
    required int type,
    required List<int> payload,
    required int startSeq,
    int maxPayloadSize = 200,
    int dir = 0,
  }) {
    final frames = <FrameModel>[];
    int offset = 0;
    int seq = startSeq;

    while (offset < payload.length) {
      final remaining = payload.length - offset;
      final chunkSize = remaining > maxPayloadSize ? maxPayloadSize : remaining;
      final chunk = payload.sublist(offset, offset + chunkSize);

      final isFirst = offset == 0;
      final isLast = offset + chunkSize >= payload.length;

      int flags = dir & 0x04;
      if (isFirst) flags |= 0x01; // SOF
      if (isLast) flags |= 0x02;  // EOF

      frames.add(FrameModel(
        type: type,
        flags: flags,
        seq: seq & 0xFF,
        len: chunk.length,
        payload: chunk,
      ));

      offset += chunkSize;
      seq = (seq + 1) & 0xFF;
    }

    return frames;
  }
}
