#pragma once

#include "../GameData.hpp"
#include <vector>
#include <string>
#include <utility>

// ===== 游戏常量 =====
constexpr f64 GRAVITY      = 300.0;   // 重力加速度 (px/s^2)
constexpr f64 WIND_FACTOR  = 80.0;    // 风力系数
constexpr f64 EXPLOSION_R  = 40.0;    // 爆炸半径
constexpr f64 MAX_DAMAGE   = 40.0;    // 最大伤害
constexpr f64 TERRAIN_W    = 800.0;   // 地形宽度
constexpr f64 TERRAIN_H    = 500.0;   // 地形最大高度
constexpr i32 TURN_TIME    = 30;      // 回合时间（秒）
constexpr i32 PHYSICS_FPS  = 120;     // 物理帧率
constexpr f64 PHYSICS_DT   = 1.0 / PHYSICS_FPS;

constexpr f64 MAX_MOVE_DISTANCE  = 60.0;  // 每回合最大移动距离
constexpr f64 MOVE_STEP          = 5.0;   // 每次移动步长
constexpr f64 MAX_CLIMB_HEIGHT   = 15.0;  // 最大可攀爬高度差
constexpr f64 ANGLE_MIN          = -90.0;
constexpr f64 ANGLE_MAX          = 90.0;
constexpr f64 POWER_MIN          = 10.0;
constexpr f64 POWER_MAX          = 100.0;
constexpr f64 WORM_OFFSET_Y      = 15.0;  // 虫底部离地高度

// ===== 武器类型 =====
enum class WeaponType : i32 {
    Standard = 0,  // 标准炮弹：中等半径、中等伤害
    Heavy    = 1,  // 重型炮弹：大半径、高伤害、深弹坑
    Cluster  = 2,  // 集束弹：小半径×3、低伤害、浅弹坑
};

struct WeaponParams {
    f64 explosionRadius;
    f64 maxDamage;
    f64 craterDepthScale;  // 弹坑深度系数
};

inline WeaponParams getWeaponParams(i32 weapon) {
    switch (static_cast<WeaponType>(weapon)) {
        case WeaponType::Heavy:
            return { 70.0, 70.0, 1.0 };   // 大爆炸
        case WeaponType::Cluster:
            return { 25.0, 25.0, 0.4 };   // 小集束
        default: // Standard
            return { 40.0, 40.0, 0.7 };   // 标准
    }
}

// ===== 物理结果 =====
struct PhysicsResult {
    std::vector<TrajectoryPoint> trajectory;
    Explosion explosion;
    std::vector<std::pair<size_t, f64>> wormDamage;   // (wormIndex, newHp)
    std::vector<std::pair<size_t, f64>> terrainHoles; // (x, newHeight)
};

// ===== 地形生成 =====
std::vector<f64> generateTerrain();

// ===== 物理引擎 =====
PhysicsResult simulateProjectile(
    f64 startX, f64 startY, f64 worldAngle, f64 power,
    const std::vector<f64>& terrain,
    const Wind& wind,
    const std::vector<Worm>& worms,
    size_t shooterIdx,
    i32 weapon = 0);

// ===== 游戏状态 =====
void initGameState(GameState& state, const std::vector<std::string>& playerNames);
void advanceTurn(GameState& state);

// ===== 角度与朝向 =====
f64 clampAngle(f64 angle);
f64 toWorldAngle(f64 relativeAngle, bool facingRight);

// ===== 移动 =====
bool tryMoveWorm(Worm& worm, i8 dir, const std::vector<f64>& terrain);

// ===== 重力 =====
// 地形变化后，让悬空的虫掉到新地面上
void applyWormGravity(std::vector<Worm>& worms, const std::vector<f64>& terrain);
