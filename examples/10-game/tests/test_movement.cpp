#include "test_common.hpp"
#include <doctest/doctest.h>

TEST_SUITE("Movement") {
    TEST_CASE("clampAngle clamps to [-90, 90]") {
        CHECK(approxEq(clampAngle(0.0), 0.0));
        CHECK(approxEq(clampAngle(-90.0), -90.0));
        CHECK(approxEq(clampAngle(90.0), 90.0));
        CHECK(approxEq(clampAngle(-95.0), -90.0));
        CHECK(approxEq(clampAngle(95.0), 90.0));
    }

    TEST_CASE("toWorldAngle converts relative angle based on facing") {
        CHECK(approxEq(toWorldAngle(45.0, true), 45.0));
        CHECK(approxEq(toWorldAngle(0.0, true), 0.0));
        CHECK(approxEq(toWorldAngle(90.0, true), 90.0));

        CHECK(approxEq(toWorldAngle(45.0, false), 135.0));
        CHECK(approxEq(toWorldAngle(0.0, false), 180.0));
        CHECK(approxEq(toWorldAngle(90.0, false), 90.0));
        CHECK(approxEq(toWorldAngle(-45.0, false), 225.0));
    }

    TEST_CASE("tryMoveWorm moves on flat terrain") {
        std::vector<f64> terrain(800, 250.0);
        Worm worm;
        worm.x = 100.0;
        worm.y = 265.0;
        worm.facingRight = true;
        worm.movedThisTurn = 0.0;

        bool ok = tryMoveWorm(worm, 1, terrain);
        CHECK(ok);
        CHECK(approxEq(worm.x, 105.0));
        CHECK(approxEq(worm.y, 265.0)); // flat, y unchanged
        CHECK(approxEq(worm.movedThisTurn, 5.0));
        CHECK(worm.facingRight == true);
    }

    TEST_CASE("tryMoveWorm is blocked by steep cliff") {
        std::vector<f64> terrain(800, 250.0);
        // create a steep cliff at x=105
        for (size_t i = 105; i < 110; i++) {
            terrain[i] = 280.0;
        }
        Worm worm;
        worm.x = 100.0;
        worm.y = 265.0;
        worm.facingRight = true;
        worm.movedThisTurn = 0.0;

        bool ok = tryMoveWorm(worm, 1, terrain);
        CHECK(!ok);
        CHECK(approxEq(worm.x, 100.0));
        CHECK(approxEq(worm.movedThisTurn, 0.0));
    }

    TEST_CASE("tryMoveWorm climbs gentle slope") {
        std::vector<f64> terrain(800, 250.0);
        // gentle slope: 5px rise over 5px step is at the limit
        terrain[105] = 255.0;
        Worm worm;
        worm.x = 100.0;
        worm.y = 265.0;
        worm.facingRight = true;
        worm.movedThisTurn = 0.0;

        bool ok = tryMoveWorm(worm, 1, terrain);
        CHECK(ok);
        CHECK(approxEq(worm.y, 270.0)); // terrain[105] + 15
    }

    TEST_CASE("tryMoveWorm stops at max move distance") {
        std::vector<f64> terrain(800, 250.0);
        Worm worm;
        worm.x = 100.0;
        worm.y = 265.0;
        worm.facingRight = true;
        worm.movedThisTurn = 58.0;

        bool ok = tryMoveWorm(worm, 1, terrain);
        CHECK(!ok); // 58 + 5 = 63 > 60
        CHECK(approxEq(worm.x, 100.0));
    }

    TEST_CASE("tryMoveWorm turns worm left") {
        std::vector<f64> terrain(800, 250.0);
        Worm worm;
        worm.x = 100.0;
        worm.y = 265.0;
        worm.facingRight = true;
        worm.movedThisTurn = 0.0;

        bool ok = tryMoveWorm(worm, -1, terrain);
        CHECK(ok);
        CHECK(worm.facingRight == false);
    }

    TEST_CASE("advanceTurn resets movedThisTurn") {
        GameState state;
        Worm w1, w2;
        w1.name = "a"; w1.alive = true; w1.movedThisTurn = 50.0;
        w2.name = "b"; w2.alive = true; w2.movedThisTurn = 30.0;
        state.worms.push_back(w1);
        state.worms.push_back(w2);
        state.currentTurn = 0;

        advanceTurn(state);
        CHECK(approxEq(state.worms[0].movedThisTurn, 0.0));
        CHECK(approxEq(state.worms[1].movedThisTurn, 0.0));
    }

    TEST_CASE("applyWormGravity drops floating worm to ground") {
        std::vector<f64> terrain(800, 250.0);
        std::vector<Worm> worms;
        Worm w;
        w.x = 100.0;
        w.y = 350.0;  // floating way above terrain (250 + 15 = 265)
        w.alive = true;
        worms.push_back(w);

        applyWormGravity(worms, terrain);
        CHECK(approxEq(worms[0].y, 265.0)); // should drop to terrain[100] + 15
    }

    TEST_CASE("applyWormGravity does not push worm below ground") {
        std::vector<f64> terrain(800, 250.0);
        std::vector<Worm> worms;
        Worm w;
        w.x = 100.0;
        w.y = 265.0;  // already on ground
        w.alive = true;
        worms.push_back(w);

        applyWormGravity(worms, terrain);
        CHECK(approxEq(worms[0].y, 265.0)); // unchanged
    }

    TEST_CASE("applyWormGravity ignores dead worms") {
        std::vector<f64> terrain(800, 250.0);
        std::vector<Worm> worms;
        Worm w;
        w.x = 100.0;
        w.y = 350.0;
        w.alive = false;
        worms.push_back(w);

        applyWormGravity(worms, terrain);
        CHECK(approxEq(worms[0].y, 350.0)); // dead, unchanged
    }
}
