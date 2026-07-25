---
name: "stream-punk-go"
description: "StreamPunk Go 集成：从 C++ 类型生成 Go 代码、读取/写入二进制数据、跨语言数据互通。当用户需要在 Go 项目中使用 StreamPunk 数据时调用。"
---

# StreamPunk Go 集成

> **前置条件**：本 Skill 假设以下步骤已完成：
> 1. C++ 类型已定义（见 [stream-punk-cpp-types](skills/stream-punk-cpp-types)）
> 2. 元数据已提取（见 [stream-punk-meta-extract](skills/stream-punk-meta-extract)）
> 3. sp-gen 已运行（见 [stream-punk-sp-gen](skills/stream-punk-sp-gen)），生成了 `stream_punk_data.go`
> 4. 运行时文件 `stream-punk.go` 已从 `runtimes/go/` 复制到项目
>
> **禁止在客户端手写二进制解析代码**。所有序列化/反序列化代码由 sp-gen 自动生成。

## 集成步骤

```bash
# 1. 从元数据生成 Go 类型代码
sp-gen -t go-meta -p ./client/stream_punk_data.go

# 2. 复制运行时文件
cp runtimes/go/stream-punk.go ./client/
```

> 运行时文件 `stream-punk.go` 零外部依赖，Go 1.18+ 即可。

## 读取 C++ 生成的二进制数据

```go
package main

import (
    "fmt"
    "os"
)

func main() {
    data, _ := os.ReadFile("chat_message.bin")
    msg := ChatMessage{}.From(NewI(data))
    fmt.Println(msg.Sender, msg.Content)
}
```

## 生成数据供 C++ 读取

```go
msg := ChatMessage{
    Sender:    "Bob",
    Content:   "Hello from Go!",
    Timestamp: 1717171200000,
}
o := NewO()
msg.To(o)
bin := o.ToBytes()  // []byte，可发送给 C++ 端
```

## 核心 API

| 类型 | 用途 |
|------|------|
| `O` | 序列化写入器，`WriteU8/16/32/64`、`WriteString`、`WriteArray`、`ToBytes()` |
| `I` | 反序列化读取器，`ReadU8/16/32/64`、`ReadString`、`ReadArray`、`HasMoreData()` |
| `SpRef` | 指针引用，含 `Value` 和 `Address` |
| `SpArray` | 定长数组，`At(index)`、`Set(index, value)`、`Size()` |
| `SpVariant` | 联合类型，`TypeIndex`、`Value` |
| `SpBase` | 接口，`TypeID() uint32`、`From_(i *I)`、`To(o *O)` |

## 类型映射

| C++ | Go | 注意事项 |
|-----|-----|---------|
| `u8` | `uint8` | |
| `u16` | `uint16` | |
| `u32` | `uint32` | |
| `u64` | `uint64` | |
| `i8` | `int8` | |
| `i16` | `int16` | |
| `i32` | `int32` | |
| `i64` | `int64` | |
| `f32` | `float32` | |
| `f64` | `float64` | |
| `bool` | `bool` | |
| `string` | `string` | 默认 UTF-8 |
| `vector<T>` | `[]T` | |
| `map<K,V>` | `map[K]V` | |
| `optional<T>` | `*T` | nil 表示空 |
| `shared_ptr<T>` | `SpRef` | |
| `variant` | `SpVariant` | |
| `array<T,N>` | `SpArray` | |

## 编译命令

```bash
go build -o client ./client/
```

## SPOI 查询 Builder（作为查询方）

Go 可以构建 SPOI 查询指令流，发送给 C++/Python/TS 等语言的执行器执行。

### 集成步骤

```bash
# 生成 Go SPOI 查询/更新 builder（含类型成员索引常量）
sp-gen -t spoi-go -p ./client/spoi_builder.go
```

> 生成的 `spoi_builder.go` 包含 `SpoiQuery`、`SpoiUpdate` 以及各类型的字段索引常量，零外部依赖。

### 使用示例

```go
import "fmt"

func main() {
    // 构建查询：Player 中 level > 10 的，按 score 降序，取前 5 名
    query := NewSpoiQuery().
        Filter(SpoiTestPlayer_level, CmpGT, int32ToBytes(10)).
        Sort(SpoiTestPlayer_score, false).
        Take(5).
        Build()  // 返回 []byte

    fmt.Printf("SPOI query hex: %x\n", query)
    // 将 query 通过 TCP/WebSocket 发送给 C++ 服务端执行
}
```

### SpoiQuery API

| 方法 | 说明 |
|------|------|
| `Filter(field, cmpOp, value)` | 条件筛选 |
| `FilterI32(field, cmpOp, value)` | i32 类型筛选便捷方法 |
| `Select(fields ...uint32)` | 字段投影 |
| `Sort(field, ascending)` | 排序 |
| `Reverse()` | 反转顺序 |
| `Take(count)` | 取前 N 个 |
| `Drop(count)` | 跳过前 N 个 |
| `Distinct()` | 去重 |
| `Count()` | 计数 |
| `Keys()` / `Values()` | 提取 map 键/值 |
| `Join(field)` | 展平嵌套容器 |
| `Enumerate(start)` | 附加索引 |
| `Chunk(size)` / `Slide(size)` / `Stride(step)` / `Adjacent(n)` | C++23 ranges |
| `Any(field, cmpOp, value)` / `All(...)` / `Find(...)` | 聚合查询 |
| `Takewhile(field, cmpOp, value)` / `Dropwhile(...)` | 条件截取/跳过 |
| `Build()` | 生成 SPOI 指令字节流 `[]byte` |

### 比较运算符（Cmp）

| 常量 | 含义 |
|------|------|
| `CmpEQ` (0) | == |
| `CmpNE` (1) | != |
| `CmpLT` (2) | < |
| `CmpGT` (3) | > |
| `CmpLE` (4) | <= |
| `CmpGE` (5) | >= |

## SPOI 更新 Builder（作为写入方）

```go
// 构建更新指令：将 Player[0].name 设为 "Alice"，level 加 5
update := NewSpoiUpdate().
    SetStr([]uint32{SpoiTestPlayer_name}, "Alice").
    AddI32([]uint32{SpoiTestPlayer_level}, 5).
    Build()  // 返回 []byte

// 发送给目标语言的 SPOI 执行器
```

### SpoiUpdate API

| 方法 | 说明 |
|------|------|
| `Set(path, value)` | 设置字段值（原始字节） |
| `SetI32(path, value)` / `SetU32` / `SetF64` / `SetStr` / `SetBool` | 类型化 set |
| `AddI32(path, delta)` / `Add(path, value)` | 数值增量 |
| `Append(path, value)` | 容器追加 |
| `Remove(path, value)` | 容器移除 |
| `Insert(path, value)` / `Replace(path, value)` | 插入/替换 |
| `Reset(path)` / `Setnull(path)` | 重置/设空 |
| `Build()` | 生成 SPOI 指令字节流 `[]byte` |

## 注意事项

- 所有数据为**小端序**（little-endian）
- Go 的方法名首字母大写（`From`、`To`、`NewI`、`NewO`）
- `optional<T>` 映射为指针 `*T`，`nil` 表示空
- 修改 C++ 类型后需重跑 `sp-gen -t go-meta` 和 `sp-gen -t spoi-go`
- 运行时零外部依赖，Go 1.18+ 即可