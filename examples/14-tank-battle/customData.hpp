#pragma once

namespace sp {
struct Vec2;
struct PlayerInput;
struct Bullet;
struct PlayerState;
struct GameState;
}

#define Xt_CustomType(X__) \
X__(sp::Vec2, Vec2) \
X__(sp::PlayerInput, PlayerInput) \
X__(sp::Bullet, Bullet) \
X__(sp::PlayerState, PlayerState) \
X__(sp::GameState, GameState)