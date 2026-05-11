class FrameModel {
  final int type;
  final int flags;
  final int seq;
  final int len;
  final List<int> payload;

  FrameModel({
    required this.type,
    required this.flags,
    required this.seq,
    required this.len,
    required this.payload,
  });

  bool get isSof => (flags & 0x01) != 0;
  bool get isEof => (flags & 0x02) != 0;
  bool get isDirUp => (flags & 0x04) != 0;
  bool get isAckReq => (flags & 0x08) != 0;
  bool get isErr => (flags & 0x10) != 0;

  List<int> toBytes() {
    return [
      type & 0xFF,
      flags & 0xFF,
      seq & 0xFF,
      len & 0xFF,
      0, // RSV
      ...payload,
    ];
  }

  static FrameModel? fromBytes(List<int> bytes) {
    if (bytes.length < 6) return null;
    final len = bytes[3];
    if (bytes.length < 5 + len) return null;
    return FrameModel(
      type: bytes[0],
      flags: bytes[1],
      seq: bytes[2],
      len: len,
      payload: bytes.sublist(5, 5 + len),
    );
  }

  @override
  String toString() {
    return 'Frame(type=0x${type.toRadixString(16)}, flags=0x${flags.toRadixString(16)}, seq=$seq, len=$len, payload=${payload.length}b)';
  }
}
