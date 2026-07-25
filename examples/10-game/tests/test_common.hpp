#pragma once

#include "../server/GameData.hpp"
#include "../server/include/GameLogic.hpp"
#include <vector>
#include <cmath>

inline bool approxEq(f64 a, f64 b, f64 eps = 1e-6) {
    return std::fabs(a - b) < eps;
}

inline f64 terrainHeightAt(const std::vector<f64>& terrain, f64 x) {
    i32 ix = static_cast<i32>(x);
    if (ix < 0) ix = 0;
    if (ix >= static_cast<i32>(terrain.size()) - 1) ix = static_cast<i32>(terrain.size()) - 2;
    f64 t = x - ix;
    return terrain[ix] * (1.0 - t) + terrain[ix + 1] * t;
}
