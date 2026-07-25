---
name: "stream-punk-sp-gen"
description: "使用 sp-gen 生成跨语言代码（TypeScript/Python/Java/Go/Rust/Kotlin/JS）。当用户需要生成其他语言的数据类型代码、SPOI 查询 builder、或运行代码生成器时调用。"
---

## 红线（违反任一条 = 项目不合格）

1. **禁止客户端手写二进制解析。** 看到 `parseGameState`、`BinaryReader`、`DataView` + 手动偏移量、`getFloat64` + `offset += 8` —— 这些都是错的。正确做法只有一条命令：
   ```
   sp-gen -t ts -p ./client/src/stream-punk-data.ts
   ```
   然后客户端 `import { GameState, I } from './stream-punk-data'`，用 `new GameState().from(new I(data.buffer))` 解析。

2. **sp-gen 生成的文件必须提交到项目中。** 不要只在构建脚本中运行 sp-gen。客户端代码中必须存在 `stream-punk-data.ts`（或 `.js`、`.py` 等）文件。

3. **sp-gen 不是可选的。** 只要项目有非 C++ 客户端，sp-gen 就是强制步骤。跳过 sp-gen = 客户端无法正确解析数据 = 项目不合格。

---

# StreamPunk sp-gen 代码生成器

sp-gen 是 StreamPunk 的统一跨语言代码生成器，从 C++ 类型定义生成其他语言的等价代码。

> **前置条件**：运行 sp-gen 之前，必须先完成：
> 1. C++ 类型已用 `UseData` 宏定义（见 [stream-punk-cpp-types](skills/stream-punk-cpp-types)）
> 2. 元数据提取器已运行，生成了 `temp/stream-punk-meta.bin`（见 [stream-punk-meta-extract](skills/stream-punk-meta-extract)）
> 3. 运行时文件将从 `runtimes/<lang>/` 复制到目标项目

## 命令格式

```bash
sp-gen --target <目标> --path <输出路径> [--meta <元数据文件路径>]
# 或简写
sp-gen -t <目标> -p <输出路径> [-m <元数据文件路径>]
```

`--meta` / `-m` 指定元数据文件路径，默认值为 `temp/stream-punk-meta.bin`。仅对 `-meta` 和 `spoi-*` 目标生效。

## 两类目标

### 数据类型生成（将 C++ 类型翻译为目标语言）

| target | 语言 | 模式 |
|--------|------|------|
| `ts` | TypeScript | 直接生成（v2，默认） |
| `ts-meta` | TypeScript | 从二进制元数据生成 |
| `py` | Python | 直接生成 |
| `py-meta` | Python | 从二进制元数据生成 |
| `java` | Java | 直接生成 |
| `java-meta` | Java | 从二进制元数据生成 |
| `go-meta` | Go | 从二进制元数据生成 |
| `rust-meta` | Rust | 从二进制元数据生成 |
| `kotlin-meta` | Kotlin | 从二进制元数据生成 |

### SPOI 查询生成（跨语言 SPOI builder）

| target | 语言 |
|--------|------|
| `spoi-ts` | TypeScript |
| `spoi-py` | Python |
| `spoi-java` | Java |
| `spoi-go` | Go |
| `spoi-rust` | Rust |
| `spoi-kotlin` | Kotlin |
| `spoi-js` | JavaScript |

## 使用示例

```bash
# 生成 TypeScript 数据类型
sp-gen -t ts -p ./frontend/src/stream-punk-data.ts

# 从元数据生成 Python 数据类型（使用默认元数据文件）
sp-gen -t py-meta -p ./client/stream_punk_data.py

# 从指定元数据文件生成 Python 数据类型
sp-gen -t py-meta -p ./client/stream_punk_data.py -m ./my-project/meta.bin

# 生成 Rust 的 SPOI 查询 builder
sp-gen -t spoi-rust -p ./rust-client/src/spoi_builder.rs
```

## 二进制元数据生成流程

所有 `-meta` 和 `spoi-*` 模式依赖 `temp/stream-punk-meta.bin` 文件。该文件由**元数据提取器**（一个独立的 C++ 程序）在编译期生成——详见 [stream-punk-meta-extract](skills/stream-punk-meta-extract) 技能。

**关键：sp-gen 是预编译的二进制，运行时读取 .bin 文件，不会随类型变更而重编译。** 需要重编译的只有「元数据提取器」——它 `#include` 了你的类型定义头文件，利用编译期常量提取信息。

```
1. 在 C++ 端用 UseData 宏定义类型（在 customData.hpp 中注册）
2. 重新编译「元数据提取器」程序（因为它 #include 了你的类型头文件）
   运行提取器 → 生成 temp/stream-punk-meta.bin
3. 运行 sp-gen（无需重编译）读取 .bin 文件 → 生成目标语言代码
```

**为什么能保证正确性：** 元数据提取器是一个正常的 C++ 程序，它 `#include` 类型定义头文件，利用编译期自动生成的 `_className`、`TypeList`、`TypeDesc::v` 等常量来提取信息。整个过程由 C++ 编译器保证——编译通过，元数据就正确。

**sp-gen 读取元数据的方式：** 所有 `-meta` 和 `spoi-*` 生成器函数默认从 `temp/stream-punk-meta.bin` 读取（函数签名为 `generate_xxx(output_path, meta_path = "temp/stream-punk-meta.bin")`）。sp-gen 编译一次即可，后续类型变更只需更新 .bin 文件，然后重新运行 sp-gen 即可。

## 生成后的集成步骤

### TypeScript
```bash
sp-gen -t ts -p ./frontend/src/stream-punk-data.ts
# 复制运行时文件
cp runtimes/ts/stream-punk.ts ./frontend/src/
```

### Python
```bash
sp-gen -t py-meta -p ./client/stream_punk_data.py
# 复制运行时文件
cp runtimes/py/stream_punk.py ./client/
```

### 其他语言同理
运行时文件位于 `runtimes/` 目录下，需要手动复制到目标项目中。

## 注意事项

- 修改 C++ 类型定义后，需要重新运行元数据提取器和 sp-gen
- 预生成模式（`-meta`）享受编译期类型安全，改了 C++ 类型后客户端编译会报错提醒
- 动态 Schema 模式适合类型频繁变化的场景，但类型安全是运行时的

## 典型集成工作流

下面以「C++ 服务端 + Python 客户端」为例，展示完整的跨语言集成流程。

### 第一步：定义 C++ 类型并生成元数据

```cpp
// customData.hpp
struct Player : public Base {
    #define Xt_Player(X__) \
    X__(std::string, name, "") \
    X__(i32, level, 1) \
    X__(f64, health, 100.0)
    UseData(Player);
    // 注意：只有需要跨语言 SPOI 查询/更新时才加 UseSPOI
};
```

```bash
# 编译并运行元数据提取器 → 生成 temp/stream-punk-meta.bin
```

### 第二步：生成目标语言代码

**必须生成（总是需要）：**

```bash
# 生成 Python 数据类型（用于序列化/反序列化）
sp-gen -t py-meta -p ./client/stream_punk_data.py

# 如果元数据文件不在默认位置，使用 -m 指定
sp-gen -t py-meta -p ./client/stream_punk_data.py -m ./custom/meta.bin

# 复制运行时文件
cp runtimes/py/stream-punk.py ./client/
```

**按需生成（仅在需要跨语言 SPOI 查询/更新时）：**

```bash
# 生成 Python SPOI 查询/更新 builder
sp-gen -t spoi-py -p ./client/spoi_builder.py

# 生成 Python SPOI 执行器注册表（让 Python 端作为被查询方）
sp-gen -t spoi-py-exec -p ./client/stream_punk_registry.py
cp runtimes/py/spoi_executor.py ./client/
```

> **如果你只做二进制序列化（路径 A+B），跳过 spoi-* 和 spoi-*-exec 目标。**

### 第三步：在目标语言项目中使用

**读取 C++ 发来的二进制数据：**

```python
from stream_punk_data import Player, I
msg = Player().from_stream(I(received_bytes))
print(msg.name, msg.level)
```

**构建 SPOI 查询发送给 C++ 服务端：**

```python
from spoi_builder import SpoiQuery, Cmp, SpoiTestPlayer as P

query = SpoiQuery("Player") \
    .filter_i32(P.level, Cmp.GT, 10) \
    .sort(P.score, ascending=False) \
    .take(5) \
    .build()  # bytes → 通过 TCP 发送给 C++ 服务端
```

**构建 SPOI 更新发送给 C++ 服务端：**

```python
from spoi_builder import SpoiUpdate

update = SpoiUpdate() \
    .set_str([P.name], "Alice") \
    .add_i32([P.level], 5) \
    .build()  # bytes → 通过 TCP 发送给 C++ 服务端
```

### 运行时文件总览

| 运行时文件 | 用途 | 生成方式 |
|-----------|------|---------|
| `stream-punk.{ts,js,py,java,go,rs,kt}` | 基础序列化/反序列化（O/I 流） | 手动复制 `runtimes/<lang>/` |
| `spoi_builder.{ts,js,py,java,go,rs,kt}` | SPOI 查询/更新指令构建器 | `sp-gen -t spoi-<lang>` |
| `spoi_executor.{ts,js,py}` | SPOI 指令执行器（作为被查询方） | 手动复制 `runtimes/<lang>/` |
| `spoi_exec_registry.{ts,js,py}` | 执行器类型注册表 | `sp-gen -t spoi-<lang>-exec` |
| 数据类型文件 | 各语言的类型定义 | `sp-gen -t <lang>(-meta)` |

### 修改 C++ 类型后的更新流程

```
1. 修改 C++ 类型定义（customData.hpp）
2. 重新编译并运行元数据提取器 → 更新 temp/stream-punk-meta.bin
3. 必须重新运行：数据类型生成
   - sp-gen -t <lang>-meta -p ...     （更新数据类型）
4. 按需重新运行：SPOI 相关（仅在使用时）
   - sp-gen -t spoi-<lang> -p ...     （更新 SPOI builder）
   - sp-gen -t spoi-<lang>-exec -p ...（更新执行器注册表）
   - 使用 -m <path> 可指定非默认路径的元数据文件
5. 目标语言重新编译（如有编译步骤）
```

## 编写包含客户端的示例/例程

当编写一个自包含的示例项目（如 C++ 服务端 + JS 客户端）时，**绝对禁止在客户端手写二进制解析代码作为"简化"**。这会误导示例读者认为手写是可接受的模式。

### 策略 A：预生成客户端代码（推荐用于示例）

1. 完成 C++ 类型定义后，运行完整的 sp-gen 流水线
2. 将生成的客户端代码**提交到示例目录**中
3. 在示例的注释中说明：`// 此文件由 sp-gen 自动生成，修改 C++ 类型后需重新生成`

```
example/
├── server/
│   └── WhiteboardData.hpp          # C++ 类型定义（手写）
├── client/
│   ├── src/
│   │   ├── stream-punk-data.js     # sp-gen 生成
│   │   ├── stream-punk-registry.js # sp-gen 生成
│   │   ├── spoi_executor.js        # 运行时（从 runtimes/js/ 复制）
│   │   └── app.js                  # 业务逻辑（手写，import 生成文件）
```

### 策略 B：在构建脚本中集成 sp-gen

在 CMakeLists.txt 或构建脚本中添加 sp-gen 调用步骤，使客户端代码跟随 C++ 类型自动更新。

### 绝对禁止

**不要在示例中手写二进制解析代码作为"简化"**。这会：
- 误导示例读者认为手写解析是可接受的模式
- 当 C++ 类型变更时，示例会悄悄损坏（手写代码不会报错）
- 使 StreamPunk 的跨语言代码生成能力在示例中完全不可见

**正确做法**：示例的客户端代码中，二进制解析相关的文件全部由 sp-gen 生成，只有业务逻辑是手写的。