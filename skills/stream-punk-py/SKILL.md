---
name: "stream-punk-py"
description: "StreamPunk Python 集成：从 C++ 类型生成 Python 代码、读取/写入二进制数据、跨语言数据互通。当用户需要在 Python 项目中使用 StreamPunk 数据时调用。"
---

# StreamPunk Python 集成

> **前置条件**：本 Skill 假设以下步骤已完成：
> 1. C++ 类型已定义（见 [stream-punk-cpp-types](skills/stream-punk-cpp-types)）
> 2. 元数据已提取（见 [stream-punk-meta-extract](skills/stream-punk-meta-extract)）
> 3. sp-gen 已运行（见 [stream-punk-sp-gen](skills/stream-punk-sp-gen)），生成了 `stream_punk_data.py` 和 `spoi_builder.py`
> 4. 运行时文件 `stream-punk.py` 已从 `runtimes/py/` 复制到项目
>
> **禁止在客户端手写二进制解析代码**。所有序列化/反序列化代码由 sp-gen 自动生成。

## 集成步骤

```bash
# 1. 从元数据生成 Python 类型代码
sp-gen -t py-meta -p ./client/stream_punk_data.py

# 2. 复制运行时文件
cp runtimes/py/stream-punk.py ./client/
```

> 运行时文件 `stream-punk.py` 零外部依赖（仅用标准库 `struct` 和 `io`），Python 3.7+ 即可。

## 读取 C++ 生成的二进制数据

```python
from stream_punk_data import ChatMessage, I

with open('chat_message.bin', 'rb') as f:
    data = f.read()
msg = ChatMessage().from_stream(I(data))
print(msg.sender, msg.content)  # "Alice", "Hello from C++!"
```

## 生成数据供 C++ 读取

```python
from stream_punk_data import ChatMessage, O

msg = ChatMessage()
msg.sender = "Bob"
msg.content = "Hello from Python!"
msg.timestamp = 1717171200000

o = O()
msg.to_stream(o)
bin_data = o.to_bytes()  # bytes，可发送给 C++ 端
```

## 核心 API

| 类 | 用途 |
|----|------|
| `O` | 序列化写入器，`write_u8/16/32/64`、`write_str`、`write_array`、`to_bytes()` |
| `I` | 反序列化读取器，`read_u8/16/32/64`、`read_str`、`read_array`、`has_more_data()` |
| `SpRef[T]` | 指针引用，含 `value` 和 `address` |
| `SpArray[T]` | 定长数组，`at(index)`、`set(index, value)`、`size()` |
| `SpVariant` | 联合类型，`value`、`type_index` |

## 类型映射

| C++ | Python | 注意事项 |
|-----|--------|---------|
| `u8/16/32` | `int` | |
| `u64/i64` | `int` | Python 无大小限制 |
| `i8/16/32` | `int` | |
| `f32/f64` | `float` | |
| `bool` | `bool` | |
| `string` | `str` | 默认 UTF-8 |
| `u16string` | `str` | UTF-16LE 自动转换 |
| `vector<T>` | `list[T]` | |
| `map<K,V>` | `dict` | |
| `optional<T>` | `Optional[T]` | None 表示空 |
| `shared_ptr<T>` | `SpRef[T]` | |
| `variant` | `SpVariant` | |
| `array<T,N>` | `SpArray[T]` | |

## SPOI 查询 Builder（作为查询方）

Python 可以构建 SPOI 查询指令流，发送给 C++/TS/Go 等语言的执行器执行。

### 集成步骤

```bash
# 生成 Python SPOI 查询/更新 builder（含类型成员索引常量）
sp-gen -t spoi-py -p ./client/spoi_builder.py
```

> 生成的 `spoi_builder.py` 包含 `SpoiQuery`、`SpoiUpdate` 以及各类型的字段索引常量，零外部依赖。

### 使用示例

```python
from spoi_builder import SpoiQuery, Cmp
from spoi_builder import SpoiTestPlayer as P

# 构建查询：Player 中 level > 10 的，按 score 降序，取前 5 名
query = SpoiQuery("Player") \
    .filter_i32(P.level, Cmp.GT, 10) \
    .sort(P.score, ascending=False) \
    .take(5) \
    .build()  # 返回 bytes

print(f"SPOI query hex: {query.hex()}")
# 将 query 通过 TCP/WebSocket 发送给 C++ 服务端执行
```

### SpoiQuery API

| 方法 | 说明 |
|------|------|
| `filter(field, cmp_op, value)` | 条件筛选（原始字节） |
| `filter_i32(field, cmp_op, value)` / `filter_f64` / `filter_str` / `filter_bool` | 类型化筛选 |
| `select(*fields)` | 字段投影 |
| `sort(field, ascending=True)` | 排序 |
| `reverse()` | 反转顺序 |
| `take(count)` / `drop(count)` | 取/跳过前 N 个 |
| `distinct()` | 去重 |
| `count()` | 计数 |
| `keys()` / `values()` | 提取 map 键/值 |
| `join(field)` | 展平嵌套容器 |
| `enumerate(start=0)` | 附加索引 |
| `chunk(size)` / `slide(size)` / `stride(step)` / `adjacent(n)` | C++23 ranges |
| `any(field, cmp_op, value)` / `all(...)` / `find(...)` | 聚合查询 |
| `takewhile(field, cmp_op, value)` / `dropwhile(...)` | 条件截取/跳过 |
| `build()` | 生成 SPOI 指令字节流 `bytes` |

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

```python
from spoi_builder import SpoiUpdate
from spoi_builder import SpoiTestPlayer as P

# 构建更新指令：将 Player[0].name 设为 "Alice"，level 加 5
update = SpoiUpdate() \
    .set_str([P.name], "Alice") \
    .add_i32([P.level], 5) \
    .build()  # 返回 bytes

# 发送给目标语言的 SPOI 执行器
```

### SpoiUpdate API

| 方法 | 说明 |
|------|------|
| `set(path, value)` | 设置字段值（原始字节） |
| `set_i32(path, value)` / `set_u32` / `set_f64` / `set_str` / `set_bool` | 类型化 set |
| `add_i32(path, delta)` / `add_f64` / `add(path, value)` | 数值增量 |
| `append(path, value)` / `remove(path, value)` | 容器追加/移除 |
| `insert(path, value)` / `replace(path, value)` | 插入/替换 |
| `reset(path)` / `setnull(path)` | 重置/设空 |
| `build()` | 生成 SPOI 指令字节流 `bytes` |

## 注意事项

- 所有数据为**小端序**（little-endian）
- Python 的 `int` 无大小限制，`i64`/`u64` 不会溢出
- `I` 构造函数接受 `bytes`，从文件读取用 `rb` 模式
- 修改 C++ 类型后需重跑 `sp-gen -t py-meta` 和 `sp-gen -t spoi-py`，并确保 `temp/stream-punk-meta.bin` 已更新
- 运行时零外部依赖，仅用标准库

## SPOI 执行器（作为被查询方）

Python 可以作为 SPOI 查询目标，接受来自其他语言的 SPOI 指令流并执行。

### 集成步骤

```bash
# 1. 生成 Python SPOI Executor 注册表
sp-gen -t spoi-py-exec -p ./client/stream_punk_registry.py

# 2. 复制运行时执行器
cp runtimes/py/spoi_executor.py ./client/
```

### 使用示例

```python
from stream_punk_data import ChatMessage, Player
from stream_punk_registry import TYPE_REGISTRY
from spoi_executor import SpoiExecutor

# 创建执行器
executor = SpoiExecutor(TYPE_REGISTRY)

# 构建数据
players = [
    Player(name="Alice", level=10, health=100, items=[]),
    Player(name="Bob",   level=20, health=80,  items=[]),
    Player(name="Carol", level=15, health=120, items=[]),
]

# 执行 SPOI 指令流（从 C++/TS 等语言发送过来的二进制指令）
result = executor.execute(players, instruction_bytes)
# result = {"resultType": 2, "value": [Player(name="Bob", ...), Player(name="Carol", ...)]}
```

### 注意事项

- 执行器需要 `TYPE_REGISTRY`（类型名→字段名列表），由 `sp-gen spoi-py-exec` 生成
- 执行器零外部依赖，仅用标准库
- 修改 C++ 类型后需重跑 `sp-gen spoi-py-exec` 更新注册表