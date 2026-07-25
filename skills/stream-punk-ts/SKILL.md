---
name: "stream-punk-ts"
description: "StreamPunk TypeScript 集成：从 C++ 类型生成 TS 代码、读取/写入二进制数据、跨语言数据互通。当用户需要在 TypeScript 项目中使用 StreamPunk 数据时调用。"
---

# StreamPunk TypeScript 集成

> **前置条件**：本 Skill 假设以下步骤已完成：
> 1. C++ 类型已定义（见 [stream-punk-cpp-types](skills/stream-punk-cpp-types)）
> 2. 元数据已提取（见 [stream-punk-meta-extract](skills/stream-punk-meta-extract)）
> 3. sp-gen 已运行（见 [stream-punk-sp-gen](skills/stream-punk-sp-gen)），生成了 `stream-punk-data.ts` 和 `spoi_builder.ts`
> 4. 运行时文件 `stream-punk.ts` 已从 `runtimes/ts/` 复制到项目
>
> **禁止在客户端手写二进制解析代码**。所有序列化/反序列化代码由 sp-gen 自动生成。

## 集成步骤

```bash
# 1. 生成 TS 类型代码
sp-gen -t ts -p ./frontend/src/stream-punk-data.ts

# 2. 复制运行时文件
cp runtimes/ts/stream-punk.ts ./frontend/src/

# 3. 编译（可选）
tsc ./frontend/src/stream-punk-data.ts --target ES2020 --skipLibCheck
```

> 运行时文件 `stream-punk.ts` 零外部依赖，直接复制即可使用。

## 读取 C++ 生成的二进制数据

```typescript
import { ChatMessage, I } from './stream-punk-data';
import { readFileSync } from 'fs';

const data = readFileSync('chat_message.bin');
const msg = new ChatMessage().from(new I(data.buffer));
console.log(msg.sender, msg.content);  // "Alice", "Hello from C++!"
```

## 生成数据供 C++ 读取

```typescript
import { ChatMessage, O } from './stream-punk-data';

const msg = new ChatMessage();
msg.sender = "Bob";
msg.content = "Hello from TS!";
msg.timestamp = 1717171200000n;  // i64 → bigint

const o = new O();
msg.to(o);
const bin = o.toBytes();  // Uint8Array，可发送给 C++ 端
```

## 核心 API

| 类 | 用途 |
|----|------|
| `O` | 序列化写入器，`write_u8/16/32/64`、`writeString`、`writeArray`、`toBytes()` |
| `I` | 反序列化读取器，`read_u8/16/32/64`、`readString`、`readArray`、`hasMoreData()` |
| `SpRef<T>` | 指针引用，含 `value` 和 `address` |
| `SpArray<T>` | 定长数组，`at(index)`、`set(index, value)`、`size` |
| `SpVariant` | 联合类型，`value`、`typeIndex` |

## 类型映射

| C++ | TypeScript | 注意事项 |
|-----|-----------|---------|
| `u8/16/32` | `number` | |
| `u64/i64` | `bigint` | 字面量加 `n`，如 `0n` |
| `i8/16/32` | `number` | |
| `f32/f64` | `number` | |
| `bool` | `boolean` | |
| `string` | `string` | 默认 UTF-8 |
| `u16string` | `string` | UTF-16LE，TS 原生支持 |
| `vector<T>` | `Array<T>` | |
| `map<K,V>` | `Map<K,V>` | |
| `optional<T>` | `T \| null` | |
| `shared_ptr<T>` | `SpRef<T>` | |
| `variant` | `SpVariant` | |
| `array<T,N>` | `SpArray<T>` | |

## 编译命令

```bash
# 基本编译
tsc stream-punk-data.ts --target ES2020 --skipLibCheck

# 指定输出文件
tsc stream-punk-data.ts --outFile stream-punk-data.js --target ES2020

# 启用严格模式
tsc stream-punk-data.ts --strict --target ES2020 --skipLibCheck
```

## SPOI 查询 Builder（作为查询方）

TypeScript 可以构建 SPOI 查询指令流，发送给 C++/Python/Go 等语言的执行器执行。

### 集成步骤

```bash
# 生成 TypeScript SPOI 查询/更新 builder（含类型成员索引常量）
sp-gen -t spoi-ts -p ./frontend/src/spoi_builder.ts
```

> 生成的 `spoi_builder.ts` 包含 `SpoiQuery`、`SpoiUpdate` 以及各类型的字段索引常量，零外部依赖。

### 使用示例

```typescript
import { SpoiQuery, Cmp, SpoiTestPlayer as P } from './spoi_builder';

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

```typescript
import { SpoiUpdate, SpoiTestPlayer as P } from './spoi_builder';

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
- `i64`/`u64` 映射为 `bigint`，赋值时需加 `n` 后缀
- `I` 构造函数接受 `ArrayBuffer`，Node.js 中 `readFileSync` 返回 `Buffer`，需 `.buffer` 转换
- 修改 C++ 类型后需重跑 `sp-gen -t ts` 和 `sp-gen -t spoi-ts` 并重新编译
- 运行时文件零依赖，ES2020+ 即可运行

## SPOI 执行器（作为被查询方）

TypeScript 可以作为 SPOI 查询目标，接受来自其他语言的 SPOI 指令流并执行。

### 集成步骤

```bash
# 1. 生成 TypeScript SPOI Executor 注册表
sp-gen -t spoi-ts-exec -p ./frontend/src/stream-punk-registry.ts

# 2. 复制运行时执行器
cp runtimes/ts/spoi_executor.ts ./frontend/src/
```

### 使用示例

```typescript
import { Player } from './stream-punk-data';
import { TYPE_REGISTRY } from './stream-punk-registry';
import { SpoiExecutor } from './spoi_executor';

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

- 执行器需要 `TYPE_REGISTRY`（类型名→字段名列表），由 `sp-gen spoi-ts-exec` 生成
- 执行器零外部依赖，ES2020+ 即可
- 修改 C++ 类型后需重跑 `sp-gen spoi-ts-exec` 更新注册表