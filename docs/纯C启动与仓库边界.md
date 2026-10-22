# 纯 C 启动与仓库边界

2026-10-22，按用户最新要求取代旧版外部启动器流程。

## 当前实现

start/CosmicLibrary.exe 就是 GCC 编译的 C 主程序，包含界面、业务、存储、拼音检索。已取消 C# 启动器、PowerShell 运行链、自动终端尺寸检查和外部单写者句柄；C 不调用 system、进程创建或 Win32 API。

Python、PowerShell、C# 文件和旧 cmd 入口从当前 Git 索引移除，原文件本地保留并由 .gitignore 排除。旧 .NET exe 移到本地 build/local-tools/ 备存，不再是当前入口。遵守保留历史要求，旧提交仍包含这些工具，不改写远端历史。拼音 C 字表、来源元数据、Unicode 许可继续提交；生成器仅留本地。

## 启动

项目根目录手动执行 chcp 65001，调整终端至少120×35，然后 make run；已有编译版本可以 make run-compiled。或直接执行 start\CosmicLibrary.exe --data build/library-data。无参数只显示用法并返回，不支持无参数双击登录；不另造需要额外函数的启动器。

Makefile 使用 cmd 作为构建命令解释器，仅调用 GCC、mkdir、copy、C exe；VS Code 任务直接调用 make。Python、PowerShell、.NET 不参与当前构建、标准测试或程序运行。

教材白名单没有创建目录的函数。首次数据目录由 Makefile 在运行前用 mkdir/copy 从 resources/empty-data/ 准备七个格式文件，再调用 C 主程序 --seed-demo 写入样例。这是明确的外部构建准备步骤，不宣称 C 已有创建目录能力。实际交互循环和所有业务均由 C 完成。

默认仍使用 build/library-data，既有目录不初始化或覆盖。新目录的样例账号为 admin/admin123、reader001/reader123。DATA_DIR 可指定简单的相对ASCII目录（不要空格或命令字符）；例如 make run DATA_DIR=build/demo。准备中断或已有数据损坏时停止排查，不自动删目录、补空表或重置。

用户人工保证同一数据目录仅运行一个实例。删除旧脚本保护后不具备并发写入保障；这不是新的并发实现。make clean 保留全部持久数据。

## 验证范围

本轮严格 C11、Wall/Wextra/Wpedantic/Werror 编译通过。新的 make test 不调用外部开发脚本，直接运行基础55、搜索22、应用153、原型67，共297项 C 断言，并验证 UI 创建借阅后另一进程读取持久化记录。使用随机独立测试目录，未重置用户数据。

本地保留的源码审计器重新检查35个活动C源文件/头文件，调用仍在已核对教材的75函数范围内，43个历史参照文件哈希不变。词法审计不等同于完整语义证明。

以前的454项为包含旧参照、迁移和外部环境专项的历史全量结果。相关 C 测试源码未删除，旧本地脚本仍可用于人工开发检查；当前 make test 不再宣称覆盖这些外部专项，尤其不再测试已取消的启动保护。不要使用旧脚本发布步骤覆盖新的纯 C exe。

另外从 Git 当前索引导出一份不含本地工具的独立源码，make编译、make prepare首次建样例通过；实际ConPTY执行make run-compiled、管理员登录并Ctrl+Q退出，退出码0。工具终端80×24只用于输入输出检查，不作为120×35视觉验收。原build/library-data全部文件在make prepare前后哈希一致。新exe的PE CLR运行时目录为空，确认不再是.NET启动器。
