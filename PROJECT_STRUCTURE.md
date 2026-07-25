# StreamPunk 项目结构

```
stream-punk/
├── include/stream-punk/          # Header-only 核心库（复制到你的项目即可用）
│   ├── StreamPunk.hpp            # 核心序列化 / 反序列化
│   ├── StreamPunkJson.hpp        # JSON 序列化支持
│   ├── StreamPunkSPOI.hpp        # SPOI 核心协议
│   ├── StreamPunkSPOIRange.hpp   # C++23 ranges 风格 SPOI 查询
│   ├── StreamPunkSPOIShadow.hpp  # SPOI Shadow 增量更新代理
│   ├── StreamPunkSchema.hpp      # 动态 Schema 解析
│   ├── StreamPunkOrmGen.hpp      # ORM SQL 生成
│   └── ...
├── tools/sp-gen/                 # 统一代码生成器（跨语言）
├── runtimes/                     # 各语言运行时文件（手动复制到你的项目）
│   ├── ts/                       # TypeScript 运行时 + SPOI 组件
│   ├── js/                       # JavaScript 运行时 + SPOI 组件
│   ├── py/                       # Python 运行时 + SPOI 组件
│   ├── java/                     # Java 运行时 + SPOI 组件
│   ├── go/                       # Go 运行时 + SPOI 组件
│   ├── rust/                     # Rust 运行时 + SPOI 组件
│   └── kotlin/                   # Kotlin 运行时 + SPOI 组件
├── skills/                       # AI 辅助技能文档（11 个 SKILL.md）
│   ├── stream-punk-cpp-types/    # C++ 类型定义
│   ├── stream-punk-meta-extract/ # 元数据提取器（生成 .bin 文件）
│   ├── stream-punk-sp-gen/       # sp-gen 代码生成器
│   ├── stream-punk-spoi/         # SPOI 查询/操作系统
│   ├── stream-punk-py/           # Python 集成
│   ├── stream-punk-ts/           # TypeScript 集成
│   ├── stream-punk-js/           # JavaScript 集成
│   ├── stream-punk-go/           # Go 集成
│   ├── stream-punk-rust/         # Rust 集成
│   ├── stream-punk-java/         # Java 集成
│   └── stream-punk-kotlin/       # Kotlin 集成
├── examples/                     # 10 个场景示例
│   ├── 01-basic-cpp/             # 纯 C++ 序列化/反序列化
│   ├── 02-cpp-to-ts/             # C++ ↔ TypeScript 跨语言
│   ├── 03-dynamic-schema/        # 动态 Schema 解析
│   ├── 04-json/                  # JSON 序列化
│   ├── 05-orm/                   # ORM SQL 生成
│   ├── 06-shadow-delta/          # 影子模式增量更新
│   ├── 07-full-stack/            # 全栈集成示例
│   ├── 07-spoi/                  # SPOI 动态查询
│   ├── 08-spoi-cross-lang/       # 跨语言 SPOI 查询（8 语言互通）
│   ├── 09-spoi-cross-lang-all/   # 8 语言全量集成测试
│   └── common/                   # 公共示例代码
└── scripts/
    ├── setup.ps1                 # 一键安装
    ├── setup.sh                  # 一键安装
    └── run-all.ps1               # 一键跑所有示例
    └── run-all.sh                # 一键跑所有示例
```