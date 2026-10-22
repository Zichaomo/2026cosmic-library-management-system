# 启动检查入口

双击 **CosmicLibrary.exe**。这里会在 `make`、测试构建或正常启动编译完成后自动更新；也可使用“启动图书馆.cmd”。不需要再次编译，保留已有 build/library-data。

CosmicLibrary.exe 是外部 .NET 环境启动器，负责调用 PowerShell 配置 UTF-8、检查120×35可见尺寸、准备初次样例及持有单写者句柄。业务程序是 app/ 中的纯C exe，仍遵守课程白名单。不要直接打开 app/ 中的程序绕过启动器；直接运行会缺少数据目录参数和外部保护。

不要把整个 start/ 从项目中单独移走；依赖上级 tools/ 和数据目录。latest.txt指向最新编译版本，旧运行实例使用自己的exe，不会被杀死或覆盖。二进制产物和latest.txt不提交Git，启动器源代码在tools/start/，重建用make。
