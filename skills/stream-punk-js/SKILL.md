---
name: "stream-punk-js"
description: "StreamPunk JavaScript 集成：读取/写入二进制数据、跨语言数据互通。当用户需要在 JavaScript 项目（非 TypeScript）中使用 StreamPunk 数据时调用。"
---

# StreamPunk JavaScript 集成

> JavaScript 无类型系统，推荐通过 TypeScript 生成代码后编译为 JS 使用。如果必须纯 JS 开发，可手动移植运行时。

> **前置条件**：本 Skill 假设以下步骤已完成：
> 1. C++ 类型已定义（见 [stream-punk-cpp-types](skills/stream-punk-cpp-types)）
> 2. 元数据已提取（见 [stream-punk-meta-extract](skills/stream-punk-meta-extract)）
> 3. sp-gen 已运行（见 [stream-punk-sp-gen](skills/stream-punk-sp-gen)），生成了 `stream-punk-data.js` 和 `spoi_builder.js`
> 4. 运行时文件 `stream-punk.js` 已从 `runtimes/js/` 复制到项目
>
> **禁止在客户端手写二进制解析代码**（如 `parseStroke`、`serializeStroke`）。所有序列化/反序列化代码由 sp-gen 自动生成。

## 禁止手写二进制解析

**StreamPunk 的核心价值是跨语言代码自动生成。手写二进制解析代码等同于放弃这个核心能力。**

手写代码的后果：
- C++ 类型字段变更时，客户端**静默**解析出错误数据（不会报错，数据悄悄损坏）
- 字段顺序依赖硬编码，与 C++ 端 `UseData` 生成的序列化顺序耦合
- SPOI 指令格式变更时，手写解析器会悄悄失效，产生难以排查的运行时 bug

以下是在 StreamPunk 项目中**绝对禁止**的模式：

```javascript
// ❌ 禁止：手写二进制解析
function parseStroke(data) {
  const r = new BinaryReader(data)
  const pointCount = r.readU32()
  // ... 手动逐个字段解析
}

// ❌ 禁止：手写二进制序列化
function serializeStroke(stroke) {
  const buf = new ArrayBuffer(100)
  const view = new DataView(buf)
  // ... 手动逐个字段写入
}

// ❌ 禁止：手写 SPOI 指令解析
function parseSpoiDelta(payload) {
  // ... 手动解析 SPOI 指令流
}
```

**正确做法是使用 sp-gen 生成的代码，零手写二进制操作。** 详见下方各章节。

## 集成步骤

```bash
# 1. 生成 TS 代码并编译为 JS
sp-gen -t ts -p ./stream-punk-data.ts
tsc stream-punk-data.ts --target ES2020 --skipLibCheck

# 2. 复制 JS 运行时文件
cp runtimes/js/stream-punk.js ./
```

> 运行时文件 `stream-punk.js` 零外部依赖，ES2020+ 即可。

## 读取 C++ 生成的二进制数据

```javascript
import { ChatMessage, I } from './stream-punk-data.js';
import { readFileSync } from 'fs';

const data = readFileSync('chat_message.bin');
const msg = new ChatMessage().from(new I(data.buffer));
console.log(msg.sender, msg.content);  // "Alice", "Hello from C++!"
```

## 生成数据供 C++ 读取

```javascript
import { ChatMessage, O } from './stream-punk-data.js';

const msg = new ChatMessage();
msg.sender = "Bob";
msg.content = "Hello from JS!";
msg.timestamp = 1717171200000n;  // i64 → BigInt

const o = new O();
msg.to(o);
const bin = o.toBytes();  // Uint8Array，可发送给 C++ 端
```

## 核心 API

| 类 | 用途 |
|----|------|
| `O` | 序列化写入器，`write_u8/16/32/64`、`writeString`、`writeArray`、`toBytes()` |
| `I` | 反序列化读取器，`read_u8/16/32/64`、`readString`、`readArray`、`hasMoreData()` |
| `SpRef` | 指针引用，含 `value` 和 `address` |
| `SpArray` | 定长数组，`at(index)`、`set(index, value)`、`size` |
| `SpVariant` | 联合类型，`value`、`typeIndex` |

## 类型映射

| C++ | JavaScript | 注意事项 |
|-----|-----------|---------|
| `u8/16/32` | `number` | |
| `u64/i64` | `BigInt` | 字面量加 `n` |
| `i8/16/32` | `number` | |
| `f32/f64` | `number` | |
| `bool` | `boolean` | |
| `string` | `string` | 默认 UTF-8 |
| `vector<T>` | `Array` | |
| `map<K,V>` | `Map` | |
| `optional<T>` | `T \| null` | |
| `shared_ptr<T>` | `SpRef` | |
| `variant` | `SpVariant` | |
| `array<T,N>` | `SpArray` | |

## 编译命令

```bash
# 从 TS 编译为 JS
tsc stream-punk-data.ts --target ES2020 --skipLibCheck
```

## 与 TypeScript 的区别

| | TypeScript | JavaScript |
|--|-----------|-----------|
| 类型检查 | 编译期 | 无 |
| 类型注解 | 有 | 无（`SpRef` 无泛型） |
| 推荐度 | 推荐 | 仅在无法使用 TS 时 |

## SPOI 查询 Builder（作为查询方）

JavaScript 可以构建 SPOI 查询指令流，发送给其他语言的执行器执行。

### 集成步骤

```bash
# 生成 JavaScript SPOI 查询/更新 builder（含类型成员索引常量）
sp-gen -t spoi-js -p ./spoi_builder.js
```

> 生成的 `spoi_builder.js` 包含 `SpoiQuery`、`SpoiUpdate` 以及各类型的字段索引常量，零外部依赖。

### 使用示例

```javascript
import { SpoiQuery, Cmp, SpoiTestPlayer as P } from './spoi_builder.js';

// 构建查询：Player 中 level > 10 的，按 score 降序，取前 5 名
const query = new SpoiQuery()
  .filterI32(P.level, Cmp.GT, 10)
  .sort(P.score, false)
  .take(5)
  .build();  // 返回 Uint8Array

console.log('SPOI query hex:', Buffer.from(query).toString('hex'));
// 将 query 通过 WebSocket 发送给 C++ 服务端执行
```

### SpoiQuery API

| 方法 | 说明 |
|------|------|
| `filter(field, cmpOp, value)` | 条件筛选（Uint8Array） |
| `filterI32(field, cmpOp, value)` / `filterStr` | 类型化筛选 |
| `select(...fields)` | 字段投影 |
| `sort(field, ascending=true)` | 排序 |
| `reverse()` | 反转顺序 |
| `take(count)` / `drop(count)` | 取/跳过前 N 个 |
| `distinct()` | 去重 |
| `count()` | 计数 |
| `keys()` / `values()` | 提取 map 键/值 |
| `join(field)` | 展平嵌套容器 |
| `enumerate(start=0)` | 附加索引 |
| `chunk(size)` / `slide(size)` / `stride(step)` / `adjacent(n)` | C++23 ranges |
| `any(field, cmpOp, value)` / `all(...)` / `find(...)` | 聚合查询 |
| `takewhile(field, cmpOp, value)` / `dropwhile(...)` | 条件截取/跳过 |
| `build()` | 生成 SPOI 指令字节流 `Uint8Array` |

### 比较运算符（Cmp）

| 常量 | 含义 |
|------|------|
| `Cmp.EQ` (0) | == |
| `Cmp.NE` (1) | != |
| `Cmp.LT` (2) | < |
| `Cmp.GT` (3) | > |
| `Cmp.LE` (4) | <= |
| `Cmp.GE` (5) | >= |

## SPOI 更新 Builder（作为写入方）

```javascript
import { SpoiUpdate, SpoiTestPlayer as P } from './spoi_builder.js';

// 构建更新指令：将 Player[0].name 设为 "Alice"，level 加 5
const update = new SpoiUpdate()
  .setStr([P.name], "Alice")
  .addI32([P.level], 5)
  .build();  // 返回 Uint8Array

// 发送给目标语言的 SPOI 执行器
```

### SpoiUpdate API

| 方法 | 说明 |
|------|------|
| `set(path, value)` | 设置字段值（Uint8Array） |
| `setI32(path, value)` / `setU32` / `setF64` / `setStr` / `setBool` | 类型化 set |
| `addI32(path, delta)` / `add(path, value)` | 数值增量 |
| `append(path, value)` / `remove(path, value)` | 容器追加/移除 |
| `insert(path, value)` / `replace(path, value)` | 插入/替换 |
| `reset(path)` / `setnull(path)` | 重置/设空 |
| `build()` | 生成 SPOI 指令字节流 `Uint8Array` |

## 注意事项

- 所有数据为**小端序**（little-endian）
- `i64`/`u64` 使用 `BigInt`，赋值需加 `n` 后缀
- `I` 构造函数接受 `ArrayBuffer`，Node.js 中 `readFileSync` 返回 `Buffer`，需 `.buffer` 转换
- JS 的 `SpRef` 无泛型，`value` 为 `any` 类型
- 推荐使用 TypeScript 而非纯 JS，以获得编译期类型安全
- 修改 C++ 类型后需重跑 `sp-gen -t spoi-js` 更新 builder
- 运行时零外部依赖，ES2020+ 即可

## SPOI 执行器（作为被查询方）

JavaScript 可以作为 SPOI 查询目标，接受来自其他语言的 SPOI 指令流并执行。

### 集成步骤

```bash
# 1. 生成 JavaScript SPOI Executor 注册表
sp-gen -t spoi-js-exec -p ./stream-punk-registry.js

# 2. 复制运行时执行器
cp runtimes/js/spoi_executor.js ./
```

### 使用示例

```javascript
import { Player } from './stream-punk-data.js';
import { TYPE_REGISTRY } from './stream-punk-registry.js';
import { SpoiExecutor } from './spoi_executor.js';

// 创建执行器
const executor = new SpoiExecutor(TYPE_REGISTRY);

// 构建数据
const players = [
    new Player({ name: "Alice", level: 10, health: 100 }),
    new Player({ name: "Bob",   level: 20, health: 80 }),
    new Player({ name: "Carol", level: 15, health: 120 }),
];

// 执行 SPOI 指令流（从 C++/Python 等语言发送过来的二进制指令）
const result = executor.execute(players, instructionBytes);
// result = { resultType: 2, value: [...] }
```

### 注意事项

- 执行器需要 `TYPE_REGISTRY`（类型名→字段名列表），由 `sp-gen spoi-js-exec` 生成
- 执行器零外部依赖，ES2020+ 即可
- 修改 C++ 类型后需重跑 `sp-gen spoi-js-exec` 更新注册表

## 接收并解析 SPOI Delta 指令流

当 JS 客户端收到 C++ 服务端通过 WebSocket 发来的 SPOI Delta 增量指令时，
使用 sp-gen 生成的 SPOI 执行器来解析，**不要手写 `parseSpoiDelta`**。

```javascript
import { WhiteboardState, I } from './stream-punk-data.js';        // sp-gen 生成
import { TYPE_REGISTRY } from './stream-punk-registry.js';          // sp-gen 生成
import { SpoiExecutor } from './spoi_executor.js';                  // 运行时

const executor = new SpoiExecutor(TYPE_REGISTRY);
let localState = new WhiteboardState();

// WebSocket 消息处理
ws.onmessage = (event) => {
  const data = new Uint8Array(event.data);
  const msgType = data[4];       // 消息类型（自定义帧头）
  const payload = data.slice(5); // SPOI 或全量数据的二进制载荷

  switch (msgType) {
    case 0x01: // FullState：全量状态
      // 用 sp-gen 生成的 I 流反序列化
      localState = new WhiteboardState().from(new I(payload.buffer));
      break;

    case 0x02: // SpoiDelta：SPOI 增量指令
      // 用 sp-gen 生成的执行器应用增量
      localState = executor.execute(localState, payload).value;
      break;
  }
};
```

> **注意**：SpoiExecutor 是按 SPOI 协议解析指令流的唯一正确方式。
> 手写的 `parseSpoiDelta` 函数会因 SPOI 协议内部格式演进产生静默 bug。
> 上例中的 `msgType` 和帧头格式（4 字节长度 + 1 字节类型）是应用层协议，
> 不属于 StreamPunk 的范畴，需要自行处理。