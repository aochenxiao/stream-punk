#pragma once
#include "customData.hpp"
#include <stream-punk/StreamPunk.hpp>

namespace sp {

struct Stone : public Base {
    #define Xt_Stone(X__) \
    X__(i32, row, 0) \
    X__(i32, col, 0) \
    X__(i32, player, 0)

    Stone() = default;
    UseData(Stone);
};

struct Move : public Base {
    #define Xt_Move(X__) \
    X__(i32, row, 0) \
    X__(i32, col, 0)

    Move() = default;
    UseData(Move);
};

struct GameState : public Base {
    #define Xt_GameState(X__) \
    X__(std::vector<Stone>, stones, std::vector<Stone>{}) \
    X__(i32, currentPlayer, 1) \
    X__(i32, winner, 0) \
    X__(i32, player1Id, 0) \
    X__(i32, player2Id, 0)

    GameState() = default;
    UseData(GameState);
};

} // namespace sp