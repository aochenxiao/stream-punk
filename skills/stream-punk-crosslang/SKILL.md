---
name: "stream-punk-crosslang"
description: "【强制触发】当用户创建或修改任何包含非C++客户端（React、Vue、TypeScript、JavaScript、Python、Java、Go、Rust、Kotlin、前端、Web、网页、浏览器、client、frontend）的StreamPunk项目时必须调用此skill。在写任何客户端代码之前必须先调用此skill。只要项目有C++服务端+任意客户端，这就是第一步。"
---

# 跨语言项目强制门禁

## STOP — 你有非 C++ 客户端

在写任何客户端代码之前，**必须**完成以下步骤。跳过任一步 = 项目不合格。

---

## 第 1 步：编写并运行元数据提取器

在项目根目录创建 `meta_extractor.cpp`：

```cpp
#include "Data.hpp"
#include <stream-punk/MetaData.hpp>
#include <fstream>

int main() {
    INIT_StreamPunk();
    sp_meta::MetaFile meta;
    #define X_extract(type, name) meta.types.push_back(sp_meta::extractTypeMeta<name>());
    Xt_CustomType(X_extract);
    #undef X_extract
    std::ofstream ofs("temp/stream-punk-meta.bin", std::ios::binary);
    sp_meta::writeMetaFile(ofs, meta);
    return 0;
}
```

编译并运行：
```bash
cl /std:c++20 /EHsc meta_extractor.cpp /I<stream-punk>/include /I. /Fe:meta_extractor.exe
meta_extractor.exe
```

输出：`temp/stream-punk-meta.bin`

---

## 第 2 步：运行 sp-gen 生成客户端代码

```bash
# TypeScript 项目
sp-gen -t ts -p ./client/src/stream-punk-data.ts

# JavaScript 项目（先生成 TS 再编译为 JS）
sp-gen -t ts -p ./client/src/stream-punk-data.ts
tsc client/src/stream-punk-data.ts --target ES2020 --skipLibCheck
```

**sp-gen 生成的文件必须提交到项目中**（不是只在构建脚本中运行）。

---

## 第 3 步：复制运行时文件

**先检查 sp-gen 是否已嵌入运行时**：打开 `stream-punk-data.ts`，看文件开头是否有 `class I`、`class O`、`class Base` 等运行时类定义。

- **如果已嵌入运行时**：无需单独复制 `stream-punk.ts`。但必须验证嵌入的运行时版本与 `runtimes/ts/stream-punk.ts` 一致。如果不一致，以 `runtimes/ts/stream-punk.ts` 为准，需更新 sp-gen 使其嵌入最新版本。
- **如果未嵌入运行时**：必须复制运行时文件。

```bash
# TypeScript（仅当 sp-gen 未嵌入运行时时需要）
cp runtimes/ts/stream-punk.ts ./client/src/

# JavaScript（仅当 sp-gen 未嵌入运行时时需要）
cp runtimes/js/stream-punk.js ./client/src/
```

**运行时版本同步**：`runtimes/ts/stream-punk.ts` 是运行时唯一权威来源。sp-gen 嵌入的运行时版本必须与 `runtimes/ts/` 保持一致。如果发现不一致（如 `SpArray` 构造函数签名不同），说明 sp-gen 的嵌入式模板过时，需要更新 sp-gen。

---
## 第 4 步：集成 sp-gen 到构建流程

在 `client/package.json` 中添加 sp-gen 脚本，确保类型变更后能重新生成客户端代码：

```json
{
  "scripts": {
    "sp-gen": "sp-gen -t ts -p ./src/stream-punk-data.ts",
    "dev": "vite",
    "build": "npm run sp-gen && tsc && vite build"
  }
}
```

> **sp-gen 生成的文件必须提交到项目中**（不是只在构建脚本中运行），但 `package.json` 中的脚本确保新开发者或 CI 可以重现生成过程。

---
## 验证：客户端目录中必须存在

```
client/src/
├── stream-punk-data.ts    # sp-gen 生成 ← 禁止手写！
└── stream-punk.ts         # 如果 sp-gen 未嵌入运行时，从 runtimes/ts/ 复制 ← 禁止手写！
                           # 如果 sp-gen 已嵌入运行时，此文件不存在是正常的
```

---
## 绝对禁止 — 客户端手写二进制解析

以下模式在客户端代码中**绝对禁止**。如果出现，删除重来：

- `DataView` + 手动偏移量
- `getFloat64` / `getInt32` / `getUint8`
- `readU8` / `readU32` / `readF64` / `readString` 等手写方法
- `parseXxx` / `serializeXxx` 函数
- `offset += 8` 手动偏移量计算
- 手写 `I` 类 / `O` 类

**正确用法有且只有一种**：

```typescript
import { GameState, I } from './stream-punk-data';
const state = new GameState().from(new I(data.buffer));
```

---

## 修改 C++ 类型后的更新

```
1. 重新编译并运行 meta_extractor.exe
2. 重新运行 sp-gen
3. 重新复制运行时文件（如运行时版本有更新）
```

---

## 参考

- 详细 sp-gen 用法 → `stream-punk-sp-gen`
- 类型定义 → `stream-punk-cpp-types`
- 元数据提取 → `stream-punk-meta-extract`
- 项目合规检查 → `stream-punk-project-check`