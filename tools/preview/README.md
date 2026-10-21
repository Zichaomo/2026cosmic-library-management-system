# 暖调界面 C 原型

120×35、ANSI 真彩色、纯键盘交互。仅操作内存样例，不访问 `data/`，退出不保留修改。不是已重构完成的正式系统。

## 启动

在项目根目录的 PowerShell 中运行：

```powershell
.\tools\preview\run-preview.ps1
```

脚本调用现有 GCC 编译器，先编译，再将当前会话代码页设为 65001 后启动。脚本不会设置持久终端主题或强制设置窗口/缓冲区尺寸；启动前读取可见窗口尺寸，不足 120 列、35 行时先提示调整，按 Enter 重新检查，满足条件后才进入原型。字符区保持 120×35；按用户最新要求使用 120×35，优先减少纵向长度，不再要求严格 3:2。若底部被截断，先放大窗口或按 Ctrl+- 缩小字号，直到完整显示底部状态栏，不减少字符行数。

演示账号：`admin` / `admin123`。输入账号后 Tab 切换密码，输入密码后 Enter 移到登录按钮，再 Enter 确认。登录校验只用于原型演示，不代表正式认证层已迁移。

也可以直接打开指定页面：

```powershell
.\tools\preview\run-preview.ps1 -Page books
.\tools\preview\run-preview.ps1 -Page form
```

`Ctrl+Q` 随时退出；登录页 Esc 退出，列表 Esc 返回登录；编辑过的新增表单 Esc 会询问是否放弃，Y 放弃、N 继续。

## 操作

- Tab / Shift+Tab：表单焦点切换；Enter：字段上移到下一项，按钮上执行。
- 左右、Home、End、Backspace、Delete：按字符边界编辑普通中文和 ASCII，长输入会水平滚动。
- 列表上下选择、PgUp/PgDn 翻页、Home/End 到首尾、Enter 查看详情。
- `K` 进入实时检索，支持书名、作者、ISBN；Enter 或 Esc 离开检索模式，保留检索条件；删除检索词可恢复全部样例。
- C 新增样例、Z 增加副本、T 停借/恢复；输入模式中的字母只作为文本。
- F1 帮助；F5 在所有页面完整重绘，窗口大小或字号变化后可按一次，当前输入不会清空；列表页同时刷新内存样例，没有跨进程同步。

## 编译与测试

```powershell
.\tools\preview\run-preview.ps1 -Test
```

当前 67 项通过、0 失败；以再次运行结果为准。脚本以 `-std=c11 -Wall -Wextra -Wpedantic -Werror` 编译，不链接 BCrypt。也可仅编译：

```powershell
.\tools\preview\run-preview.ps1 -BuildOnly
```

原型由 `ui_preview.c`（输入与入口）、`preview_model.c`（状态、UTF-8 编辑与样例）、`preview_view.c`（字符网格、真彩色、支持变化行输出；交互按键后完整补画）组成，接口在 `preview.h`。与正式程序构建相互独立。

导出检查用字符网格或 ANSI 输出：

```powershell
$preview = Get-Content .\build\preview\latest.txt
& $preview --text books
& $preview --print books
```

`capture_preview.py` 可将 C 可执行文件的 ANSI 输出复现为 PNG，写入 `build/preview-captures/`。这是离线验证辅助工具，需 Python 与 Pillow；不是 C 运行依赖，生成图也不是终端窗口截图。

## 输出与编码约定

交互画面写到 `stderr`，以利用目标 Windows CRT 的无缓冲错误流，使画面在阻塞 `getch()` 前显示；不调用 `fflush()` 或 `setvbuf()`。这只是输出通道的选择，不代表界面内容是错误。交互时不要将标准错误重定向到文件，或通过某些 IDE 的非交互输出面板运行。

启动前必须设置代码页 65001。实测默认 936 下 `getch()` 读到 GBK 字节，65001 下收到 UTF-8 字节。原型不使用 Win32 或宽字符 API 自动探测/切换编码。

本轮验证了 Windows ConPTY 中提交后的中文文本、扩展键、编辑、分页与退出；没有验证所有真实输入法候选窗。当前仅保证普通中文和 ASCII 的基本编辑，不支持 Emoji、组合字符及完整 Unicode 字素簇编辑。还需在实际 Windows Terminal / VS Code 终端检查字体、边框、真彩色与中文输入法观感。

验证记录见 [原型验证记录](../../docs/design/原型验证记录.md)。

每次编译使用独立的可执行文件名，避免覆盖仍在运行的旧原型；最新路径记录在 build/preview/latest.txt。按钮初始均为灰色，仅获得焦点时变色；已采用方案 A：浅绿底、绿字、左侧 >，无矩形包围边框。说明见 docs/design/选中样式方案.md。

## 共用基础层与弹窗边框

原型使用 `include/utils/foundation.h`、`src/utils/foundation_utf8.c`、`foundation_text.c`，中文/文本/整数处理不再在 preview_model.c 中重复实现。基础层完整检查运行 `make test-foundation`。

弹窗跨过中文字符的半边时会先清除整个旧字形，修复详情框左边缘错位；详情、帮助与放弃编辑的行宽回归检查均通过。实际字体观感仍以真实终端为准。

## 第五步后的入口区分

本命令继续用于内存草图。已接入 CSV 和业务的新应用运行 `tools/run-library.ps1`，参见[新应用说明](../application/README.md)。原型的编辑、按键与绘制现已抽到 src/ui_next/ 共用，仍通过 67 项检查。

缩放后放大到120×35并按F5恢复；普通有效按键也会完整补画。不监听无输入时的尺寸变化，退出恢复自动折行。
