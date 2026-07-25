#pragma once

// 示例 10：StreamWorms 游戏类型定义
// 展示：C++ 定义游戏数据模型（唯一真理来源），通过 sp-gen 自动生成 JS 等价类型
// StreamPunk 亮点：UseData 自动序列化 + UseSPOI 跨语言操作协议

#include <stream-punk/StreamPunk.hpp>
#include <stream-punk/StreamPunkJson.hpp>
#include <stream-punk/StreamPunkSPOI.hpp>
#include <stream-punk/StreamPunkSPOIRange.hpp>
#include <stream-punk/StreamPunkSPOIShadow.hpp>
#include <stream-punk/StreamPunkSPOIExecutor.hpp>

// 注册表（前向声明 + TypeDesc 特化，必须在类型定义之前）
#include "customData.hpp"

using namespace sp;

// ===== 基础类型 =====

struct Vec2 : public Base {
    #define Xt_Vec2(X__) \
    X__(f64, x, 0.0) \
    X__(f64, y, 0.0)
    UseData(Vec2);
};
namespace sp { UseSPOI(::Vec2, Xt_Vec2); }

struct Wind : public Base {
    #define Xt_Wind(X__) \
    X__(f64, direction, 0.0) \
    X__(f64, strength, 0.0)
    UseData(Wind);
};
namespace sp { UseSPOI(::Wind, Xt_Wind); }

// ===== 游戏实体 =====

struct Worm : public Base {
    #define Xt_Worm(X__) \
    X__(std::string, name,        "") \
    X__(f64,         hp,          100.0) \
    X__(f64,         x,           0.0) \
    X__(f64,         y,           0.0) \
    X__(f64,         angle,       45.0) \
    X__(f64,         power,       50.0) \
    X__(i32,         weapon,      0) \
    X__(bool,        alive,       true) \
    X__(i32,         color,       0) \
    X__(bool,        facingRight, true) \
    X__(f64,         movedThisTurn, 0.0)
    UseData(Worm);
};
namespace sp { UseSPOI(::Worm, Xt_Worm); }

struct Explosion : public Base {
    #define Xt_Explosion(X__) \
    X__(f64, cx,     0.0) \
    X__(f64, cy,     0.0) \
    X__(f64, radius, 0.0) \
    X__(f64, damage, 0.0)
    UseData(Explosion);
};
namespace sp { UseSPOI(::Explosion, Xt_Explosion); }

struct TrajectoryPoint : public Base {
    #define Xt_TrajectoryPoint(X__) \
    X__(f64, x, 0.0) \
    X__(f64, y, 0.0)
    UseData(TrajectoryPoint);
};
namespace sp { UseSPOI(::TrajectoryPoint, Xt_TrajectoryPoint); }

// ===== 游戏状态（核心同步对象） =====

struct GameState : public Base {
    #define Xt_GameState(X__) \
    X__(std::vector<Worm>,             worms,           {}) \
    X__(std::vector<f64>,              terrain,         {}) \
    X__(Wind,                          wind,            Wind{}) \
    X__(i32,                           currentTurn,     0) \
    X__(std::vector<TrajectoryPoint>,  trajectory,      {}) \
    X__(std::vector<Explosion>,        explosions,       {}) \
    X__(std::string,                   phase,           "waiting") \
    X__(i32,                           turnTimeLeft,    30) \
    X__(i32,                           winner,          -1)
    UseData(GameState);
};
namespace sp {
    UseSPOI(::GameState, Xt_GameState);
    UseSPOIShadow(::GameState, Xt_GameState);
}

// ===== 大厅类型 =====

struct RoomInfo : public Base {
    #define Xt_RoomInfo(X__) \
    X__(std::string, roomId,      "") \
    X__(i32,         playerCount, 0) \
    X__(i32,         maxPlayers,  4) \
    X__(std::string, status,      "waiting")
    UseData(RoomInfo);
};
namespace sp { UseSPOI(::RoomInfo, Xt_RoomInfo); }

struct LobbyMessage : public Base {
    #define Xt_LobbyMessage(X__) \
    X__(std::string, type,    "") \
    X__(std::string, roomId,  "") \
    X__(std::string, playerName, "") \
    X__(std::string, payload, "")
    UseData(LobbyMessage);
};
namespace sp { UseSPOI(::LobbyMessage, Xt_LobbyMessage); }