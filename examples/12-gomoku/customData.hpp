#pragma once

namespace sp {
struct Stone;
struct Move;
struct GameState;
}

#define Xt_CustomType(X__) \
X__(sp::Stone, Stone) \
X__(sp::Move, Move) \
X__(sp::GameState, GameState)