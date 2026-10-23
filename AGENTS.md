# 项目协作约定

- 当前交付范围为课程合规单进程。现行 C 源码、界面和 C 测试遵守 docs/课程允许函数清单.md。
- 用户最新要求：Gitee 当前版本不提交 Python、PowerShell、C# 开发工具；文件保留本地并忽略，保留 Git 历史，不重写旧提交。
- 程序运行全程直接使用 C exe，不调用 PowerShell、Python、.NET 或外部启动器。Makefile 在启动 C 程序的同一 cmd 命令中执行 chcp 65001；120×35 尺寸与避免重复启动由用户手动保证，不重新添加外部保护。
- make 直接用 GCC 编译到 start/CosmicLibrary.exe，这个文件就是 C 业务程序。make run 在项目根目录直接启动；make run-compiled 跳过编译。C 不调用 system 或 Win32 启动 API。
- 教材范围没有创建目录函数。Makefile 在运行前用 cmd 的 mkdir/copy 准备新的 build/library-data，再调用 C --seed-demo。既有目录不初始化、不覆盖；准备失败保留现场，不自动重置数据。
- 每轮程序修改通过检查后更新 start/ 的 C exe。保留正在运行的实例，不能杀进程或清空 build/，不能覆盖被占用的 exe；需要时提示用户关闭后重编译。
- exe、缓存、真实运行 CSV 与快照只留本地；resources/empty-data/ 是可提交的空数据格式模板。make clean 保留数据。
- 保留源码历史并按功能分批 commit，仅在用户要求时推送。
- PPT/报告暂缓，未选功能不自动实施。开发用审计/回归工具可在本地使用，但不得重新加入 Git，也不得通过旧发布脚本覆盖纯 C 入口。
