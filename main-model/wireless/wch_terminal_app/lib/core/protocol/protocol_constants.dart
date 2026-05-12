class ProtocolConstants {
  // Frame structure
  static const int frameHeaderLen = 5; // TYPE + FLAGS + SEQ + LEN + RSV

  // MSG_TYPE
  static const int msgTypeCliData = 0x01;
  static const int msgTypeCtrl = 0x02;
  static const int msgTypeFileMeta = 0x03;
  static const int msgTypeFileChunk = 0x04;
  static const int msgTypeAck = 0x05;
  static const int msgTypeHeartbeat = 0xFF;
}

class FrameFlags {
  static const int sof = 0x01;
  static const int eof = 0x02;
  static const int dir = 0x04;
  static const int ackReq = 0x08;
  static const int err = 0x10;
}

class CtrlCmd {
  static const int ping = 0x01;
  static const int pong = 0x02;
  static const int mtuReq = 0x03;
  static const int mtuRsp = 0x04;
  static const int flush = 0x05;
  static const int statusReq = 0x06;
  static const int statusRsp = 0x07;
}
