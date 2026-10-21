# 旧实现归档

此目录仅用于离线兼容与历史回归，不属于课程白名单交付源码，不是应用启动入口。Win32、wchar、BCrypt 和旧多进程能力均只在这里保留。

2026-10-21 从原 src/、include/、tests/、tools/demo/ 原样移动 43 个文件。manifest.json 记录原 Git blob 和原始字节 SHA-256；建立清单时已与 HEAD 逐一核对。不要修改这些参照文件。tools/audit_source.py 会复核其哈希。

`make test-legacy` 编译旧认证、流通、UI 三套测试（91 项），在 build/legacy/run-*/ 的工作目录运行，不访问真实 data/。迁移测试另从此处编译旧仓储生成合成样本，再检查新旧数据与密码双向兼容。

旧演示说明中的入口和路径只适用于历史版本。不要启动旧程序写入新快照数据目录。当前启动使用仓库根目录的 tools/run-library.ps1 或 make run。
