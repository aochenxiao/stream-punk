// Copyright (c) 2026 aochenxiao
// SPDX-License-Identifier: MIT
#pragma once
/*
    StreamPunkSPOI.hpp — SPOI 协议层 (v0.1.0)

    SPOI = StreamPunk Operation Instruction
    统一的数据操作/查询指令协议，囊括 Delta（写操作）和 Select（读操作）。

    定义：
    - 操作码枚举（X 宏）
    - 比较表达式
    - 指令 / 指令流数据结构
    - 结果类型枚举
    - 序列化/反序列化辅助函数

    依赖：StreamPunk.hpp（O/I 流、UseData 宏、基本类型）
*/

#include "StreamPunk.hpp"
#include <vector>
#include <cstdint>

namespace sp {

// =============================== 操作码（X 宏） ===============================

#define Xt_SPOI_ops(X__) \
    /* ── 导航（0x00-0x03）── */ \
    X__(e_nav)          /* 0x00 按成员索引导航到子字段 */ \
    X__(e_idx)          /* 0x01 按容器索引访问元素 */ \
    X__(e_deref)        /* 0x02 指针解引用 */ \
    X__(e_unwrap)       /* 0x03 optional 解包 */ \
    \
    /* ── 写操作（0x04-0x0B）── */ \
    X__(e_set)          /* 0x04 覆盖值 */ \
    X__(e_add)          /* 0x05 加法增量 */ \
    X__(e_append)       /* 0x06 容器追加 */ \
    X__(e_remove)       /* 0x07 容器删除 */ \
    X__(e_insert)       /* 0x08 容器插入 */ \
    X__(e_replace)      /* 0x09 替换元素 */ \
    X__(e_reset)        /* 0x0A 清空 optional */ \
    X__(e_setnull)      /* 0x0B 指针置空 */ \
    \
    /* ── 读操作（0x0C-0x14）── */ \
    X__(e_filter)       /* 0x0C 条件筛选 */ \
    X__(e_select)       /* 0x0D 字段投影 */ \
    X__(e_sort)         /* 0x0E 排序 */ \
    X__(e_reverse)      /* 0x0F 反转 */ \
    X__(e_take)         /* 0x10 取前 N 个 */ \
    X__(e_drop)         /* 0x11 跳过前 N 个 */ \
    X__(e_takewhile)    /* 0x12 条件截取 */ \
    X__(e_dropwhile)    /* 0x13 条件跳过 */ \
    X__(e_distinct)     /* 0x14 去重 */ \
    \
    /* ── 聚合（0x15-0x18）── */ \
    X__(e_count)        /* 0x15 计数 */ \
    X__(e_any)          /* 0x16 存在性检查 */ \
    X__(e_all)          /* 0x17 全量检查 */ \
    X__(e_find)         /* 0x18 查找第一个匹配 */ \
    \
    /* ── 容器操作（0x19-0x1B）── */ \
    X__(e_keys)         /* 0x19 map keys */ \
    X__(e_values)       /* 0x1A map values */ \
    X__(e_join)         /* 0x1B 展平嵌套容器 */ \
    \
    /* ── C++23 ranges（0x1C-0x20）── */ \
    X__(e_enumerate)    /* 0x1C 枚举 std::views::enumerate */ \
    X__(e_chunk)        /* 0x1D 分块 std::views::chunk */ \
    X__(e_slide)        /* 0x1E 滑动窗口 std::views::slide */ \
    X__(e_stride)       /* 0x1F 步长 std::views::stride */ \
    X__(e_adjacent)     /* 0x20 相邻元素 std::views::adjacent */ \
    \
    /* ── 控制（0x21-0x22）── */ \
    X__(e_exec)         /* 0x21 执行并返回结果 */ \
    X__(e_pipe)         /* 0x22 管道连接（链式） */ \
    \
    /* ── 字符串/容器片段操作（0x23-）── */ \
    X__(e_move)         /* 0x23 移动片段（当前支持 std::string 子串搬移） */

// 生成枚举
#define X_spoi_op_enum(name__) name__,
enum class SpoiOp : u8 { Xt_SPOI_ops(X_spoi_op_enum) };
#undef X_spoi_op_enum

// 操作码总数
constexpr u32 kSpoiOpCount = []{
    u32 n = 0;
    #define X_spoi_op_count(name__) +1
    n = Xt_SPOI_ops(X_spoi_op_count);
    #undef X_spoi_op_count
    return n;
}();

// 操作码名称字符串（用于调试）
#define X_spoi_op_name(name__) #name__,
constexpr const char* kSpoiOpNames[kSpoiOpCount] = { Xt_SPOI_ops(X_spoi_op_name) };
#undef X_spoi_op_name

// =============================== 比较运算符 ===============================

#define Xt_CmpOp(X__) \
    X__(e_eq)   /* 0: == */ \
    X__(e_ne)   /* 1: != */ \
    X__(e_lt)   /* 2: <  */ \
    X__(e_gt)   /* 3: >  */ \
    X__(e_le)   /* 4: <= */ \
    X__(e_ge)   /* 5: >= */

#define X_cmpop_enum(name__) name__,
enum class CmpOp : u8 { Xt_CmpOp(X_cmpop_enum) };
#undef X_cmpop_enum

// =============================== 结果类型 ===============================

#define Xt_ResultType(X__) \
    X__(e_undef)       /* 0: 未定义 */ \
    X__(e_single)      /* 1: 单个对象 */ \
    X__(e_vector)      /* 2: 对象数组 */ \
    X__(e_count)       /* 3: 计数值 */ \
    X__(e_bool)        /* 4: 布尔值 */ \
    X__(e_optional)    /* 5: 可空对象 */ \
    X__(e_error)       /* 6: 错误信息 */

#define X_resulttype_enum(name__) name__,
enum class ResultType : u8 { Xt_ResultType(X_resulttype_enum) };
#undef X_resulttype_enum

// =============================== 路径特殊标记 ===============================

constexpr u32 PATH_DEREF  = 0xFFFF;  // 指针解引用
constexpr u32 PATH_MAPKEY = 0xFFFE;  // 后跟 key 的序列化值

// =============================== Varint 编解码 ===============================

inline void writeVarint(std::ostream& os, u32 v) {
    u8 buf[5]; int i = 0;
    while (v >= 0x80) { buf[i++] = (v & 0x7F) | 0x80; v >>= 7; }
    buf[i++] = static_cast<u8>(v);
    os.write(reinterpret_cast<char*>(buf), i);
}

inline u32 readVarint(std::istream& is) {
    u32 r = 0; int s = 0; u8 b;
    while (is.read(reinterpret_cast<char*>(&b), 1)) {
        if (s >= 32) {
            throw SpDataError("readVarint: value overflow (too many bytes)");
        }
        r |= (static_cast<u32>(b & 0x7F) << s);
        if (!(b & 0x80)) return r;
        s += 7;
    }
    throw SpDataError("readVarint: unexpected EOF");
}

// =============================== 比较表达式 ===============================

struct SpoiCmpExpr : public Base {
    #define Xt_SpoiCmpExpr(X__) \
        X__(u32, memberIdx, 0) \
        X__(u8,  cmpOp,     0) \
        X__(std::vector<u8>, value, {})
    UseData(SpoiCmpExpr);

    // 从 vector<u8> 反序列化（用于 operand 解析）
    static SpoiCmpExpr deserialize(std::vector<u8> const& bytes) {
        std::string ss(bytes.begin(), bytes.end());
        std::stringstream is(ss);
        I i(is);
        SpoiCmpExpr expr;
        i >> expr;
        return expr;
    }

    // 序列化到 vector<u8>
    std::vector<u8> serialize() const {
        std::stringstream os;
        O o(os);
        o << *this;
        auto str = os.str();
        return std::vector<u8>(str.begin(), str.end());
    }
};

// SpoiCmpExpr 的 TypeDesc（SpoiCmpExpr 不依赖其他 SPOI 类型，可在此定义）
template<> struct TypeDesc<SpoiCmpExpr> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};

// =============================== SPOI 指令 ===============================

struct SpoiInstruction : public Base {
    #define Xt_SpoiInstruction(X__) \
        X__(u8,                   op,       0) \
        X__(std::vector<u32>,     path,     {}) \
        X__(std::vector<SpToken>, typeDesc, {}) \
        X__(std::vector<u8>,      operand,  {})
    UseData(SpoiInstruction);
};

// SpoiInstruction 的 TypeDesc（必须在 SpoiStream 之前，因为 SpoiStream 包含 vector<SpoiInstruction>）
template<> struct TypeDesc<SpoiInstruction> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};

// =============================== SPOI 指令流 ===============================

struct SpoiStream : public Base {
    #define Xt_SpoiStream(X__) \
        X__(std::vector<SpoiInstruction>, instructions, {})
    UseData(SpoiStream);
};

// SpoiStream 的 TypeDesc
template<> struct TypeDesc<SpoiStream> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};

// =============================== SPOI 结果 ===============================

struct SpoiResult : public Base {
    #define Xt_SpoiResult(X__) \
        X__(u8,              resultType, 0) \
        X__(std::vector<u8>, data,       {})
    UseData(SpoiResult);
};

// SpoiResult 的 TypeDesc
template<> struct TypeDesc<SpoiResult> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};

// =============================== 辅助函数 ===============================

// 操作码分类判断
inline bool isWriteOp(SpoiOp op) {
    return static_cast<u8>(op) >= static_cast<u8>(SpoiOp::e_set)
        && static_cast<u8>(op) <= static_cast<u8>(SpoiOp::e_setnull);
}

inline bool isReadOp(SpoiOp op) {
    return static_cast<u8>(op) >= static_cast<u8>(SpoiOp::e_filter)
        && static_cast<u8>(op) <= static_cast<u8>(SpoiOp::e_distinct);
}

inline bool isAggregateOp(SpoiOp op) {
    return static_cast<u8>(op) >= static_cast<u8>(SpoiOp::e_count)
        && static_cast<u8>(op) <= static_cast<u8>(SpoiOp::e_find);
}

inline bool isContainerOp(SpoiOp op) {
    return static_cast<u8>(op) >= static_cast<u8>(SpoiOp::e_keys)
        && static_cast<u8>(op) <= static_cast<u8>(SpoiOp::e_join);
}

inline bool isCpp23Op(SpoiOp op) {
    return static_cast<u8>(op) >= static_cast<u8>(SpoiOp::e_enumerate)
        && static_cast<u8>(op) <= static_cast<u8>(SpoiOp::e_adjacent);
}

inline bool isTerminalOp(SpoiOp op) {
    return op == SpoiOp::e_exec || isAggregateOp(op);
}

// 比较运算符求值（仅支持有比较运算符的类型）
template<typename T>
inline bool evaluateCmp(T const& val, CmpOp op, T const& target) {
    if constexpr (requires(T a, T b) { a == b; a < b; }) {
        switch (op) {
        case CmpOp::e_eq: return val == target;
        case CmpOp::e_ne: return val != target;
        case CmpOp::e_lt: return val <  target;
        case CmpOp::e_gt: return val >  target;
        case CmpOp::e_le: return val <= target;
        case CmpOp::e_ge: return val >= target;
        default: return false;
        }
    }
    return false;
}

// =============================== TypeDesc 辅助 ===============================

// 将编译期 TypeDesc<T>::v 转换为运行时 vector<SpToken>
template<typename T>
inline std::vector<SpToken> makeTypeDesc() {
    constexpr auto arr = TypeDesc<T>::v;
    return std::vector<SpToken>(arr.begin(), arr.end());
}

} // namespace sp