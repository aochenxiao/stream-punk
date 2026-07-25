---
name: "stream-punk-cpp-types"
description: "在 StreamPunk 项目中定义 C++ 可序列化类型。当用户需要新建 StreamPunk 数据类型、添加 UseData 宏、注册自定义类型、或定义跨语言数据结构时调用。定义完类型后，跨语言项目必须继续走 sp-gen 流程生成客户端代码。"
---

## 红线（违反任一条 = 项目不合格）

1. **每个 UseData 类型必须注册到 Xt_CustomType。** `UseData(Tank)` 写了但 `Xt_CustomType` 中没有 `X__(::Tank, Tank)` → 类型系统不认这个类型 → 反序列化崩溃。两者缺一不可。

2. **禁止在 customData.hpp 中手写 TypeDesc 特化。** TypeDesc 由 `UseData` 宏自动生成，不需要你手动写。手写的 TypeDesc 特化会覆盖自动生成的正确版本，导致类型信息丢失。`customData.hpp` 的唯一职责是定义 `Xt_CustomType` 宏，仅此而已。

3. **禁止包含未使用的头文件。** 不需要 JSON 就别 `#include <stream-punk/StreamPunkJson.hpp>`。不需要 SPOI 就别 `#include <stream-punk/StreamPunkSPOI.hpp>`。对照能力选择表，只加实际需要的。

---

# StreamPunk C++ 类型定义

本技能指导如何在 StreamPunk 项目中正确定义 C++ 可序列化类型。

## 类型定义流程

### 1. 定义类型（使用 X 宏 + UseData）

```cpp
#include <stream-punk/StreamPunk.hpp>

struct Player : public Base {
    // 定义成员列表（类型, 成员名, 默认值）
    #define Xt_Player(X__) \
    X__(std::string, name, "") \
    X__(i32, level, 1) \
    X__(f64, health, 100.0) \
    X__(std::vector<std::string>, items, std::vector<std::string>{})

    UseData(Player);  // 自动生成序列化/反序列化代码
};
```

### 2. 注册类型到全局注册表

在 `customData.hpp` 中，将新类型名添加到 `Xt_CustomType` 宏中：

```cpp
#define Xt_CustomType(X__) \
X__(customType, Player) \
X__(customType, Enemy) \
X__(customType, YourNewType)  // 添加新类型
```

### 3. 如需 JSON 支持

在类型定义中加上 `UseDataJson`：

```cpp
struct Player : public Base {
    #define Xt_Player(X__) /* ... */
    UseData(Player);
    UseDataJson(Player);  // 添加 JSON 支持
};
REGISTER_JSON_TYPE(Player);  // 对于多态类型，注册 JSON 工厂
```

### 4. 如需 ORM 支持

```cpp
#include <stream-punk/StreamPunkOrmMeta.hpp>

struct Product : public Base {
    #define Xt_Product(X__) \
    X__(std::string, name, "") \
    X__(f64, price, 0.0)

    UseData(Product);
    UseDataOrm(Product);  // 添加 ORM 支持
};
```

### 5. 如需 SPOI 查询支持

```cpp
#include <stream-punk/StreamPunkSPOIRange.hpp>

struct Player : public Base {
    #define Xt_Player(X__) /* ... */
    UseData(Player);
    UseSPOI(Player);  // 在 struct 内部调用，生成 SPOI 查询代理
};
```

### 6. 如需 SPOI Shadow 增量更新支持

```cpp
#include <stream-punk/StreamPunkSPOIShadow.hpp>

struct Player : public Base {
    #define Xt_Player(X__) /* ... */
    UseData(Player);
    UseSPOIShadow(Player);  // 同样在 struct 内部调用，生成 Shadow 代理
};
```

> **重要：`UseSPOI` 和 `UseSPOIShadow` 的调用位置**——都在 struct 定义内部调用，与 `UseData` 同级。不要在 `namespace sp {}` 或 struct 外部调用。它们的正确位置如下：
>
> ```cpp
> struct Foo : public Base {
>     #define Xt_Foo(X__) /* ... */
>     UseData(Foo);         // ✅ 在 struct 内部
>     UseSPOI(Foo);         // ✅ 在 struct 内部
>     UseSPOIShadow(Foo);   // ✅ 在 struct 内部
> };
> // ❌ 不要在 struct 外部调用：namespace sp { UseSPOI(::Foo, Xt_Foo); }
> ```

### 最小化原则

**不要一次性加所有宏。** 只加你实际需要的：

- 只做序列化 → `UseData` 就够了
- 需要 JSON → 加 `UseDataJson` + `REGISTER_JSON_TYPE`
- 需要 ORM → 加 `UseDataOrm`
- 需要 SPOI 查询 → 加 `UseSPOI`
- 需要 SPOI 增量更新 → 加 `UseSPOIShadow`

声明了但未使用的宏会产生死代码（多余的 include、未使用的生成代码），且会误导后续维护者。

## 重要约束

- **必须继承 `Base`**：所有自定义类型必须直接或间接继承 `Base`
- **不支持 `private` 成员**：默认所有成员为 public；若使用 private，需声明友元
- **禁止的成员类型**：`std::string_view`、`std::span`（可序列化但不可反序列化）
- **不支持宽字节**：跨语言推荐 `std::u8string` 或 `std::u16string`
- **不建议多继承**：避免菱形继承问题
- **必须初始化**：程序启动时调用 `INIT_StreamPunk()`
- **必须开启 RTTI**：编译选项必须包含 RTTI

## 指针使用规范

| 指针类型 | 用途 | 说明 |
|---------|------|------|
| `Sptr<T>` + `Wptr<T>` | 共享所有权 + 弱引用 | 推荐用于类内引用，避免循环 |
| `Uptr<T>` + 裸指针 | 独占所有权 + 引用 | 推荐用于独占场景 |
| 裸指针 | 堆分配对象 | 反序列化自动从堆分配 |
| `void*` | 禁止 | 不能用于首次序列化，无类型信息 |

- 不要使用 `T&` 引用作为成员
- `char*` 被当作单个 `char`，而非 C 字符串
- 原始指针反序列化后指向新对象，原引用关系会断裂

## 深拷贝（DeepCopier）与 ABA 问题

StreamPunk 使用对象地址追踪身份，避免重复拷贝。ABA 问题指同一地址被不同对象复用时导致身份混淆。

**防范：**
- 深拷贝完成后必须调用 `dc.clear()`
- 不要复用同一个 `DeepCopier` 实例
- 序列化流 `O`/`I` 完成操作后也应调用 `clear()`

```cpp
DeepCopier dc;
Player p2;
deepCopy(dc, p2, p1);
dc.clear();  // 必须手动清理！
```

## 多模块类型注册（customData.hpp 分离方案）

当项目规模增大，将所有自定义类型集中在一个 `customData.hpp` 中维护会变得困难。以下方案支持按模块拆分类型定义，最终在 `customData.hpp` 中集中拼接。

### 步骤 1：各模块独立定义 X 宏

```cpp
// game_types.hpp（游戏核心类型）
#pragma once
#define Xt_CustomType_Game(X__) \
X__(Player, Player) \
X__(Item, Item) \
X__(Enemy, Enemy)

// ui_types.hpp（UI 类型）
#pragma once
#define Xt_CustomType_UI(X__) \
X__(Window, Window) \
X__(Button, Button) \
X__(Label, Label)
```

### 步骤 2：在 customData.hpp 中拼接

```cpp
// customData.hpp
#pragma once
#include "game_types.hpp"
#include "ui_types.hpp"

#define Xt_CustomType(X__) \
Xt_CustomType_Game(X__) \
Xt_CustomType_UI(X__)
```

### 注意事项

- 各模块的 X 宏表名称必须全局唯一，避免冲突
- 新增模块时，只需在 `customData.hpp` 中添加一行拼接即可
- 所有模块的类型最终仍然共享同一个 `E_type` 枚举空间，由 `StreamPunk.hpp` 中的 `Xt_Type` 宏统一展开
- 如果某个模块的类型被移除，记得同时从 `customData.hpp` 的拼接中删除，否则会编译报错

## 反模式：在客户端手写二进制解析

在 StreamPunk 跨语言项目中，**绝对禁止**在客户端手写以下类型的代码：

```javascript
// ❌ 禁止：手写二进制解析
function parseStroke(data) { ... }        // 手写字段解析
function serializeStroke(stroke) { ... }  // 手写二进制序列化
function parseSpoiDelta(payload) { ... }  // 手写 SPOI 指令解析
```

原因：
1. StreamPunk 的 `sp-gen` 工具会自动生成这些代码，且与 C++ 端格式严格一致
2. 手写代码在 C++ 类型字段变更时**不会报错**，但会**静默产生数据错乱**
3. SPOI 协议内部格式可能随版本演进，手写解析器会悄悄失效
4. 手写代码浪费了 StreamPunk 最核心的跨语言代码生成能力

**正确做法**：走 sp-gen 流水线（见下文"跨语言项目：强制后续步骤"）。

## 跨语言项目：强制后续步骤

> **这不是可选的。** 如果你有非 C++ 客户端（如 React 前端），以下步骤**必须全部完成**。跳过任何一步，客户端将无法正确解析数据，且字段变更时不会报错、会静默产生数据错乱。

1. **编写元数据提取器** → 见 [stream-punk-meta-extract](skills/stream-punk-meta-extract) Skill
2. **运行 sp-gen 生成客户端代码** → 见 [stream-punk-sp-gen](skills/stream-punk-sp-gen) Skill
3. **复制运行时文件** → 从 `runtimes/<lang>/` 复制到客户端项目
4. **在客户端 import 使用** → 见对应语言 Skill（[stream-punk-js](skills/stream-punk-js) / [stream-punk-ts](skills/stream-punk-ts) / [stream-punk-py](skills/stream-punk-py) 等）

> **红线**：在客户端手写 `BinaryReader`、`parseXxx`、`serializeXxx` 等二进制解析代码是**绝对禁止**的。这些代码必须由 sp-gen 自动生成。手写代码在 C++ 类型字段变更时不会报错，但会静默产生数据错乱——这正是 StreamPunk 设计 sp-gen 要防止的问题。

## 反模式：SPOI Shadow 全量替换

使用 SPOI Shadow 进行增量更新时，常见错误是将整个容器赋值给 shadow：

```cpp
// ❌ 错误：生成 e_set 全量替换指令，不是增量！
auto shadow = sp::spoi(room.state, deltaStream);
shadow.strokes = room.state.strokes;  // 整个 vector 被序列化到 operand 中

// ✅ 正确：用 append 追加单个元素，生成 e_append 增量指令
auto shadow = sp::spoi(room.state, deltaStream);
shadow.strokes.append(newStroke);  // 只传一个笔画
```

`SPOIShadowField::operator=` 生成的是 `e_set` 指令，operand 是字段的完整序列化值。对容器类型应使用 `SPOIShadowField::append()` / `remove(idx)` 做增量操作。详见 [stream-punk-spoi](skills/stream-punk-spoi) Skill。

## 常见错误代码（错误 vs 正确）

### 错误 1：customData.hpp 中手写 TypeDesc 特化

```cpp
// ❌ 错误：customData.hpp 中手写 TypeDesc 特化
// 这会把 UseData 自动生成的正确 TypeDesc 覆盖掉！
template<> struct TypeDesc<Tank> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};
template<> struct TypeDesc<Bullet> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};
```

```cpp
// ✅ 正确：customData.hpp 只定义 Xt_CustomType，TypeDesc 由 UseData 宏自动生成
#define Xt_CustomType(X__) \
X__(::Tank,      Tank)      \
X__(::Bullet,    Bullet)    \
X__(::GameState, GameState)
```

### 错误 2：Xt_CustomType 为空或缺失

```cpp
// ❌ 错误：定义了 UseData 但没注册到 Xt_CustomType
// TankData.hpp:
struct Tank : public Base {
    #define Xt_Tank(X__) X__(f64, x, 0.0) X__(f64, y, 0.0)
    UseData(Tank);
};
// customData.hpp 不存在或 Xt_CustomType 为空 → INIT_StreamPunk() 不会注册 Tank
```

```cpp
// ✅ 正确：类型定义 + 注册缺一不可
// customData.hpp（项目根目录，只做注册）:
#pragma once
#define Xt_CustomType(X__) \
X__(::Tank, Tank) \
X__(::Bullet, Bullet)

// Data.hpp（项目根目录，类型定义）:
#pragma once
#include <stream-punk/StreamPunk.hpp>
#include "customData.hpp"

struct Tank : public Base {
    #define Xt_Tank(X__) X__(f64, x, 0.0) X__(f64, y, 0.0)
    UseData(Tank);
};
```

### 错误 3：include 顺序导致 Xt_CustomType 不生效

```cpp
// ❌ 错误：先 include StreamPunk.hpp，后定义 Xt_CustomType
// StreamPunk.hpp 内部有 #ifndef Xt_CustomType → #define Xt_CustomType(X__) /* 空 */
// 此时 Xt_CustomType 已经被定义为空，后面再定义无效
#include <stream-punk/StreamPunk.hpp>
#define Xt_CustomType(X__) X__(::Tank, Tank)  // 太晚了！
```

```cpp
// ✅ 正确：先定义 Xt_CustomType，再 include StreamPunk.hpp
// 用 customData.hpp 做中间层：
// 
// customData.hpp:
#pragma once
#define Xt_CustomType(X__) \
X__(::Tank, Tank)

// Data.hpp:
#pragma once
#include <stream-punk/StreamPunk.hpp>  // 此时 Xt_CustomType 已定义，会被正确使用
#include "customData.hpp"
```

### 错误 4：客户端手写二进制解析

```typescript
// ❌ 错误：手写二进制解析 —— 字段顺序与 C++ 强耦合，字段变更时静默损坏
function parseGameState(data: Uint8Array): GameState {
    const view = new DataView(data.buffer, data.byteOffset, data.byteLength)
    let offset = 0
    const tankCount = view.getUint32(offset, true); offset += 4
    const tanks: Tank[] = []
    for (let i = 0; i < tankCount; i++) {
        const x = view.getFloat64(offset, true); offset += 8
        const y = view.getFloat64(offset, true); offset += 8
        // ... 几十行手动偏移量，极易出错
    }
    // ...
}
```

```typescript
// ✅ 正确：使用 sp-gen 生成的代码
import { GameState, I } from './stream-punk-data'

// 解析 C++ 发来的二进制数据
const state = new GameState().from(new I(data.buffer))
console.log(state.tanks[0].x, state.tanks[0].y)
```

### 错误 5：包含未使用的头文件

```cpp
// ❌ 错误：不需要 JSON 和 SPOI 却 include 了相关头文件
#include <stream-punk/StreamPunk.hpp>
#include <stream-punk/StreamPunkJson.hpp>     // 从未使用 UseDataJson / toJson / fromJson
#include <stream-punk/StreamPunkSPOI.hpp>     // 从未使用 sp::query() / sp::spoi()
#include <stream-punk/StreamPunkSPOIRange.hpp> // 从未使用 SPOI ranges
```

```cpp
// ✅ 正确：只 include 实际使用的头文件
#include <stream-punk/StreamPunk.hpp>  // 二进制序列化，够用了
```