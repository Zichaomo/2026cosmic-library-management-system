# UTF-8 数据与认证迁移模块

代码位于 `include/migration/` 和 `src/migration/`，复用基础工具层，纯 C 实现不链接 BCrypt。现已由 `src/ui_next/` 新应用接入，启动见 `tools/run-library.ps1`；`tools/preview/` 仍是内存演示，`make all` 仍构建旧主体。

## 一条命令验证

在项目根目录的 PowerShell：

```powershell
make test-migration
```

也可执行 `.\tools\test-migration.ps1`。完整回归 `make test` 包含基础层、迁移层和旧系统测试；原型检查单独执行 `.\tools\preview\run-preview.ps1 -Test`。

每次在 `build/migration/run-随机编号/` 新建隔离目录，保留样本供检查，不覆盖真实 `data/`。迁移代码严格警告编译；同一脚本还编译未经修改的旧代码作为格式/散列兼容性参照，只有这部分参照程序仍链接 BCrypt。生成的参照源码、可执行文件与 CSV 都在 build/，不属于新 C 应用。

已通过 66 项 C 检查，另有旧代码反向读取、新进程登录、实际 Windows 独占文件锁恢复和真实 CSV 哈希未变化检查。详见 [验证记录](../../docs/CSV与认证迁移验证记录.md)。

## 准备新目录或导入副本

```powershell
# 必须指定一个尚不存在的目录；此命令只准备文件，不启动界面。
.\tools\migration\prepare-data.ps1 -Destination 'build/my-migrated-data'

# 导入已有五张 CSV 的副本，同样要求目标目录尚不存在。
.\tools\migration\prepare-data.ps1 -Destination 'build/imported-copy' -SourceDirectory 'path-to-copy-source'
```

源目录必须包含 readers.csv、books.csv、loans.csv、fines.csv、reservations.csv；导入只读源文件。脚本写入两枚 `EMPTY` 初始标记；C 模块首次加载并完整校验五张表后建立基线快照。不要在已有目录重新写 EMPTY 标记。

C 接口使用相对 ASCII 目录名，例如 `build/imported-copy`，文件内容为 UTF-8；不把 Windows 窄字符 fopen 宣称为支持任意 UTF-8 绝对路径。中文项目目录可作为工作目录，相对 ASCII 子路径已通过实际验证。

## 保存与恢复规则

1. 导入前，五张 CSV 是数据来源；缺失、无权限或格式错误均停止，不默认为空库。
2. 建立基线后，`state.0`、`state.1` 是两个完整的全表快照，作为恢复依据。写入非当前槽，包含版本、递增代次、各表行数、数据和校验提交尾记录；关闭后重新打开验证，成功后才更新 CSV。
3. CSV 更新中断时，重新打开从最新有效快照恢复全部五张表；较新快照不完整或校验失败时，退到上一份完整快照，并设置 `recovered` 标志。回退可能丢失最新一代的更改，调用页面必须提示恢复状态。
4. 两枚快照都损坏、代次冲突或关键 I/O 失败时停止。不能从可能只更新一半的 CSV 猜测恢复，也不能自动重建空库。
5. `CL_STORE_PENDING_EXPORT` / `CL_REGISTER_PENDING_EXPORT` 表示已经提交，不能再次注册；`UNCERTAIN` 表示状态未能确认，仓储会阻止继续操作，须重新打开后再查询。普通提交前失败则由注册/冻结操作撤销内存编辑。

单进程运行；不要让旧程序或其他进程同时写入迁移目录，也不要手动修改已建立快照的 CSV。旧代码的反向读取只用于验证导出兼容，未授权并发写入。`fclose` 与重读验证不等于磁盘落盘保证，本方案验证的是进程中断恢复，不承诺断电和底层介质故障下的持久性。

快照保留最近两代，文件大小受每表 10000 行、每字段 511 字节限制。未设计历史归档或无损修复损坏的最新快照。

## 认证边界

- 管理员仍为 admin / admin123；读者支持登录、注册、冻结/解冻、注销状态拒绝登录。失败登录清除旧会话。
- FNV-1a 继续使用旧常量，先处理 16 个盐字节，再处理 UTF-8 密码；固定 16 位小写十六进制。新盐用允许的 rand/srand/time，仅限课程演示。
- 密码编码校验独立于 UI 列宽限制，保留旧有效 UTF-8 密码，包括组合符及补充平面字符；新应用密码框已单独支持这些字符的掩码输入；普通字段仍不作完整字素簇承诺。
- 新注册编号为 ASCII 字母/数字、下划线、连字符或基本汉字，3～31 个 UTF-16 单元；已有合法 UTF-8 编号在导入后按原字节匹配，不改变大小写。姓名上限保留旧 UTF-16 单元数并检查显示范围，密码至少 6 个 UTF-16 单元、最多 255 个 UTF-8 字节。
- 五张表保留 BOM、列顺序、中文/英文状态值、日期字符串以及逗号/双引号转义。新解析器额外支持引号内换行；旧解析器本身不能读取这种多行字段，因此该扩展不属于与旧解析器双向兼容的保证。
