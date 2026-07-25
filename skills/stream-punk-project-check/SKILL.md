---
name: "stream-punk-project-check"
description: "StreamPunk 项目合规检查。当项目构建完成、代码写完、或用户要求审查/检查/评价 StreamPunk 项目时必须调用。逐项检查类型注册、sp-gen 集成、反模式等。"
---

# StreamPunk 项目合规检查

本技能对 StreamPunk 项目进行系统性合规检查，逐项核对类型注册、sp-gen 集成、反模式等关键要求。

## 检查流程

按以下顺序逐项检查，每项标记通过/失败，失败项必须给出修复指引。

### 检查 1：类型注册

**检查方法**：打开 `custom_type.hpp`（或 `customData.hpp`），确认 `Xt_CustomType` 非空。

```cpp
// 正确：每个自定义类型都在这里注册
#define Xt_CustomType(X__) \
X__(TankState, TankState) \
X__(BulletState, BulletState) \
X__(GameState, GameState)

// 错误：空注册
#define Xt_CustomType(X__) /* 空 */
```

**失败症状**：类型没有 `E_type` 枚举值，没有 `TypeID_t` 特化，没有 creator 函数，反序列化时无法通过 typeID 创建对象。

**修复**：在 `custom_type.hpp` 中将每个自定义类型写入 `Xt_CustomType`。

### 检查 2：类型定义合规

**检查方法**：确认每个自定义类型满足：
- 继承 `Base`
- 使用 `UseData(TypeName)` 宏
- 有默认构造函数

**失败症状**：编译错误或运行时序列化失败。

### 检查 3：服务端手写 JSON 解析

**检查方法**：搜索 `findVal`、`json.find`、`"\"key\""` 等模式。

```
grep -rn "findVal\|find(\"\\\"\|json\.find" server/
```

**失败症状**：手写的 JSON 解析器脆弱且字段变更时静默失败。

**修复**：用 `UseData` + `UseDataJson` 定义消息类型，调用 `msg.fromJsonString(json)` 解析。

### 检查 4：客户端手写二进制解析

**检查方法**：搜索 `BinaryReader`、`parseXxx`、`serializeXxx`、`DataView`、`readU8`、`readU32`、`readF64` 等模式。

```
grep -rn "BinaryReader\|parse[A-Z]\|serialize[A-Z]\|DataView\|readU8\|readU32\|readF64\|readString" client/
```

**注意**：`stream-punk-data.js`（sp-gen 生成）和 `stream-punk.js`（运行时）中包含这些函数是正常的。需要检查的是**业务代码**（如 `useGameSync.js`、`app.js`）中是否有手写版本。

**失败症状**：手写解析器与 C++ 端 `UseData` 宏展开的序列化顺序强耦合；字段变更时不会报错，但会静默产生数据错乱。

**修复**：删除手写解析代码，运行 `sp-gen` 生成客户端代码，从 `runtimes/<lang>/` 复制运行时文件。

### 检查 5：sp-gen 集成

**检查方法**：确认客户端目录中存在 sp-gen 生成的文件和运行时文件。

对于 TypeScript 客户端，应存在：
- `client/src/stream-punk-data.ts`（sp-gen 生成）
- `client/src/stream-punk.ts`（运行时，从 `runtimes/ts/` 复制）

对于 JavaScript 客户端：
- `client/src/stream-punk-data.js`（sp-gen 生成）
- `client/src/stream-punk.js`（运行时，从 `runtimes/js/` 复制）

**如果没有这些文件**：说明 sp-gen 流程被跳过，客户端无法正确解析二进制数据。

**修复**：参考 [stream-punk-sp-gen](skills/stream-punk-sp-gen) 运行完整流水线。

### 检查 6：未使用的头文件

**检查方法**：确认每个 `#include` 的头文件都有对应的功能使用。

| 头文件 | 何时需要 |
|--------|---------|
| `StreamPunk.hpp` | 总是需要（序列化基础） |
| `StreamPunkJson.hpp` | 仅当使用了 `UseDataJson`、`spToJson`、`spFromJson`、`JsonVal` 等 |
| `StreamPunkOrmMeta.hpp` | 仅当使用了 `UseDataOrm` |
| `StreamPunkSPOIRange.hpp` | 仅当使用了 `sp::query()` 或 `UseSPOI` |
| `StreamPunkSPOIShadow.hpp` | 仅当使用了 `sp::spoi()` 或 `UseSPOIShadow` |

**失败症状**：多余的 include 产生死代码，误导后续维护者。

**修复**：删除不需要的 `#include`。

### 检查 7：INIT_StreamPunk 调用

**检查方法**：确认 `main()` 函数中调用了 `INIT_StreamPunk()`。

**失败症状**：编译错误"无法解析的外部符号"或运行时类型注册失败。

### 检查 8：运行时文件

**检查方法**：如果项目有非 C++ 客户端，确认运行时文件已从 `runtimes/<lang>/` 复制到客户端项目。

**失败症状**：客户端 `import { O, I } from './stream-punk'` 失败。

## 检查报告模板

完成检查后，按以下格式输出：

```
## StreamPunk 项目合规检查报告

| # | 检查项 | 结果 | 说明 |
|---|--------|------|------|
| 1 | 类型注册 | ✅/❌ | ... |
| 2 | 类型定义合规 | ✅/❌ | ... |
| 3 | 服务端手写 JSON 解析 | ✅/❌ | ... |
| 4 | 客户端手写二进制解析 | ✅/❌ | ... |
| 5 | sp-gen 集成 | ✅/❌ | ... |
| 6 | 未使用的头文件 | ✅/❌ | ... |
| 7 | INIT_StreamPunk 调用 | ✅/❌ | ... |
| 8 | 运行时文件 | ✅/❌ | ... |

通过: X/8  失败: Y/8
```

## 注意事项

- 每个失败项必须给出具体修复指引（文件路径、代码示例）
- 不要跳过任何检查项，即使"看起来没问题"
- 如果项目是纯 C++（无客户端），检查 4、5、8 标记为 N/A