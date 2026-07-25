---
name: "stream-punk-project-init"
description: "新建要使用 StreamPunk 的项目。当用户需要创建 StreamPunk 项目、示例、example、demo、游戏服务端、C++ 服务端+前端项目、React/Vue 前端+C++后端时必须调用。跨语言项目必须使用 sp-gen 生成客户端代码，禁止手写二进制解析。"
---

# StreamPunk 项目初始化

本技能指导如何从零开始创建一个使用 StreamPunk 库的项目——包括纯 C++ 项目和含非 C++ 客户端的跨语言项目。

## 红线（违反任一条 = 项目不合格）

1. **禁止客户端手写二进制解析。** 看到 `parseGameState`、`BinaryReader`、`DataView` + 手动偏移量、`getFloat64` + `offset += 8` —— 这些都是错的。正确做法：`sp-gen -t ts -p ./client/src/stream-punk-data.ts`，然后客户端 `import { GameState, I } from './stream-punk-data'`。

2. **禁止 Xt_CustomType 为空。** 每个 `UseData` 的类型必须注册。`customData.hpp` 中 `#define Xt_CustomType(X__) \ X__(::Tank, Tank) \ X__(::GameState, GameState)`。

3. **禁止在 customData.hpp 中手写 TypeDesc 特化。** TypeDesc 由 `UseData` 宏自动生成。`customData.hpp` 唯一职责是定义 `Xt_CustomType` 宏。

4. **禁止包含未使用的头文件。** 不需要 JSON 就别 include `StreamPunkJson.hpp`。不需要 SPOI 就别 include `StreamPunkSPOI.hpp`。

5. **禁止服务端手写 JSON 解析。** 看到 `json.find("\"key\"")`、手写 `findVal` lambda → 改用 `UseDataJson` + `fromJsonString`。

## 项目完成检查清单

在声明项目"完成"之前，**必须逐项核对**以下清单。任一未通过则项目不可交付。

- [ ] `customData.hpp`（或 `custom_type.hpp`）中 `Xt_CustomType` 非空？每个自定义类型都已注册？
- [ ] 所有自定义类型都继承 `Base` 且使用 `UseData(TypeName)`？
- [ ] 服务端代码中**没有**手写 `findVal` / `json.find("\"key\"")` 等 JSON 解析？
- [ ] 如果有非 C++ 客户端：是否运行了 `sp-gen` 生成客户端代码？
- [ ] 客户端代码中**没有**手写 `BinaryReader` / `parseXxx` / `serializeXxx` 等二进制解析？
- [ ] 没有包含未使用的头文件（如不需要 JSON 却 include 了 `StreamPunkJson.hpp`）？
- [ ] `INIT_StreamPunk()` 在 `main()` 中调用？
- [ ] 如果项目有非 C++ 客户端：运行时文件（`stream-punk.ts` 等）是否已从 `runtimes/<lang>/` 复制到客户端？

## 两种项目类型

| 类型 | 适用场景 | 需要哪些 Skill |
|------|---------|---------------|
| **纯 C++ 项目** | 只需序列化/JSON/深拷贝 | 本 Skill |
| **跨语言项目**（C++ 服务端 + 其他语言客户端） | 游戏服务端、实时协作工具、微服务通信 | 本 Skill + `stream-punk-cpp-types` + `stream-punk-meta-extract` + `stream-punk-sp-gen` + 对应语言 Skill |

> **重要**：跨语言项目必须走 sp-gen 代码生成流程，客户端代码由工具自动生成，**禁止手写二进制序列化/解析代码**。手写会导致字段变更时静默数据错乱，且失去 StreamPunk 最核心的跨语言优势。

## 能力选择：只加你实际使用的

StreamPunk 提供多种能力（序列化、JSON、ORM、SPOI 查询、SPOI Shadow 增量更新），但**不需要全部启用**。根据实际需求逐项选择：

| 你需要什么？ | 要加的宏 | 要 include 的头文件 | 要生成的 sp-gen 目标 |
|-------------|---------|-------------------|---------------------|
| 二进制序列化 | `UseData` | `StreamPunk.hpp` | `sp-gen -t <lang>` |
| JSON 解析/生成 | `UseDataJson` + `REGISTER_JSON_TYPE` | `StreamPunkJson.hpp` | 同上（含在数据类型中） |
| ORM 建表 | `UseDataOrm` | `StreamPunkOrmMeta.hpp` | 同上 |
| C++ 端 SPOI 查询（`sp::query()`） | `UseSPOI` | `StreamPunkSPOIRange.hpp` | — |
| C++ 端 SPOI Shadow 增量更新（`sp::spoi()`） | `UseSPOIShadow` | `StreamPunkSPOIShadow.hpp` | — |
| 客户端跨语言 SPOI 查询 | — | — | `sp-gen -t spoi-<lang>` |
| 客户端作为 SPOI 被查询方 | — | — | `sp-gen -t spoi-<lang>-exec` |

**决策原则：只加你会在代码中实际调用的。**
- 如果你不会写 `sp::spoi(obj, stream)` 或 `sp::query(players)`，就不要在类型上加 `UseSPOI` / `UseSPOIShadow`。
- 如果你不会在客户端 `import { SpoiQuery } from './spoi_builder'`，就不要生成 `spoi-*` 目标。
- 如果你的增量同步粒度是"整个对象"（每次新增一个对象就序列化整个对象发送），用普通二进制序列化就够了，不需要 SPOI。

> 反例：白板应用只需"每次新增一笔画就序列化这个 Stroke 对象发送"，这是对象级增量，不需要 SPOI。加了 `UseSPOI` + `UseSPOIShadow` + 生成 `spoi-*` 只会产生未使用的死代码。

## 纯 C++ 项目目录结构

```
my-project/
├── CMakeLists.txt          # CMake 构建配置
├── customData.hpp          # 类型注册表（仅供 Data.hpp 包含）
├── Data.hpp                # 类型定义（实际使用的头文件）
├── main.cpp                # 入口文件
├── include/                # 项目头文件
└── src/                    # 项目源文件
```

## 跨语言项目目录结构

> **文件位置约定**：
> - `Data.hpp` 和 `customData.hpp` 放在**项目根目录**（与 CMakeLists.txt 同级）
> - `server/` 中的代码通过 `#include "../Data.hpp"` 引用
> - **不要**在 `server/` 子目录中重复创建 `Data.hpp` 或包装文件
> - 客户端生成的代码放在 `client/src/` 中

```
my-project/
├── CMakeLists.txt                         # [手写] [强制] 构建配置
├── customData.hpp                         # [手写] [强制] 只定义 Xt_CustomType 宏，不定义任何 struct
├── Data.hpp                               # [手写] [强制] 类型定义 + UseData，include customData.hpp
├── meta_extractor.cpp                     # [手写] [强制] 元数据提取器（跨语言项目必需）
├── server/                                # C++ 服务端
│   ├── CMakeLists.txt
│   ├── main.cpp                           # [手写] 必须调用 INIT_StreamPunk()
│   ├── include/
│   └── src/
├── client/                                # 前端（React / Vue 等）
│   ├── src/
│   │   ├── stream-punk-data.js            # [自动生成] sp-gen 生成 ← 禁止手写！
│   │   ├── stream-punk.js                 # [自动复制] 从 runtimes/js/ 复制
│   │   └── spoi_builder.js                # [自动生成] sp-gen 生成 ← 仅 SPOI 时需要
│   └── package.json
├── scripts/
│   └── gen-client.sh                      # [手写] 一键脚本：提取元数据 → sp-gen → 复制运行时
└── temp/
    └── stream-punk-meta.bin               # [自动生成] 元数据提取器输出
```

**客户端代码生成流程**（每次修改 C++ 类型后执行）：

```
# === 必须执行（总是需要） ===
1. 重新编译并运行 meta_extractor.cpp → 生成 temp/stream-punk-meta.bin
2. sp-gen -t js -p ./client/src/stream-punk-data.js          # 生成 JS 类型
3. cp runtimes/js/stream-punk.js ./client/src/                # 复制运行时

# === 按需执行（仅在需要跨语言 SPOI 查询/更新时） ===
4. sp-gen -t spoi-js -p ./client/src/spoi_builder.js          # 生成 SPOI builder
5. sp-gen -t spoi-js-exec -p ./client/src/registry.js         # 生成 SPOI 执行器注册表
6. cp runtimes/js/spoi_executor.js ./client/src/              # 复制 SPOI 执行器运行时
```

> **重要约定**：库本身将「注册」和「定义」分离为两个文件：
> - `include/stream-punk/customData.hpp` — 只定义 `Xt_CustomType` 宏，不定义任何类型
> - `include/stream-punk/Data.hpp` — 包含 `StreamPunk.hpp`（进而包含 `customData.hpp`），然后定义所有 struct
>
> 项目应遵循同样的约定：`customData.hpp` 只做注册，`Data.hpp`（或 `GameData.hpp`）做类型定义并包含 `customData.hpp`。其他文件应包含 `Data.hpp` 而非 `customData.hpp`。

## 第一步：创建 CMakeLists.txt

依赖 vcpkg 管理第三方库（doctest 用于测试，cxxopts 可选）：

```cmake
cmake_minimum_required(VERSION 3.20)
project(my-project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 引入 StreamPunk 库 ----
# 方式一：将 StreamPunk 作为子目录引入
add_subdirectory(path/to/stream-punk)

# 方式二：StreamPunk 是 header-only，直接添加 include 路径
# target_include_directories(my-app PRIVATE path/to/stream-punk/include)

# ---- 主程序 ----
add_executable(my-app main.cpp)
target_link_libraries(my-app PRIVATE stream-punk)

# ---- 测试（可选） ----
find_package(doctest CONFIG REQUIRED)
add_executable(my-tests
    test/test_main.cpp
    test/test_my_types.cpp
)
target_link_libraries(my-tests PRIVATE stream-punk doctest::doctest)
```

## 第二步：创建类型注册表（customData.hpp）

此文件只做类型注册，不定义任何 struct。它被 `Data.hpp` 包含。

```cpp
#pragma once
// 类型注册表 —— 只定义 Xt_CustomType 宏，不做其他事
// 库的 include/stream-punk/customData.hpp 也是同样的模式

#define Xt_CustomType(X__) \
X__(Player, Player) \
X__(Enemy, Enemy)
```

## 第三步：定义自定义类型（Data.hpp）

所有需要序列化/反序列化的自定义类型集中定义在此文件中。它包含 `StreamPunk.hpp`（进而包含 `customData.hpp`），然后定义类型。

```cpp
#pragma once
#include <stream-punk/StreamPunk.hpp>
#include "customData.hpp"

// ==================== 类型定义 ====================

struct Player : public Base {
    #define Xt_Player(X__) \
    X__(std::string, name, "") \
    X__(i32, level, 1) \
    X__(f64, health, 100.0) \
    X__(std::vector<std::string>, items, std::vector<std::string>{})
    Player() = default;
    UseData(Player);
};

struct Enemy : public Base {
    #define Xt_Enemy(X__) \
    X__(std::string, type, "") \
    X__(i32, hp, 100) \
    X__(f64, speed, 1.0)
    Enemy() = default;
    UseData(Enemy);
};
```

> **注意**：其他文件应 `#include "Data.hpp"`（而非 `customData.hpp`）来使用类型。`INIT_StreamPunk()` 在 `main.cpp` 中调用，它会读取 `Xt_CustomType` 宏完成注册。

### 类型定义的约束

| 约束 | 说明 |
|------|------|
| 必须继承 `Base` | 所有自定义类型必须直接或间接继承 `Base` |
| 必须有默认构造函数 | 反序列化依赖默认构造 |
| 不支持 `private` 成员 | 默认所有成员为 public |
| 禁止 `std::string_view` | 可序列化但不可反序列化 |
| 禁止 `std::span` | 同上 |
| 不建议多继承 | 避免菱形继承 |
| 编译必须开启 RTTI | `/GR` (MSVC) 或默认 (GCC/Clang) |

## 第四步：编写入口文件（main.cpp）

```cpp
#include <iostream>
#include <sstream>
#include "Data.hpp"

int main() {
    // 必须初始化！注册所有自定义类型
    INIT_StreamPunk();

    // 序列化
    Player p;
    p.name = "Alice";
    p.level = 10;
    p.health = 95.5;

    std::stringstream ss;
    O o{ss};
    o << p;

    // 反序列化
    I i{ss};
    Player p2;
    i >> p2;

    std::cout << "Name: " << p2.name
              << ", Level: " << p2.level
              << ", Health: " << p2.health << std::endl;

    return 0;
}
```

## 第五步：构建并运行

```bash
# 配置（使用 vcpkg toolchain）
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake

# 构建
cmake --build build

# 运行
./build/Debug/my-app    # Windows
./build/my-app           # Linux/macOS
```

## 消息类型也应该用 UseData 定义

如果你的服务端需要解析客户端发来的 JSON 消息（如 `{"roomId": "123", "userName": "Alice"}`），**不要手写 JSON 解析器**。用 `UseData` + `UseDataJson` 定义消息类型：

```cpp
#include <stream-punk/StreamPunkJson.hpp>

struct JoinRoomMsg : public Base {
    #define Xt_JoinRoomMsg(X__) \
    X__(std::string, roomId, "") \
    X__(std::string, userName, "")
    UseData(JoinRoomMsg);
    UseDataJson(JoinRoomMsg);
};
REGISTER_JSON_TYPE(JoinRoomMsg);

// 解析收到的 JSON 字符串
JoinRoomMsg msg;
msg.fromJsonString(receivedJson);
std::cout << msg.roomId << ", " << msg.userName << std::endl;
```

> **反模式**：手写 `findVal`、`json.find("\"key\"")` 等 JSON 解析逻辑。这些代码脆弱且与 StreamPunk 的"类型定义一次，自动生成"的理念相悖。

## 进阶：添加 JSON 支持

在类型定义中加上 `UseDataJson` 和 `REGISTER_JSON_TYPE`：

```cpp
#include <stream-punk/StreamPunkJson.hpp>

struct Player : public Base {
    #define Xt_Player(X__) \
    X__(std::string, name, "") \
    X__(i32, level, 1)

    Player() = default;
    UseData(Player);
    UseDataJson(Player);
};
REGISTER_JSON_TYPE(Player);
```

使用：

```cpp
Player p;
p.name = "Alice";
p.level = 10;

// 序列化为 JSON
std::stringstream ss;
p.toJsonStream(ss);
std::cout << ss.str() << std::endl;  // {"name":"Alice","level":10}

// 从 JSON 反序列化
JsonStreamReader reader(R"({"name":"Bob","level":20})");
Player p2;
p2.fromJsonStream(reader);
```

## 进阶：添加 ORM 支持

```cpp
#include <stream-punk/StreamPunkOrmMeta.hpp>
#include <stream-punk/StreamPunkOrmGen.hpp>

struct Product : public Base {
    #define Xt_Product(X__) \
    X__(i64, id, 0, ORM_PRIMARY_KEY | ORM_AUTO_INCREMENT) \
    X__(std::string, name, "", ORM_NOT_NULL) \
    X__(f64, price, 0.0)

    Product() = default;
    UseData(Product);
    UseDataOrm(Product);
};
REGISTER_JSON_TYPE(Product);

// 使用
auto ddl = sp::createTable<Product>();
std::cout << ddl.sql << std::endl;
// CREATE TABLE Product (id BIGINT NOT NULL AUTO_INCREMENT, name VARCHAR(255) NOT NULL, price DOUBLE NOT NULL, PRIMARY KEY (id))
```

## 反模式（常见错误）

以下是在 StreamPunk 项目中**绝对不能做**的事情：

| 反模式 | 为什么错 | 正确做法 |
|--------|---------|---------|
| **客户端手写二进制解析器**（如 JS 端手写 `parseStroke`、`serializeStroke`） | 字段顺序与 C++ `UseData` 宏展开的序列化顺序强耦合；字段变更时静默失败，无编译期检查 | 用 `sp-gen` 生成客户端代码，`sp-gen -t js -p ./data.js` |
| **服务端手写 JSON 解析器**（如 `json.find("\"key\"")`、手写 `findVal` lambda） | 脆弱、易出错、字段变更时静默失败 | 用 `UseData` + `UseDataJson` 定义消息类型，`msg.fromJsonString(json)` |
| **SPOI Shadow 全量赋值**（`shadow.strokes = fullVector`） | 生成 `e_set` 指令，operand 是整个 vector 的全量序列化数据，比直接发全量更浪费（多了 SPOI 指令元数据开销） | 用 `shadow.strokes.append(newItem)` 做增量追加，用 `shadow.strokes.remove(idx)` 做删除 |
| **忘记复制运行时文件** | 生成的代码依赖 `O`/`I` 等运行时类，缺少运行时文件会导致 `import` 失败 | 从 `runtimes/<lang>/` 复制运行时文件到客户端项目 |
| **跳过元数据提取步骤** | sp-gen 需要 `temp/stream-punk-meta.bin` 才能生成代码 | 先写 `meta_extractor.cpp`，编译运行后再跑 sp-gen |
| **`customData.hpp` 中定义类型** | 破坏注册/定义分离约定，导致循环依赖 | `customData.hpp` 只做 `Xt_CustomType` 注册，类型定义放在 `Data.hpp` |
| **声明了 UseSPOI/UseSPOIShadow 但未使用** | 产生死代码：include 了头文件、注册了宏，但实际代码中从未调用 `sp::spoi()` 或 `sp::query()` | 对照能力选择表，只加实际需要的宏 |

## 跨语言代码生成（sp-gen）快速参考

详见 [stream-punk-sp-gen](skills/stream-punk-sp-gen) 技能。完整流程：

```bash
# 1. 编译并运行元数据提取器 → 生成 temp/stream-punk-meta.bin

# 2. 必须生成：目标语言数据类型
sp-gen -t js -p ./client/src/stream-punk-data.js          # JavaScript
sp-gen -t ts -p ./frontend/src/data.ts                     # TypeScript
sp-gen -t py-meta -p ./client/data.py                      # Python
sp-gen -t java-meta -p ./client/Data.java                  # Java
sp-gen -t go-meta -p ./client/data.go                      # Go
sp-gen -t rust-meta -p ./client/src/data.rs                # Rust
sp-gen -t kotlin-meta -p ./client/Data.kt                  # Kotlin

# 3. 必须复制：运行时文件
cp runtimes/js/stream-punk.js ./client/src/
cp runtimes/ts/stream-punk.ts ./frontend/src/

# 4. 按需生成：SPOI builder（仅在需要跨语言 SPOI 查询/更新时）
sp-gen -t spoi-js -p ./client/src/spoi_builder.js
sp-gen -t spoi-ts -p ./frontend/src/spoi_builder.ts
```

## 常见问题

**Q: 编译报错 "无法解析的外部符号"？**
A: 检查是否调用了 `INIT_StreamPunk()`，以及所有自定义类型是否在 `Xt_CustomType` 中注册。

**Q: 序列化结果不对？**
A: 确保 `O`/`I` 流在每次操作后调用 `clear()`，否则指针去重表会残留。

**Q: 如何按模块拆分 customData.hpp？**
A: 各模块定义独立的 X 宏表（如 `Xt_CustomType_Game`），在 `customData.hpp` 中拼接。类型定义同样可以在 `Data.hpp` 中按模块拆分。详见 [stream-punk-cpp-types](skills/stream-punk-cpp-types) 技能。

**Q: `customData.hpp` 和 `Data.hpp` 的区别？**
A: `customData.hpp` 只做注册（定义 `Xt_CustomType` 宏），不包含任何 struct 定义。`Data.hpp` 包含 `StreamPunk.hpp`（进而包含 `customData.hpp`），然后定义所有 struct 类型。项目代码应 `#include "Data.hpp"` 而非 `customData.hpp`。这个约定来自库本身的文件结构。

**Q: 跨平台兼容性？**
A: StreamPunk 是 header-only 库，依赖 vcpkg 管理第三方库。只要目标平台有 C++20 编译器并开启 RTTI，即可编译运行。

## 常见错误代码（错误 vs 正确）

### 错误 1：customData.hpp 中手写 TypeDesc 特化

```cpp
// ❌ 错误：customData.hpp 中手写 TypeDesc 特化
// 这会把 UseData 自动生成的正确 TypeDesc 覆盖掉！
struct Tank;
struct GameState;
namespace sp {
template<> struct TypeDesc<Tank> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};
template<> struct TypeDesc<GameState> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};
}
```

```cpp
// ✅ 正确：customData.hpp 只定义 Xt_CustomType，TypeDesc 由 UseData 宏自动生成
#pragma once
#define Xt_CustomType(X__) \
X__(::Tank,      Tank)      \
X__(::GameState, GameState)
```

### 错误 2：客户端手写二进制解析

```typescript
// ❌ 错误：手写 DataView 偏移量解析 —— 字段变更时静默损坏
function parseGameState(data: Uint8Array): GameState {
    const view = new DataView(data.buffer)
    let offset = 0
    const tankCount = view.getUint32(offset, true); offset += 4
    for (let i = 0; i < tankCount; i++) {
        const x = view.getFloat64(offset, true); offset += 8
        const y = view.getFloat64(offset, true); offset += 8
        // ... 几十行手动偏移量
    }
}
```

```typescript
// ✅ 正确：sp-gen 生成代码，零手写偏移量
import { GameState, I } from './stream-punk-data'
const state = new GameState().from(new I(data.buffer))
```

### 错误 3：服务端手写 JSON 解析

```cpp
// ❌ 错误：手写 findVal lambda 解析 JSON —— 脆弱、易出错
auto findVal = [&](const std::string& key) -> std::string {
    auto pos = json.find("\"" + key + "\"");
    // ... 几十行脆弱的手写解析
};
std::string type = findVal("type");
std::string roomId = findVal("roomId");
```

```cpp
// ✅ 正确：用 UseData + UseDataJson 定义消息类型
struct JoinRoomMsg : public Base {
    #define Xt_JoinRoomMsg(X__) \
    X__(std::string, type, "") \
    X__(std::string, roomId, "") \
    X__(std::string, playerName, "")
    UseData(JoinRoomMsg);
    UseDataJson(JoinRoomMsg);
};
REGISTER_JSON_TYPE(JoinRoomMsg);

// 一行解析
JoinRoomMsg msg;
msg.fromJsonString(json);
std::cout << msg.type << ", " << msg.roomId << std::endl;
```

### 错误 4：包含未使用的头文件

```cpp
// ❌ 错误：不需要 JSON 和 SPOI 却 include 了
#include <stream-punk/StreamPunk.hpp>
#include <stream-punk/StreamPunkJson.hpp>     // 从未使用
#include <stream-punk/StreamPunkSPOI.hpp>     // 从未使用
#include <stream-punk/StreamPunkSPOIRange.hpp> // 从未使用
```

```cpp
// ✅ 正确：只 include 实际使用的
#include <stream-punk/StreamPunk.hpp>  // 二进制序列化，够用了
```

### 错误 5：Xt_CustomType 为空导致类型未注册

```cpp
// ❌ 错误：定义了 UseData(Tank) 但 customData.hpp 中 Xt_CustomType 为空
// TankData.hpp:
struct Tank : public Base {
    #define Xt_Tank(X__) X__(f64, x, 0.0) X__(f64, y, 0.0)
    UseData(Tank);
};
// customData.hpp 不存在或 Xt_CustomType 为空
// → INIT_StreamPunk() 不会注册 Tank
// → 反序列化时 create_custom_type_from_typeID() 抛异常
```

```cpp
// ✅ 正确：customData.hpp 非空
#pragma once
#define Xt_CustomType(X__) \
X__(::Tank, Tank) \
X__(::Bullet, Bullet)
```