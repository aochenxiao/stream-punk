#include "test_common.hpp"
#include <doctest/doctest.h>

TEST_SUITE("Physics") {
    TEST_CASE("simulateProjectile produces a trajectory") {
        std::vector<f64> terrain(800, 250.0);
        Wind wind;
        wind.direction = 0.0;
        wind.strength = 0.0;
        std::vector<Worm> worms;

        auto result = simulateProjectile(100.0, 265.0, 45.0, 50.0, terrain, wind, worms, 0);

        CHECK(result.trajectory.size() > 10);
        CHECK(result.trajectory.front().x == 100.0);
        CHECK(result.trajectory.front().y == 265.0);
        CHECK(result.explosion.radius > 0.0);
    }

    TEST_CASE("projectile arcs upward then downward") {
        std::vector<f64> terrain(800, 300.0); // high flat terrain so it lands
        Wind wind;
        wind.direction = 0.0;
        wind.strength = 0.0;
        std::vector<Worm> worms;

        auto result = simulateProjectile(100.0, 315.0, 60.0, 80.0, terrain, wind, worms, 0);
        REQUIRE(result.trajectory.size() >= 3);

        f64 startY = result.trajectory.front().y;
        f64 peakY = startY;
        for (const auto& pt : result.trajectory) {
            if (pt.y > peakY) peakY = pt.y;
        }
        CHECK(peakY > startY + 10.0); // must rise noticeably
        CHECK(result.trajectory.back().y <= peakY); // then descend
    }

    TEST_CASE("explosion damages terrain") {
        std::vector<f64> terrain(800, 250.0);
        Wind wind;
        wind.direction = 0.0;
        wind.strength = 0.0;
        std::vector<Worm> worms;

        auto result = simulateProjectile(100.0, 265.0, 0.0, 80.0, terrain, wind, worms, 0);

        CHECK(!result.terrainHoles.empty());

        // 弹坑中心应该明显低于原始地形
        bool hasDeepCrater = false;
        for (const auto& [x, newH] : result.terrainHoles) {
            // 中心区域至少下降 10
            f64 dx = static_cast<f64>(x) - result.explosion.cx;
            if (std::abs(dx) < EXPLOSION_R * 0.3) {
                CHECK(newH < terrain[x] - 10.0);
                hasDeepCrater = true;
            }
            // 边缘隆起不超过 5
            CHECK(newH < terrain[x] + 5.0);
        }
        CHECK(hasDeepCrater);
    }
}
