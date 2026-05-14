# 手机 APP 开发详细规划

> 基于 **Flutter 3.41.9** + **Dart 3.x** 开发的跨平台（Android/iOS）蓝牙终端 APP。  
> 通过 BLE 连接 CH585F，以 CLI 为核心交互方式，同时解析 Core 返回的结构化数据，实现音乐控制、文件浏览器等可视化功能。

---

## 1. 概述与技术栈

### 1.1 核心功能

| 功能 | 说明 | 优先级 |
|------|------|--------|
| 蓝牙设备管理 | 扫描、连接、断开、自动重连 | P0 |
| CLI 终端 | 命令输入、历史记录、实时输出 | P0 |
| 音乐播放器控制 | 状态栏显示、播放/暂停/切歌、进度条 | P1 |
| 文件浏览器 | 可视化浏览目录、进入/返回、文件操作 | P1 |
| 系统状态面板 | 电量、蓝牙连接状态、存储空间 | P2 |
| 文件传输 | 上传/下载文件（P3 扩展） | P3 |

### 1.2 技术栈选型

| 层级 | 选型 | 版本/说明 |
|------|------|----------|
| 框架 | Flutter | 3.41.9 |
| 语言 | Dart | 3.x（支持 Records、Patterns、Class Modifiers） |
| 状态管理 | flutter_riverpod | ^2.5.x（Flutter 官方推荐趋势，支持 AsyncNotifier） |
| BLE 插件 | flutter_blue_plus | ^1.32.x（最活跃的 BLE 库，支持 MTU 协商、多平台） |
| 本地存储 | shared_preferences + path_provider | 保存连接历史、设置 |
| JSON 序列化 | json_serializable + freezed | 数据模型代码生成 |
| 日志 | logger | 开发调试 |
| UI 组件 | Material 3 + 自定义 Widget | 遵循 Material You 设计规范 |

### 1.3 目标平台

- **Android 8.0+** (API 26+)
- **iOS 13+**

---

## 2. 项目目录结构

```
wch_terminal_app/
├── android/                          # Android 原生配置
│   └── app/src/main/AndroidManifest.xml  # 蓝牙权限声明
├── ios/                              # iOS 原生配置
│   └── Runner/Info.plist             # 蓝牙权限声明
├── lib/
│   ├── main.dart                     # 入口
│   ├── app.dart                      # MaterialApp + ProviderScope
│   ├── router.dart                   # GoRouter 路由定义
│   │
│   ├── core/                         # 核心业务逻辑（纯 Dart，无 UI）
│   │   ├── ble/                      # BLE 蓝牙管理
│   │   │   ├── ble_manager.dart      # BLE 扫描/连接/断开/MTU协商
│   │   │   ├── ble_device.dart       # 设备模型封装
│   │   │   └── ble_constants.dart    # UUID 常量、连接参数
│   │   │
│   │   ├── protocol/                 # APP-Wireless 协议层
│   │   │   ├── protocol_handler.dart # 协议帧打包/解析/重组主入口
│   │   │   ├── frame_builder.dart    # 构造发送帧（分包逻辑）
│   │   │   ├── frame_parser.dart     # 接收帧解析（重组逻辑）
│   │   │   ├── frame_model.dart      # 帧数据结构（Type/Flags/Seq/Len/Payload）
│   │   │   └── protocol_constants.dart # 协议常量定义
│   │   │
│   │   ├── cli/                      # CLI 引擎
│   │   │   ├── cli_engine.dart       # 命令发送、响应匹配、超时控制
│   │   │   ├── cli_command.dart      # 预定义命令枚举（ls/cd/player等）
│   │   │   ├── cli_response.dart     # 响应封装（成功/失败/文本/JSON）
│   │   │   ├── cli_parser.dart       # 文本输出解析器（正则/JSON）
│   │   │   └── cli_history.dart      # 命令历史管理
│   │   │
│   │   └── state/                    # 状态解析与管理
│   │       ├── state_manager.dart    # 状态总线：分发事件到各状态模块
│   │       ├── music_state.dart      # 音乐播放状态
│   │       ├── file_state.dart       # 文件浏览状态
│   │       └── system_state.dart     # 系统状态（电量、连接等）
│   │
│   ├── models/                       # 数据模型（freezed 生成）
│   │   ├── ble_device_info.dart
│   │   ├── file_item.dart            # 文件/目录项
│   │   ├── directory_node.dart       # 目录树节点
│   │   ├── music_status.dart         # 播放状态
│   │   ├── cli_output.dart           # CLI 输出块
│   │   └── app_settings.dart         # APP 设置
│   │
│   ├── providers/                    # Riverpod Providers
│   │   ├── ble_provider.dart         # BLE 连接状态
│   │   ├── cli_provider.dart         # CLI 引擎实例
│   │   ├── terminal_provider.dart    # 终端输出流
│   │   ├── music_provider.dart       # 音乐状态
│   │   ├── file_provider.dart        # 文件浏览状态
│   │   └── settings_provider.dart    # 设置持久化
│   │
│   ├── pages/                        # 页面（Screens）
│   │   ├── main_shell.dart             # 主壳：底部 Tab 切换 终端/资源管理器
│   │   ├── device_scan_page.dart       # 设备扫描列表（首次连接前显示）
│   │   ├── terminal_page.dart          # CLI 终端界面（Tab 1）
│   │   ├── file_browser_page.dart      # 资源管理器主界面（Tab 2）
│   │   ├── music_player_sheet.dart     # 音乐播放界面（底部全屏弹窗）
│   │   ├── text_editor_page.dart       # 文本文件编辑界面
│   │   └── settings_page.dart          # 设置页面
│   │
│   ├── widgets/                        # 可复用 UI 组件
│   │   ├── connection_status_bar.dart    # 顶部连接状态条
│   │   ├── terminal_view.dart            # 终端输出区域
│   │   ├── command_input.dart            # 命令输入框
│   │   ├── file_list_item.dart           # 文件列表项（支持单击/双击）
│   │   ├── breadcrumb_bar.dart           # 路径面包屑导航栏
│   │   ├── context_menu.dart             # 长按/右键上下文菜单
│   │   ├── music_player_bar.dart         # 音乐播放控制条（播放界面内）
│   │   ├── text_editor.dart              # 文本编辑区域
│   │   ├── loading_overlay.dart          # 加载遮罩
│   │   ├── new_item_dialog.dart          # 新建文件/文件夹弹窗
│   │   ├── confirm_dialog.dart           # 删除/覆盖确认弹窗
│   │   └── error_toast.dart              # 错误提示
│   │
│   └── utils/                        # 工具类
│       ├── constants.dart            # 全局常量
│       ├── extensions.dart           # Dart 扩展方法
│       ├── debouncer.dart            # 防抖器
│       └── ansi_parser.dart          # ANSI 颜色码解析（终端着色）
│
├── test/                             # 单元测试
│   ├── protocol_test.dart            # 协议帧编解码测试
│   ├── cli_parser_test.dart          # CLI 解析测试
│   └── frame_reassembly_test.dart    # 帧重组测试
│
├── pubspec.yaml
└── analysis_options.yaml
```

---

## 3. 核心模块详细设计

### 3.1 BLE 管理层（core/ble/）

#### BleManager — 蓝牙生命周期管理

```dart
class BleManager {
  // 扫描
  Stream<List<ScanResult>> startScan({Duration? timeout});
  void stopScan();

  // 连接
  Future<void> connect(BleDevice device);
  Future<void> disconnect();
  Stream<BluetoothConnectionState> get connectionStream;

  // 服务发现
  Future<List<BluetoothService>> discoverServices();

  // MTU 协商
  Future<int> requestMtu(int mtu);

  // 特征值操作
  Future<void> writeCharacteristic(List<int> data, {bool withoutResponse = false});
  Stream<List<int>> get notifyStream;

  // 连接状态
  bool get isConnected;
  BluetoothDevice? get connectedDevice;
}
```

**关键实现细节**：
- 扫描时过滤名称包含 `"WCH"` 或 Service UUID 为 `0xFFE0` 的设备
- 连接成功后自动发现 Service，获取 CLI_RX (0xFFF1) 和 CLI_TX (0xFFF2)
- 自动使能 CLI_TX 的 CCCD Notify
- 连接断开时自动重连（最多 3 次，间隔 3 秒）
- MTU 协商目标：优先 247，fallback 185，最低接受 23

#### 权限配置

**Android** (`AndroidManifest.xml`)：
```xml
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
<uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />
```

**iOS** (`Info.plist`)：
```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>需要蓝牙权限连接 WCH 终端设备</string>
```

---

### 3.2 协议适配层（core/protocol/）

严格遵循 `protocol_app.md` 的帧格式，实现 Dart 端编解码。

#### FrameModel — 帧数据结构

```dart
@freezed
class FrameModel with _$FrameModel {
  const factory FrameModel({
    required int type,        // MSG_TYPE_CLI_DATA=0x01, CTRL=0x02, ...
    required int flags,       // SOF/EOF/DIR/ACK_REQ/ERR
    required int seq,         // 0~255 循环
    required int len,         // payload 长度
    required List<int> payload,
  }) = _FrameModel;

  factory FrameModel.fromBytes(List<int> bytes) = ...;
  List<int> toBytes() => ...;
}

// Flags 位掩码
class FrameFlags {
  static const int sof = 0x01;      // Start of Frame
  static const int eof = 0x02;      // End of Frame
  static const int dir = 0x04;      // 0=下行, 1=上行
  static const int ackReq = 0x08;   // 请求 ACK
  static const int err = 0x10;      // 错误标志
}
```

#### FrameBuilder — 发送帧构造（含分包）

```dart
class FrameBuilder {
  /// 将完整消息切分为多个帧
  static List<FrameModel> buildFrames({
    required int type,
    required List<int> payload,
    required int startSeq,
    int? maxPayloadSize,  // 根据当前 MTU 计算
    int dir = 0,          // 0=下行(APP→Core)
  }) {
    // 按 maxPayloadSize 切分
    // 首包 SOF=1，尾包 EOF=1
    // SEQ 连续递增
  }
}
```

#### FrameParser — 接收帧解析（含重组）

```dart
class FrameParser {
  final _reassemblyBuffer = <int>[];
  int? _expectedSeq;
  Timer? _timeoutTimer;

  /// 喂入一帧原始数据，返回完整消息（若重组完成）或 null
  FrameModel? feed(FrameModel frame) {
    // 1. 检查 SEQ 连续性
    // 2. 首包(SOF=1)清空缓冲区
    // 3. 中间包追加 Payload
    // 4. 尾包(EOF=1)返回完整消息，停止超时定时器
    // 5. 超时或 SEQ 错误时丢弃并上报
  }
}
```

#### ProtocolHandler — 协议总控

```dart
class ProtocolHandler {
  final _frameBuilder = FrameBuilder();
  final _frameParser = FrameParser();
  int _txSeq = 0;   // 发送序号
  int _rxSeq = 0;   // 接收序号

  /// 将 CLI 字符串打包为 BLE 帧列表
  List<List<int>> encodeCliData(String text, {bool isCommand = true}) {
    final payload = utf8.encode(text);
    final frames = _frameBuilder.buildFrames(
      type: ProtocolConstants.msgTypeCliData,
      payload: payload,
      startSeq: _txSeq,
      dir: isCommand ? 0 : 1,  // 下行
    );
    _txSeq = (_txSeq + frames.length) & 0xFF;
    return frames.map((f) => f.toBytes()).toList();
  }

  /// 解析 BLE 原始字节流，产生完整消息事件
  Stream<AppMessage> decodeStream(Stream<List<int>> rawStream) async* {
    await for (final bytes in rawStream) {
      final frame = FrameModel.fromBytes(bytes);
      final message = _frameParser.feed(frame);
      if (message != null) {
        yield AppMessage.fromFrame(message);
      }
    }
  }
}
```

---

### 3.3 CLI 引擎（core/cli/）

CLI 引擎是 APP 与 Core 交互的核心枢纽，需要解决 **请求-响应匹配** 和 **主动事件处理** 两个问题。

#### 核心问题分析

```
场景 1：正常请求-响应
  APP ──"ls\n"─────> Core
  APP <──"file1\n"── Core    ← 匹配的响应

场景 2：长响应分多包到达
  APP ──"tree\n"────> Core
  APP <──"dir1\n"─── Core     ← tree 响应包 1
  APP <──"dir2\n"─── Core     ← tree 响应包 2（同属 tree 的响应）

场景 3：长响应
  APP ──"tree\n"───> Core
  APP <──"dir1\n"── Core     ← 响应包 1
  APP <──"dir2\n"── Core     ← 响应包 2（同属 tree 的响应）
```

#### CliEngine 设计

```dart
class CliEngine {
  final ProtocolHandler _protocol;
  final BleManager _ble;

  // 请求队列：维护未完成的命令请求
  final _pendingCommands = <_PendingCommand>[];

  // 输出总线：所有从 Core 来的文本（响应 + 事件）
  final _outputController = StreamController<CliOutput>.broadcast();
  Stream<CliOutput> get outputStream => _outputController.stream;

  // 发送命令并等待响应完成
  Future<CliResponse> execute(String command, {Duration? timeout}) async {
    final completer = Completer<CliResponse>();
    final pending = _PendingCommand(
      command: command,
      completer: completer,
      buffer: StringBuffer(),
      timeout: timeout ?? const Duration(seconds: 5),
    );
    _pendingCommands.add(pending);

    // 通过 BLE 发送
    final frames = _protocol.encodeCliData(command, isCommand: true);
    for (final frame in frames) {
      await _ble.writeCharacteristic(frame, withoutResponse: false);
    }

    return completer.future;
  }

  // 处理从 ProtocolHandler 来的上行消息
  void _onUpstreamMessage(AppMessage message) {
    if (message.type == MsgType.cliData) {
      final text = utf8.decode(message.payload);

      // 尝试匹配到最近的 pending 命令
      if (_pendingCommands.isNotEmpty) {
        final pending = _pendingCommands.first;
        pending.buffer.write(text);

        // 判断响应是否结束：收到 EOF 帧 或 特定结束标记
        if (message.isEof) {
          _pendingCommands.removeAt(0);
          pending.completer.complete(CliResponse.success(
            command: pending.command,
            output: pending.buffer.toString(),
          ));
        }
      } else {
        // 无 pending 命令，视为主动事件/通知
        _outputController.add(CliOutput.event(text));
      }
    }
  }
}
```

#### 预定义 CLI 命令（类型安全封装）

```dart
/// 基于实际 cli.c 实现的 CLI 命令封装
/// 所有命令均为纯文本输出，无 --json 参数支持
class CliCommands {
  // ── 文件操作 ──
  static String ls() => 'ls';
  static String cd(String path) => 'cd "$path"';
  static String pwd() => 'pwd';
  static String tree([String? path]) => path != null ? 'tree "$path"' : 'tree';
  static String cat(String path) => 'cat "$path"';
  static String mkdir(String dir) => 'mkdir "$dir"';
  static String touch(String file) => 'touch "$file"';
  static String rm(String path) => 'rm "$path"';
  static String rmRf(String dir) => 'rm -rf "$dir"';
  static String cp(String src, String dst) => 'cp "$src" "$dst"';
  static String mv(String oldName, String newName) => 'mv "$oldName" "$newName"';
  static String hexdump(String file) => 'hexdump "$file"';
  static String head(String file, [int? n]) => n != null ? 'head "$file" $n' : 'head "$file"';
  static String tail(String file, [int? n]) => n != null ? 'tail "$file" $n' : 'tail "$file"';
  static String du([String? dir]) => dir != null ? 'du "$dir"' : 'du';
  static String find(String pattern) => 'find "$pattern"';
  static String stat(String file) => 'stat "$file"';
  static String chmod(String file, String attr) => 'chmod "$file" $attr';

  // ── 音频控制 ──
  /// 查询播放状态，输出：
  /// ```
  /// Track: song.wav
  /// Time:  01:30
  /// ```
  static String playst() => 'playst';

  /// 播放 WAV 文件，输出：Playing: file.wav (12345 bytes audio data)
  static String play(String wavFile) => 'play "$wavFile"';

  /// 暂停，输出：Paused
  static String pause() => 'pause';

  /// 继续，输出：Resumed
  static String resume() => 'resume';

  /// 设置音量 0-100，输出：Volume: 80
  static String vol(int level) => 'vol $level';

  // ── 系统 ──
  static String df() => 'df';
  static String free() => 'free';
  static String device() => 'device';
  static String deviceSwitch(String mode) => 'device $mode'; // usb / sd
  static String ver() => 'ver';
  static String help() => 'help';
  static String clear() => 'clear';
}
```

---

### 3.4 状态解析层（core/state/）

#### 核心设计原则

由于 Core 返回的是纯文本输出，状态解析层负责：
1. **正则解析**：使用正则表达式从标准文本输出中提取结构化数据
2. **无事件驱动**：Core 不会主动推送状态变化，所有状态必须由 APP 主动轮询获取
3. **命令-响应对应**：每个状态查询都有明确的请求-响应关系

#### MusicState — 音乐播放状态

基于 `playst` 命令的实际输出解析：

```
Track: song.wav
Time:  01:30
```

```dart
@freezed
class MusicStatus with _$MusicStatus {
  const factory MusicStatus({
    required PlayerState state,      // stopped / playing / paused
    String? title,                   // 歌曲名（从 Track 行解析）
    String? filePath,                // 完整文件路径
    Duration? position,              // 当前播放位置（从 Time 行解析）
    int? volume,                     // 音量 0~100（需单独执行 vol 后解析）
  }) = _MusicStatus;

  /// 从 `playst` 输出文本解析
  /// 输入："Track: song.wav\r\nTime:  01:30\r\n"
  static MusicStatus? parsePlayst(String text) {
    String? track;
    Duration? pos;

    final trackMatch = RegExp(r'Track:\s*(.+)').firstMatch(text);
    if (trackMatch != null) {
      track = trackMatch.group(1)?.trim();
      if (track == '(none)') track = null;
    }

    final timeMatch = RegExp(r'Time:\s*(\d{2}):(\d{2})').firstMatch(text);
    if (timeMatch != null) {
      final min = int.parse(timeMatch.group(1)!);
      final sec = int.parse(timeMatch.group(2)!);
      pos = Duration(minutes: min, seconds: sec);
    }

    // 判断状态：有 track 且有 pos 说明正在播放；
    // 如果能获取到播放进度在更新则确认为 playing，否则需结合上下文
    final state = track != null && track.isNotEmpty
        ? PlayerState.playing
        : PlayerState.stopped;

    return MusicStatus(state: state, title: track, position: pos);
  }
}

enum PlayerState { stopped, playing, paused }
```

**状态更新流程**：

```dart
class MusicStateManager {
  final CliEngine _cli;
  Timer? _pollTimer;

  final _stateController = StreamController<MusicStatus>.broadcast();
  Stream<MusicStatus> get stateStream => _stateController.stream;

  /// 启动轮询：每 2 秒执行一次 `playst` 查询播放状态
  void startPolling() {
    _pollTimer = Timer.periodic(const Duration(seconds: 2), (_) async {
      final resp = await _cli.execute(CliCommands.playst());
      if (resp.isSuccess) {
        final status = MusicStatus.parsePlayst(resp.output);
        if (status != null) _stateController.add(status);
      }
    });
  }

  // 控制命令：直接发送，执行成功后触发一次状态刷新
  Future<void> play(String wavFile) async {
    await _cli.execute(CliCommands.play(wavFile));
    await _refreshStatus();
  }

  Future<void> pause() async {
    await _cli.execute(CliCommands.pause());
    _stateController.add(const MusicStatus(state: PlayerState.paused));
  }

  Future<void> resume() async {
    await _cli.execute(CliCommands.resume());
    _stateController.add(const MusicStatus(state: PlayerState.playing));
  }

  Future<void> setVolume(int level) async {
    await _cli.execute(CliCommands.vol(level));
  }

  Future<void> _refreshStatus() async {
    final resp = await _cli.execute(CliCommands.playst());
    if (resp.isSuccess) {
      final status = MusicStatus.parsePlayst(resp.output);
      if (status != null) _stateController.add(status);
    }
  }
}
```

#### FileState — 文件浏览状态

基于 `ls` 命令的实际输出解析：

```
--- \DOC\MUSIC ---
  [DIR]  SUBFOLDER
  [FILE] song1.wav  (12345 bytes)
  [FILE] readme.md  (256 bytes)
```

```dart
@freezed
class FileItem with _$FileItem {
  const factory FileItem({
    required String name,
    required bool isDirectory,
    int? size,                       // 字节
  }) = _FileItem;

  /// 从 `ls` 输出文本解析单条
  /// 匹配："  [DIR]  name" 或 "  [FILE] name  (123 bytes)"
  static FileItem? parseLsLine(String line) {
    final dirMatch = RegExp(r'^\s+\[DIR\]\s+(.+)$').firstMatch(line);
    if (dirMatch != null) {
      return FileItem(name: dirMatch.group(1)!.trim(), isDirectory: true);
    }

    final fileMatch = RegExp(r'^\s+\[FILE\]\s+(.+?)\s+\((\d+) bytes\)$')
        .firstMatch(line);
    if (fileMatch != null) {
      return FileItem(
        name: fileMatch.group(1)!.trim(),
        isDirectory: false,
        size: int.parse(fileMatch.group(2)!),
      );
    }
    return null;
  }

  /// 解析完整的 `ls` 输出
  static List<FileItem> parseLsOutput(String text) {
    final items = <FileItem>[];
    for (final line in text.split('\n')) {
      final item = parseLsLine(line);
      if (item != null) items.add(item);
    }
    return items;
  }
}

@freezed
class DirectoryNode with _$DirectoryNode {
  const factory DirectoryNode({
    required String path,
    required List<FileItem> items,
    String? parentPath,
  }) = _DirectoryNode;
}
```

**文件浏览器流程**：

```dart
class FileStateManager {
  final CliEngine _cli;

  final _currentDirController = StreamController<DirectoryNode>.broadcast();
  Stream<DirectoryNode> get currentDirStream => _currentDirController.stream;

  String _currentPath = '\\';

  /// 加载当前目录内容
  /// 先执行 `pwd` 确认路径，再执行 `ls` 获取列表
  Future<void> loadDirectory() async {
    // 获取当前路径
    final pwdResp = await _cli.execute(CliCommands.pwd());
    if (pwdResp.isSuccess) {
      _currentPath = pwdResp.output.trim();
    }

    // 获取目录列表
    final lsResp = await _cli.execute(CliCommands.ls());
    if (lsResp.isSuccess) {
      final items = FileItem.parseLsOutput(lsResp.output);
      _currentDirController.add(DirectoryNode(
        path: _currentPath,
        items: items,
        parentPath: _getParentPath(_currentPath),
      ));
    }
  }

  /// 进入子目录：发送 `cd` 然后刷新
  Future<void> enterDirectory(String dirName) async {
    await _cli.execute(CliCommands.cd(dirName));
    await loadDirectory();
  }

  /// 返回上级：发送 `cd ..` 然后刷新
  Future<void> goUp() async {
    await _cli.execute(CliCommands.cd('..'));
    await loadDirectory();
  }

  /// 删除文件/目录
  Future<void> delete(FileItem item) async {
    if (item.isDirectory) {
      await _cli.execute(CliCommands.rmRf(item.name));
    } else {
      await _cli.execute(CliCommands.rm(item.name));
    }
    await loadDirectory();
  }

  String? _getParentPath(String path) {
    // FAT 路径分隔符为 \
    final idx = path.lastIndexOf('\\');
    if (idx <= 0) return null;
    return path.substring(0, idx);
  }
}
```

---

## 4. UI 页面设计

APP 只有两个主界面，通过底部 Tab 切换：

| Tab | 名称 | 说明 |
|-----|------|------|
| Tab 1 | **终端** | CLI 命令行界面，用户自由输入命令 |
| Tab 2 | **资源管理器** | 图形化文件管理，所有操作封装为 GUI 交互 |

---

### 4.1 主壳（MainShell）

```
┌─────────────────────────────────────┐
│ 🔵 WCH Terminal          [⚙️设置]   │  ← AppBar + 连接状态指示
├─────────────────────────────────────┤
│                                     │
│        [当前 Tab 的内容区域]         │
│                                     │
│                                     │
│                                     │
│                                     │
├─────────────────────────────────────┤
│     [🏠 终端]    [📁 资源管理器]    │  ← 底部 Tab 导航
└─────────────────────────────────────┘
```

**连接状态规则**：
- 🔵 蓝色：已连接
- ⚪ 灰色：未连接
- 🔄 旋转：连接中/扫描中
- 未连接时点击任意 Tab 自动跳转到设备扫描页

---

### 4.2 设备扫描页（DeviceScanPage）

首次打开 APP 或未连接设备时显示的全屏页面。

- 下拉刷新扫描
- 过滤显示名称含 `"WCH"` 或 Service UUID `0xFFE0` 的设备
- 已配对/已连接过的设备置顶显示
- 点击设备卡片连接，显示进度指示器
- 连接失败显示 SnackBar 重试
- 连接成功后自动跳转到资源管理器 Tab

---

### 4.3 CLI 终端页（TerminalPage）— Tab 1

```
┌─────────────────────────────────────┐
│ 🔵 WCH Terminal          [⚙️设置]   │
├─────────────────────────────────────┤
│ $ ls                                │
│ --- \DOC ---                        │
│   [DIR]  MUSIC                      │
│   [FILE] readme.md  (256 bytes)     │
│ $ playst                            │
│ Track: song.wav                     │
│ Time:  01:30                        │
│                                     │
│                                     │
├─────────────────────────────────────┤
│ > cd MUSIC               [发送]     │  ← 命令输入区
└─────────────────────────────────────┘
```

**功能细节**：
- 输出区域支持 ANSI 颜色码解析（`clear` 命令输出的 `\x1B[2J\x1B[H` 等）
- 命令历史：向上箭头按钮翻阅历史命令
- 长按输出文本可复制到剪贴板
- 自动滚动到最新输出

---

### 4.4 资源管理器页（FileBrowserPage）— Tab 2

资源管理器是 APP 的核心工作界面，**所有文件操作均封装为 GUI 交互**，无需用户手动输入 CLI 命令。

#### 4.4.1 主界面布局

```
┌─────────────────────────────────────┐
│ 🔵 WCH Terminal          [⚙️设置]   │
├─────────────────────────────────────┤
│ [←]  \DOC\MUSIC  [新建▼] [刷新]    │  ← 面包屑导航 + 操作按钮
├─────────────────────────────────────┤
│ 📁 ..                               │  ← 上级目录（始终置顶）
│ 📁 SUBFOLDER                        │
│ 📄 readme.md              256 B     │
│ 🎵 song.wav               12 KB     │
│ 📝 notes.txt              1 KB      │
│ 🖼️  image.bmp              45 KB    │
│                                     │
│                                     │
└─────────────────────────────────────┘
```

**自动加载机制**：
- 进入资源管理器 Tab 时，自动执行 `pwd` + `ls`
- 每次进入文件夹后自动执行 `ls` 刷新列表
- 下拉列表触发 `ls` 刷新

**面包屑导航**：
- 显示当前完整路径（如 `\DOC\MUSIC`）
- 点击路径中的任意段可直接跳转到该目录
- 左上角 `←` 按钮执行 `cd ..` 并刷新

**文件图标规则**：

| 扩展名 | 图标 | 类型 |
|--------|------|------|
| 无 / 文件夹 | 📁 | 目录 |
| `.wav` | 🎵 | 音频文件 |
| `.txt` / `.md` / `.json` / `.c` / `.h` | 📝 | 文本文件 |
| `.bmp` / `.png` / `.jpg` | 🖼️ | 图片文件 |
| 其他 | 📄 | 未知文件 |

#### 4.4.2 操作交互

| 操作 | 触发方式 | 背后执行的 CLI |
|------|---------|--------------|
| 进入文件夹 | 双击文件夹行 | `cd <dir>` → `ls` |
| 返回上级 | 点击 `📁 ..` 或 `←` | `cd ..` → `ls` |
| 新建文件夹 | 点击 `[新建▼]` → 新建文件夹 | `mkdir <name>` → `ls` |
| 新建文件 | 点击 `[新建▼]` → 新建文件 | `touch <name>` → `ls` |
| 删除 | 长按文件 → 删除 | `rm <file>` / `rm -rf <dir>` → `ls` |
| 复制 | 长按文件 → 复制 | `cp <src> <dst>` → `ls` |
| 属性查看 | 长按文件 → 属性 | `stat <file>` |
| 刷新 | 点击 `[刷新]` | `ls` |

**长按上下文菜单**：
```
┌─────────────────┐
│ 📄 readme.md    │
├─────────────────┤
│ 复制            │
│ 删除            │
│ 重命名          │  → mv（仅短文件名）
│ 属性            │  → stat
│ 取消            │
└─────────────────┘
```

#### 4.4.3 新建文件/文件夹弹窗

```
┌─────────────────────────┐
│ 新建文件夹               │
├─────────────────────────┤
│ 名称: [______________]  │
│                         │
│     [取消]  [确定]      │
└─────────────────────────┘
```

- 输入名称后点击确定
- 执行 `mkdir <name>` 或 `touch <name>`
- 成功后自动刷新列表

---

### 4.5 音乐播放界面（MusicPlayerSheet）

**触发方式**：在资源管理器中双击 `.wav` 文件

**呈现方式**：从底部滑出的全屏弹窗（BottomSheet）

```
┌─────────────────────────────────────┐
│        ───── 向下拖动关闭 ─────      │
├─────────────────────────────────────┤
│                                     │
│         [🎵 专辑封面占位图]          │
│                                     │
│      song.wav                       │
│      \DOC\MUSIC                     │
│                                     │
│  01:30  [━━━━━●──────────────]  --:--│
│                                     │
│    ⏸️/▶️          🔊 ──────●───      │
│                                     │
├─────────────────────────────────────┤
│  音量: [━━━━━━━━━━━━●━━━━]  80     │
└─────────────────────────────────────┘
```

**交互逻辑**：

| 用户操作 | 背后执行的 CLI | 界面响应 |
|----------|--------------|---------|
| 双击 `.wav` 文件 | `play "file.wav"` | 打开播放界面，显示文件名 |
| 点击 ⏸️ | `pause` | 按钮变为 ▶️，状态显示"已暂停" |
| 点击 ▶️ | `resume` | 按钮变为 ⏸️，状态显示"播放中" |
| 拖动音量滑块 | `vol <0-100>` | 实时显示音量百分比 |
| 拖动进度条 | **暂不支持**（CLI 无 seek 命令） | 提示"暂不支持进度跳转" |
| 关闭弹窗 | 无 | 返回资源管理器，音乐继续后台播放 |

**状态轮询**：
- 播放界面打开后，每 2 秒执行一次 `playst`
- 解析 `Track:` 和 `Time:` 更新界面
- 若 `Track: (none)` 则表示已停止，自动关闭播放界面

---

### 4.6 文本文件编辑界面（TextEditorPage）

**触发方式**：在资源管理器中双击文本文件（`.txt` / `.md` / `.json` / `.c` / `.h` 等）

**大小限制**：先执行 `stat <file>` 获取文件大小，**大于 20KB 时提示"文件过大，请在终端中使用编辑器"**，拒绝打开。

**呈现方式**：导航到新页面（非弹窗，保留编辑状态）

```
┌─────────────────────────────────────┐
│ [←] 编辑: readme.md     [💾 保存]  │
├─────────────────────────────────────┤
│                                     │
│ 这是文件的内容...                    │
│ 用户可以在这里自由编辑...            │
│                                     │
│                                     │
│                                     │
│                                     │
│                                     │
├─────────────────────────────────────┤
│ 大小: 256 B  编码: UTF-8            │
└─────────────────────────────────────┘
```

**编辑流程**：

1. **打开文件**：
   - 执行 `cat <file>` 获取完整内容
   - 内容加载到 `TextEditingController`
   - 底部显示文件大小和编码

2. **用户编辑**：
   - 使用 Flutter `TextField`（maxLines: null, expands: true）
   - 支持多行文本输入
   - 软键盘正常弹出

3. **保存文件**：
   - 点击 `[💾 保存]`
   - 将编辑后的文本通过 `echo "content" > file` 写回
   - 由于 `echo` 支持重定向，APP 需要处理文本中的特殊字符（如 `"` 需转义）
   - 若文件较大（> 256 字节），可能需要分多条 `echo` 命令追加（`>>`）
   - 保存成功后返回资源管理器并刷新

**保存实现细节**：

```dart
Future<void> saveFile(String filename, String content) async {
  final cli = ref.read(cliEngineProvider);

  // 策略：若内容较短（< 200 字符），单条 echo > file
  // 若内容较长，分多条 echo >> file
  final lines = content.split('\n');
  for (int i = 0; i < lines.length; i++) {
    final line = lines[i];
    // 处理转义：将内容中的 " 替换为 \"
    final escaped = line.replaceAll('"', '\\"');
    final cmd = i == 0
        ? 'echo "$escaped" > "$filename"'
        : 'echo "$escaped" >> "$filename"';
    await cli.execute(cmd);
  }
}
```

> **注意**：`echo` 重定向在 cli.c 中已实现，`>` 覆盖写入，`>>` 追加写入。

**取消编辑**：
- 点击返回键时若内容已修改，弹出确认框："内容已修改，是否保存？"
- 选项：保存 / 不保存 / 取消

---

### 4.7 各界面路由关系

```
设备扫描页（首次打开/未连接）
    │
    ▼ 连接成功
主壳（MainShell）
    ├── Tab 1: 终端页（TerminalPage）
    │
    └── Tab 2: 资源管理器页（FileBrowserPage）
            │
            ├── 双击 .wav 文件 ──→ 音乐播放界面（BottomSheet）
            │
            ├── 双击文本文件 ───→ 文本编辑页（TextEditorPage）
            │
            └── 长按文件 ───────→ 上下文菜单（ContextMenu）
```

---

## 5. Riverpod Provider 设计

```dart
// ble_provider.dart
final bleManagerProvider = Provider<BleManager>((ref) => BleManager());

final connectionStateProvider = StreamProvider<BluetoothConnectionState>((ref) {
  final ble = ref.watch(bleManagerProvider);
  return ble.connectionStream;
});

final isConnectedProvider = Provider<bool>((ref) {
  final state = ref.watch(connectionStateProvider);
  return state.value == BluetoothConnectionState.connected;
});

// cli_provider.dart
final cliEngineProvider = Provider<CliEngine>((ref) {
  final ble = ref.watch(bleManagerProvider);
  final protocol = ProtocolHandler();
  return CliEngine(protocol: protocol, ble: ble);
});

// terminal_provider.dart
final terminalOutputProvider = StreamProvider<CliOutput>((ref) {
  final cli = ref.watch(cliEngineProvider);
  return cli.outputStream;
});

// music_provider.dart
@riverpod
class MusicState extends _$MusicState {
  @override
  MusicStatus build() => const MusicStatus(state: PlayerState.stopped);

  Future<void> play(String? path) async {
    final cli = ref.read(cliEngineProvider);
    await cli.execute(CliCommands.playerPlay(path));
    await _refreshStatus();
  }

  Future<void> pause() async {
    final cli = ref.read(cliEngineProvider);
    await cli.execute(CliCommands.playerPause());
    await _refreshStatus();
  }

  Future<void> _refreshStatus() async {
    final cli = ref.read(cliEngineProvider);
    final resp = await cli.execute(CliCommands.playst());
    if (resp.isSuccess) {
      final status = MusicStatus.parsePlayst(resp.output);
      if (status != null) state = status;
    }
  }
}

// file_provider.dart
@riverpod
class FileBrowser extends _$FileBrowser {
  @override
  Future<DirectoryNode> build() async => _loadRoot();

  Future<void> navigateTo(String path) async {
    state = const AsyncLoading();
    state = await AsyncValue.guard(() => _loadDirectory(path));
  }

  Future<DirectoryNode> _loadDirectory(String path) async {
    final cli = ref.read(cliEngineProvider);
    final resp = await cli.execute(CliCommands.ls());
    // ... 使用 FileItem.parseLsOutput(resp.output) 解析
  }
}
```

---

## 6. Core 侧配合需求

为了让 APP 的解析逻辑简单可靠，建议 Core 固件扩展以下 **机器可读输出** 命令。如果 Core 暂不支持，APP 会先实现文本解析 Fallback。

### 6.1 Core CLI 实际输出格式速查

APP 的所有解析逻辑必须基于以下**实际文本格式**实现：

| 命令 | 实际输出示例 | 解析要点 |
|------|-------------|---------|
| `ls` | `--- \DOC ---\n  [DIR]  MUSIC\n  [FILE] readme.md  (256 bytes)` | 首行为路径，后续 `[DIR]`/`[FILE]` 行 |
| `pwd` | `\DOC\MUSIC` | 纯路径字符串 |
| `playst` | `Track: song.wav\nTime:  01:30` | `Track:` 行 + `Time:` 行 |
| `play file.wav` | `Playing: file.wav (12345 bytes audio data)` | 确认播放启动 |
| `pause` | `Paused` | 确认暂停 |
| `resume` | `Resumed` | 确认继续 |
| `vol 80` | `Volume: 80` | 确认音量设置 |
| `tree` | `\DOC\MUSIC\n  [DIR]  SUB\n    [FILE] x.wav (100 bytes)` | 缩进树形，按层级解析 |
| `stat file` | `File: x\nSize: 123\nDate: 2024-01-01\nTime: 12:00:00\nAttr: 0x20 A` | 多行键值对 |
| `free` | `Free space: 123456 sectors x 512 bytes\n = 123456 bytes (120 MB)` | 取第二行的 bytes 数 |
| `df` | （CH378 库直接输出）| 无固定格式，直接显示 |
| `device` | `Current device: USB` / `Current device: SD` | 设备模式 |

### 6.2 无主动事件推送

**重要**：当前 CLI 实现中，Core **不会主动推送任何事件**。所有状态变化必须由 APP 主动轮询获取：
- 音乐状态：APP 轮询 `playst`（每 2 秒）
- 文件列表：进入目录时主动 `ls`
- 系统状态：需要时主动查询

CLI 引擎的 `outputStream` 中接收到的所有文本均为**命令响应**，不存在无对应请求的 `[EVENT]` 主动消息。CLI 引擎无需区分事件与响应，所有输出均可直接显示在终端中。

### 6.3 如需结构化数据（未来扩展）

如果未来 Core 侧需要支持 APP 的更高效解析，建议新增以下命令（**不影响现有 CLI 兼容性**）：

| 建议新增命令 | 说明 |
|-------------|------|
| `lsplain` | 以固定分隔符输出，如 `name|type|size\n` |
| `playstplain` | 以固定分隔符输出，如 `state|track|mm|ss\n` |

但当前阶段 APP 应完全基于现有文本输出实现解析。

---

## 7. 实现路线图

### Phase 1：基础框架（1~2 周）

| 任务 | 内容 |
|------|------|
| 工程搭建 | Flutter 项目创建，配置 Android/iOS 权限，集成 flutter_blue_plus、riverpod |
| BLE 层 | 实现 BleManager：扫描、连接、MTU 协商、Notify 订阅 |
| 协议层 | 实现 FrameModel、FrameBuilder、FrameParser，编写单元测试 |
| CLI 层 | 实现 CliEngine 基础：发送命令、接收响应、简单请求-响应匹配 |
| 终端页 | CLI 终端 UI：输入框、输出滚动区域、历史记录 |
| 设备扫描页 | 扫描列表、连接、状态显示 |

**验收**：APP 能连接 CH585F，发送 `help` 命令并看到响应输出。

### Phase 2：资源管理器核心（1~2 周）

| 任务 | 内容 |
|------|------|
| 心跳与重连 | 实现 5 秒心跳、断线自动重连、连接状态持久化 |
| 分包重组 | 完整实现多帧 CLI 命令/响应的分包与重组，支持长输出 |
| 资源管理器主界面 | `ls`/`cd`/`pwd` 自动封装、文件列表渲染、面包屑导航、双击进入文件夹 |
| 文件操作 | 新建文件/文件夹（`touch`/`mkdir`）、删除（`rm`/`rm -rf`）、复制（`cp`）、属性（`stat`） |
| 音乐播放界面 | 双击 `.wav` 打开 BottomSheet、播放/暂停/继续（`play`/`pause`/`resume`）、音量调节（`vol`）、`playst` 轮询 |
| 文本编辑界面 | 双击文本文件打开编辑器、`cat` 读取内容、`echo` 分条保存、20KB 大小限制检测 |
| 错误处理 | 超时提示、连接错误、命令失败的用户友好提示、操作成功/失败 Toast |

**验收**：
- 资源管理器能浏览目录、新建/删除/复制文件
- 能双击 `.wav` 播放音乐并控制暂停/继续/音量
- 能双击文本文件编辑并保存

### Phase 3：体验优化（1 周）

| 任务 | 内容 |
|------|------|
| ANSI 颜色 | 终端输出支持颜色码解析（`clear` 命令等） |
| 主题切换 | Material 3 动态取色 / 深色模式 |
| 快捷操作 | 终端页命令自动补全、常用命令收藏、资源管理器快速返回根目录 |
| 性能优化 | 大目录列表虚拟滚动、图片懒加载、BLE 传输节流 |
| 文件传输 UI | 上传/下载进度条、队列管理（需 Core P3 支持） |

---

## 8. 测试策略

| 测试类型 | 内容 |
|----------|------|
| 单元测试 | Protocol 编解码、Frame 重组、CLI 解析正则 |
| Widget 测试 | 终端渲染、文件列表交互、音乐控制按钮 |
| 集成测试 | BLE 连接流程（需真实设备或模拟器） |
| 手动测试 | 长命令输出、弱网环境分包、多页面切换状态保持 |

---

## 9. 附录：关键常量

```dart
// lib/utils/constants.dart
class AppConstants {
  // BLE
  static const String bleServiceUuid = 'ffe0';
  static const String bleRxCharUuid = 'fff1';   // Write
  static const String bleTxCharUuid = 'fff2';   // Notify
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
  static const int filePageSize = 50;
}
```

---

## 版本记录

| 版本 | 日期 | 说明 |
|------|------|------|
| V1.0 | 2026-05-06 | 初始版本：Flutter 架构设计、模块接口、UI 规划、实现路线 |
