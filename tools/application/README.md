> 历史记录说明（2026-10-22）：本文涉及的 Python/PowerShell/.NET 工具仅留本地，已从当前仓库取消跟踪，旧启动与单写者保护说明已被[纯 C 启动流程](../../docs/纯C启动与仓库边界.md)取代；历史测试结果仅对应当时版本。

# 新应用与持久化界面

在项目根目录执行 `chcp 65001`、把终端调到至少 120×35，再执行 `make run`（旧的 `tools/run-library.ps1` 已移除）。界面保持暖白/暖石/鼠尾草绿和方案 A 焦点。

默认数据目录为 `build/library-data`，首次提供 `reader001 / reader123` 和 20 种图书；管理员为 `admin / admin123`。后续启动保留修改，不重建样本。原始 `data/` 不会被使用。

列表 K 检索、J 借书、Y 预约；借阅 G 归还/X 续借，预约 Q 取消，罚款 L 模拟缴费。图书管理 C 新增图书/Z 增加副本/T 停借或恢复，读者 D 冻结或解冻；F 返回工作台。输入框内字母只作输入，确认弹窗 Y/N 保留。F5 恢复/重读，Ctrl+Q 退出。

交互每次有效按键后完整补画，不先清屏；首次或 F5 才清屏，禁用自动折行，退出恢复折行。缩放损坏后放大窗口并按 F5 可恢复；没有尺寸监听，无按键不会自动恢复。

默认构建 `make`，启动 `make run`；完整回归 `make test`，单独应用测试 `make test-application`。`make migrated` / `make run-migrated` 保留为别名。旧实现已独立归档到 legacy/，只作兼容参照。

新界面使用 `src/ui_next/`，业务使用 `src/migration/library.c`，仓储与认证沿用迁移层。旧 `tools/preview/run-preview.ps1` 已移除（仅留本地），原型现在手动编译运行，见 [preview 说明](../preview/README.md)；它不是持久化界面的入口。

原启动器曾为整次运行持有数据目录独占句柄；该启动器已取消，现在由使用者人工保证同一数据目录只开一个写入者。不要让旧程序写新迁移目录，也不要手改已建立快照的 CSV。

`capture.py` 把最新应用测试导出的 C ANSI 帧复现为 docs/implementation 图片，不是实际终端截图。普通字段显示范围与真实输入法/字体限制见[第五步验证记录](../../docs/第五步业务与界面验证记录.md)。

工作台现为左导航、右侧数据概览和当前事项说明，Enter进入选中事项。表单底部有光标定位/插入/删除提示，PPT的admn插入i修正为admin已通过实际登录回归。按钮统一预留选中符号与空格，切换焦点时文字不跳动。

新增：工作台D到期提醒；借阅S筛选/P日期排序；图书管理B编辑ISBN/书名/作者。管理员按读者姓名/账号与书名/ISBN办理借书/预约，多结果候选选择，确认显示名称。start/CosmicLibrary.exe 就是 GCC 编译出来的 C 主程序本身（`make` 直接覆盖它），也可执行 `start\CosmicLibrary.exe --data build/library-data`；无参数只打印用法。
