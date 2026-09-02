# 宇宙图书馆管理系统（Library Management System）

纯 C 语言实现的 Windows 控制台图书馆管理系统：字符画 TUI 界面（支持鼠标点击）、CSV 持久化。
课程设计 / 个人项目。详细需求见 [`docs/PRD.md`](docs/PRD.md)。

## 分支约定

- `main`：稳定主线
- `wdy_branch`：开发分支（当前）

## 目录结构

```
.
├── docs/
│   └── PRD.md            # 产品需求文档
├── src/
│   ├── main.c            # 入口：控制台初始化、事件循环、全局状态
│   ├── types.h           # 公共类型：Result / Date / DynArray
│   ├── entities.h        # 实体结构：Book/Author/Publisher/Category/Reader/Loan/Reservation/Fine
│   ├── ui/               # 界面层（Win32 控制台封装）
│   │   ├── console.c/h   # 输出/颜色/光标/清屏
│   │   ├── input.c/h     # 键盘 + 鼠标事件解析
│   │   ├── widget.c/h    # 控件：按钮/列表/表单/弹窗/状态栏
│   │   ├── screen.c/h    # 页面路由与布局
│   │   └── theme.c/h     # 颜色与样式
│   ├── core/             # 业务逻辑层
│   │   ├── auth.c/h      # 登录/注册/会话
│   │   ├── book.c/h      # 图书与副本、字典（作者/出版社/分类）
│   │   ├── reader.c/h    # 读者管理
│   │   ├── loan.c/h      # 借还/续借/逾期
│   │   ├── reservation.c/h # 预约队列
│   │   ├── fine.c/h      # 罚款计算与缴费
│   │   ├── search.c/h    # 检索与过滤
│   │   └── stats.c/h     # 统计报表
│   ├── data/             # 数据层
│   │   ├── csv.c/h       # RFC 4180 CSV 解析/写出
│   │   └── repository.c/h# 各实体加载/保存/原子写
│   └── utils/            # 通用工具
│       ├── string.c/h    # 字符串处理
│       ├── date.c/h      # 日期时间
│       ├── hash.c/h      # FNV-1a 散列 + 盐
│       ├── file.c/h      # 文件读写 / 原子写
│       └── validator.c/h # 输入校验
├── Makefile              # 构建脚本（gcc / MinGW-w64）
└── data/                 # 运行时生成（CSV 数据，已 gitignore）
```

## 构建与运行

需要 [w64devkit](https://github.com/skeeto/w64devkit)（含 MinGW-w64 gcc + make）。

```bash
make          # 编译，生成 library.exe
make run      # 编译并运行
make clean    # 清理
```

或手动编译：

```bash
gcc -std=c11 -Wall -Wextra -fexec-charset=UTF-8 \
    src/main.c src/utils/*.c src/data/*.c src/core/*.c src/ui/*.c \
    -o library.exe -static
```

> 控制台字体建议「新宋体 / 等宽字体」，保证中文与框线字符对齐（PRD §7）。

## 里程碑（详见 PRD §8）

| 阶段 | 内容 |
|---|---|
| M1 | 工程骨架 + 控制台框架 + 鼠标/键盘事件 + CSV 读写 + 登录/注册 |
| M2 | 图书/字典 CRUD + 检索 + 图书详情 |
| M3 | 读者管理 + 借还/续借 + 预约 + 罚款 |
| M4 | 统计报表 + 导出 + 帮助/文档 + 打磨 |

## 开发协作约定

- 用户自行修改的代码：使用 VSCode 提交/推送（分支 `wdy_branch`）
- 助手生成的代码：由助手直接执行 git 命令提交
- 提交前保持工作区干净，避免互相冲突
