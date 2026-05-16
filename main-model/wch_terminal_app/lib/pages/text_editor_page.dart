import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../providers/file_provider.dart';

class TextEditorPage extends ConsumerStatefulWidget {
  final String fileName;
  final String initialContent;

  const TextEditorPage({
    super.key,
    required this.fileName,
    required this.initialContent,
  });

  @override
  ConsumerState<TextEditorPage> createState() => _TextEditorPageState();
}

class _TextEditorPageState extends ConsumerState<TextEditorPage> {
  late final TextEditingController _controller;
  late final ScrollController _scrollController;
  bool _isModified = false;
  bool _isSaving = false;
  int _lineCount = 1;

  @override
  void initState() {
    super.initState();
    _controller = TextEditingController(text: widget.initialContent);
    _scrollController = ScrollController();
    _lineCount = '\n'.allMatches(widget.initialContent).length + 1;
    _controller.addListener(_onTextChanged);
  }

  void _onTextChanged() {
    final modified = _controller.text != widget.initialContent;
    final lines = '\n'.allMatches(_controller.text).length + 1;
    if (modified != _isModified || lines != _lineCount) {
      setState(() {
        _isModified = modified;
        _lineCount = lines;
      });
    }
  }

  Future<void> _save() async {
    setState(() => _isSaving = true);
    try {
      await ref.read(fileProvider.notifier).writeFile(
        widget.fileName,
        _controller.text,
      );
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: const Text('保存成功'),
            behavior: SnackBarBehavior.floating,
            shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
          ),
        );
        setState(() => _isModified = false);
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('保存失败: $e'),
            behavior: SnackBarBehavior.floating,
            backgroundColor: Theme.of(context).colorScheme.error,
          ),
        );
      }
    } finally {
      setState(() => _isSaving = false);
    }
  }

  Future<bool> _onWillPop() async {
    if (!_isModified) return true;
    final result = await showDialog<String>(
      context: context,
      builder: (_) => AlertDialog(
        icon: const Icon(Icons.save),
        title: const Text('保存修改？'),
        content: Text('文件 "${widget.fileName}" 已修改，是否保存？'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, 'discard'),
            child: const Text('不保存'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, 'save'),
            child: const Text('保存'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, 'cancel'),
            child: const Text('取消'),
          ),
        ],
      ),
    );
    if (result == 'cancel' || result == null) return false;
    if (result == 'save') await _save();
    return true;
  }

  String _fileExtension() {
    final dotIdx = widget.fileName.lastIndexOf('.');
    if (dotIdx > 0) return widget.fileName.substring(dotIdx + 1).toLowerCase();
    return '';
  }

  IconData _fileTypeIcon() {
    switch (_fileExtension()) {
      case 'c':
      case 'h':
        return Icons.code;
      case 'json':
        return Icons.data_object;
      case 'md':
        return Icons.article;
      default:
        return Icons.description;
    }
  }

  @override
  Widget build(BuildContext context) {
    final colorScheme = Theme.of(context).colorScheme;

    return PopScope(
      canPop: false,
      onPopInvokedWithResult: (didPop, result) async {
        if (didPop) return;
        final canPop = await _onWillPop();
        if (canPop && context.mounted) {
          Navigator.of(context).pop();
        }
      },
      child: Scaffold(
        appBar: AppBar(
          leading: IconButton(
            icon: const Icon(Icons.arrow_back),
            onPressed: () async {
              final canPop = await _onWillPop();
              if (canPop && context.mounted) {
                Navigator.of(context).pop();
              }
            },
          ),
          title: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(_fileTypeIcon(), size: 20),
              const SizedBox(width: 8),
              Flexible(
                child: Text(
                  widget.fileName,
                  overflow: TextOverflow.ellipsis,
                ),
              ),
            ],
          ),
          actions: [
            if (_isSaving)
              const Padding(
                padding: EdgeInsets.all(16),
                child: SizedBox(
                  width: 20,
                  height: 20,
                  child: CircularProgressIndicator(strokeWidth: 2),
                ),
              )
            else
              IconButton(
                icon: Icon(
                  Icons.save,
                  color: _isModified ? colorScheme.primary : null,
                ),
                tooltip: '保存',
                onPressed: _isModified ? _save : null,
              ),
          ],
        ),
        body: Column(
          children: [
            Expanded(
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  _buildLineNumbers(colorScheme),
                  Expanded(
                    child: TextField(
                      controller: _controller,
                      scrollController: _scrollController,
                      maxLines: null,
                      expands: true,
                      textAlignVertical: TextAlignVertical.top,
                      decoration: InputDecoration(
                        contentPadding: const EdgeInsets.symmetric(
                          horizontal: 12,
                          vertical: 12,
                        ),
                        border: InputBorder.none,
                        hintStyle: TextStyle(
                          color: colorScheme.onSurfaceVariant.withValues(alpha: 0.5),
                        ),
                      ),
                      style: TextStyle(
                        fontFamily: 'monospace',
                        fontSize: 14,
                        height: 1.5,
                        color: colorScheme.onSurface,
                      ),
                    ),
                  ),
                ],
              ),
            ),
            _buildStatusBar(colorScheme),
          ],
        ),
      ),
    );
  }

  Widget _buildLineNumbers(ColorScheme colorScheme) {
    return Container(
      width: 48,
      decoration: BoxDecoration(
        color: colorScheme.surfaceContainerLow,
        border: Border(
          right: BorderSide(
            color: colorScheme.outlineVariant,
            width: 1,
          ),
        ),
      ),
      child: ListView.builder(
        padding: const EdgeInsets.symmetric(vertical: 12),
        itemCount: _lineCount,
        itemBuilder: (context, index) {
          return SizedBox(
            height: 21,
            child: Text(
              '${index + 1}',
              textAlign: TextAlign.center,
              style: TextStyle(
                fontFamily: 'monospace',
                fontSize: 12,
                color: colorScheme.onSurfaceVariant.withValues(alpha: 0.6),
                height: 1.5,
              ),
            ),
          );
        },
      ),
    );
  }

  Widget _buildStatusBar(ColorScheme colorScheme) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 6),
      decoration: BoxDecoration(
        color: colorScheme.surfaceContainerHighest,
        border: Border(
          top: BorderSide(
            color: colorScheme.outlineVariant,
            width: 1,
          ),
        ),
      ),
      child: Row(
        children: [
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
            decoration: BoxDecoration(
              color: colorScheme.tertiaryContainer,
              borderRadius: BorderRadius.circular(4),
            ),
            child: Text(
              _fileExtension().toUpperCase(),
              style: TextStyle(
                fontSize: 11,
                fontWeight: FontWeight.w600,
                color: colorScheme.onTertiaryContainer,
              ),
            ),
          ),
          const SizedBox(width: 12),
          Text(
            '$_lineCount 行',
            style: TextStyle(
              fontSize: 12,
              color: colorScheme.onSurfaceVariant,
            ),
          ),
          const SizedBox(width: 12),
          Text(
            '${_controller.text.length} 字符',
            style: TextStyle(
              fontSize: 12,
              color: colorScheme.onSurfaceVariant,
            ),
          ),
          const Spacer(),
          if (_isModified)
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
              decoration: BoxDecoration(
                color: Colors.orange.withValues(alpha: 0.15),
                borderRadius: BorderRadius.circular(4),
              ),
              child: const Text(
                '已修改',
                style: TextStyle(
                  fontSize: 11,
                  fontWeight: FontWeight.w600,
                  color: Colors.orange,
                ),
              ),
            ),
        ],
      ),
    );
  }

  @override
  void dispose() {
    _controller.removeListener(_onTextChanged);
    _controller.dispose();
    _scrollController.dispose();
    super.dispose();
  }
}
