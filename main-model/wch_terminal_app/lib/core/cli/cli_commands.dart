/// 基于实际 cli.c 实现的 CLI 命令封装
class CliCommands {
  /// Sanitize a path/name for embedding in a command line: ls parsing can
  /// leave a stray CR/LF inside a file name, and Core parses commands
  /// line-by-line — an embedded newline would truncate the command.
  static String _p(String path) => path.trim().replaceAll(RegExp(r'[\r\n]'), '');

  // ── 文件操作 ──
  static String ls() => 'ls';
  static String cd(String path) => 'cd "${_p(path)}"';
  static String pwd() => 'pwd';
  static String tree([String? path]) => path != null ? 'tree "${_p(path)}"' : 'tree';
  static String cat(String path) => 'cat "${_p(path)}"';
  static String mkdir(String dir) => 'mkdir "${_p(dir)}"';
  static String touch(String file) => 'touch "${_p(file)}"';
  static String rm(String path) => 'rm "${_p(path)}"';
  static String rmRf(String dir) => 'rm -rf "${_p(dir)}"';
  static String cp(String src, String dst) => 'cp "${_p(src)}" "${_p(dst)}"';
  static String mv(String oldName, String newName) => 'mv "${_p(oldName)}" "${_p(newName)}"';
  static String hexdump(String file) => 'hexdump "${_p(file)}"';
  static String head(String file, [int? n]) => n != null ? 'head "${_p(file)}" $n' : 'head "${_p(file)}"';
  static String tail(String file, [int? n]) => n != null ? 'tail "${_p(file)}" $n' : 'tail "${_p(file)}"';

  /// Read a file by byte range: read <file> <offset> [maxLen]
  /// Returns exactly the file bytes [offset, offset+maxLen) — used by the
  /// paged e-book readers to stream long books in chunks.
  static String read(String file, int offset, [int? maxLen]) =>
      maxLen != null ? 'read "${_p(file)}" $offset $maxLen' : 'read "${_p(file)}" $offset';
  static String du([String? dir]) => dir != null ? 'du "${_p(dir)}"' : 'du';
  static String find(String pattern) => 'find "${_p(pattern)}"';
  static String stat(String file) => 'stat "${_p(file)}"';
  static String chmod(String file, String attr) => 'chmod "${_p(file)}" $attr';

  // ── 音频控制 ──
  static String playst() => 'playst';
  static String play(String wavFile) => 'play "$wavFile"';
  static String pause() => 'pause';
  static String resume() => 'resume';
  static String vol(int level) => 'vol $level';
  static String speaker(bool on) => on ? 'speaker on' : 'speaker off';

  // ── 系统 ──
  static String df() => 'df';
  static String free() => 'free';
  static String device() => 'device';
  static String deviceSwitch(String mode) => 'device $mode';
  static String ver() => 'ver';
  static String help() => 'help';
  static String clear() => 'clear';

  // ── echo 重定向（文本编辑保存用）──
  static String echoOverwrite(String text, String file) => 'echo "$text" > "$file"';
  static String echoAppend(String text, String file) => 'echo "$text" >> "$file"';

  // ── Images（BMP 浏览器）──
  static String bmpLs() => 'bmp ls';
  static String bmpGet(String name) => 'bmp get "$name"';
}
