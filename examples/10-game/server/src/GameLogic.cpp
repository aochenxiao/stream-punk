#include "../include/GameLogic.hpp"

#include <cmath>
#include <random>
#include <algorithm>

// ===== 随机数 =====
static std::mt19937 rng(std::random_device{}());

static f64 randRange(f64 min, f64 max) {
    return std::uniform_real_distribution<f64>(min, max)(rng);
}

static i32 randInt(i32 min, i32 max) {
    return std::uniform_int_distribution<i32>(min, max)(rng);
}

// ===== 地形生成 =====
std::vector<f64> generateTerrain() {
    std::vector<f64> terrain(static_cast<size_t>(TERRAIN_W));
    f64 base = 250.0;
    for (size_t i = 0; i < terrain.size(); i++) {
        f64 x = static_cast<f64>(i) / TERRAIN_W;
        f64 noise = std::sin(x * 6.283185307) * 50.0
                  + std::sin(x * 12.566370614 + 1.0) * 12.0;
        terrain[i] = base + noise;
    }
    return terrain;
}

// ===== 物理引擎 =====
PhysicsResult simulateProjectile(
    f64 startX, f64 startY, f64 worldAngle, f64 power,
    const std::vector<f64>& terrain,
    const Wind& wind,
    const std::vector<Worm>& worms,
    size_t shooterIdx,
    i32 weapon)
{
    PhysicsResult result;

    auto wp = getWeaponParams(weapon);
    f64 boomR = wp.explosionRadius;

    f64 rad = worldAngle * 3.141592653589793 / 180.0;
    f64 vx = std::cos(rad) * power * 3.0;
    f64 vy = std::sin(rad) * power * 3.0;  // Y-up: positive = up
    f64 px = startX, py = startY;

    result.trajectory.push_back(TrajectoryPoint{});
    result.trajectory.back().x = px;
    result.trajectory.back().y = py;

    bool hit = false;
    f64 hitX = 0, hitY = 0;

    while (!hit && px >= 0 && px < TERRAIN_W && py < TERRAIN_H * 1.5) {
        vx += wind.direction * wind.strength * WIND_FACTOR * PHYSICS_DT;
        vy -= GRAVITY * PHYSICS_DT;  // Y-up: gravity pulls down
        px += vx * PHYSICS_DT;
        py += vy * PHYSICS_DT;

        result.trajectory.push_back(TrajectoryPoint{});
        result.trajectory.back().x = px;
        result.trajectory.back().y = py;

        // 碰撞检测：Y低于地形高度即命中
        i32 ix = static_cast<i32>(px);
        if (ix >= 0 && ix < static_cast<i32>(terrain.size()) && py <= terrain[ix]) {
            hit = true;
            hitX = px;
            hitY = terrain[ix];
        }

        if (result.trajectory.size() > 500) break;
    }

    // 爆炸
    result.explosion.cx = hitX;
    result.explosion.cy = hitY;
    result.explosion.radius = boomR;
    result.explosion.damage = wp.maxDamage;

    // 地形破坏（圆形截面弹坑）
    f64 craterR2 = boomR * boomR;
    for (i32 x = static_cast<i32>(hitX - boomR);
         x <= static_cast<i32>(hitX + boomR); x++) {
        if (x < 0 || x >= static_cast<i32>(terrain.size())) continue;
        f64 dx = x - hitX;
        f64 dist2 = dx * dx;
        if (dist2 > craterR2) continue;
        f64 depth = std::sqrt(craterR2 - dist2) * wp.craterDepthScale;
        f64 newH = terrain[x] - depth;
        if (std::abs(newH - terrain[x]) > 0.01) {
            result.terrainHoles.push_back({static_cast<size_t>(x), newH});
        }
    }

    // 虫伤害
    for (size_t i = 0; i < worms.size(); i++) {
        if (i == shooterIdx || !worms[i].alive) continue;
        f64 dx = worms[i].x - hitX;
        f64 dy = worms[i].y - hitY;
        f64 dist = std::sqrt(dx * dx + dy * dy);
        if (dist < boomR) {
            f64 dmg = wp.maxDamage * (1.0 - dist / boomR);
            f64 newHp = std::max(0.0, worms[i].hp - dmg);
            result.wormDamage.push_back({i, newHp});
        }
    }

    return result;
}

// ===== 游戏初始化 =====
void initGameState(GameState& state, const std::vector<std::string>& playerNames) {
    state.terrain = generateTerrain();
    state.phase = "aiming";
    state.currentTurn = 0;
    state.turnTimeLeft = TURN_TIME;
    state.winner = -1;
    state.worms.clear();
    state.trajectory.clear();
    state.explosions.clear();

    state.wind.direction = randRange(-1.0, 1.0);
    state.wind.strength = randRange(0.0, 1.0);

    f64 spacing = TERRAIN_W / (playerNames.size() + 1);
    for (size_t i = 0; i < playerNames.size(); i++) {
        Worm w;
        w.name = playerNames[i];
        w.hp = 100.0;
        w.x = spacing * (i + 1);
        i32 ix = static_cast<i32>(w.x);
        if (ix >= 0 && ix < static_cast<i32>(state.terrain.size())) {
            w.y = state.terrain[ix] + WORM_OFFSET_Y;
        }
        w.alive = true;
        w.color = static_cast<i32>(i);
        w.angle = 45.0;
        w.power = 50.0;
        w.facingRight = (i == 0); // 玩家1朝右，玩家2朝左（互相对峙）
        w.movedThisTurn = 0.0;
        state.worms.push_back(w);
    }
}

// ===== 回合推进 =====
void advanceTurn(GameState& state) {
    i32 aliveCount = 0;
    i32 lastAlive = -1;
    for (size_t i = 0; i < state.worms.size(); i++) {
        if (state.worms[i].alive) {
            aliveCount++;
            lastAlive = static_cast<i32>(i);
        }
    }
    if (aliveCount <= 1) {
        state.phase = "gameover";
        state.winner = lastAlive;
        return;
    }

    i32 next = state.currentTurn;
    do {
        next = (next + 1) % static_cast<i32>(state.worms.size());
    } while (!state.worms[next].alive);

    state.currentTurn = next;
    state.phase = "aiming";
    state.turnTimeLeft = TURN_TIME;
    state.wind.direction = randRange(-1.0, 1.0);
    state.wind.strength = randRange(0.0, 1.0);
    state.trajectory.clear();

    // 重置每回合移动距离
    for (auto& worm : state.worms) {
        worm.movedThisTurn = 0.0;
    }
}

// ===== 角度与朝向 =====
f64 clampAngle(f64 angle) {
    return std::clamp(angle, ANGLE_MIN, ANGLE_MAX);
}

f64 toWorldAngle(f64 relativeAngle, bool facingRight) {
    if (facingRight) {
        return relativeAngle;
    }
    // facing left: 0 -> 180 (left), +90 -> 90 (up), -90 -> 270/-90 (down)
    return 180.0 - relativeAngle;
}

// ===== 移动 =====
bool tryMoveWorm(Worm& worm, i8 dir, const std::vector<f64>& terrain) {
    if (dir != 1 && dir != -1) return false;
    if (worm.movedThisTurn + MOVE_STEP > MAX_MOVE_DISTANCE) return false;

    f64 oldX = worm.x;
    f64 targetX = oldX + dir * MOVE_STEP;
    if (targetX < 0.0) targetX = 0.0;
    if (targetX >= static_cast<f64>(terrain.size())) targetX = static_cast<f64>(terrain.size()) - 1.0;

    i32 currentIx = static_cast<i32>(oldX);
    i32 targetIx = static_cast<i32>(targetX);
    if (currentIx < 0) currentIx = 0;
    if (currentIx >= static_cast<i32>(terrain.size())) currentIx = static_cast<i32>(terrain.size()) - 1;
    if (targetIx < 0) targetIx = 0;
    if (targetIx >= static_cast<i32>(terrain.size())) targetIx = static_cast<i32>(terrain.size()) - 1;

    f64 currentGround = terrain[currentIx];
    f64 targetGround = terrain[targetIx];

    // 被陡坡挡住：只能向上爬 MAX_CLIMB_HEIGHT
    if (targetGround - currentGround > MAX_CLIMB_HEIGHT) return false;

    worm.x = targetX;
    worm.y = targetGround + WORM_OFFSET_Y;
    worm.movedThisTurn += std::abs(targetX - oldX);
    worm.facingRight = (dir > 0);
    return true;
}

// ===== 重力 =====
void applyWormGravity(std::vector<Worm>& worms, const std::vector<f64>& terrain) {
    for (auto& worm : worms) {
        if (!worm.alive) continue;
        i32 ix = static_cast<i32>(worm.x);
        if (ix < 0) ix = 0;
        if (ix >= static_cast<i32>(terrain.size())) ix = static_cast<i32>(terrain.size()) - 1;
        f64 groundY = terrain[ix] + WORM_OFFSET_Y;
        if (worm.y > groundY + 0.5) {
            worm.y = groundY;
        }
    }
}
