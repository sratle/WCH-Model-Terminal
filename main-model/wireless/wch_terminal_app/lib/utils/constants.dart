class AppConstants {
  // BLE
  static const String bleServiceUuid = 'ffe0';
  static const String bleRxCharUuid = 'fff1';
  static const String bleTxCharUuid = 'fff2';
  static const String deviceNameFilter = 'WCH';
  static const int targetMtu = 247;
  static const int minMtu = 23;

  // Protocol
  static const int heartbeatIntervalSec = 5;
  static const int heartbeatTimeoutSec = 15;
  static const int frameReassemblyTimeoutMs = 2000;
  static const int cliCommandTimeoutSec = 5;
  static const int musicPollIntervalSec = 2;

  // UI
  static const int terminalMaxLines = 1000;
  static const int maxEditableFileSize = 20 * 1024; // 20KB
}
