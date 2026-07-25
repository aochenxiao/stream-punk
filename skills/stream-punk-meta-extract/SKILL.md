---
name: "stream-punk-meta-extract"
description: "生成 StreamPunk 二进制元数据文件（stream-punk-meta.bin）。当用户需要从 C++ 类型定义中提取元数据以供 sp-gen 跨语言代码生成时调用。"
---

# StreamPunk 元数据提取器

本技能解释如何从 C++ 类型定义中提取元数据，生成 `temp/stream-punk-meta.bin` 二进制文件。

> **适用场景**：项目有非 C++ 客户端（JS/TS/Python/Java/Go/Rust/Kotlin），需要跨语言代码生成时，必须运行元数据提取器。纯 C++ 项目不需要此步骤。
>
> **在整个跨语言流程中的位置**：
> 1. `stream-punk-cpp-types` → 定义 C++ 类型
> 2. **本 Skill** → 提取元数据
> 3. `stream-punk-sp-gen` → 生成客户端代码
> 4. 对应语言 Skill（`stream-punk-js` / `stream-punk-ts` 等）→ 在客户端使用

## 为什么需要元数据文件？

StreamPunk 的跨语言代码生成器（sp-gen）需要知道 C++ 端定义了哪些类型、每个类型有哪些成员、每个成员是什么类型，才能生成等价的其他语言代码。元数据文件就是这份"类型目录"。

## 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│  customData.hpp（类型注册表）                                 │
│    Xt_CustomType(X__)                                       │
│      X__(Player, Player)                                    │
│      X__(Enemy, Enemy)                                      │
│      ...                                                    │
│                                                             │
│  Data.hpp（类型定义）                                         │
│    struct Player : public Base {                             │
│      #define Xt_Player(X__) ...                             │
│      UseData(Player);  ← 宏展开生成编译期常量                  │
│    };                                                       │
└─────────────────────────────────────────────────────────────┘
         │
         │  meta-extractor.cpp  #include 了上述头文件
         ▼
┌─────────────────────────────────────────────────────────────┐
│  meta-extractor（元数据提取器，一个 C++ 程序）                  │
│                                                             │
│  1. INIT_StreamPunk() → Xt_CustomType(X_reg_custom)          │
│     → 注册所有自定义类型的工厂函数                              │
│                                                             │
│  2. Xt_CustomType(X_extract) → extractTypeMeta<T>()          │
│     → 对每个注册的类型，读取编译期常量：                         │
│       · _className   = "Player"    ← 类名字符串                │
│       · _baseName    = "Base"      ← 基类名字符串              │
│       · _membersName = {"name","level","health"} ← 成员名数组  │
│       · TypeDesc<T>::v = [SpToken...]  ← 类型描述符数组        │
│       · TypeID_t<T>::id = ...        ← 类型 ID               │
│                                                             │
│  3. writeMetaFile() → 序列化写入 temp/stream-punk-meta.bin    │
└─────────────────────────────────────────────────────────────┘
         │
         │  二进制文件（小端序，自定义格式）
         ▼
┌─────────────────────────────────────────────────────────────┐
│  temp/stream-punk-meta.bin                                   │
│                                                             │
│  [4B] Magic: "SPMD" (0x53504D44)                            │
│  [4B] Version: 1                                            │
│  [4B] TypeCount: N                                          │
│                                                             │
│  For each type:                                             │
│    [4B] typeID         ← E_type 枚举值                       │
│    [2B+str] className  ← 类名                               │
│    [2B+str] baseName   ← 基类名                              │
│    [2B] memberCount    ← 成员数量                            │
│    For each member:                                          │
│      [2B+str] memberName  ← 成员名                           │
│      [2B] descLen         ← 类型描述符 token 数量             │
│      [4B*N] typeDesc[]    ← 类型描述符 SpToken 数组          │
└─────────────────────────────────────────────────────────────┘
```

## 关键文件

| 文件 | 作用 |
|------|------|
| `include/stream-punk/StreamPunk.hpp` | 定义 `UseData` 宏、`TypeDesc<T>`、`TypeID_t<T>` 等编译期基础设施 |
| `include/stream-punk/customData.hpp` | 类型注册表，`Xt_CustomType` 列出所有需要跨语言支持的类型 |
| `v0.9_ago/code-generator-meta/meta-extractor.cpp` | 元数据提取器主程序，读取编译期常量并写入 .bin 文件 |
| `include/stream-punk/MetaData.hpp` | 元数据文件的二进制格式定义（读写共享），已合并原 `meta-format.hpp` |

## UseData 宏展开后生成了什么？

以 `struct Player` 为例，`UseData(Player)` 在编译期展开为：

```cpp
// 类名字符串（编译期常量）
static constexpr inline char const* _className = "Player";
static constexpr inline char const* _baseName  = "Base";

// 成员名数组（编译期常量）
static constexpr inline char const* _membersName[] = {
    "name", "level", "health"
};

// 类型描述符（SpToken 数组，描述完整的类型结构）
// TypeDesc<std::string>::v = {50, ...}   ← 编译期常量
// TypeDesc<i32>::v          = {1, ...}    ← 编译期常量
// TypeDesc<f64>::v          = {3, ...}    ← 编译期常量
static constexpr inline auto _desc = TypesDesc<Base, std::string, i32, f64>::v;

// 类型 ID（编译期常量）
// TypeID_t<Player>::id = E_type::Player

// 成员类型元组（编译期类型信息）
struct M {
    using TypeList = std::tuple<std::string, i32, f64, E>;
    // E 是计数值，不参与实际成员
};
```

元数据提取器通过 `extractTypeMeta<Player>()` 读取这些编译期常量，用 `MetaData.hpp` 中定义的 `writeMetaFile()` 序列化为二进制。

## 如何构建和运行

### 构建元数据提取器

元数据提取器是一个独立的 C++ 程序，需要与你的项目一起编译。每次 C++ 类型定义变更后，需要重新编译它：

```bash
# 编译元数据提取器（示例，具体取决于你的构建系统）
cl /std:c++20 /EHsc meta-extractor.cpp /I../include /I../include/stream-punk
```

### 运行提取器

```bash
# 默认输出到 temp/stream-punk-meta.bin
./meta-extractor

# 或指定输出路径
./meta-extractor ./my-project/stream-punk-meta.bin
```

运行后会输出：
```
Metadata extracted: 5 types -> temp/stream-punk-meta.bin
```

### 完整工作流

```
1. 定义 C++ 类型（Data.hpp 中 struct + UseData）
2. 注册到 Xt_CustomType（customData.hpp）
3. 重新编译元数据提取器（因为它 #include 了你的类型头文件）
4. 运行元数据提取器 → 生成 temp/stream-punk-meta.bin
5. 运行 sp-gen（无需重编译）→ 读取 .bin → 生成目标语言代码
```

## 元数据提取器源码解读

```cpp
// meta-extractor.cpp 核心逻辑

int main(int argc, char** argv) {
    std::string outputPath = "temp/stream-punk-meta.bin";
    if (argc >= 2) outputPath = argv[1];

    // 初始化 StreamPunk：注册所有自定义类型的工厂函数
    INIT_StreamPunk();

    sp_meta::MetaFile meta;

    // 遍历 Xt_CustomType 中注册的所有类型，逐个提取元数据
    #define X_extract(type, name) meta.types.push_back(extractTypeMeta<name>());
    Xt_CustomType(X_extract);
    #undef X_extract

    // 写入二进制文件
    std::ofstream ofs(outputPath, std::ios::binary);
    sp_meta::writeMetaFile(ofs, meta);
}

// extractTypeMeta<T>() 读取编译期常量
template<typename T>
sp_meta::TypeMeta extractTypeMeta() {
    sp_meta::TypeMeta tm;
    tm.typeID    = TypeID_t<T>::id;       // 类型 ID
    tm.className = T::_className;          // 类名
    tm.baseName  = T::_baseName;           // 基类名

    // 从 TypeList 元组中提取每个成员的类型描述符
    using TupleType = typename T::M::TypeList;
    // 展开元组，为每个成员创建 MemberMeta
    // ...
    return tm;
}
```

## 二进制格式详解

### 文件头

| 偏移 | 大小 | 字段 | 说明 |
|------|------|------|------|
| 0 | 4 | Magic | `0x53504D44` = "SPMD" |
| 4 | 4 | Version | 当前为 1 |
| 8 | 4 | TypeCount | 类型数量 |

### 每个类型条目

| 偏移 | 大小 | 字段 | 说明 |
|------|------|------|------|
| 0 | 4 | typeID | E_type 枚举值 |
| 4 | 2+len | className | 2字节长度 + UTF-8 字符串 |
| .. | 2+len | baseName | 2字节长度 + UTF-8 字符串 |
| .. | 2 | memberCount | 成员数量 |

### 每个成员条目

| 偏移 | 大小 | 字段 | 说明 |
|------|------|------|------|
| 0 | 2+len | memberName | 2字节长度 + UTF-8 字符串 |
| .. | 2 | descLen | 类型描述符 SpToken 数量 |
| .. | 4*N | typeDesc[] | SpToken 数组，每个 4 字节 |

### SpToken（类型描述符）

SpToken 是 `uint32_t` 类型的枚举值，描述类型结构。例如：

| SpToken 值 | 含义 |
|-----------|------|
| `1` | i32 |
| `3` | f64 |
| `50` | std::string |
| `类型ID` | 自定义类型引用 |
| 特殊值 | 容器类型标记（vector/map/optional 等） |

完整的 SpToken 定义在 `StreamPunk.hpp` 的 `E_type` 枚举中。

## sp-gen 如何使用元数据

sp-gen 通过 `sp_meta::readMetaFile()` 读取 .bin 文件，重建 `MetaFile` 结构，然后传给各语言生成器。生成器根据 `className`、`baseName`、`members` 等信息生成目标语言代码。

元数据文件路径默认为 `temp/stream-punk-meta.bin`，可通过 `--meta` / `-m` 选项指定：

```bash
sp-gen -t py-meta -p ./data.py                          # 使用默认路径
sp-gen -t py-meta -p ./data.py -m ./custom/meta.bin     # 指定元数据文件
```

```
temp/stream-punk-meta.bin
    │
    ▼
sp_meta::readMetaFile("temp/stream-punk-meta.bin")
    │
    ▼
MetaFile { types: [TypeMeta { className: "Player", members: [...] }, ...] }
    │
    ├── generate_python_meta()  → 生成 Python 类型代码
    ├── generate_spoi_python()  → 生成 Python SPOI builder
    ├── generate_ts_meta()      → 生成 TypeScript 类型代码
    └── ...
```

## 注意事项

- 元数据提取器 **必须** `#include` 你的类型定义头文件，否则编译期常量不存在
- 元数据提取器需要编译为 **相同的 C++ 标准**（C++20 或更高）并启用 RTTI
- 修改 C++ 类型后，必须重新编译并运行提取器，再运行 sp-gen
- sp-gen 本身不需要重新编译，它只是读取 .bin 文件
- .bin 文件使用小端序，跨平台时注意端序
- 如果 Xt_CustomType 中注册了类型但未定义，元数据提取器编译会失败