#pragma once
namespace sp {
struct Vec2;
struct PlayerState;
}
#define Xt_CustomType(X__) \
X__(sp::Vec2, Vec2) \
X__(sp::PlayerState, PlayerState)