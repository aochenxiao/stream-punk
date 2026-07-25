---
name: "stream-punk-java"
description: "StreamPunk Java 集成：从 C++ 类型生成 Java 代码、读取/写入二进制数据、跨语言数据互通。当用户需要在 Java 项目中使用 StreamPunk 数据时调用。"
---

# StreamPunk Java 集成

> **前置条件**：本 Skill 假设以下步骤已完成：
> 1. C++ 类型已定义（见 [stream-punk-cpp-types](skills/stream-punk-cpp-types)）
> 2. 元数据已提取（见 [stream-punk-meta-extract](skills/stream-punk-meta-extract)）
> 3. sp-gen 已运行（见 [stream-punk-sp-gen](skills/stream-punk-sp-gen)），生成了 `StreamPunkData.java`
> 4. 运行时文件 `stream-punk.java` 已从 `runtimes/java/` 复制到项目
>
> **禁止在客户端手写二进制解析代码**。所有序列化/反序列化代码由 sp-gen 自动生成。

## 集成步骤

```bash
# 1. 从元数据生成 Java 类型代码
sp-gen -t java-meta -p ./src/main/java/StreamPunkData.java

# 2. 复制运行时文件
cp runtimes/java/stream-punk.java ./src/main/java/
```

> 运行时文件 `stream-punk.java` 零外部依赖，Java 8+ 即可。

## 读取 C++ 生成的二进制数据

```java
import java.nio.file.*;

byte[] data = Files.readAllBytes(Paths.get("chat_message.bin"));
ChatMessage msg = new ChatMessage().from(new I(data));
System.out.println(msg.sender + " " + msg.content);
```

## 生成数据供 C++ 读取

```java
ChatMessage msg = new ChatMessage();
msg.sender = "Bob";
msg.content = "Hello from Java!";
msg.timestamp = 1717171200000L;

O o = new O();
msg.to(o);
byte[] bin = o.toBytes();  // 可发送给 C++ 端
```

## 核心 API

| 类 | 用途 |
|----|------|
| `O` | 序列化写入器，`write_u8/16/32/64`、`writeString`、`writeArray`、`toBytes()` |
| `I` | 反序列化读取器，`read_u8/16/32/64`、`readString`、`readArray`、`hasMoreData()` |
| `SpRef<T>` | 指针引用，含 `value` 和 `address` |
| `SpArray<T>` | 定长数组，`at(index)`、`set(index, value)`、`size()` |
| `SpVariant` | 联合类型，`value`、`typeIndex` |

## 类型映射

| C++ | Java | 注意事项 |
|-----|------|---------|
| `u8` | `int` (0-255) | |
| `u16` | `int` | |
| `u32` | `long` | 无符号用 long |
| `u64` | `long` | 可能溢出 |
| `i8` | `byte` | |
| `i16` | `short` | |
| `i32` | `int` | |
| `i64` | `long` | |
| `f32` | `float` | |
| `f64` | `double` | |
| `bool` | `boolean` | |
| `string` | `String` | 默认 UTF-8 |
| `u16string` | `String` | UTF-16LE |
| `vector<T>` | `ArrayList<T>` | |
| `map<K,V>` | `HashMap<K,V>` | |
| `optional<T>` | `T` (nullable) | null 表示空 |
| `shared_ptr<T>` | `SpRef<T>` | |
| `variant` | `SpVariant` | |
| `array<T,N>` | `SpArray<T>` | |

## 编译命令

```bash
javac stream-punk.java StreamPunkData.java
```

## SPOI 查询 Builder（作为查询方）

Java 可以构建 SPOI 查询指令流，发送给 C++/Python/TS 等语言的执行器执行。

### 集成步骤

```bash
# 生成 Java SPOI 查询/更新 builder（含类型成员索引常量）
sp-gen -t spoi-java -p ./src/main/java/SPOI.java
```

> 生成的 `SPOI.java` 包含 `SpoiQuery`、`SpoiUpdate` 以及各类型的字段索引常量，零外部依赖。

### 使用示例

```java
// 构建查询：Player 中 level > 10 的，按 score 降序，取前 5 名
byte[] query = new SPOI.SpoiQuery()
    .filterI32(SPOI.SpoiTestPlayer_level, Cmp.GT, 10)
    .sort(SPOI.SpoiTestPlayer_score, false)
    .take(5)
    .build();  // 返回 byte[]

System.out.println("SPOI query hex: " + SPOI.SpoiStream.bytesToHex(query));
// 将 query 通过 TCP 发送给 C++ 服务端执行
```

### SpoiQuery API

| 方法 | 说明 |
|------|------|
| `filter(field, cmpOp, value)` | 条件筛选（byte[]） |
| `filterI32(field, cmpOp, value)` / `filterStr` | 类型化筛选 |
| `select(int... fields)` | 字段投影 |
| `sort(field, ascending)` | 排序 |
| `reverse()` | 反转顺序 |
| `take(count)` / `drop(count)` | 取/跳过前 N 个 |
| `distinct()` | 去重 |
| `count()` | 计数 |
| `keys()` / `values()` | 提取 map 键/值 |
| `join(field)` | 展平嵌套容器 |
| `enumerate(start)` | 附加索引 |
| `chunk(size)` / `slide(size)` / `stride(step)` / `adjacent(n)` | C++23 ranges |
| `any(field, cmpOp, value)` / `all(...)` / `find(...)` | 聚合查询 |
| `takewhile(field, cmpOp, value)` / `dropwhile(...)` | 条件截取/跳过 |
| `build()` | 生成 SPOI 指令字节流 `byte[]` |

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

```java
// 构建更新指令：将 Player[0].name 设为 "Alice"，level 加 5
byte[] update = new SPOI.SpoiUpdate()
    .setStr(new int[]{SPOI.SpoiTestPlayer_name}, "Alice")
    .addI32(new int[]{SPOI.SpoiTestPlayer_level}, 5)
    .build();  // 返回 byte[]

// 发送给目标语言的 SPOI 执行器
```

### SpoiUpdate API

| 方法 | 说明 |
|------|------|
| `set(path, value)` | 设置字段值（byte[]） |
| `setI32(path, value)` / `setU32` / `setF64` / `setStr` / `setBool` | 类型化 set |
| `addI32(path, delta)` / `add(path, value)` | 数值增量 |
| `append(path, value)` / `remove(path, value)` | 容器追加/移除 |
| `insert(path, value)` / `replace(path, value)` | 插入/替换 |
| `reset(path)` / `setnull(path)` | 重置/设空 |
| `build()` | 生成 SPOI 指令字节流 `byte[]` |

## 注意事项

- 所有数据为**小端序**（little-endian）
- `u32` 映射为 `long`（无符号 32 位超出 Java `int` 范围）
- `u64` 映射为 `long`，值可能溢出（Java 无无符号 64 位）
- 修改 C++ 类型后需重跑 `sp-gen -t java-meta` 和 `sp-gen -t spoi-java`
- 运行时零外部依赖，Java 8+ 即可