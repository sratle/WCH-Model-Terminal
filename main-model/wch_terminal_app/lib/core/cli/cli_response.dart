class CliResponse {
  final String command;
  final String output;
  final bool isSuccess;
  final String? error;

  CliResponse({
    required this.command,
    required this.output,
    this.isSuccess = true,
    this.error,
  });

  factory CliResponse.success(String command, String output) {
    return CliResponse(command: command, output: output, isSuccess: true);
  }

  factory CliResponse.error(String command, String error) {
    return CliResponse(
      command: command,
      output: '',
      isSuccess: false,
      error: error,
    );
  }
}
