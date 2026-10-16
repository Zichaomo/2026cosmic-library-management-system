# 宇宙图书馆管理系统（2026cosmic-library-management-system）

> ## ⚠️ 当前状态：非最终版，正在按课程函数约束重构
>
> **口径更正日期：2026-10-16。**
>
> 课程设计要求：**只能使用教材中提到的库函数**（教材附录 C，共 **66** 个函数，清单见 [`docs/课程允许函数清单.md`](docs/课程允许函数清单.md)）。
>
> 检查现有代码后发现，实现大量使用了清单之外的库函数与头文件——**Win32 控制台 API、`<wchar.h>`/`<wctype.h>` 宽字符库、`<bcrypt.h>`、`<time.h>`，以及 `memset`/`snprintf`/`strncpy`/`memcpy` 等**——已**超出课程允许范围，属于重大返工项**。
>
> 因此，仓库内旧文档中的「**最终版**」「**最终汇报版本**」「**v1.0.0 已完成**」等表述**一律作废**，它们只代表重构前的历史状态；界面部分另有可优化空间，一并纳入本轮重做。

---

## 1. 这是什么

北京化工大学 C 语言实训课程设计「宇宙图书馆管理系统」，纯 C 语言、Windows 控制台程序，业务覆盖图书/读者管理、借还续借、预约、罚款与统计，数据以 CSV 持久化。

- 语言与标准：C（不使用 C++/C#/Python/JS）
- 运行平台：Windows 控制台
- 持久化：纯文本 CSV
- 交互：重构前为「键盘 + 鼠标 + 宽字符彩色 TUI」；重构后需在函数约束内重新设计

## 2. 为什么现在不是最终版

| 问题 | 说明 |
| --- | --- |
| 函数越界（重大） | 课程要求只用教材附录 C 列出的 66 个库函数，现有代码远远超出 |
| 界面待优化 | 界面的信息层级、分页、提示与中文排版仍有提升空间，需在约束重做时一并解决 |
| 测试待重写 | 现有自动化测试依赖 Win32 控制台行为，重构后需要同步改写 |

## 3. 现有实现与课程约束的差距（重构依据）

以下为对 `src/`、`include/`、`tools/` 的静态扫描统计（数字 = 出现次数，仅作返工量评估）：

| 类别 | 当前用法 | 是否在附录 C | 说明 |
| --- | --- | --- | --- |
| 控制台输出/输入 | `<windows.h>` 出现在 7 个文件；`SetConsoleMode`、`GetConsoleScreenBufferInfo`、`SetConsoleTextAttribute`、`SetConsoleCursorPosition`、`SetConsoleCursorInfo`、`GetConsoleMode`、`SetConsoleOutputCP`、`SetConsoleCP`、`GetConsoleCursorInfo`、`GetStdHandle` 等 | ❌ 不允许 | 清屏、定位、颜色、鼠标与即时按键全部依赖它 |
| 宽字符 | `<wchar.h>` 16 处、`<wctype.h>` 4 处；`swprintf` 63、`wcslen` 19、`wcsncpy` 18、`wcscpy` 14、`wcscmp` 9、`wcsrchr` 4、`wcsncat`/`wcscat`/`wcstol` 各 1 | ❌ 不允许 | 中文字符串处理几乎全走宽字符 |
| 随机数/加盐 | `<bcrypt.h>` + `BCryptGenRandom` 1 处 | ❌ 不允许 | 清单内只有 `rand` |
| 时间日期 | `<time.h>` 5 处；`time` 27、`strftime` 2 | ⚠️ 部分可以 | `time` 有教材依据（书上第 64 页）可保留；`strftime`、`localtime` 教材里没有，年月日换算要自己写 |
| 内存/字符串 | `memset` 29、`snprintf` 17、`strncpy` 10、`strtol` 3、`memmove` 3、`memcpy` 2、`memcmp` 1 | ❌ 不允许 | 需手写循环或改用 `strcpy`/`strcat`/`strlen` |
| 其它 | `fflush` 2、`sscanf` 2、`exit` 1 | ⚠️ 部分可以 | `exit` 教材多处使用（可保留）；`fflush`、`sscanf` 教材里没有，需要替换 |
| 类型头 | `<stdbool.h>` 15、`<stddef.h>` 10、`<stdint.h>` 2、`<limits.h>` 1、`<errno.h>` 2 | ✅ 自写 typedef 替代 | 不再包含这些头文件：`size_t`/`NULL`/`INT_MAX`/`EOF`/`FILE` 由 `<stdio.h>` 自带（实测重定义会报 conflicting types），`bool`/`u8`/`u16`/`u32`/`u64` 放进项目自己的 `include/base_types.h` —— typedef 与 #define 是语言特性、不是库函数 |

按教材附录 C，实现时**实际可用的库函数只有这 66 个**：数学 20（`abs`…`tanh`）、字符 13（`isalnum`…`toupper`）、字符串 6（`strcat`/`strchr`/`strcmp`/`strcpy`/`strlen`/`strstr`）、输入输出 6（`getchar`/`gets`/`printf`/`putchar`/`puts`/`scanf`）、文件 17（`fclose`…`rewind`）、内存 4（`calloc`/`free`/`malloc`/`realloc`）。

**老师口径（2026-10-16）：正文各章提到的函数也算数。** 对教材全文（230 页扫描件）做中文 OCR 核查后，除上述 66 个之外还有 6 个可用：

| 函数 | 教材出处 | 用处 |
| --- | --- | --- |
| `getch()` / `getche()` | 第 3 章例 3-16，书上第 42 页（`#include <conio.h>`） | 菜单「按一下即响应」，不必回车 —— **保留 TUI 操作手感的关键** |
| `srand()` / `time()` | 书上第 64 页 `srand(time(0))`（`<stdlib.h>`、`<time.h>`） | 随机数种子、取当前时间 |
| `strlwr()` / `strupr()` | 书上第 87 页专节讲解 | 大小写转换、忽略大小写比较 |
| `exit()` | 书上第 87、88、177、179～187 页 | 出错退出 |
| `ferror()` / `clearerr()` | 第 9 章 9.4，书上第 185 页 | 文件读写查错 |

**教材通篇没有出现**（仍然越界）：`memset`、`memcpy`、`memmove`、`memcmp`、`sprintf`、`snprintf`、`strncpy`、`strncat`、`strncmp`、`strtok`、`atoi`、`atof`、`strtol`、`fflush`、`sscanf`、`system`、`Sleep`、`<windows.h>` 全部 API、`<wchar.h>`/`<wctype.h>` 宽字符库、`<bcrypt.h>`，以及 `gotoxy`/`textcolor`/`clrscr` 一类屏幕控制函数。详见 [`docs/课程允许函数清单.md`](docs/课程允许函数清单.md)。

## 4. 重构方向（待办，尚未动代码）

1. **输出层**：只用 `printf` / `puts` / `putchar` / `fputs` 绘制，禁止 Win32 光标、颜色、清屏 API；彩色 TUI 改用 `printf` 打印 ANSI 转义序列（清屏 `"\033[2J"`、定位 `"\033[10;20H"`、上色 `"\033[31m"`）—— **2026-10-16 已确认可行**（它是字符串不是库函数，依赖终端支持 VT）；中文用 UTF-8（`chcp 65001`）。
2. **输入层**：`getch()` / `getche()` 有教材依据（书上第 42 页，`<conio.h>`），且本机 w64devkit 工具链实测可直接编译 —— **按键菜单、无回车响应可以保留**；鼠标与滚轮依赖 Win32，必须放弃。
3. **日期时间**：`time()` 有教材依据（书上第 64 页），可以取当前时间；但 `localtime` / `strftime` 教材里没有，年月日换算要自己写。
4. **字符串与格式化**：用 `printf` / `fprintf` 直出替代 `snprintf` / `sprintf`；用 `strcpy` + `strcat` + `strlen` 替代 `strncpy`；用 `for` 循环替代 `memcpy` / `memset` / `memmove`。
5. **随机与散列**：盐值用 `rand`；FNV-1a 散列自己写循环（本来就不依赖库）。
6. **界面**：重做为纯标准 C 的菜单式 TUI，重新设计信息层级、分页与提示文案。
7. **测试与文档**：重写依赖控制台行为的测试；同步更新本 README 与 `docs/` 下各文档口径。
8. **类型定义**：新建 `include/base_types.h`（只有 typedef / #define，不含任何函数调用），替代 `<stdbool.h>` / `<stdint.h>` / `<stddef.h>` / `<limits.h>` / `<errno.h>`。`size_t` / `NULL` / `INT_MAX` / `EOF` / `FILE` 由 `<stdio.h>` 自带、不再重复定义（实测重定义会报 `conflicting types`）；`bool` / `true` / `false` 共 509 处可原样保留；`uint64_t` / `uint16_t` 改名 `u64` / `u16`；`errno` 7 处改用返回值 / `feof` / `ferror` 判断。详见清单文档第 9 节。

## 5. 仓库与分支状态

| 项 | 值 |
| --- | --- |
| 当前分支 | `main`（与 `xyt_branch` 完全一致） |
| HEAD | `b757d9d` — chore: 忽略汇报材料、压缩包和本地版本副本 |
| 远端 | `origin` = https://gitee.com/zichaomo/2026cosmic-library-management-system.git |
| 2026-10-16 操作 | 把 `main` 硬重置到 `xyt_branch` 并强推远端；`origin/main` 原有 4 个提交（`d404013`/`fc07449`/`85af0d6`/`b563e00`）已按决定舍弃 |

> ⚠️ 强推之后再协同时，其他人需先 `git fetch origin` 再 `git reset --hard origin/main`（或改用 `xyt_branch`），否则推送会被非快进拒绝。

## 6. 构建与运行（重构前的历史方式，仅供对照）

```powershell
mingw32-make          # 编译，产物在 build\
.\build\library.exe    # 运行
mingw32-make test      # 运行测试
```

管理员演示账号：`admin` / `admin123`。建议终端窗口至少 `100 × 30`。

## 7. 文档索引

| 文档 | 作用 |
| --- | --- |
| [`docs/项目交接文档.md`](docs/项目交接文档.md) | **接手重构先看这份**：硬约束、代码地图、越界量化、分阶段计划、构建坑位、替代写法速查表、验收标准 |
| [`docs/课程允许函数清单.md`](docs/课程允许函数清单.md) | **硬约束**：教材附录 C 全部 66 个允许函数 + 正文另外 6 个 + 自写类型方案；开工前必读 |
| [`docs/PRD.md`](docs/PRD.md) | 需求蓝图（历史版）。其中 Win32/宽字符/鼠标的技术选型违反函数约束，已在其顶部标注 |
| [`docs/最终汇报版本说明.md`](docs/最终汇报版本说明.md) | 重构前的汇报材料，口径已在其顶部更正 |
| [`docs/最终版Bug修复与账号说明-20261012.md`](docs/最终版Bug修复与账号说明-20261012.md) | 重构前的修复记录，口径已在其顶部更正 |
| [`项目学习与交接记录.md`](项目学习与交接记录.md) | 学习与交接记录（写于 2026-09-13），口径已在其顶部更正 |
