# 内存界面原型

此目录保留 120×35、ANSI 真彩色、键盘操作的 C 内存原型，退出不保存、不读取正式数据。正式应用使用项目根目录的 `make run`，见[运行说明](../../docs/运行说明.md)。

原型由 `ui_preview.c`、`preview_model.c`、`preview_view.c` 和 `preview.h` 组成，复用 `src/ui_next/` 的终端、按键、编辑模块及基础工具。演示账号为 `admin/admin123`，只用于原型交互。

## 测试

在项目根目录执行：

```text
make test-preview
```

此目标只运行 C 测试，不打开交互界面。最近执行结果见[验证记录](../../docs/验证记录.md)。

## 手动编译和体验

以下命令在项目根目录的 **cmd** 中执行；编译前确保没有占用目标 exe 的旧原型：

```cmd
if not exist build\preview mkdir build\preview
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -g -Iinclude -Itools/preview tools/preview/ui_preview.c tools/preview/preview_model.c tools/preview/preview_view.c src/ui_next/terminal.c src/ui_next/edit.c src/ui_next/key.c src/utils/foundation_utf8.c src/utils/foundation_text.c src/utils/foundation_date.c -o build/preview/ui_preview.exe
chcp 65001 >nul && build\preview\ui_preview.exe
```

可在 exe 后加 `books` 或 `form` 打开指定页，追加 `--text` / `--print` 导出网格或 ANSI。交互前手动保证窗口至少 120×35；缩放后按 F5，Ctrl+Q 退出。

原型交互仍输出到 stderr；正式应用通过 CONOUT$ 输出，两者入口和持久化能力不同。不要在非交互输出面板中运行原型。视觉规则见[功能与交互](../../docs/功能与交互.md)，输入法和真实字体仍需人工验收。
