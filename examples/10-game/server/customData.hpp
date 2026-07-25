#pragma once

// 类型注册表（仿照库的 customData.hpp）
// 只做注册：前向声明 + TypeDesc 特化
// 类型定义在 GameData.hpp 中

struct Vec2;
struct Wind;
struct Worm;
struct Explosion;
struct TrajectoryPoint;
struct GameState;
struct RoomInfo;
struct LobbyMessage;

// TypeDesc 现在在 sp 命名空间，特化必须在 sp 内
// 类型本身在全局命名空间，用 ::Vec2 限定
namespace sp {

#define Xt_GameTypeDesc(X__) \
X__(::Vec2)            \
X__(::Wind)            \
X__(::Worm)            \
X__(::Explosion)       \
X__(::TrajectoryPoint) \
X__(::GameState)       \
X__(::RoomInfo)        \
X__(::LobbyMessage)

#define X_GEN_TYPEDESC(T) \
template<> struct TypeDesc<T> { \
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) }; \
};

Xt_GameTypeDesc(X_GEN_TYPEDESC)

#undef X_GEN_TYPEDESC
#undef Xt_GameTypeDesc

} // namespace sp