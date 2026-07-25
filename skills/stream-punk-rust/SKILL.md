---
name: "stream-punk-rust"
description: "StreamPunk Rust 集成：从 C++ 类型生成 Rust 代码、读取/写入二进制数据、跨语言数据互通。当用户需要在 Rust 项目中使用 StreamPunk 数据时调用。"
---

# StreamPunk Rust 集成

> **前置条件**：本 Skill 假设以下步骤已完成：
> 1. C++ 类型已定义（见 [stream-punk-cpp-types](skills/stream-punk-cpp-types)）
> 2. 元数据已提取（见 [stream-punk-meta-extract](skills/stream-punk-meta-extract)）
> 3. sp-gen 已运行（见 [stream-punk-sp-gen](skills/stream-punk-sp-gen)），生成了 `stream_punk_data.rs`
> 4. 运行时文件 `stream-punk.rs` 已从 `runtimes/rust/` 复制到项目
>
> **禁止在客户端手写二进制解析代码**。所有序列化/反序列化代码由 sp-gen 自动生成。

## 集成步骤

```bash
# 1. 从元数据生成 Rust 类型代码
sp-gen -t rust-meta -p ./client/src/stream_punk_data.rs

# 2. 复制运行时文件
cp runtimes/rust/stream-punk.rs ./client/src/
```

> 运行时文件 `stream-punk.rs` 零外部依赖，Rust 1.65+ 即可。

## 读取 C++ 生成的二进制数据

```rust
use std::fs;

fn main() -> std::io::Result<()> {
    let data = fs::read("chat_message.bin")?;
    let msg = ChatMessage::default().from(&mut I::new(&data));
    println!("{} {}", msg.sender, msg.content);
    Ok(())
}
```

## 生成数据供 C++ 读取

```rust
let msg = ChatMessage {
    sender: "Bob".to_string(),
    content: "Hello from Rust!".to_string(),
    timestamp: 1717171200000,
    ..Default::default()
};
let mut o = O::new();
msg.to(&mut o);
let bin = o.to_bytes();  // Vec<u8>，可发送给 C++ 端
```

## 核心 API

| 类型 | 用途 |
|------|------|
| `O` | 序列化写入器，`write_u8/16/32/64`、`write_string`、`write_array`、`to_bytes()` |
| `I` | 反序列化读取器，`read_u8/16/32/64`、`read_string`、`read_array`、`has_more_data()` |
| `SpRef<T>` | 指针引用，含 `value: Option<T>` 和 `address: u64` |
| `SpArray<T>` | 定长数组，`at(index)`、`set(index, value)`、`size()` |
| `SpVariant` | 联合类型，`type_index: u32`、`value` |

## 类型映射

| C++ | Rust | 注意事项 |
|-----|------|---------|
| `u8` | `u8` | |
| `u16` | `u16` | |
| `u32` | `u32` | |
| `u64` | `u64` | |
| `i8` | `i8` | |
| `i16` | `i16` | |
| `i32` | `i32` | |
| `i64` | `i64` | |
| `f32` | `f32` | |
| `f64` | `f64` | |
| `bool` | `bool` | |
| `string` | `String` | 默认 UTF-8 |
| `vector<T>` | `Vec<T>` | |
| `map<K,V>` | `HashMap<K,V>` | |
| `optional<T>` | `Option<T>` | |
| `shared_ptr<T>` | `SpRef<T>` | |
| `variant` | `SpVariant` | |
| `array<T,N>` | `SpArray<T>` | |

## 编译命令

```bash
cargo build
```

## SPOI 查询 Builder（作为查询方）

Rust 可以构建 SPOI 查询指令流，发送给 C++/Python/TS 等语言的执行器执行。

### 集成步骤

```bash
# 生成 Rust SPOI 查询/更新 builder（含类型成员索引常量）
sp-gen -t spoi-rust -p ./client/src/spoi_builder.rs
```

> 生成的 `spoi_builder.rs` 包含 `SpoiQuery`、`SpoiUpdate` 以及各类型的字段索引常量，零外部依赖。

### 使用示例

```rust
use spoi_builder::{SpoiQuery, cmp, SpoiTestPlayer as P};

fn main() {
    // 构建查询：Player 中 level > 10 的，按 score 降序，取前 5 名
    let query = SpoiQuery::new()
        .filter_i32(P::level, cmp::GT, 10)
        .sort(P::score, false)
        .take(5)
        .build();  // 返回 Vec<u8>

    println!("SPOI query hex: {}", hex::encode(&query));
    // 将 query 通过 TCP 发送给 C++ 服务端执行
}
```

### SpoiQuery API

| 方法 | 说明 |
|------|------|
| `filter(field, cmp_op, value)` | 条件筛选（Vec\<u8\>） |
| `filter_i32(field, cmp_op, value)` / `filter_str` | 类型化筛选 |
| `select(fields: &[u32])` | 字段投影 |
| `sort(field, ascending)` | 排序 |
| `reverse()` | 反转顺序 |
| `take(count)` / `drop(count)` | 取/跳过前 N 个 |
| `distinct()` | 去重 |
| `count()` | 计数 |
| `keys()` / `values()` | 提取 map 键/值 |
| `join(field)` | 展平嵌套容器 |
| `enumerate(start)` | 附加索引 |
| `chunk(size)` / `slide(size)` / `stride(step)` / `adjacent(n)` | C++23 ranges |
| `any(field, cmp_op, value)` / `all(...)` / `find(...)` | 聚合查询 |
| `takewhile(field, cmp_op, value)` / `dropwhile(...)` | 条件截取/跳过 |
| `build()` | 生成 SPOI 指令字节流 `Vec<u8>` |

### 比较运算符（cmp）

| 常量 | 含义 |
|------|------|
| `cmp::EQ` (0) | == |
| `cmp::NE` (1) | != |
| `cmp::LT` (2) | < |
| `cmp::GT` (3) | > |
| `cmp::LE` (4) | <= |
| `cmp::GE` (5) | >= |

## SPOI 更新 Builder（作为写入方）

```rust
use spoi_builder::{SpoiUpdate, SpoiTestPlayer as P};

// 构建更新指令：将 Player[0].name 设为 "Alice"，level 加 5
let update = SpoiUpdate::new()
    .set_str(vec![P::name], "Alice")
    .add_i32(vec![P::level], 5)
    .build();  // 返回 Vec<u8>

// 发送给目标语言的 SPOI 执行器
```

### SpoiUpdate API

| 方法 | 说明 |
|------|------|
| `set(path, value)` | 设置字段值（Vec\<u8\>） |
| `set_i32(path, value)` / `set_u32` / `set_f64` / `set_str` / `set_bool` | 类型化 set |
| `add_i32(path, delta)` / `add(path, value)` | 数值增量 |
| `append(path, value)` / `remove(path, value)` | 容器追加/移除 |
| `insert(path, value)` / `replace(path, value)` | 插入/替换 |
| `reset(path)` / `setnull(path)` | 重置/设空 |
| `build()` | 生成 SPOI 指令字节流 `Vec<u8>` |

## 注意事项

- 所有数据为**小端序**（little-endian）
- Rust 类型需要 `#[derive(Debug, Clone, Default)]` 等派生宏
- `SpRef<T>` 要求 `T: Debug + Clone`
- 方法名使用 snake_case（`from`、`to`、`new`、`to_bytes`）
- 修改 C++ 类型后需重跑 `sp-gen -t rust-meta` 和 `sp-gen -t spoi-rust`
- 运行时零外部依赖，Rust 1.65+ 即可