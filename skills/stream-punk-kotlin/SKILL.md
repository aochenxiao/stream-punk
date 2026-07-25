---
name: "stream-punk-kotlin"
description: "StreamPunk Kotlin 集成：从 C++ 类型生成 Kotlin 代码、读取/写入二进制数据、跨语言数据互通。当用户需要在 Kotlin 项目中使用 StreamPunk 数据时调用。"
---

# StreamPunk Kotlin 集成

> **前置条件**：本 Skill 假设以下步骤已完成：
> 1. C++ 类型已定义（见 [stream-punk-cpp-types](skills/stream-punk-cpp-types)）
> 2. 元数据已提取（见 [stream-punk-meta-extract](skills/stream-punk-meta-extract)）
> 3. sp-gen 已运行（见 [stream-punk-sp-gen](skills/stream-punk-sp-gen)），生成了 `StreamPunkData.kt`
> 4. 运行时文件 `stream-punk.kt` 已从 `runtimes/kotlin/` 复制到项目
>
> **禁止在客户端手写二进制解析代码**。所有序列化/反序列化代码由 sp-gen 自动生成。

## 集成步骤

```bash
# 1. 从元数据生成 Kotlin 类型代码
sp-gen -t kotlin-meta -p ./src/main/kotlin/StreamPunkData.kt

# 2. 复制运行时文件
cp runtimes/kotlin/stream-punk.kt ./src/main/kotlin/
```

> 运行时文件 `stream-punk.kt` 零外部依赖，Kotlin/JVM 即可。

## 读取 C++ 生成的二进制数据

```kotlin
import java.io.File

val data = File("chat_message.bin").readBytes()
val msg = ChatMessage().from(I(data))
println("${msg.sender} ${msg.content}")
```

## 生成数据供 C++ 读取

```kotlin
val msg = ChatMessage().apply {
    sender = "Bob"
    content = "Hello from Kotlin!"
    timestamp = 1717171200000L
}
val o = O()
msg.to(o)
val bin = o.toBytes()  // ByteArray，可发送给 C++ 端
```

## 核心 API

| 类 | 用途 |
|----|------|
| `O` | 序列化写入器，`write_u8/16/32/64`、`writeString`、`writeArray`、`toBytes()` |
| `I` | 反序列化读取器，`read_u8/16/32/64`、`readString`、`readArray`、`hasMoreData()` |
| `SpRef<T>` | 指针引用，含 `value: T?` 和 `address: Long` |
| `SpArray<T>` | 定长数组，`at(index)`、`set(index, value)`、`size()` |
| `SpVariant` | 联合类型，`typeIndex`、`value` |

## 类型映射

| C++ | Kotlin | 注意事项 |
|-----|--------|---------|
| `u8` | `Int` | |
| `u16` | `Int` | |
| `u32` | `Long` | 无符号用 Long |
| `u64` | `Long` | 可能溢出 |
| `i8` | `Byte` | |
| `i16` | `Short` | |
| `i32` | `Int` | |
| `i64` | `Long` | |
| `f32` | `Float` | |
| `f64` | `Double` | |
| `bool` | `Boolean` | |
| `string` | `String` | 默认 UTF-8 |
| `u16string` | `String` | UTF-16LE |
| `vector<T>` | `MutableList<T>` | |
| `map<K,V>` | `MutableMap<K,V>` | |
| `optional<T>` | `T?` | null 表示空 |
| `shared_ptr<T>` | `SpRef<T>` | |
| `variant` | `SpVariant` | |
| `array<T,N>` | `SpArray<T>` | |

## 编译命令

```bash
kotlinc stream-punk.kt StreamPunkData.kt -include-runtime -d output.jar
```

## SPOI 查询 Builder（作为查询方）

Kotlin 可以构建 SPOI 查询指令流，发送给 C++/Python/TS 等语言的执行器执行。

### 集成步骤

```bash
# 生成 Kotlin SPOI 查询/更新 builder（含类型成员索引常量）
sp-gen -t spoi-kotlin -p ./src/main/kotlin/SpoiBuilder.kt
```

> 生成的 `SpoiBuilder.kt` 包含 `SpoiQuery`、`SpoiUpdate` 以及各类型的字段索引常量，零外部依赖。

### 使用示例

```kotlin
// 构建查询：Player 中 level > 10 的，按 score 降序，取前 5 名
val query = SpoiQuery()
    .filterI32(SpoiTestPlayer_level, Cmp.GT, 10)
    .sort(SpoiTestPlayer_score, false)
    .take(5)
    .build()  // 返回 ByteArray

println("SPOI query hex: ${query.joinToString("") { "%02x".format(it) }}")
// 将 query 通过 TCP 发送给 C++ 服务端执行
```

### SpoiQuery API

| 方法 | 说明 |
|------|------|
| `filter(field, cmpOp, value)` | 条件筛选（ByteArray） |
| `filterI32(field, cmpOp, value)` / `filterStr` | 类型化筛选 |
| `select(vararg fields)` | 字段投影 |
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
| `build()` | 生成 SPOI 指令字节流 `ByteArray` |

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

```kotlin
// 构建更新指令：将 Player[0].name 设为 "Alice"，level 加 5
val update = SpoiUpdate()
    .setStr(intArrayOf(SpoiTestPlayer_name), "Alice")
    .addI32(intArrayOf(SpoiTestPlayer_level), 5)
    .build()  // 返回 ByteArray

// 发送给目标语言的 SPOI 执行器
```

### SpoiUpdate API

| 方法 | 说明 |
|------|------|
| `set(path, value)` | 设置字段值（ByteArray） |
| `setI32(path, value)` / `setU32` / `setF64` / `setStr` / `setBool` | 类型化 set |
| `addI32(path, delta)` / `add(path, value)` | 数值增量 |
| `append(path, value)` / `remove(path, value)` | 容器追加/移除 |
| `insert(path, value)` / `replace(path, value)` | 插入/替换 |
| `reset(path)` / `setnull(path)` | 重置/设空 |
| `build()` | 生成 SPOI 指令字节流 `ByteArray` |

## 注意事项

- 所有数据为**小端序**（little-endian）
- `u32` 映射为 `Long`（无符号 32 位超出 Kotlin `Int` 范围）
- `u64` 映射为 `Long`，值可能溢出
- `optional<T>` 映射为 `T?`，`null` 表示空
- 修改 C++ 类型后需重跑 `sp-gen -t kotlin-meta` 和 `sp-gen -t spoi-kotlin`
- 运行时零外部依赖，Kotlin/JVM 即可