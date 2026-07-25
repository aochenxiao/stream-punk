#pragma once
/*
    spoi_generator.hpp — SPOI 代码生成器公共基础设施

    为各语言生成 SPOI 查询/写入 builder 代码。
    复用 MetaData.hpp 和 meta-reader.hpp 的类型元数据读取能力。
*/

#include "stream-punk/StreamPunk.hpp"
#include "stream-punk/MetaData.hpp"
#include <string>
#include <vector>
#include <map>
#include <span>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <algorithm>

using namespace sp;

namespace spoi_gen {

// =============================== 语言无关的类型信息 ===============================

struct SpoiFieldInfo {
    std::string name;       // 成员名
    uint32_t index;         // 成员索引（即 M::E 枚举值）
    std::string cppType;    // C++ 类型名
    uint32_t typeId;        // 基本类型的 E_type 枚举值（用于 operand 序列化 type_id）
    std::string typeName;   // 基本类型名（"u32", "f64", "string", "bool" 等）；对于容器为值类型，对于 map 为 value 类型
    bool isContainer;       // 是否为容器类型
    bool isOptional;        // 是否为 optional
    bool isPointer;         // 是否为指针
    bool isString;          // 是否为 string 类型
    bool isBool;            // 是否为 bool 类型
    // 容器类型详细信息
    std::string containerKind;          // "vector", "deque", "list", "map", "set", "uset", "umap", "opt"
    std::string containerKeyTypeName;   // 对于 map/umap: key 类型名；对于其他容器: 空
    std::string containerValueTypeName; // 对于容器: value 类型名（与 typeName 相同，但显式存储）
};

struct SpoiTypeInfo {
    std::string className;
    std::vector<SpoiFieldInfo> fields;
};

// =============================== SPOI 操作码常量 ===============================

// 写操作
inline constexpr uint8_t SPOI_OP_SET      = 0x04;
inline constexpr uint8_t SPOI_OP_ADD      = 0x05;
inline constexpr uint8_t SPOI_OP_APPEND   = 0x06;
inline constexpr uint8_t SPOI_OP_REMOVE   = 0x07;
inline constexpr uint8_t SPOI_OP_INSERT   = 0x08;
inline constexpr uint8_t SPOI_OP_REPLACE  = 0x09;
inline constexpr uint8_t SPOI_OP_RESET    = 0x0A;
inline constexpr uint8_t SPOI_OP_SETNULL  = 0x0B;

// 读操作
inline constexpr uint8_t SPOI_OP_FILTER   = 0x0C;
inline constexpr uint8_t SPOI_OP_SELECT   = 0x0D;
inline constexpr uint8_t SPOI_OP_SORT     = 0x0E;
inline constexpr uint8_t SPOI_OP_REVERSE  = 0x0F;
inline constexpr uint8_t SPOI_OP_TAKE     = 0x10;
inline constexpr uint8_t SPOI_OP_DROP     = 0x11;
inline constexpr uint8_t SPOI_OP_TAKEWHILE= 0x12;
inline constexpr uint8_t SPOI_OP_DROPWHILE= 0x13;
inline constexpr uint8_t SPOI_OP_DISTINCT = 0x14;

// 聚合
inline constexpr uint8_t SPOI_OP_COUNT    = 0x15;
inline constexpr uint8_t SPOI_OP_ANY      = 0x16;
inline constexpr uint8_t SPOI_OP_ALL      = 0x17;
inline constexpr uint8_t SPOI_OP_FIND     = 0x18;

// 容器
inline constexpr uint8_t SPOI_OP_KEYS     = 0x19;
inline constexpr uint8_t SPOI_OP_VALUES   = 0x1A;
inline constexpr uint8_t SPOI_OP_JOIN     = 0x1B;

// C++23
inline constexpr uint8_t SPOI_OP_ENUMERATE= 0x1C;
inline constexpr uint8_t SPOI_OP_CHUNK    = 0x1D;
inline constexpr uint8_t SPOI_OP_SLIDE    = 0x1E;
inline constexpr uint8_t SPOI_OP_STRIDE   = 0x1F;
inline constexpr uint8_t SPOI_OP_ADJACENT = 0x20;

// 控制
inline constexpr uint8_t SPOI_OP_EXEC     = 0x21;
inline constexpr uint8_t SPOI_OP_PIPE     = 0x22;

// 比较运算符
inline constexpr uint8_t CMP_EQ = 0;
inline constexpr uint8_t CMP_NE = 1;
inline constexpr uint8_t CMP_LT = 2;
inline constexpr uint8_t CMP_GT = 3;
inline constexpr uint8_t CMP_LE = 4;
inline constexpr uint8_t CMP_GE = 5;

// 路径特殊标记
inline constexpr uint32_t PATH_DEREF = 0xFFFF;

// =============================== 类型分析 ===============================

// 是否为基本类型（非容器、非指针、非 optional）
inline bool isBasicTypeT(sp_meta::SpToken token) {
    return token == static_cast<sp_meta::SpToken>(E_type::u8)
        || token == static_cast<sp_meta::SpToken>(E_type::u16)
        || token == static_cast<sp_meta::SpToken>(E_type::u32)
        || token == static_cast<sp_meta::SpToken>(E_type::u64)
        || token == static_cast<sp_meta::SpToken>(E_type::i8)
        || token == static_cast<sp_meta::SpToken>(E_type::i16)
        || token == static_cast<sp_meta::SpToken>(E_type::i32)
        || token == static_cast<sp_meta::SpToken>(E_type::i64)
        || token == static_cast<sp_meta::SpToken>(E_type::f32)
        || token == static_cast<sp_meta::SpToken>(E_type::f64)
        || token == static_cast<sp_meta::SpToken>(E_type::ch)
        || token == static_cast<sp_meta::SpToken>(E_type::ch8)
        || token == static_cast<sp_meta::SpToken>(E_type::ch16)
        || token == static_cast<sp_meta::SpToken>(E_type::ch32)
        || token == static_cast<sp_meta::SpToken>(E_type::bl)
        || token == static_cast<sp_meta::SpToken>(E_type::string);
}

// 从 TypeDesc 提取最内层基本类型的 SpToken（E_type 枚举值）
// 例如: [u32] → u32, [vector, u32] → u32, [opt, string] → string, [map, string, u32] → u32
inline sp_meta::SpToken getBasicTypeId(std::span<const sp_meta::SpToken> desc) {
    if (desc.empty()) return static_cast<sp_meta::SpToken>(E_type::e_unknowType);
    // 对于基本类型，直接返回
    if (desc.size() == 1 && isBasicTypeT(desc[0])) return desc[0];
    // string 类型: TypeDesc<string> = [string, ch]，应返回 string 而非 ch
    if (desc[0] == static_cast<sp_meta::SpToken>(E_type::string)) return desc[0];
    // 对于容器/optional/指针，取最后一个元素（值类型）
    auto last = desc.back();
    if (isBasicTypeT(last)) return last;
    // 自定义类型：返回原值
    return last;
}

// 将 SpToken 转换为语言无关的类型名
inline std::string getBasicTypeName(sp_meta::SpToken token) {
    using T = sp_meta::SpToken;
    if (token == static_cast<T>(E_type::u8))  return "u8";
    if (token == static_cast<T>(E_type::u16)) return "u16";
    if (token == static_cast<T>(E_type::u32)) return "u32";
    if (token == static_cast<T>(E_type::u64)) return "u64";
    if (token == static_cast<T>(E_type::i8))  return "i8";
    if (token == static_cast<T>(E_type::i16)) return "i16";
    if (token == static_cast<T>(E_type::i32)) return "i32";
    if (token == static_cast<T>(E_type::i64)) return "i64";
    if (token == static_cast<T>(E_type::f32)) return "f32";
    if (token == static_cast<T>(E_type::f64)) return "f64";
    if (token == static_cast<T>(E_type::ch))  return "ch";
    if (token == static_cast<T>(E_type::ch8)) return "ch8";
    if (token == static_cast<T>(E_type::ch16)) return "ch16";
    if (token == static_cast<T>(E_type::ch32)) return "ch32";
    if (token == static_cast<T>(E_type::bl))  return "bool";
    if (token == static_cast<T>(E_type::string)) return "string";
    return "custom";
}

// 从 TypeDesc 判断是否为容器类型
inline bool isContainerTypeT(std::span<const sp_meta::SpToken> desc) {
    if (desc.empty()) return false;
    auto first = desc[0];
    return first == static_cast<sp_meta::SpToken>(E_type::vector)
        || first == static_cast<sp_meta::SpToken>(E_type::deque)
        || first == static_cast<sp_meta::SpToken>(E_type::list)
        || first == static_cast<sp_meta::SpToken>(E_type::flist)
        || first == static_cast<sp_meta::SpToken>(E_type::set)
        || first == static_cast<sp_meta::SpToken>(E_type::uset)
        || first == static_cast<sp_meta::SpToken>(E_type::map)
        || first == static_cast<sp_meta::SpToken>(E_type::umap);
}

// 判断是否为 optional 类型
inline bool isOptionalTypeT(std::span<const sp_meta::SpToken> desc) {
    if (desc.empty()) return false;
    return desc[0] == static_cast<sp_meta::SpToken>(E_type::opt);
}

// 判断是否为指针类型
inline bool isPointerTypeT(std::span<const sp_meta::SpToken> desc) {
    if (desc.empty()) return false;
    auto first = desc[0];
    return first == static_cast<sp_meta::SpToken>(E_type::ptr)
        || first == static_cast<sp_meta::SpToken>(E_type::sptr)
        || first == static_cast<sp_meta::SpToken>(E_type::wptr)
        || first == static_cast<sp_meta::SpToken>(E_type::uptr);
}

// 判断是否为 string 类型
inline bool isStringTypeT(std::span<const sp_meta::SpToken> desc) {
    if (desc.empty()) return false;
    return desc[0] == static_cast<sp_meta::SpToken>(E_type::string);
}

// 判断是否为 bool 类型
inline bool isBoolTypeT(std::span<const sp_meta::SpToken> desc) {
    if (desc.empty()) return false;
    return desc[0] == static_cast<sp_meta::SpToken>(E_type::bl);
}

// 获取 TypeDesc 中第一个类型的 token 数量（递归解析）
// 基本类型: 1 token（string 除外，占 2 tokens: [string, ch]）
// 容器类型: 1 + element_tokens（map/umap: 1 + key_tokens + value_tokens）
// optional/pointer: 1 + inner_tokens
inline size_t countTypeTokens(std::span<const sp_meta::SpToken> desc) {
    if (desc.empty()) return 0;
    auto first = desc[0];

    // string 类型: 2 tokens [string, ch]
    if (first == static_cast<sp_meta::SpToken>(E_type::string)) {
        return 2;
    }

    // 容器类型
    if (isContainerTypeT(desc)) {
        if (first == static_cast<sp_meta::SpToken>(E_type::map) ||
            first == static_cast<sp_meta::SpToken>(E_type::umap)) {
            // map/umap: 1 + key_tokens + value_tokens
            if (desc.size() < 2) return 1;
            size_t keyTokens = countTypeTokens(desc.subspan(1));
            size_t total = 1 + keyTokens;
            if (desc.size() > total) {
                total += countTypeTokens(desc.subspan(total));
            }
            return total;
        } else {
            // vector/deque/list/set/uset/flist: 1 + element_tokens
            if (desc.size() < 2) return 1;
            return 1 + countTypeTokens(desc.subspan(1));
        }
    }

    // optional/pointer: 1 + inner_tokens
    if (isOptionalTypeT(desc) || isPointerTypeT(desc)) {
        if (desc.size() < 2) return 1;
        return 1 + countTypeTokens(desc.subspan(1));
    }

    // 基本类型: 1 token
    return 1;
}

// 获取容器种类名（"vector", "deque", "list", "map", "set", "uset", "umap", "opt"）
inline std::string getContainerKind(std::span<const sp_meta::SpToken> desc) {
    if (desc.empty()) return "";
    auto first = desc[0];
    using T = sp_meta::SpToken;
    if (first == static_cast<T>(E_type::vector)) return "vector";
    if (first == static_cast<T>(E_type::deque))  return "deque";
    if (first == static_cast<T>(E_type::list))   return "list";
    if (first == static_cast<T>(E_type::flist))  return "flist";
    if (first == static_cast<T>(E_type::set))    return "set";
    if (first == static_cast<T>(E_type::uset))   return "uset";
    if (first == static_cast<T>(E_type::map))    return "map";
    if (first == static_cast<T>(E_type::umap))   return "umap";
    if (first == static_cast<T>(E_type::opt))    return "opt";
    return "";
}

// =============================== 从 MetaFile 提取 SPOI 类型信息 ===============================

inline std::vector<SpoiTypeInfo> extractSpoiTypes(const sp_meta::MetaFile& meta) {
    std::vector<SpoiTypeInfo> result;

    for (auto& t : meta.types) {
        // 只提取有成员的自定义类型（Base 派生类）
        if (t.members.empty()) continue;

        SpoiTypeInfo info;
        info.className = t.className;

        for (size_t i = 0; i < t.members.size(); ++i) {
            auto& m = t.members[i];
            SpoiFieldInfo field;
            field.name = m.name;
            field.index = static_cast<uint32_t>(i);
            field.cppType = "?"; // 简化：不解析具体类型名
            field.isContainer = isContainerTypeT(m.typeDesc);
            field.isOptional = isOptionalTypeT(m.typeDesc);
            field.isPointer = isPointerTypeT(m.typeDesc);
            field.isString = isStringTypeT(m.typeDesc);
            field.isBool = isBoolTypeT(m.typeDesc);
            // 提取基本类型 ID 和名称
            auto basicId = getBasicTypeId(m.typeDesc);
            field.typeId = static_cast<uint32_t>(basicId);
            field.typeName = getBasicTypeName(basicId);
            // 提取容器类型详细信息
            field.containerKind = getContainerKind(m.typeDesc);
            if (field.isContainer) {
                // 使用 countTypeTokens 正确跳过 key 类型的全部 token
                if (field.containerKind == "map" || field.containerKind == "umap") {
                    if (m.typeDesc.size() >= 2) {
                        // 跳过 container token，解析 key type
                        auto keyDesc = std::span(m.typeDesc).subspan(1);
                        field.containerKeyTypeName = getBasicTypeName(keyDesc[0]);
                        size_t keyTokenCount = countTypeTokens(keyDesc);
                        // 解析 value type（在 key 之后）
                        if (m.typeDesc.size() > 1 + keyTokenCount) {
                            auto valDesc = std::span(m.typeDesc).subspan(1 + keyTokenCount);
                            field.containerValueTypeName = getBasicTypeName(valDesc[0]);
                        }
                    }
                } else {
                    // 对于 vector/deque/list/set 等: [container, value_type]
                    if (m.typeDesc.size() >= 2) {
                        field.containerValueTypeName = getBasicTypeName(m.typeDesc[1]);
                    }
                }
            } else if (field.isOptional) {
                // 对于 optional: [opt, value_type]
                if (m.typeDesc.size() >= 2) {
                    field.containerValueTypeName = getBasicTypeName(m.typeDesc[1]);
                }
            }
            info.fields.push_back(field);
        }

        result.push_back(std::move(info));
    }

    return result;
}

} // namespace spoi_gen