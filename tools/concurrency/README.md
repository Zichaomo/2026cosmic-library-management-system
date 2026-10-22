> 历史记录说明（2026-10-22）：本文涉及的 Python/PowerShell/.NET 工具仅留本地，已从当前仓库取消跟踪，旧启动与单写者保护说明已被[纯 C 启动流程](../../docs/纯C启动与仓库边界.md)取代；历史测试结果仅对应当时版本。

# 多终端可行性实验

仓库根目录执行 `make test-concurrency`。需要 Windows GCC 与 Python 3，使用build/concurrency/run-GUID下的全新空文件，不访问应用CSV。结果不是“多终端已实现”，只说明当前CRT的实际模式行为。

C探针只调用课程白名单函数，纳入make audit；Python多进程、管道、线程和kill只是外部测试驱动，不能视作C程序能力。24个独立进程分别竞争普通fopen、x模式及rename，记录成功/拒绝数；持有文件、正常关闭、测试子进程被kill后的再次打开均记录。

输出JSON与probe.exe保留在独立目录，最新相对目录见build/concurrency/latest.txt。进程退出码10表示预期的打开/rename拒绝，0表示成功；实验不会因模式不支持而伪报实现通过。初始异常返回码会使实验失败。测试只结束它自己创建的子进程。

输入探针需单独在真实交互终端执行：读取latest.txt后运行其中probe.exe input；先显示WAITING_FOR_GETCH，按一个键才显示GETCH_RETURNED及键码。不要把重定向stdin的getchar持有文件实验当作getch终端实验。

当前结论与本轮实际结果见[多终端可行性记录](../../docs/多终端可行性记录.md)。不解除应用启动器的单写者保护。
