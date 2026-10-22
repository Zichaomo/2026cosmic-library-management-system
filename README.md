# 宇宙图书馆管理系统

纯 C、Windows 控制台、UTF-8 CSV 持久化的课程设计，提供管理员与读者、图书管理、借还续借、预约、罚款及统计，共 17 页。默认构建已切换到符合当前课程函数范围的新实现，旧实现独立归档于 legacy/。用户已认可本轮展示界面，选择课程合规单进程交付；完整输入法专项仍需按实际演示环境确认，当前不标记为最终版。

交付范围、演示流程及源码包见[单进程交付说明](docs/单进程交付说明.md)。多终端自动同步与并发写入保留为后续扩展。

## 启动

使用 Windows GCC/MinGW，将 gcc 和 make 加入 PATH。在项目根目录的 PowerShell 执行：

```powershell
.\tools\run-library.ps1
```

或 `make run`。终端支持 ANSI 真彩色，至少 120 列 ×35 行；脚本设置 UTF-8 代码页并检查实际可见尺寸。Tab/方向键导航，Enter 确认，F1 帮助，F5 重读，Ctrl+Q 退出。列表按 K 键进入检索，支持中文、全拼、首字母及混合输入；超过一页时显示键盘位置滚动条；完整按键表见[快捷键与缩放修正](docs/快捷键与缩放修正记录.md)。

欢迎页标题、标语与按钮按同一中轴居中。管理员/读者工作台采用左导航、右侧概览与事项说明；表单提示左右定位、插入和删除，见[工作台与编辑调整](docs/工作台与编辑交互调整.md)。缩小窗口再放大后，下一次有效按键会完整补画；推荐按 F5 直接恢复，不触发业务操作。白名单下仍使用阻塞 getch，静置时不能自动检测尺寸。

首次在 `build/library-data` 创建独立样本：读者 `reader001 / reader123`、管理员 `admin / admin123`，20 种图书各 3 册。再次启动保留修改，不会重置。原始 data/ 不会使用；请保留 build/library-data，`make clean` 也不会删除它。

启动器在整个运行期间持有目录独占句柄，请保持单进程并从脚本启动。直接执行 exe 没有此保护。两槽快照支持进程中断恢复，不保证断电落盘；新快照建立后不要手改 CSV 或让旧程序同时写入。F5 是手动刷新，无按键时不自动同步。

## 构建与验证

```powershell
make
make test
```

`make` 仅编译新应用，唯一 exe 路径保存在 `build/application/latest.txt`，不启动、不准备数据、不覆盖已运行实例。`make test` 一次完成源码清单核对、新构建、基础55、迁移66、应用115、原型67、拼音检索22与旧参照91项，以及新进程持久化、真实文件锁失败恢复和启动器独占检查。测试需要 Python 3（PATH 或本机 Codex bundled runtime），兼容参照独立链接 BCrypt，新应用不链接。

可分别执行 `make audit`、`make test-foundation`、`make test-migration`、`make test-application`、`make test-preview`、`make test-legacy`。`make migrated` / `make run-migrated` 保留为新构建/运行别名。VS Code 的默认构建、运行和测试任务调用同一套脚本。

新增离线检索位于src/search/、include/search/，Unicode数据许可及生成方式见[搜索说明](tools/search/README.md)。更多功能候选见[功能补齐与参考方案](docs/功能补齐与参考方案.md)，由用户选择后实施，PPT/报告暂缓。

## 源码与文档

- `src/ui_next/`、`include/ui_next/`：当前应用、17页导航和共用终端；目录名保留迁移期间命名。
- `src/migration/`、`include/migration/`：当前业务、认证、CSV与快照仓储。
- `src/utils/foundation_*.c`、`include/utils/foundation.h`、`include/base_types.h`：基础工具与课程类型。
- `tests/`：当前 C 测试，遵守同一函数范围。
- `tools/`：外部 PowerShell/Python 工具与合规内存原型 C 源码。原型 `tools/preview/run-preview.ps1` 不持久化。
- `legacy/`：原样保留的旧源码、头文件、测试和演示，单独核对哈希，不纳入合规应用。
- `build/`：被 Git 忽略的构建产物、独立测试和持久化体验数据。

[第六步回归与合规记录](docs/第六步回归与合规记录.md)提供范围与验收限制；接续工作看[续接摘要](docs/续接摘要.md)。另见[计划](docs/plan.md)、[课程函数清单](docs/课程允许函数清单.md)、[第五步业务与界面](docs/第五步业务与界面验证记录.md)、[CSV与认证](docs/CSV与认证迁移验证记录.md)、[基础工具](docs/基础工具层验证记录.md)、[页面草图](docs/design/页面草图说明.md)。

中文项目根路径可用；数据目录参数须项目内相对 ASCII 路径。安全导入步骤见[迁移工具说明](tools/migration/README.md)。新盐仅为课程演示随机方案，不作为真实用户系统的密码安全设计。
