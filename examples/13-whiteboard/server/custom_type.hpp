#pragma once
// 13-whiteboard 项目 — 自定义类型注册
// 不使用 Xt_CustomType 机制（它会在 namespace sp 中创建不完整类型的 using 别名），
// 而是像 10-game 示例一样，在类型定义之前手动提供 TypeDesc 特化
#define Xt_CustomType(X__) /* 空：不使用 X_using_struct 机制 */