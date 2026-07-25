// Copyright (c) 2026 aochenxiao
// SPDX-License-Identifier: MIT
#pragma once
/*
    StreamPunkSPOIBuilder.hpp — SPOI 查询构建器 (v0.1.0)

    Builder 风格 API，提供与 pipe 风格等效但更面向对象的查询写法。
    用法示例：

        sp::builder(q.players)
            .where([](auto& p) { return p.hp < 50; })
            .select([](auto& p) { return std::tie(p.name, p.hp); })
            .take(10)
            .send();

    与 pipe 风格对比：

        Pipe:   q.players | sp::filter(...) | sp::transform(...) | sp::take(10) | sp::send();
        Builder: sp::builder(q.players).where(...).select(...).take(10).send();

    注意：
    - 每个方法返回新的 Builder（按值），因此可以安全链式调用
    - where/select/sort 等中间操作不触发 send，需要显式调用 .send() 或聚合终操作
    - Buidler 是对现有 Adaptor 的薄封装，不引入额外开销

    依赖：StreamPunkSPOIRange.hpp（SPOIRange 和所有适配器）
*/

#include "StreamPunkSPOIRange.hpp"

namespace sp {

// =============================== SPOIQueryBuilder ===============================

template<typename RangeT>
class SPOIQueryBuilder {
    RangeT _range;

public:
    explicit SPOIQueryBuilder(RangeT range) : _range(std::move(range)) {}

    // ── 筛选 ──

    /// 条件筛选（对应 pipe 的 sp::filter）
    template<typename Pred>
    auto where(Pred pred) {
        auto newRange = sp::filter(std::move(pred)).apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    // ── 投影 ──

    /// 字段投影（对应 pipe 的 sp::transform）
    /// 单字段：.select([](auto& p) { return p.name; })
    /// 多字段：.select([](auto& p) { return std::tie(p.name, p.hp); })
    template<typename Proj>
    auto select(Proj proj) {
        auto newRange = sp::transform(std::move(proj)).apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    // ── 排序 ──

    /// 排序（对应 pipe 的 sp::sort）
    /// ascending 默认为 true，传 false 为降序
    template<typename KeyFn>
    auto sort(KeyFn keyFn, bool ascending = true) {
        SortAdaptor<KeyFn> adaptor{std::move(keyFn)};
        if (!ascending) adaptor.desc();
        auto newRange = adaptor.apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    /// 反转（对应 pipe 的 sp::reverse）
    auto reverse() {
        auto newRange = sp::reverse.apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    // ── 分页 ──

    /// 取前 N 个（对应 pipe 的 sp::take）
    auto take(u32 n) {
        auto newRange = sp::take(n).apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    /// 跳过前 N 个（对应 pipe 的 sp::drop）
    auto drop(u32 n) {
        auto newRange = sp::drop(n).apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    // ── 去重 ──

    /// 去重（对应 pipe 的 sp::distinct）
    auto distinct() {
        auto newRange = sp::distinct.apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    // ── 容器操作 ──

    /// 取 map 的 keys（对应 pipe 的 sp::keys）
    auto keys() {
        auto newRange = sp::keys.apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    /// 取 map 的 values（对应 pipe 的 sp::values）
    auto values() {
        auto newRange = sp::values.apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    /// 展平嵌套容器（对应 pipe 的 sp::join）
    /// 注意：join 会改变 SPOIRange 的元素类型，因此返回新类型的 Builder
    template<typename MemberFn>
    auto join(MemberFn memberFn) {
        auto newRange = sp::join(std::move(memberFn)).apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    // ── C++23 ranges ──

    /// 枚举（对应 pipe 的 sp::enumerate）
    auto enumerate(u32 start = 0) {
        auto newRange = sp::enumerate(start).apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    /// 分块（对应 pipe 的 sp::chunk）
    auto chunk(u32 size) {
        auto newRange = sp::chunk(size).apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    /// 滑动窗口（对应 pipe 的 sp::slide）
    auto slide(u32 size) {
        auto newRange = sp::slide(size).apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    /// 步长（对应 pipe 的 sp::stride）
    auto stride(u32 step) {
        auto newRange = sp::stride(step).apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    /// 相邻元素组（对应 pipe 的 sp::adjacent<N>）
    template<u32 N>
    auto adjacent() {
        auto newRange = sp::adjacent<N>().apply(std::move(_range));
        return SPOIQueryBuilder<decltype(newRange)>(std::move(newRange));
    }

    // ── 聚合（终操作，执行后立即 flush）──

    /// 计数（终操作）
    void count() {
        sp::count.apply(std::move(_range));
    }

    /// 存在性检查（终操作）
    template<typename Pred>
    void any(Pred pred) {
        sp::any(std::move(pred)).apply(std::move(_range));
    }

    /// 全量检查（终操作）
    template<typename Pred>
    void all(Pred pred) {
        sp::all(std::move(pred)).apply(std::move(_range));
    }

    /// 查找第一个匹配（终操作）
    template<typename Pred>
    void find(Pred pred) {
        sp::find(std::move(pred)).apply(std::move(_range));
    }

    // ── 发送（终操作）──

    /// 发送结果并执行（终操作）
    void send() {
        sp::send.apply(std::move(_range));
    }
};

// =============================== 工厂函数 ===============================

/// 从 SPOIRange 创建 Builder
template<typename T>
SPOIQueryBuilder<SPOIRange<T>> builder(SPOIRange<T> range) {
    return SPOIQueryBuilder<SPOIRange<T>>(std::move(range));
}

} // namespace sp