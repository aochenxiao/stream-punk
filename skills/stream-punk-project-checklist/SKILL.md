---
name: "stream-punk-project-checklist"
description: "StreamPunk 项目交付前强制检查清单。任何项目在声明完成之前，必须逐项核对本清单。任一未通过则项目不可交付。当项目声称完成、或需要审查 StreamPunk 项目质量时调用。"
---

# StreamPunk 项目交付检查清单

> **这不是建议，是强制要求。** 逐项核对。任何一项未通过，项目就是不合格的。

## 一、类型注册

- [ ] **搜索 `Xt_CustomType`** —— 在项目的 `customData.hpp`（或等效文件）中，`Xt_CustomType` 宏必须非空
- [ ] **每个 `UseData` 类型都已注册** —— 搜 `UseData(`，每个类型名必须出现在 `Xt_CustomType` 中
- [ ] **`customData.hpp` 只做注册** —— 该文件不应包含任何 struct 定义、TypeDesc 手写特化、或其他代码。只定义 `Xt_CustomType` 宏
- [ ] **搜索 `INIT_StreamPunk`** —— `main()` 中必须调用

## 二、客户端代码（跨语言项目）

- [ ] **搜索 `parseGameState\|parseTank\|parseXxx\|BinaryReader\|serializeXxx`** —— 客户端代码中必须零匹配
- [ ] **搜索 `getFloat64\|getInt32\|getUint8\|DataView.*offset`** —— 手写二进制字段解析的典型特征，必须零匹配
- [ ] **搜索 `sp-gen`** —— 项目的构建脚本或文档中必须有 sp-gen 调用
- [ ] **搜索 `stream-punk-data`** —— 客户端必须有 sp-gen 生成的数据类型文件
- [ ] **搜索 `stream-punk\.ts\|stream-punk\.js`** —— 客户端必须有从 `runtimes/` 复制的运行时文件
- [ ] **客户端 import 来源正确** —— 客户端 import 的数据类型必须来自 `./stream-punk-data`（或等效的生成文件），不能是手写的 interface

## 三、头文件

- [ ] **搜索 `StreamPunkJson.hpp`** —— 如果匹配，确认代码中实际使用了 `UseDataJson` / `toJson` / `fromJson` 等 JSON 功能。否则删除该 include
- [ ] **搜索 `StreamPunkSPOI.hpp`** —— 如果匹配，确认代码中实际使用了 `sp::query()` 或 SPOI 相关功能。否则删除该 include
- [ ] **搜索 `StreamPunkSPOIShadow.hpp`** —— 如果匹配，确认代码中实际使用了 `sp::spoi()`。否则删除该 include

## 四、服务端代码

- [ ] **搜索 `findVal\|json\.find`** —— 手写 JSON 解析的特征，必须零匹配。应改用 `UseDataJson` + `fromJsonString`
- [ ] **序列化方式正确** —— 使用 `O o(stream); o << obj;` 而非手写字节写入
- [ ] **反序列化方式正确** —— 使用 `I i(stream); i >> obj;` 而非手写字节读取

## 五、项目结构

- [ ] `customData.hpp` 在项目根目录（与 CMakeLists.txt 同级）
- [ ] `Data.hpp`（类型定义）在项目根目录
- [ ] 客户端生成文件在 `client/src/` 下
- [ ] 没有在 `server/` 子目录中重复创建 `Data.hpp`

## 快速搜索命令（复制粘贴执行）

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