#pragma once
#define Xt_CustomType(X__) \
X__(sp::Vec2, Vec2) \
X__(sp::PlayerState, PlayerState)

#include <stream-punk/StreamPunk.hpp>

namespace sp {

struct Vec2 : public Base {
    #define Xt_Vec2(X__) \
    X__(f64, x, 0.0) \
    X__(f64, y, 0.0)

    Vec2() = default;
    UseData(Vec2);
};

struct PlayerState : public Base {
    #define Xt_PlayerState(X__) \
    X__(i32, pid, 0) \
    X__(f64, px, 0.0) \
    X__(f64, py, 0.0) \
    X__(f64, prot, 0.0) \
    X__(i32, php, 100)

    PlayerState() = default;
    UseData(PlayerState);
};

} // namespace sp