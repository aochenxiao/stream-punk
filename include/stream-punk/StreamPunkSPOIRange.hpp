// Copyright (c) 2026 aochenxiao
// SPDX-License-Identifier: MIT
#pragma once
/*
    StreamPunkSPOIRange.hpp — SPOI 客户端 API (v0.1.0)

    C++ 查询代理对象和适配器。用法与 C++20/23 ranges 一致，
    但实际产生的是 SPOI 二进制指令。

    核心组件：
    - QueryField：单个字段的代理，比较运算返回可序列化的 SpoiCmpExpr
    - QueryProxy：结构体的代理，由 UseSPOI 宏生成
    - SPOIRange：查询范围，管道入口，缓冲指令并在 send 时写入 SpoiStream
    - sp::filter / sp::transform / sp::take 等适配器
    - sp::query()：创建查询入口
    - sp::send()：发送并执行查询

    依赖：StreamPunkSPOI.hpp（协议定义）
*/

#include "StreamPunkSPOI.hpp"
#include <sstream>
#include <tuple>
#include <type_traits>
#include <vector>

namespace sp {

// =============================== 前向声明 ===============================

template<typename T> struct QueryProxy;
template<typename T> struct SPOIRange;

// =============================== 类型特征：SPOI 元素类型 ===============================
// 对于 map/unordered_map，管道元素是 mapped_type（值类型），而非 value_type（pair）
// 对于其他容器，管道元素是 value_type

template<typename T, typename = void> struct spoi_elem_type { using type = T; };
template<typename T> struct spoi_elem_type<T, std::void_t<typename T::value_type>> { using type = typename T::value_type; };
template<typename K, typename V, typename... Args> struct spoi_elem_type<std::map<K, V, Args...>> { using type = V; };
template<typename K, typename V, typename... Args> struct spoi_elem_type<std::unordered_map<K, V, Args...>> { using type = V; };
template<typename T> using spoi_elem_type_t = typename spoi_elem_type<T>::type;

// =============================== QueryField — 叶子字段代理 ===============================

template<typename FieldType, u32 Idx>
struct QueryField {
    static constexpr u32 memberIdx = Idx;

    // 每个比较运算符返回 SpoiCmpExpr
    SpoiCmpExpr operator<(FieldType const& val) const {
        return makeCmpExpr(CmpOp::e_lt, val);
    }
    SpoiCmpExpr operator>(FieldType const& val) const {
        return makeCmpExpr(CmpOp::e_gt, val);
    }
    SpoiCmpExpr operator==(FieldType const& val) const {
        return makeCmpExpr(CmpOp::e_eq, val);
    }
    SpoiCmpExpr operator!=(FieldType const& val) const {
        return makeCmpExpr(CmpOp::e_ne, val);
    }
    SpoiCmpExpr operator<=(FieldType const& val) const {
        return makeCmpExpr(CmpOp::e_le, val);
    }
    SpoiCmpExpr operator>=(FieldType const& val) const {
        return makeCmpExpr(CmpOp::e_ge, val);
    }

private:
    SpoiCmpExpr makeCmpExpr(CmpOp op, FieldType const& val) const {
        SpoiCmpExpr expr;
        expr.memberIdx = memberIdx;
        expr.cmpOp = static_cast<u8>(op);
        std::stringstream ss;
        O o(ss);
        o << val;
        auto str = ss.str();
        expr.value = std::vector<u8>(str.begin(), str.end());
        return expr;
    }
};

// =============================== SPOIRange — 查询范围 ===============================

template<typename T>
struct SPOIRange {
    T*            _ptr = nullptr; // 数据指针（仅用于类型推导，不实际访问）
    std::ostream& _os;        // SPOI 输出流（仅在 send 时写入）
    std::vector<u32> _path;   // 当前路径
    std::vector<SpoiInstruction> _buf; // 指令缓冲区

    SPOIRange(T* ptr, std::ostream& os, std::vector<u32> path = {})
        : _ptr(ptr), _os(os), _path(std::move(path)) {}

    // 从引用构造（兼容旧代码）
    SPOIRange(T& ref, std::ostream& os, std::vector<u32> path = {})
        : _ptr(&ref), _os(os), _path(std::move(path)) {}

    // 缓冲 SPOI 指令
    void writeSPOI(SpoiOp op, std::vector<u8> operand = {}) {
        SpoiInstruction inst;
        inst.op = static_cast<u8>(op);
        inst.path = _path;
        inst.operand = std::move(operand);
        _buf.push_back(std::move(inst));
    }

    // 缓冲带操作数的 SPOI 指令（操作数为 Base 子类）
    template<typename OperandT>
    void writeSPOIOperand(SpoiOp op, OperandT const& operand) {
        std::stringstream opSS;
        O o(opSS);
        o << operand;
        auto str = opSS.str();
        std::vector<u8> operandBytes(str.begin(), str.end());
        writeSPOI(op, std::move(operandBytes));
    }

    // 缓冲带原始字节操作数的 SPOI 指令
    void writeSPOIRaw(SpoiOp op, std::vector<u8> operand) {
        writeSPOI(op, std::move(operand));
    }

    // 将缓冲区中的所有指令写入 SpoiStream 到输出流
    void flush() {
        if (!_buf.empty()) {
            SpoiStream stream;
            stream.instructions = std::move(_buf);
            std::stringstream ss;
            O o(ss);
            o << stream;
            auto str = ss.str();
            _os.write(str.data(), str.size());
        }
    }

    // 管道操作符：接收适配器（按值传递，同时支持左值和右值）
    template<typename Adaptor>
    auto friend operator|(SPOIRange range, Adaptor&& adaptor) {
        return adaptor.apply(std::move(range));
    }
};

// =============================== QueryRoot — 查询入口 ===============================

template<typename T>
struct QueryRoot {
    T& _ref;
    std::ostream& _os;

    QueryRoot(T& ref, std::ostream& os) : _ref(ref), _os(os) {}
};

template<typename T>
QueryRoot<T> query(T& ref, std::ostream& os) {
    return QueryRoot<T>(ref, os);
}

// =============================== 辅助函数前向声明 ===============================

// 序列化 u32 向量为原始字节
std::vector<u8> serializeIndices(std::vector<u32> const& indices);

// 提取 select 字段索引
template<typename... Ts>
std::vector<u32> extractSelectIndices(std::tuple<Ts...> const& t);

template<typename FieldType, u32 Idx>
std::vector<u32> extractSelectIndices(QueryField<FieldType, Idx> const&);

// =============================== 适配器 ===============================

// ── filter ──
template<typename Pred>
struct FilterAdaptor {
    Pred pred;

    template<typename T>
    auto apply(SPOIRange<T>&& range) -> SPOIRange<T> {
        using ElemType = spoi_elem_type_t<std::decay_t<T>>;
        QueryProxy<ElemType> proxy;
        auto expr = pred(proxy);
        range.writeSPOIOperand(SpoiOp::e_filter, expr);
        return std::move(range);
    }
};

struct FilterFn {
    template<typename Pred>
    auto operator()(Pred pred) const {
        return FilterAdaptor<Pred>{std::move(pred)};
    }
};
inline constexpr FilterFn filter;

// ── transform (select) ──
template<typename Proj>
struct TransformAdaptor {
    Proj proj;

    template<typename T>
    auto apply(SPOIRange<T>&& range) -> SPOIRange<T> {
        using ElemType = spoi_elem_type_t<std::decay_t<T>>;
        QueryProxy<ElemType> proxy;
        auto result = proj(proxy);

        // 提取字段索引并序列化为原始字节
        auto indices = extractSelectIndices(result);
        std::vector<u8> operand = serializeIndices(indices);
        range.writeSPOIRaw(SpoiOp::e_select, std::move(operand));
        return std::move(range);
    }
};

struct TransformFn {
    template<typename Proj>
    auto operator()(Proj proj) const {
        return TransformAdaptor<Proj>{std::move(proj)};
    }
};
inline constexpr TransformFn transform;

// ── take ──
struct TakeAdaptor {
    u32 n;

    template<typename T>
    auto apply(SPOIRange<T>&& range) -> SPOIRange<T> {
        std::vector<u8> operand(sizeof(u32));
        std::memcpy(operand.data(), &n, sizeof(u32));
        range.writeSPOIRaw(SpoiOp::e_take, std::move(operand));
        return std::move(range);
    }
};

struct TakeFn {
    auto operator()(u32 n) const { return TakeAdaptor{n}; }
};
inline constexpr TakeFn take;

// ── drop ──
struct DropAdaptor {
    u32 n;

    template<typename T>
    auto apply(SPOIRange<T>&& range) -> SPOIRange<T> {
        std::vector<u8> operand(sizeof(u32));
        std::memcpy(operand.data(), &n, sizeof(u32));
        range.writeSPOIRaw(SpoiOp::e_drop, std::move(operand));
        return std::move(range);
    }
};

struct DropFn {
    auto operator()(u32 n) const { return DropAdaptor{n}; }
};
inline constexpr DropFn drop;

// ── sort ──
template<typename KeyFn>
struct SortAdaptor {
    KeyFn keyFn;
    bool ascending = true;

    auto desc() { ascending = false; return *this; }

    template<typename T>
    auto apply(SPOIRange<T>&& range) -> SPOIRange<T> {
        using ElemType = spoi_elem_type_t<std::decay_t<T>>;
        QueryProxy<ElemType> proxy;
        auto field = keyFn(proxy);

        // 序列化排序参数：[memberIdx: u32] [ascending: u8]
        std::vector<u8> operand(sizeof(u32) + sizeof(u8));
        std::memcpy(operand.data(), &field.memberIdx, sizeof(u32));
        operand[sizeof(u32)] = static_cast<u8>(ascending ? 1 : 0);
        range.writeSPOIRaw(SpoiOp::e_sort, std::move(operand));
        return std::move(range);
    }
};

struct SortFn {
    template<typename KeyFn>
    auto operator()(KeyFn keyFn) const { return SortAdaptor<KeyFn>{std::move(keyFn)}; }
};
inline constexpr SortFn sort;

// ── reverse ──
struct ReverseAdaptor {
    template<typename T>
    auto apply(SPOIRange<T>&& range) const -> SPOIRange<T> {
        range.writeSPOI(SpoiOp::e_reverse);
        return std::move(range);
    }
};
inline constexpr ReverseAdaptor reverse;

// ── count ──
struct CountAdaptor {
    template<typename T>
    void apply(SPOIRange<T>&& range) const {
        range.writeSPOI(SpoiOp::e_count);
        range.writeSPOI(SpoiOp::e_exec);
        range.flush();
    }
};
inline constexpr CountAdaptor count;

// ── any ──
template<typename Pred>
struct AnyAdaptor {
    Pred pred;

    template<typename T>
    void apply(SPOIRange<T>&& range) {
        using ElemType = spoi_elem_type_t<std::decay_t<T>>;
        QueryProxy<ElemType> proxy;
        auto expr = pred(proxy);
        range.writeSPOIOperand(SpoiOp::e_any, expr);
        range.writeSPOI(SpoiOp::e_exec);
        range.flush();
    }
};

struct AnyFn {
    template<typename Pred>
    auto operator()(Pred pred) const { return AnyAdaptor<Pred>{std::move(pred)}; }
};
inline constexpr AnyFn any;

// ── all ──
template<typename Pred>
struct AllAdaptor {
    Pred pred;

    template<typename T>
    void apply(SPOIRange<T>&& range) {
        using ElemType = spoi_elem_type_t<std::decay_t<T>>;
        QueryProxy<ElemType> proxy;
        auto expr = pred(proxy);
        range.writeSPOIOperand(SpoiOp::e_all, expr);
        range.writeSPOI(SpoiOp::e_exec);
        range.flush();
    }
};

struct AllFn {
    template<typename Pred>
    auto operator()(Pred pred) const { return AllAdaptor<Pred>{std::move(pred)}; }
};
inline constexpr AllFn all;

// ── find ──
template<typename Pred>
struct FindAdaptor {
    Pred pred;

    template<typename T>
    void apply(SPOIRange<T>&& range) {
        using ElemType = spoi_elem_type_t<std::decay_t<T>>;
        QueryProxy<ElemType> proxy;
        auto expr = pred(proxy);
        range.writeSPOIOperand(SpoiOp::e_find, expr);
        range.writeSPOI(SpoiOp::e_exec);
        range.flush();
    }
};

struct FindFn {
    template<typename Pred>
    auto operator()(Pred pred) const { return FindAdaptor<Pred>{std::move(pred)}; }
};
inline constexpr FindFn find;

// ── distinct ──
struct DistinctAdaptor {
    template<typename T>
    auto apply(SPOIRange<T>&& range) const -> SPOIRange<T> {
        range.writeSPOI(SpoiOp::e_distinct);
        return std::move(range);
    }
};
inline constexpr DistinctAdaptor distinct;

// ── keys / values ──
struct KeysAdaptor {
    template<typename T>
    auto apply(SPOIRange<T>&& range) const -> SPOIRange<T> {
        range.writeSPOI(SpoiOp::e_keys);
        return std::move(range);
    }
};
inline constexpr KeysAdaptor keys;

struct ValuesAdaptor {
    template<typename T>
    auto apply(SPOIRange<T>&& range) const -> SPOIRange<T> {
        range.writeSPOI(SpoiOp::e_values);
        return std::move(range);
    }
};
inline constexpr ValuesAdaptor values;

// =============================== 类型特征：QueryField 元素类型 ===============================
// 从 QueryField<FieldType, Idx> 中提取 FieldType

template<typename T> struct query_field_type {};
template<typename FT, u32 Idx> struct query_field_type<QueryField<FT, Idx>> { using type = FT; };
template<typename T> using query_field_type_t = typename query_field_type<T>::type;

// ── join ──
template<typename MemberFn>
struct JoinAdaptor {
    MemberFn memberFn;

    template<typename T>
    auto apply(SPOIRange<T>&& range) {
        using ElemType = spoi_elem_type_t<std::decay_t<T>>;
        QueryProxy<ElemType> proxy;
        auto field = memberFn(proxy);

        // 提取 join 后的容器类型和元素类型
        using MemberContainerType = query_field_type_t<std::decay_t<decltype(field)>>;
        using JoinedElemType = spoi_elem_type_t<MemberContainerType>;

        std::vector<u8> operand(sizeof(u32));
        std::memcpy(operand.data(), &field.memberIdx, sizeof(u32));

        // 创建新的 SPOIRange，类型变为 join 后的元素类型对应的容器
        // 使用 std::vector<JoinedElemType> 作为新的容器类型（仅用于类型推导）
        using NewContainerType = std::vector<JoinedElemType>;
        SPOIRange<NewContainerType> newRange((NewContainerType*)nullptr, range._os, range._path);
        newRange._buf = std::move(range._buf);
        newRange.writeSPOIRaw(SpoiOp::e_join, std::move(operand));
        return newRange;
    }
};

struct JoinFn {
    template<typename MemberFn>
    auto operator()(MemberFn memberFn) const { return JoinAdaptor<MemberFn>{std::move(memberFn)}; }
};
inline constexpr JoinFn join;

// ── C++23 ranges ──
struct EnumerateAdaptor {
    u32 start = 0;
    template<typename T>
    auto apply(SPOIRange<T>&& range) -> SPOIRange<T> {
        std::vector<u8> operand(sizeof(u32));
        std::memcpy(operand.data(), &start, sizeof(u32));
        range.writeSPOIRaw(SpoiOp::e_enumerate, std::move(operand));
        return std::move(range);
    }
};
struct EnumerateFn {
    auto operator()(u32 start = 0) const { return EnumerateAdaptor{start}; }
};
inline constexpr EnumerateFn enumerate;

struct ChunkAdaptor {
    u32 size;
    template<typename T>
    auto apply(SPOIRange<T>&& range) -> SPOIRange<T> {
        std::vector<u8> operand(sizeof(u32));
        std::memcpy(operand.data(), &size, sizeof(u32));
        range.writeSPOIRaw(SpoiOp::e_chunk, std::move(operand));
        return std::move(range);
    }
};
struct ChunkFn {
    auto operator()(u32 size) const { return ChunkAdaptor{size}; }
};
inline constexpr ChunkFn chunk;

struct SlideAdaptor {
    u32 size;
    template<typename T>
    auto apply(SPOIRange<T>&& range) -> SPOIRange<T> {
        std::vector<u8> operand(sizeof(u32));
        std::memcpy(operand.data(), &size, sizeof(u32));
        range.writeSPOIRaw(SpoiOp::e_slide, std::move(operand));
        return std::move(range);
    }
};
struct SlideFn {
    auto operator()(u32 size) const { return SlideAdaptor{size}; }
};
inline constexpr SlideFn slide;

struct StrideAdaptor {
    u32 step;
    template<typename T>
    auto apply(SPOIRange<T>&& range) -> SPOIRange<T> {
        std::vector<u8> operand(sizeof(u32));
        std::memcpy(operand.data(), &step, sizeof(u32));
        range.writeSPOIRaw(SpoiOp::e_stride, std::move(operand));
        return std::move(range);
    }
};
struct StrideFn {
    auto operator()(u32 step) const { return StrideAdaptor{step}; }
};
inline constexpr StrideFn stride;

template<u32 N>
struct AdjacentAdaptor {
    template<typename T>
    auto apply(SPOIRange<T>&& range) -> SPOIRange<T> {
        std::vector<u8> operand(sizeof(u32));
        u32 n = N;
        std::memcpy(operand.data(), &n, sizeof(u32));
        range.writeSPOIRaw(SpoiOp::e_adjacent, std::move(operand));
        return std::move(range);
    }
};
template<u32 N>
struct AdjacentFn {
    auto operator()() const { return AdjacentAdaptor<N>{}; }
};
template<u32 N>
inline constexpr AdjacentFn<N> adjacent;

// ── send（终操作）──
struct SendFn {
    template<typename T>
    void apply(SPOIRange<T>&& range) const {
        range.writeSPOI(SpoiOp::e_exec);
        range.flush();
    }
};
inline constexpr SendFn send;

// =============================== 辅助函数 ===============================

// 序列化 u32 向量为原始字节：[count: u32][idx0: u32][idx1: u32]...
inline std::vector<u8> serializeIndices(std::vector<u32> const& indices) {
    std::vector<u8> result;
    u32 count = static_cast<u32>(indices.size());
    result.resize(sizeof(u32) * (1 + indices.size()));
    std::memcpy(result.data(), &count, sizeof(u32));
    for (size_t i = 0; i < indices.size(); ++i) {
        std::memcpy(result.data() + sizeof(u32) * (1 + i), &indices[i], sizeof(u32));
    }
    return result;
}

// 提取 select 字段索引：tuple 版本
template<typename... Ts>
std::vector<u32> extractSelectIndices(std::tuple<Ts...> const& t) {
    return std::apply([](auto const&... fields) {
        return std::vector<u32>{static_cast<u32>(fields.memberIdx)...};
    }, t);
}

// 提取 select 字段索引：单字段版本
template<typename FieldType, u32 Idx>
std::vector<u32> extractSelectIndices(QueryField<FieldType, Idx> const&) {
    return {Idx};
}

// =============================== UseSPOI 宏 ===============================

// 生成 QueryProxy 成员的辅助宏
#define X_spoiProxyMember(type__, name__, ...) \
    QueryField<type__, _SpType::M::E::e_##name__> name__;

// 生成 QueryRoot 成员的辅助宏
#define X_spoiRootMember(type__, name__, ...) \
    SPOIRange<type__> name__;

// 生成 QueryRoot 构造函数初始化列表
#define X_spoiRootMemberInit(type__, name__, ...) \
    , name__(&_ref.name__, _os, {_SpType::M::E::e_##name__})

// UseSPOI：为类型生成 QueryProxy 和 QueryRoot 特化
#define UseSPOI(TypeName__, Xt__) \
    template<> struct QueryProxy<TypeName__> { \
        using _SpType = TypeName__; \
        Xt__(X_spoiProxyMember) \
    }; \
    template<> struct QueryRoot<TypeName__> { \
        TypeName__& _ref; \
        std::ostream& _os; \
        using _SpType = TypeName__; \
        Xt__(X_spoiRootMember) \
        \
        QueryRoot(TypeName__& ref, std::ostream& os) \
            : _ref(ref), _os(os) \
            Xt__(X_spoiRootMemberInit) \
        {} \
    };

} // namespace sp