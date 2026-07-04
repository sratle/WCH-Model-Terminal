/// 基于实际 cli.c 实现的 CLI 命令封装
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
