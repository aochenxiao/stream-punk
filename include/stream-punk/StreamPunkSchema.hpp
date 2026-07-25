// Copyright (c) 2026 aochenxiao
// SPDX-License-Identifier: MIT
#pragma once
// ===================================================================
// StreamPunkSchema.hpp — 跨语言 Schema 元数据生成 (v0.3.2)
//
// 将 TypeDesc 编译期类型描述符转换为 JSON Schema，供 JS/Python 等
// 动态语言端使用。动态语言端只需一套固定的解析代码，根据 Schema
// 即可通用地解析所有 C++ 类型的数据。
//
// 用法：
//   1. C++ 端调用 buildAllSchemas() 生成 Schema JSON
//   2. 将 Schema JSON 发送给 JS/Python 端
//   3. JS/Python 端使用 Schema 注册并解析后续数据
// ===================================================================
#include "StreamPunkJson.hpp"
#include <unordered_map>
#include <span>
#include <cstring>

// ============================== token → 名称映射 ==============================

namespace sp {

// tokenToName 直接复用 Xt_BasicType / Xt_template 的枚举名做字符串化，
// 仅对显示名与枚举名不一致的例外做显式覆盖（bl→"bool", opt→"optional"）。
inline const char* tokenToName(SpToken t) {
    // 例外：显示名与枚举名不同，if-else 避免与 X 宏生成的 case 冲突
    if (t == E_type::bl)  return "bool";
    if (t == E_type::opt) return "optional";

    switch (t) {
        #define X_tn_basic(type, name) case E_type::name: return #name;
        Xt_BasicType(X_tn_basic)
        Xt_template(X_tn_basic)
        #undef X_tn_basic
        case E_type::ptr:       return "ptr";
        case E_type::voidPtr:   return "voidPtr";
        case E_type::cst:       return "cst";
        case E_type::dur:       return "dur";
        case E_type::timepoint: return "timepoint";
        case E_type::Base:      return "Base";
        default: return nullptr;
    }
}

// 返回 {消耗的 token 数, 类型 schema 的 JsonVal}
inline std::pair<size_t, JsonVal> walkTypeDesc(std::span<const SpToken> tokens) {
    if (tokens.empty()) return {0, JsonVal{}};
    SpToken t = tokens[0];

    // ---- 基础类型（1 token） ----
    const char* name = tokenToName(t);
    if (name != nullptr && t >= E_type::u8 && t <= E_type::bl) {
        return {1, JsonVal{std::string(name)}};
    }

    // ---- 单 token 类型（无参数） ----
    switch (t) {
        case E_type::path:      return {1, JsonVal{"path"}};
        case E_type::voidPtr:   return {1, JsonVal{"voidPtr"}};
        case E_type::Base:      return {1, JsonVal{"Base"}};
    }

    // ---- dur: [dur, Rep_desc..., num, den] ----
    if (t == E_type::dur) {
        auto [repN, repSchema] = walkTypeDesc(tokens.subspan(1));
        JsonVal arr;
        arr.type = JsonVal::Array;
        arr.arrVal.push_back(JsonVal{"dur"});
        arr.arrVal.push_back(std::move(repSchema));
        JsonVal numVal, denVal;
        numVal.type = JsonVal::Int;
        numVal.iVal = tokens[1 + repN];
        denVal.type = JsonVal::Int;
        denVal.iVal = tokens[1 + repN + 1];
        arr.arrVal.push_back(std::move(numVal));
        arr.arrVal.push_back(std::move(denVal));
        return {1 + repN + 2, std::move(arr)};
    }

    // ---- timepoint: [timepoint, Duration_desc...] ----
    if (t == E_type::timepoint) {
        auto [durN, durSchema] = walkTypeDesc(tokens.subspan(1));
        JsonVal arr;
        arr.type = JsonVal::Array;
        arr.arrVal.push_back(JsonVal{"timepoint"});
        arr.arrVal.push_back(std::move(durSchema));
        return {1 + durN, std::move(arr)};
    }

    // ---- 1 个类型参数: vector, deque, list, flist, set, uset, sptr, wptr, uptr, opt, atomic, ptr, cst, string ----
    switch (t) {
        case E_type::string:
        case E_type::vector:
        case E_type::deque:
        case E_type::list:
        case E_type::flist:
        case E_type::set:
        case E_type::uset:
        case E_type::sptr:
        case E_type::wptr:
        case E_type::uptr:
        case E_type::opt:
        case E_type::atomic:
        case E_type::ptr:
        case E_type::cst: {
            auto [subN, subSchema] = walkTypeDesc(tokens.subspan(1));
            JsonVal arr;
            arr.type = JsonVal::Array;
            arr.arrVal.push_back(JsonVal{std::string(tokenToName(t))});
            arr.arrVal.push_back(std::move(subSchema));
            return {1 + subN, std::move(arr)};
        }
    }

    // ---- 2 个类型参数: map, umap ----
    switch (t) {
        case E_type::map:
        case E_type::umap: {
            auto [n1, key] = walkTypeDesc(tokens.subspan(1));
            auto [n2, val] = walkTypeDesc(tokens.subspan(1 + n1));
            JsonVal arr;
            arr.type = JsonVal::Array;
            arr.arrVal.push_back(JsonVal{std::string(tokenToName(t))});
            arr.arrVal.push_back(std::move(key));
            arr.arrVal.push_back(std::move(val));
            return {1 + n1 + n2, std::move(arr)};
        }
    }

    // ---- array<N, T>: 3 tokens (array, N, T) ----
    if (t == E_type::array) {
        if (tokens.size() < 2) return {1, JsonVal{"array"}};
        Sz N = tokens[1];
        auto [subN, subSchema] = walkTypeDesc(tokens.subspan(2));
        JsonVal arr;
        arr.type = JsonVal::Array;
        arr.arrVal.push_back(JsonVal{"array"});
        arr.arrVal.push_back(JsonVal{static_cast<i64>(N)});
        arr.arrVal.push_back(std::move(subSchema));
        return {2 + subN, std::move(arr)};
    }

    // ---- bitset<N>: 2 tokens ----
    if (t == E_type::bitset) {
        if (tokens.size() < 2) return {1, JsonVal{"bitset"}};
        Sz N = tokens[1];
        JsonVal arr;
        arr.type = JsonVal::Array;
        arr.arrVal.push_back(JsonVal{"bitset"});
        arr.arrVal.push_back(JsonVal{static_cast<i64>(N)});
        return {2, std::move(arr)};
    }

    // ---- variant / tuple: N 个类型参数，以 ed 终止 ----
    if (t == E_type::variant || t == E_type::tuple) {
        JsonVal arr;
        arr.type = JsonVal::Array;
        arr.arrVal.push_back(JsonVal{std::string(tokenToName(t))});
        size_t pos = 1;
        while (pos < tokens.size() && tokens[pos] != static_cast<SpToken>(E_type::ed)) {
            auto [subN, subSchema] = walkTypeDesc(tokens.subspan(pos));
            arr.arrVal.push_back(std::move(subSchema));
            pos += subN;
        }
        if (pos < tokens.size() && tokens[pos] == static_cast<SpToken>(E_type::ed)) {
            ++pos; // 跳过 ed
        }
        return {pos, std::move(arr)};
    }

    // ---- 自定义类型：通过 TypeID 查找类名 ----
    {
        auto& map = typeID2ClassName();
        auto it = map.find(static_cast<Sz>(t));
        if (it != map.end()) {
            return {1, JsonVal{it->second}};
        }
    }

    // 未知类型
    return {1, JsonVal{std::string("unknown_") + std::to_string(t)}};
}

// ============================== 类型 Schema 构建 ==============================

// 为单个自定义类型生成完整的字段级 Schema
inline JsonVal buildTypeSchema(Base const& example) {
    JsonVal result;
    result.type = JsonVal::Object;

    auto className = example.getClassName();
    result["name"] = className;

    // 基类名（如果不是 Base）
    auto baseName = example.getBaseName();
    if (baseName && baseName[0] != '\0' && std::strcmp(baseName, "Base") != 0) {
        result["base"] = baseName;
    }

    // 字段列表
    auto desc = example.getDesc();
    auto memberNames = example.getMemberNames();
    auto memberCount = example.getMemberCount();

    JsonVal fields;
    fields.type = JsonVal::Array;

    if (memberCount > 0 && !desc.empty()) {
        // 跳过基类部分的 TypeDesc
        size_t offset = 0;
        auto [baseConsumed, baseSchema] = walkTypeDesc(desc.subspan(offset));
        offset += baseConsumed;

        // 遍历每个成员
        for (size_t i = 0; i < memberCount && offset < desc.size(); ++i) {
            auto [consumed, typeSchema] = walkTypeDesc(desc.subspan(offset));
            offset += consumed;

            JsonVal field;
            field.type = JsonVal::Object;
            if (i < memberNames.size()) {
                field["name"] = memberNames[i];
            }
            field["type"] = std::move(typeSchema);
            fields.arrVal.push_back(std::move(field));
        }
    }

    result["fields"] = std::move(fields);
    return result;
}

// 为一批类型实例生成完整的 Schema JSON
inline JsonVal buildSchemas(std::span<Base* const> examples) {
    JsonVal result;
    result.type = JsonVal::Object;
    result["schemaVer"] = "0.3.2";

    JsonVal types;
    types.type = JsonVal::Object;

    for (auto* ex : examples) {
        if (ex) {
            auto schema = buildTypeSchema(*ex);
            types[schema["name"].strVal] = std::move(schema);
        }
    }

    result["types"] = std::move(types);
    return result;
}

} // namespace sp