#pragma once

// 示例 13：多人协同画板 — 类型定义
// 展示 StreamPunk 核心优势：
//   1. UseData — 定义一次，自动获得序列化/反序列化
//   2. UseSPOIShadow — 自动追踪字段变更，生成增量同步指令
//   3. 二进制协议 — 紧凑高效，一个笔画仅传输实际数据
//   4. 增量同步 — 新笔画 append 到 vector，仅发送新增部分
//
// 所有类型定义在 namespace sp 中，TypeDesc 在前向声明后手动特化

#include <stream-punk/StreamPunk.hpp>
#include <stream-punk/StreamPunkJson.hpp>
#include <stream-punk/StreamPunkSPOI.hpp>
#include <stream-punk/StreamPunkSPOIRange.hpp>
#include <stream-punk/StreamPunkSPOIShadow.hpp>

namespace sp {

// 前向声明（供 TypeDesc 特化使用）
struct StrokePoint;
struct Stroke;
struct WhiteboardState;

// TypeDesc 特化 — 必须在类型定义之前，使 TypeDesc<vector<T>> 能展开
// 参考 10-game 的 customData.hpp 模式
template<> struct TypeDesc<StrokePoint> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};
template<> struct TypeDesc<Stroke> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};
template<> struct TypeDesc<WhiteboardState> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};

// ===== 笔画点 =====
struct StrokePoint : public Base {
    #define Xt_StrokePoint(X__) \
    X__(f64, x, 0.0) \
    X__(f64, y, 0.0)
    UseData(StrokePoint);
};

// ===== 笔画 =====
// tool: 0=pen, 1=rect, 2=circle, 3=line, 4=eraser
struct Stroke : public Base {
    #define Xt_Stroke(X__) \
    X__(std::vector<StrokePoint>, points, {}) \
    X__(u32,                      color,  0xFF000000) \
    X__(f64,                      width,  2.0) \
    X__(u8,                       tool,   0)
    UseData(Stroke);
};

// ===== 画板状态（核心同步对象） =====
struct WhiteboardState : public Base {
    #define Xt_WhiteboardState(X__) \
    X__(std::vector<Stroke>, strokes, {})
    UseData(WhiteboardState);
};

// SPOI / SPOI Shadow
UseSPOI(StrokePoint, Xt_StrokePoint);
UseSPOI(Stroke, Xt_Stroke);
UseSPOI(WhiteboardState, Xt_WhiteboardState);
UseSPOIShadow(WhiteboardState, Xt_WhiteboardState);

} // namespace sp