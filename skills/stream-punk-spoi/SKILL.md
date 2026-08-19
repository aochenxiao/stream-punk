---
name: "stream-punk-spoi"
description: "使用 StreamPunk SPOI 协议进行动态查询、更新和跨语言数据操作。当用户需要构建 SPOI 查询、使用 Pipe/Builder 风格查询数据、通过 SPOI Shadow 进行增量更新、或跨语言 SPOI 操作时调用。"
---

# StreamPunk SPOI 查询/操作系统

SPOI（StreamPunk Operation Instruction）是 StreamPunk 的统一数据操作/查询指令协议，支持 35 种操作码，覆盖导航、读写、聚合、C++23 ranges 等。

## 头文件

| 功能 | 头文件 |
|------|--------|
| 协议定义 | `#include <stream-punk/StreamPunkSPOI.hpp>` |
| 查询 API（Pipe/Builder） | `#include <stream-punk/StreamPunkSPOIRange.hpp>` |
| 写操作代理（Shadow） | `#include <stream-punk/StreamPunkSPOIShadow.hpp>` |

## 类型定义（需要 SPOI 支持）

在类型定义中加上 `UseSPOI` 宏：

```cpp
#include <stream-punk/StreamPunkSPOIRange.hpp>

struct Player : public Base {
    #define Xt_Player(X__) \
    X__(std::string, name, "") \
    X__(i32, level, 1) \
    X__(f64, health, 100.0) \
    X__(std::vector<std::string>, items, std::vector<std::string>{})

    UseData(Player);
    UseSPOI(Player);  // 生成 SPOI 查询代理
};
```

## 查询 API：Pipe 风格

与 C++20/23 ranges 语法一致，但实际产生的是 SPOI 二进制指令：

```cpp
#include <stream-punk/StreamPunkSPOIRange.hpp>

// 链式查询
auto result = sp::query(players)
    | sp::filter([](auto& f) { return f.level > 10; })
    | sp::sort([](auto& f) { return f.score; })
    | sp::transform([](auto& f) { return f.name; })
    | sp::take(5)
    | sp::send();  // 执行并返回结果
```

### 支持的 Pipe 适配器

| 适配器 | 功能 | 说明 |
|--------|------|------|
| `sp::filter(pred)` | 条件筛选 | 保留满足条件的元素 |
| `sp::transform(fn)` | 字段投影 | 转换每个元素 |
| `sp::sort(keyFn)` | 排序 | 按键排序 |
| `sp::sort(keyFn, desc)` | 降序排序 | 指定排序方向 |
| `sp::reverse` | 反转 | 反转顺序 |
| `sp::take(n)` | 取前 N 个 | 限制数量 |
| `sp::drop(n)` | 跳过前 N 个 | 跳过元素 |
| `sp::takewhile(pred)` | 条件截取 | 满足条件就取 |
| `sp::dropwhile(pred)` | 条件跳过 | 满足条件就跳过 |
| `sp::distinct` | 去重 | 去除重复元素 |
| `sp::keys` | Map keys | 提取 map 的键 |
| `sp::values` | Map values | 提取 map 的值 |
| `sp::join` | 展平容器 | 展平嵌套容器 |
| `sp::enumerate` | 枚举 | 附加索引 |
| `sp::chunk(n)` | 分块 | 固定大小分块 |
| `sp::slide(n)` | 滑动窗口 | 滑动窗口 |
| `sp::stride(n)` | 步长 | 等距采样 |
| `sp::adjacent(n)` | 相邻元素 | 相邻元素组 |
| `sp::count` | 计数 | 返回元素数量 |
| `sp::any(pred)` | 存在性 | 是否存在满足条件的 |
| `sp::all(pred)` | 全量检查 | 是否全部满足条件 |
| `sp::find(pred)` | 查找 | 返回第一个匹配元素 |

## 查询 API：Builder 风格

与 Pipe 风格等效，但使用面向对象的 Builder API：

```cpp
auto result = sp::query(players)
    .where("level", sp::gt, 10)
    .sort("score", true)  // true = 降序
    .select("name")
    .take(5)
    .send();
```

## 写操作：SPOI Shadow

替代旧版 Delta 模式，用 SPOI 指令进行增量更新：

```cpp
#include <stream-punk/StreamPunkSPOIShadow.hpp>

std::stringstream deltaStream;
auto shadow = sp::spoi(player, deltaStream);

// 写操作
shadow.name = "Alice";       // SET 指令
shadow.level += 5;           // ADD 指令（仅数值类型）
shadow.items.append("sword"); // APPEND 指令（容器类型）
shadow.items.remove("shield"); // REMOVE 指令

// shadow 析构时自动将所有指令写入 deltaStream
```

### 字符串字段的增量操作（子串级，v0.2+）

`std::string` 字段除整体赋值（`=` → `e_set`）外，还可以当作字符容器做子串级增量：

```cpp
shadow.msg.append("!!");            // e_append  追加子串到末尾
shadow.msg.insert(7, "SPOI ");      // e_insert  在偏移 7 处插入子串
shadow.msg.erase(0, 5);             // e_remove  删除 [0, 5) 子串（len 默认 1）
shadow.msg.replace(2, 4, "NEW");    // e_replace 用 "NEW" 替换 [2, 6)
shadow.msg.move(1, 3, 6);           // e_move    把 [1, 4) 子串搬到偏移 6（原子指令）
```

约定：pos/len 越界时自动钳制（不崩溃）；`erase` 的 len、`replace` 的 (len+chunk) 编码在 operand 中，与 SP 序列化格式一致。配套示例：`examples/21-string-delta`。

## 执行器：在服务端执行 SPOI 指令

```cpp
#include <stream-punk/StreamPunkSPOIExecutor.hpp>

// 解析客户端发来的 SPOI 指令流
SpoiStream stream;
// ... 从网络读取指令到 stream ...

// 执行指令
sp::SPOIExecutor executor;
executor.execute(stream, rootObject);
```

## SPOI 操作码总览（35 个）

| 类别 | 操作码 | 功能 |
|------|--------|------|
| 导航 | nav, idx, deref, unwrap | 路径导航 |
| 写操作 | set, add, append, remove, insert, replace, reset, setnull | 数据修改 |
| 读操作 | filter, select, sort, reverse, take, drop, takewhile, dropwhile, distinct | 数据查询 |
| 聚合 | count, any, all, find | 聚合计算 |
| 容器 | keys, values, join | 容器操作 |
| Ranges | enumerate, chunk, slide, stride, adjacent | C++23 ranges |
| 控制 | exec, pipe | 执行/管道 |

## 跨语言 SPOI

通过 sp-gen 可以为每种语言生成 SPOI builder（查询方）：

```bash
sp-gen -t spoi-py   -p ./client/spoi_builder.py
sp-gen -t spoi-ts   -p ./frontend/src/spoi_builder.ts
sp-gen -t spoi-go   -p ./go-client/spoi_builder.go
sp-gen -t spoi-rust -p ./rust-client/src/spoi_builder.rs
sp-gen -t spoi-java -p ./src/main/java/SPOI.java
sp-gen -t spoi-kotlin -p ./src/main/kotlin/SpoiBuilder.kt
sp-gen -t spoi-js   -p ./spoi_builder.js
```

各语言 SPOI builder 的具体用法见对应语言技能：
- [stream-punk-py](skills/stream-punk-py) — Python `SpoiQuery` / `SpoiUpdate`
- [stream-punk-ts](skills/stream-punk-ts) — TypeScript `SpoiQuery` / `SpoiUpdate`
- [stream-punk-js](skills/stream-punk-js) — JavaScript `SpoiQuery` / `SpoiUpdate`
- [stream-punk-go](skills/stream-punk-go) — Go `SpoiQuery` / `SpoiUpdate`
- [stream-punk-rust](skills/stream-punk-rust) — Rust `SpoiQuery` / `SpoiUpdate`
- [stream-punk-java](skills/stream-punk-java) — Java `SpoiQuery` / `SpoiUpdate`
- [stream-punk-kotlin](skills/stream-punk-kotlin) — Kotlin `SpoiQuery` / `SpoiUpdate`

也可以生成 SPOI 执行器（被查询方），让 Python/TS/JS 接受 SPOI 指令流并执行：

```bash
sp-gen -t spoi-py-exec  -p ./client/stream_punk_registry.py
sp-gen -t spoi-ts-exec  -p ./frontend/src/stream-punk-registry.ts
sp-gen -t spoi-js-exec  -p ./stream-punk-registry.js
```

## SPOI 头文件架构

SPOI 协议完整实现在独立的头文件中，与 `StreamPunk.hpp` 的核心序列化逻辑分离：

| 头文件 | 职责 |
|--------|------|
| `StreamPunkSPOI.hpp` | 协议定义、操作码枚举（35 个）、SPOI 指令流编解码、varint 读写 |
| `StreamPunkSPOIRange.hpp` | Pipe/Builder 查询 API、`UseSPOI` 宏、`sp::query()` 入口 |
| `StreamPunkSPOIExecutor.hpp` | 服务端执行器，函数指针表 O(1) 分发，完整 35 个操作码的处理函数 |
| `StreamPunkSPOIShadow.hpp` | 写操作 Shadow 代理，增量更新 |

> 注意：`StreamPunk.hpp` 中的旧版 `execSp` 函数（仅支持 4 种操作）已被移除。请使用上述头文件中的完整 SPOI 实现。

## 注意事项

- SPOI 是 Delta 的继任者，旧版 `StreamPunkShadow.hpp` 已废弃
- 写操作 Shadow 在析构时自动写入指令，确保 `deltaStream` 在 shadow 生命周期内有效
- Pipe 和 Builder 两种风格等效，按团队偏好选择
- 执行器使用函数指针表分发，O(1) 按 opcode 索引，性能优于 switch 语句
- 跨语言 SPOI 查询时，builder（查询方）生成指令字节流，执行器（被查询方）执行指令并返回结果

## 常见错误：SPOI Shadow 全量替换 vs 增量追加

**这是最常见的 SPOI 使用错误。** `SPOIShadowField::operator=` 生成的是 `e_set` 指令，operand 是该字段的**完整序列化值**——不是增量。

```cpp
// ❌ 错误：每次都赋值整个 vector，生成 e_set 全量替换指令
//    operand 是整个 strokes vector 的序列化数据
//    比直接发全量更浪费（多了 SPOI 指令元数据的开销）
void broadcastDelta(Room& room) {
    std::stringstream deltaStream;
    auto shadow = sp::spoi(room.state, deltaStream);
    shadow.strokes = room.state.strokes;   // ← 全量替换！
    shadow.cursors = room.state.cursors;   // ← 全量替换！
}

// ✅ 正确：每次只追加新增的单个元素，生成 e_append 增量指令
void handleNewStroke(Room& room, Stroke& newStroke) {
    // 先更新本地状态
    room.state.strokes.push_back(newStroke);

    // 生成增量：只传这一个笔画
    std::stringstream deltaStream;
    auto shadow = sp::spoi(room.state, deltaStream);
    shadow.strokes.append(newStroke);   // ← 只追加一个元素
    // shadow 析构时写入 e_append 指令，operand 仅包含一个 Stroke
}
```

**判断规则**：
- 字段是**容器类型**（`vector`、`map` 等）→ 用 `append()` / `remove()` / `insert()`，不要用 `=`
- 字段是**值类型**（`i32`、`f64` 等）→ 用 `=` 或 `+=`，这是正确的增量用法
- 字段是 **`std::string`** → 用 `=` 整体覆盖，或用 `append/insert/erase/replace/move` 做子串级增量（见上文）
- 字段是**整个对象**（`shadow = newState`）→ 这永远不是增量，不要在 SPOI 场景下这样做

## SPOI 与 WebSocket 消息协议集成

### 模式 A：C++ 服务端用 SPOI Shadow 生成增量，通过 WebSocket 广播

```cpp
// 在消息处理函数中：
void handleNewStroke(Room& room, const Stroke& newStroke) {
    // 1. 更新本地状态
    room.state.strokes.push_back(newStroke);

    // 2. 生成 SPOI 增量指令
    std::stringstream deltaStream;
    {
        auto shadow = sp::spoi(room.state, deltaStream);
        shadow.strokes.append(newStroke);
    } // shadow 析构，指令写入 deltaStream

    // 3. 通过 WebSocket 广播 SpoiDelta 消息
    auto str = deltaStream.str();
    std::vector<u8> payload(str.begin(), str.end());
    broadcastToRoom(room, MsgType::SpoiDelta, payload);
}
```

### 模式 A 对应的客户端（JS/TS 端）

服务端用 SPOI Shadow 生成增量后，客户端**必须用 sp-gen 生成的代码来解析**，不得手写二进制解析。

```javascript
// 客户端 WebSocket 消息处理（使用 sp-gen 生成代码，零手写二进制解析）
import { WhiteboardState, I } from './stream-punk-data.js';        // sp-gen 生成
import { TYPE_REGISTRY } from './stream-punk-registry.js';          // sp-gen 生成
import { SpoiExecutor } from './spoi_executor.js';                  // 运行时

const executor = new SpoiExecutor(TYPE_REGISTRY);
let localState = new WhiteboardState();

ws.onmessage = (event) => {
  const data = new Uint8Array(event.data);
  const msgType = data[4];       // 消息类型（应用层协议）
  const payload = data.slice(5); // 二进制载荷

  switch (msgType) {
    case 0x01: // FullState：全量状态
      // 用 sp-gen 生成的 I 流反序列化
      localState = new WhiteboardState().from(new I(payload.buffer));
      break;

    case 0x02: // SpoiDelta：SPOI 增量指令
      // 用 sp-gen 生成的执行器解析并应用增量
      localState = executor.execute(localState, payload).value;
      break;
  }
};
```

> **反模式警告**：不要在客户端手写 `parseStroke`、`parseSpoiDelta`、`serializeStroke` 等函数。
> 这些全部由 sp-gen 自动生成。手写代码在 C++ 类型字段变更时会静默产生数据错乱。

### 模式 B：C++ 服务端执行客户端发来的 SPOI 指令

```cpp
// 在消息分发循环中：
case MsgType::SpoiDelta: {
    SpoiStream stream;
    stream.write(payload.data(), payload.size());
    sp::SPOIExecutor executor;
    executor.execute(stream, room.state);  // 在 room.state 上执行 SPOI 指令
    break;
}
```

### 完整的消息类型枚举

```cpp
enum class MsgType : u8 {
    FullState   = 0x01,  // 全量状态（二进制序列化整个 WhiteboardState）
    SpoiDelta   = 0x02,  // SPOI 增量指令（仅在用 SPOI Shadow 时使用）
    // 仅在用 SPOI 时才定义 SpoiDelta。如果不用 SPOI，可删掉此消息类型。
};
```

## 何时不需要 SPOI

**SPOI 的价值在于字段级增量**（只传变化的字段，如 `shadow.name = "Bob"` → 只传 name 字段）。如果你的增量粒度是"整个对象"，用普通二进制序列化就够了。

### 对比：对象级增量 vs SPOI 字段级增量

```cpp
// 方案 1：对象级增量（不需要 SPOI）—— 推荐用于"每次新增/删除整个对象"的场景
void handleNewStroke(Room& room, const Stroke& newStroke) {
    room.state.strokes.push_back(newStroke);
    std::stringstream ss;
    O{ss} << newStroke;                     // 序列化整个 Stroke
    broadcastToRoom(room, MsgType::StrokeAdded, ss);
}

// 方案 2：SPOI 字段级增量（需要 UseSPOIShadow）—— 适用于"修改对象内部字段"的场景
void handleNewStroke(Room& room, const Stroke& newStroke) {
    room.state.strokes.push_back(newStroke);
    std::stringstream deltaStream;
    auto shadow = sp::spoi(room.state, deltaStream);
    shadow.strokes.append(newStroke);       // 只传一个 Stroke 的 append 指令
    broadcastToRoom(room, MsgType::SpoiDelta, deltaStream);
}
```

**选择指南**：
| 场景 | 推荐方案 | 需要 SPOI？ |
|------|---------|:----------:|
| 新增一个对象到容器 | 对象级增量（`O << newStroke`） | 否 |
| 删除容器中的一个对象 | 对象级增量（发送删除索引） | 否 |
| 修改对象内部字段（如 `player.name = "Bob"`） | SPOI Shadow（`shadow.name = "Bob"`） | 是 |
| 多个字段同时变更 | SPOI Shadow（多条指令自动合并） | 是 |
| 跨语言查询（"查所有 level > 10 的玩家"） | SPOI Query（`sp::query()` / `SpoiQuery`） | 是 |

**原则**：如果你只做对象级增量（新增/删除整个对象），不要加 `UseSPOI` / `UseSPOIShadow`，不要生成 `spoi-*` 目标，不要定义 `SpoiDelta` 消息类型。这些只会产生未使用的死代码。