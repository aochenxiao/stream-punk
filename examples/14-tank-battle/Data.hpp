#pragma once
#include "customData.hpp"
#include <stream-punk/StreamPunk.hpp>

namespace sp {

struct Vec2 : public Base {
    #define Xt_Vec2(X__) \
    X__(f64, x, 0.0) \
    X__(f64, y, 0.0)

    Vec2() = default;
    UseData(Vec2);
};

struct PlayerInput : public Base {
    #define Xt_PlayerInput(X__) \
    X__(bl, up, false) \
    X__(bl, down, false) \
    X__(bl, left, false) \
    X__(bl, right, false) \
    X__(bl, fire, false)

    PlayerInput() = default;
    UseData(PlayerInput);
};

struct Bullet : public Base {
    #define Xt_Bullet(X__) \
    X__(f64, x, 0.0) \
    X__(f64, y, 0.0) \
    X__(f64, vx, 0.0) \
    X__(f64, vy, 0.0) \
    X__(i32, ownerId, 0)

    Bullet() = default;
    UseData(Bullet);
};

struct PlayerState : public Base {
    #define Xt_PlayerState(X__) \
    X__(i32, id, 0) \
    X__(f64, x, 0.0) \
    X__(f64, y, 0.0) \
    X__(f64, rotation, 0.0) \
    X__(i32, hp, 100)

    PlayerState() = default;
    UseData(PlayerState);
};

struct GameState : public Base {
    #define Xt_GameState(X__) \
    X__(std::vector<PlayerState>, players, std::vector<PlayerState>{}) \
    X__(std::vector<Bullet>, bullets, std::vector<Bullet>{})

    GameState() = default;
    UseData(GameState);
};

} // namespace sp