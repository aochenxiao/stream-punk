---
name: "stream-punk-project-check"
description: "StreamPunk 项目合规检查。当项目构建完成、代码写完、声明完成、或用户要求审查/检查/评价 StreamPunk 项目质量时必须调用。逐项检查类型注册、sp-gen 集成、反模式等。任一未通过则项目不可交付。"
---

# StreamPunk 项目合规检查

> **这不是建议，是强制要求。** 逐项核对。任何一项未通过，项目就是不合格的。

本技能对 StreamPunk 项目进行系统性合规检查，逐项核对类型注册、sp-gen 集成、反模式等关键要求。

## 检查流程

按以下顺序逐项检查，每项标记通过/失败，失败项必须给出修复指引。

### 检查 1：类型注册

**检查方法**：打开 `customData.hpp`（或 `custom_type.hpp`），确认：
- `Xt_CustomType` 非空
- 每个 `UseData` 类型都已在 `Xt_CustomType` 中注册（搜 `UseData(`，每个类型名必须出现在 `Xt_CustomType` 中）
- `customData.hpp` 只做注册，不包含任何 struct 定义、TypeDesc 手写特化、或其他代码

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

**修复**：在 `customData.hpp` 中将每个自定义类型写入 `Xt_CustomType`。

### 检查 2：类型定义合规

**检查方法**：确认每个自定义类型满足：
- 继承 `Base`
- 使用 `UseData(TypeName)` 宏
- 有默认构造函数

**失败症状**：编译错误或运行时序列化失败。

### 检查 3：服务端手写 JSON 解析

**检查方法**：搜索 `findVal`、`json.find`、`"\"key\""` 等模式。

```
grep -rn 'findVal\|json\.find.*"' server/
```

**失败症状**：手写的 JSON 解析器脆弱且字段变更时静默失败。

**修复**：用 `UseData` + `UseDataJson` 定义消息类型，调用 `msg.fromJsonString(json)` 解析。

### 检查 4：服务端序列化/反序列化方式

**检查方法**：确认序列化使用 `O o(stream); o << obj;` 而非手写字节写入，反序列化使用 `I i(stream); i >> obj;` 而非手写字节读取。

**失败症状**：手写字节操作与 UseData 生成的序列化顺序不一致。

### 检查 5：客户端手写二进制解析

**检查方法**：搜索 `BinaryReader`、`parseXxx`、`serializeXxx`、`DataView`、`readU8`、`readU32`、`readF64`、`getFloat64`、`getInt32`、`getUint8` 等模式。

```
grep -rn "BinaryReader\|parse[A-Z]\|serialize[A-Z]\|DataView\|readU8\|readU32\|readF64\|readString\|getFloat64\|getInt32\|getUint8" client/
```

**注意**：`stream-punk-data.js`（sp-gen 生成）和 `stream-punk.js`（运行时）中包含这些函数是正常的。需要检查的是**业务代码**（如 `useGameSync.js`、`app.js`）中是否有手写版本。

**失败症状**：手写解析器与 C++ 端 `UseData` 宏展开的序列化顺序强耦合；字段变更时不会报错，但会静默产生数据错乱。

**修复**：删除手写解析代码，运行 `sp-gen` 生成客户端代码，从 `runtimes/<lang>/` 复制运行时文件。

### 检查 6：sp-gen 集成

**检查方法**：确认客户端目录中存在 sp-gen 生成的文件和运行时文件，且客户端 import 的数据类型来自 `./stream-punk-data`（或等效的生成文件），不能是手写的 interface。

对于 TypeScript 客户端，应存在：
- `client/src/stream-punk-data.ts`（sp-gen 生成）
- `client/src/stream-punk.ts`（运行时，从 `runtimes/ts/` 复制）

对于 JavaScript 客户端：
- `client/src/stream-punk-data.js`（sp-gen 生成）
- `client/src/stream-punk.js`（运行时，从 `runtimes/js/` 复制）

**如果没有这些文件**：说明 sp-gen 流程被跳过，客户端无法正确解析二进制数据。

**修复**：参考 [stream-punk-sp-gen](skills/stream-punk-sp-gen) 运行完整流水线。

### 检查 7：未使用的头文件

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

### 检查 8：INIT_StreamPunk 调用

**检查方法**：确认 `main()` 函数中调用了 `INIT_StreamPunk()`。

**失败症状**：编译错误"无法解析的外部符号"或运行时类型注册失败。

### 检查 9：运行时文件

**检查方法**：如果项目有非 C++ 客户端，确认运行时文件已从 `runtimes/<lang>/` 复制到客户端项目。

**失败症状**：客户端 `import { O, I } from './stream-punk'` 失败。

### 检查 10：项目结构

**检查方法**：确认：
- `customData.hpp` 在项目根目录（与 CMakeLists.txt 同级）
- `Data.hpp`（类型定义）在项目根目录
- 客户端生成文件在 `client/src/` 下
- 没有在 `server/` 子目录中重复创建 `Data.hpp`

## 快速搜索命令

在项目根目录运行以下命令，逐一检查：

```bash
# 1. Xt_CustomType 是否非空
grep -r "Xt_CustomType" --include="*.hpp" --include="*.h"

# 2. 客户端是否有手写二进制解析
grep -rn "getFloat64\|getInt32\|getUint8\|DataView" client/src/

# 3. 客户端是否有手写 parse 函数
grep -rn "parseGameState\|parseTank\|parseXxx\|BinaryReader\|serializeXxx" client/src/

# 4. 服务端是否有手写 JSON 解析
grep -rn 'findVal\|json\.find.*"' server/

# 5. 是否有未使用的头文件
grep -rn "StreamPunkJson.hpp\|StreamPunkSPOI.hpp\|StreamPunkSPOIShadow.hpp" --include="*.hpp" --include="*.cpp"
```

## 检查报告模板

完成检查后，按以下格式输出：

```
## StreamPunk 项目合规检查报告

| # | 检查项 | 结果 | 说明 |
|---|--------|------|------|
| 1 | 类型注册 | ✅/❌ | ... |
| 2 | 类型定义合规 | ✅/❌ | ... |
| 3 | 服务端手写 JSON 解析 | ✅/❌ | ... |
| 4 | 服务端序列化/反序列化方式 | ✅/❌ | ... |
| 5 | 客户端手写二进制解析 | ✅/❌ | ... |
| 6 | sp-gen 集成 | ✅/❌ | ... |
| 7 | 未使用的头文件 | ✅/❌ | ... |
| 8 | INIT_StreamPunk 调用 | ✅/❌ | ... |
| 9 | 运行时文件 | ✅/❌ | ... |
| 10 | 项目结构 | ✅/❌ | ... |

通过: X/10  失败: Y/10
```

## 注意事项

- 每个失败项必须给出具体修复指引（文件路径、代码示例）
- 不要跳过任何检查项，即使"看起来没问题"
- 如果项目是纯 C++（无客户端），检查 5、6、9 标记为 N/A