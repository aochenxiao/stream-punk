// Copyright (c) 2026 aochenxiao
// SPDX-License-Identifier: MIT
#pragma once
/*
    StreamPunkSPOIShadow.hpp — SPOI 写操作代理 (v0.1.0)

    替代已废弃的 Delta Shadow 模式（StreamPunkShadow.hpp），用 SPOI 指令格式进行写操作。
    用法与现有 Shadow 一致，但生成的是 SPOI 指令流而非 DeltaNode 链表。

    迁移对照：
      Delta                      →  SPOI
      ─────────────────────────────────────────────
      #include "StreamPunkShadow.hpp"  →  #include "StreamPunkSPOIShadow.hpp"
      makeShadow(obj, os)              →  spoi(obj, os)
      shadow.x = val                   →  shadow.x = val（用法相同）
      shadow.x += delta                →  shadow.x += delta（用法相同）
      DeltaNode                       →  SpoiInstruction
      DeltaReader                     →  SPOIExecutor

    核心组件：
    - SPOIShadowField：字段代理，赋值时缓冲 SPOI SET 指令；对容器类型自动启用 append/remove
    - SPOIRoot：SPOI 代理根，析构时将缓冲的指令写入 SpoiStream

    依赖：StreamPunkSPOI.hpp（协议定义）
*/

#include "StreamPunkSPOI.hpp"
#include <sstream>
#include <vector>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace sp {

// =============================== SPOI 写操作代理 ===============================

// 字段代理：赋值时缓冲 SPOI SET 指令
// 对容器类型（std::vector<T> 等），通过 requires 自动启用 append/remove
// 对数值类型，通过 SPOIShadowNumField 子类额外支持 +=
template<typename FieldType>
struct SPOIShadowField {
    std::vector<SpoiInstruction>& _buf;   // 共享指令缓冲区
    std::vector<u32>             _path;

    void operator=(FieldType const& val) {
        if constexpr (is_optional_v<FieldType>) {
            if (val.has_value()) {
                // optional 有值：序列化内部值（不含 has_value 标志），与 Executor 的解包逻辑一致
                std::stringstream valSS;
                O o(valSS);
                o << *val;
                auto valStr = valSS.str();
                std::vector<u8> operand(valStr.begin(), valStr.end());

                SpoiInstruction inst;
                inst.op = static_cast<u8>(SpoiOp::e_set);
                inst.path = _path;
                inst.typeDesc = makeTypeDesc<typename FieldType::value_type>();
                inst.operand = std::move(operand);
                _buf.push_back(std::move(inst));
            } else {
                // optional 无值：发送 RESET 指令
                SpoiInstruction inst;
                inst.op = static_cast<u8>(SpoiOp::e_reset);
                inst.path = _path;
                _buf.push_back(std::move(inst));
            }
        } else {
            // 非 optional：序列化值
            std::stringstream valSS;
            O o(valSS);
            o << val;
            auto valStr = valSS.str();
            std::vector<u8> operand(valStr.begin(), valStr.end());

            SpoiInstruction inst;
            inst.op = static_cast<u8>(SpoiOp::e_set);
            inst.path = _path;
            inst.typeDesc = makeTypeDesc<FieldType>();
            inst.operand = std::move(operand);
            _buf.push_back(std::move(inst));
        }
    }

    // ===== 容器操作（仅当 FieldType 有 value_type 且不是 std::string 时可用）=====

    template<typename U = FieldType> requires (requires { typename U::value_type; } && !is_string_v<U>)
    void append(typename U::value_type const& elem) {
        std::stringstream valSS;
        O o(valSS);
        o << elem;
        auto valStr = valSS.str();
        std::vector<u8> operand(valStr.begin(), valStr.end());

        SpoiInstruction inst;
        inst.op = static_cast<u8>(SpoiOp::e_append);
        inst.path = _path;
        inst.typeDesc = makeTypeDesc<typename U::value_type>();
        inst.operand = std::move(operand);
        _buf.push_back(std::move(inst));
    }

    template<typename U = FieldType> requires (requires { typename U::value_type; } && !is_string_v<U>)
    void remove(u32 idx) {
        SpoiInstruction inst;
        inst.op = static_cast<u8>(SpoiOp::e_remove);
        inst.path = _path;
        inst.path.push_back(idx);  // executor 从 path 末尾读取元素索引
        _buf.push_back(std::move(inst));
    }

    // ===== 字符串操作（仅当 FieldType 为 std::string 时可用）=====
    // 约定（与 Executor 的 is_string_v 分支一一对应）：
    //   - append(chunk)          e_append   operand = 序列化(std::string chunk)
    //   - insert(pos, chunk)     e_insert   path += [pos]，operand = 序列化(chunk)
    //   - erase(pos, len)        e_remove   path += [pos]，operand = 序列化(u32 len)
    //   - replace(pos,len,chunk) e_replace  path += [pos]，operand = 序列化(u32 len) + 序列化(chunk)
    //   - move(from,len,to)      e_move     path += [from, len, to]，operand 为空
    // 序列化均复用 StreamPunk 的 O/I 流，与 e_set 对字符串的编码格式一致（u32 裸 4 字节、string 为 u32 长度 + 字节）。

private:
    // 将若干参数按 SP 流格式序列化为 operand 字节
    template<typename... Args>
    static std::vector<u8> _strOperand(Args const&... args) {
        std::stringstream ss;
        O o(ss);
        (o << ... << args);
        auto s = ss.str();
        return std::vector<u8>(s.begin(), s.end());
    }

public:
    template<typename U = FieldType> requires is_string_v<U>
    void append(std::string_view chunk) {
        SpoiInstruction inst;
        inst.op = static_cast<u8>(SpoiOp::e_append);
        inst.path = _path;
        inst.operand = _strOperand(std::string(chunk));
        _buf.push_back(std::move(inst));
    }

    template<typename U = FieldType> requires is_string_v<U>
    void insert(u32 pos, std::string_view chunk) {
        SpoiInstruction inst;
        inst.op = static_cast<u8>(SpoiOp::e_insert);
        inst.path = _path;
        inst.path.push_back(pos);
        inst.operand = _strOperand(std::string(chunk));
        _buf.push_back(std::move(inst));
    }

    template<typename U = FieldType> requires is_string_v<U>
    void erase(u32 pos, u32 len = 1) {
        SpoiInstruction inst;
        inst.op = static_cast<u8>(SpoiOp::e_remove);
        inst.path = _path;
        inst.path.push_back(pos);
        inst.operand = _strOperand(len);
        _buf.push_back(std::move(inst));
    }

    template<typename U = FieldType> requires is_string_v<U>
    void replace(u32 pos, u32 len, std::string_view chunk) {
        SpoiInstruction inst;
        inst.op = static_cast<u8>(SpoiOp::e_replace);
        inst.path = _path;
        inst.path.push_back(pos);
        inst.operand = _strOperand(len, std::string(chunk));
        _buf.push_back(std::move(inst));
    }

    template<typename U = FieldType> requires is_string_v<U>
    void move(u32 fromPos, u32 len, u32 toPos) {
        SpoiInstruction inst;
        inst.op = static_cast<u8>(SpoiOp::e_move);
        inst.path = _path;
        inst.path.push_back(fromPos);
        inst.path.push_back(len);
        inst.path.push_back(toPos);
        _buf.push_back(std::move(inst));
    }
};

// 数值字段代理：支持 +=（ADD 指令）
template<typename FieldType>
struct SPOIShadowNumField : SPOIShadowField<FieldType> {
    using SPOIShadowField<FieldType>::_buf;
    using SPOIShadowField<FieldType>::_path;

    void operator+=(FieldType const& delta) {
        std::stringstream valSS;
        O o(valSS);
        o << delta;
        auto valStr = valSS.str();
        std::vector<u8> operand(valStr.begin(), valStr.end());

        SpoiInstruction inst;
        inst.op = static_cast<u8>(SpoiOp::e_add);
        inst.path = _path;
        inst.typeDesc = makeTypeDesc<FieldType>();
        inst.operand = std::move(operand);
        _buf.push_back(std::move(inst));
    }
};

// =============================== sp::spoi() — 创建 SPOI 代理 ===============================

// 前向声明（实际定义由 UseSPOIShadow 宏生成特化）
template<typename T>
struct SPOIRoot;

template<typename T>
SPOIRoot<T> spoi(T& ref, std::ostream& os) {
    return SPOIRoot<T>(ref, os);
}

// =============================== UseSPOIShadow 宏 ===============================

// 生成 SPOIRoot 特化（写操作代理）
// 对每个成员生成相应的代理字段，赋值时缓冲指令，析构时写入 SpoiStream

#define UseSPOIShadow(TypeName__, Xt__) \
    template<> struct SPOIRoot<TypeName__> { \
        TypeName__&   _ref; \
        std::ostream& _os; \
        std::vector<SpoiInstruction> _buf; \
        using _SpType = TypeName__; \
        \
        SPOIRoot(TypeName__& ref, std::ostream& os) \
            : _ref(ref), _os(os) \
            Xt__(X_spoiShadowFieldInit) \
        {} \
        \
        ~SPOIRoot() { \
            if (!_buf.empty()) { \
                SpoiStream stream; \
                stream.instructions = std::move(_buf); \
                std::stringstream ss; \
                O o(ss); \
                o << stream; \
                auto str = ss.str(); \
                _os.write(str.data(), str.size()); \
            } \
        } \
        \
        /* 禁止拷贝，确保析构只触发一次 */ \
        SPOIRoot(SPOIRoot const&) = delete; \
        SPOIRoot& operator=(SPOIRoot const&) = delete; \
        SPOIRoot(SPOIRoot&&) = default; \
        SPOIRoot& operator=(SPOIRoot&&) = default; \
        \
        Xt__(X_spoiShadowField) \
    };

#define X_spoiShadowFieldInit(type__, name__, ...) \
    , name__(_buf, {_SpType::M::E::e_##name__})

#define X_spoiShadowField(type__, name__, ...) \
    SPOIShadowField<type__> name__;

#define X_spoiShadowNumField(type__, name__, ...) \
    SPOIShadowNumField<type__> name__;

} // namespace sp