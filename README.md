# 宇宙图书馆管理系统

Windows 控制台纯 C 课程设计：管理员与读者、图书管理、借还续借、预约、罚款及统计，共17页。当前 C 实现遵守[教材函数范围](docs/课程允许函数清单.md)，使用 UTF-8 CSV 与两槽快照保存数据。旧实现位于 legacy/，不参与当前程序构建。

## 编译与运行

安装 Windows GCC/MinGW 和 GNU Make，将 gcc、make 加入 PATH。在项目根目录执行：

```text
chcp 65001
make run
```

运行前手动把终端调整到至少120列×35行，使用支持 ANSI 真彩色的 Windows Terminal。`chcp 65001` 是用户手动设置环境的命令，不由 C 程序调用。程序通过教材允许的 getch 和输出函数完成交互，不启动 Python、PowerShell 或 .NET。

- `make`：严格 C11 编译，输出 `start/CosmicLibrary.exe`；这个 exe 就是 C 主程序，没有启动器。
- `make run`：编译、首次准备数据，再直接运行 C 主程序。
- `make run-compiled`：使用已有 C exe，跳过编译。
- `make test`：直接编译运行基础、拼音、应用与原型 C 测试，不依赖 Python/PowerShell。

数据准备完成后，也可从项目根目录直接运行：

```text
start\CosmicLibrary.exe --data build/library-data
```

exe 需要数据目录参数，不作为无参数双击入口。教材白名单没有创建目录和启动其他进程所需的函数，因此不另写启动器。Makefile 仅在运行前通过 cmd 内建 mkdir/copy 将 resources/empty-data/ 的空模板放入新目录，然后由 C 程序生成样例。已有目录保持原样，文件缺失或损坏会报错，不会静默重置。目录准备或首次样例生成失败时请保留现场排查，不要直接删除现有数据。

默认数据仍为 `build/library-data`，保留之前的使用进度；新目录提供 admin/admin123、reader001/reader123 和20种样例书。也可用 `make run DATA_DIR=build/demo` 创建独立演示数据，路径请使用简单的相对 ASCII 目录，不含空格或命令字符。原始 data/ 不使用，make clean 不清理数据。

这是单进程版本，请人工避免多个实例同时操作同一目录。已取消外部单写者保护、窗口尺寸检测和代码页设置。两槽快照支持进程中断恢复，不保证断电落盘。建立快照后不要手改导出 CSV。

## 功能与操作

Tab/方向键导航，Enter确认，F1帮助，F5刷新，Ctrl+Q退出。窗口缩放后按F5完整补画，静置时不自动检测窗口变化。

K检索支持中文、拼音全拼、首字母和混合输入；超过8项显示键盘位置滚动条。借书/预约支持姓名、账号、书名或ISBN，重名候选需选择，确认框显示名称。借阅S筛选/P排序，图书管理B编辑资料，工作台D查看到期提醒。

## 仓库范围

- src/、include/：现行纯 C 业务、界面、基础工具与离线拼音字表。
- tests/、tools/preview/：C 测试与内存界面原型。
- resources/empty-data/：无用户数据的初始格式模板。
- tools/search/：拼音数据来源、元数据及必须保留的 Unicode 许可。
- docs/：整理后的运行、课程约束、功能、架构、验证与计划。
- legacy/：保留历史源码，不编译进当前应用。
- start/、build/：本地 exe、运行数据和测试产物，二进制不提交。

Python、PowerShell、C# 开发工具保留在本机但已取消跟踪，当前版本不包含这些源码。Git 历史按用户要求保留，旧提交中的工具仍可查阅。拼音字表已提交，普通构建与运行无需生成器或联网。

运行与排错见[运行说明](docs/运行说明.md)，功能、架构、验证与后续计划统一在[文档索引](docs/README.md)。PPT与报告暂缓。
