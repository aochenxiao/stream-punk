#pragma once

#include "stream-punk/StreamPunk.hpp"
#include "stream-punk/MetaData.hpp"
#include "MemberInfoBase.hpp"
#include <span>
#include <map>
#include <vector>
#include <string>

using namespace sp;

// ============== 共享 TypeDesc 解释器 ==============
//
// TypeDesc 的 SpToken 数组是语言无关的结构描述，各语言的 getXxxTypeDescLength()
// 函数逻辑完全一致，此处提取为共享函数。
//
// 各语言的 interpretXxxTypeDescInner() 共享相同的递归调度结构，但生成的代码
// 片段（类型名、序列化/反序列化表达式）不同，通过 LangCodeGen 策略类定制。

// 计算 TypeDesc 消耗的 token 数量（纯结构性，语言无关）
inline size_t getTypeDescLength(std::span<const sp_meta::SpToken> desc) {
    if (desc.empty()) return 0;
    auto first = desc[0];

    if (first >= static_cast<Sz>(E_type::Base) + 1 && first < static_cast<Sz>(E_type::e_customType))
        return 1;
    if (first == E_type::Base) return 1;

    if (first == E_type::ptr || first == E_type::sptr || first == E_type::wptr || first == E_type::uptr)
        return 1 + getTypeDescLength(desc.subspan(1));

    if (first == E_type::vector || first == E_type::deque || first == E_type::list || first == E_type::flist)
        return 1 + getTypeDescLength(desc.subspan(1));

    if (first == E_type::set || first == E_type::uset)
        return 1 + getTypeDescLength(desc.subspan(1));

    if (first == E_type::map || first == E_type::umap) {
        size_t keyLen = getTypeDescLength(desc.subspan(1));
        return 1 + keyLen + getTypeDescLength(desc.subspan(1 + keyLen));
    }

    if (first == E_type::opt)
        return 1 + getTypeDescLength(desc.subspan(1));

    if (first == E_type::bitset) return 2;

    if (first == E_type::atomic)
        return 1 + getTypeDescLength(desc.subspan(1));

    if (first == E_type::array)
        return 2 + getTypeDescLength(desc.subspan(2));

    if (first == E_type::string)
        return 1;

    if (first == E_type::variant) {
        size_t pos = 1;
        while (pos < desc.size() && desc[pos] != E_type::ed)
            pos += getTypeDescLength(desc.subspan(pos));
        return pos + 1;
    }

    if (first == E_type::tuple) {
        size_t pos = 1;
        while (pos < desc.size() && desc[pos] != E_type::ed)
            pos += getTypeDescLength(desc.subspan(pos));
        return pos + 1;
    }

    if (first == E_type::cst)
        return 1 + getTypeDescLength(desc.subspan(1));

    if (first == E_type::dur)
        return 1 + getTypeDescLength(desc.subspan(1)) + 2;

    if (first == E_type::timepoint)
        return 1 + getTypeDescLength(desc.subspan(1));

    return 1;
}

// 检查 TypeDesc 是否表示自定义类型或 Base 类型
inline bool isCustomOrBaseType(std::span<const sp_meta::SpToken> desc) {
    if (desc.empty()) return false;
    auto first = desc[0];
    return (first >= static_cast<Sz>(E_type::Base) + 1 && first < static_cast<Sz>(E_type::e_customType))
        || first == E_type::Base;
}

// 检查 TypeDesc 是否表示指针类型
inline bool isPointerType(std::span<const sp_meta::SpToken> desc) {
    if (desc.empty()) return false;
    auto first = desc[0];
    return first == E_type::ptr || first == E_type::sptr || first == E_type::wptr || first == E_type::uptr;
}

// 自定义类型 ID 查找
inline const sp_meta::TypeMeta* findTypeMeta(uint32_t typeID,
    const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    auto it = typeMap.find(typeID);
    return (it != typeMap.end()) ? it->second : nullptr;
}

// 收集 variant/tuple 的子元素描述符
inline std::vector<std::span<const sp_meta::SpToken>> collectSubDescs(std::span<const sp_meta::SpToken> desc) {
    std::vector<std::span<const sp_meta::SpToken>> result;
    size_t pos = 1;
    while (pos < desc.size() && desc[pos] != E_type::ed) {
        size_t elemLen = getTypeDescLength(desc.subspan(pos));
        result.push_back(desc.subspan(pos, elemLen));
        pos += elemLen;
    }
    return result;
}