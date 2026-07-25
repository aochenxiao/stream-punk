#include "test_common.hpp"
#include <doctest/doctest.h>

TEST_SUITE("Terrain") {
    TEST_CASE("generateTerrain produces reasonable heights") {
        auto terrain = generateTerrain();
        CHECK(terrain.size() == 800);

        f64 minH = terrain[0];
        f64 maxH = terrain[0];
        for (auto h : terrain) {
            CHECK(!std::isnan(h));
            if (h < minH) minH = h;
            if (h > maxH) maxH = h;
        }
        CHECK(minH > 100.0);
        CHECK(maxH < 400.0);
    }

    TEST_CASE("new terrain is smoother than old bumpy terrain") {
        auto terrain = generateTerrain();
        f64 totalDiff = 0.0;
        for (size_t i = 1; i < terrain.size(); i++) {
            totalDiff += std::fabs(terrain[i] - terrain[i - 1]);
        }
        f64 avgDiff = totalDiff / static_cast<f64>(terrain.size() - 1);
        // 旧地形平均相邻差约 5~8；新地形应明显小于 5
        CHECK(avgDiff < 5.0);
    }
}
