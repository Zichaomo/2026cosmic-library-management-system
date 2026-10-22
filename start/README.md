# 本地纯 C 程序

make 编译得到这里的 CosmicLibrary.exe，现已直接替换为 C 主程序，不含 C# 启动器。编译时若程序被占用，请先正常退出再编译，不会强制结束实例。

在项目根目录手动调整终端到120×35后运行（Makefile在同cmd命令中设置UTF-8，C不调用系统命令）：

```text
make run
```

已有编译产物用 make run-compiled；已准备好数据可直接执行 cmd /d /c "chcp 65001 >nul && start\CosmicLibrary.exe --data build/library-data"。exe需要参数，不支持无参数双击直接进入。旧 app/、latest.txt、cmd 和 .NET 启动器属于本地历史产物，当前启动流程不使用它们。不要再调用本地旧 PowerShell 发布脚本。

exe 不提交 Git；克隆后 make 编译即可。数据仍保存在项目的 build/library-data，已有进度不会重置。
