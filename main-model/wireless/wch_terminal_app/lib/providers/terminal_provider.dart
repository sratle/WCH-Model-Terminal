import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'cli_provider.dart';

final terminalOutputProvider = StreamProvider<String>((ref) {
  final cli = ref.watch(cliEngineProvider);
  return cli.outputStream;
});
