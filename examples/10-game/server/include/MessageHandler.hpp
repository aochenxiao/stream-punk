#pragma once

#include "../GameData.hpp"
#include "GameLogic.hpp"
#include "WebSocket.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <chrono>

// ===== 消息协议常量 =====
enum class MsgType : u8 {
    // 服务器 → 客户端
    FullState    = 0x01,  // 全量 GameState
    SpoiDelta    = 0x02,  // SPOI 增量指令
    Trajectory   = 0x03,  // 弹道结果
    SystemMsg    = 0x04,  // 系统消息（加入/离开/准备）
    // 客户端 → 服务器
    SpoiCommand  = 0x10,  // SPOI 操作指令
    LobbyAction  = 0x11,  // 大厅操作
};

// ===== 房间管理 =====
struct PendingExplosion {
    PhysicsResult result;
    std::chrono::steady_clock::time_point applyAt;
    size_t shooterIdx;
};

struct Room {
    std::string roomId;
    GameState state;
    std::vector<WebSocket*> clients;
    std::vector<std::string> playerNames;
    std::vector<bool> ready;
    bool gameStarted = false;
    std::chrono::steady_clock::time_point turnStart;
    std::vector<PendingExplosion> pendingExplosions;
};

struct ReceivedMsg {
    MsgType type;
    std::vector<u8> payload;
};

// ===== 全局状态 =====
extern std::map<std::string, Room> rooms;
extern std::mutex roomsMutex;
extern std::vector<std::unique_ptr<WebSocket>> allClients;

// ===== 消息处理函数 =====
void handleSpoiCommand(GameState& state, const std::vector<u8>& data);

// WebSocket 通信
bool wsSendBinary(WebSocket* ws, MsgType type, const std::vector<u8>& payload);
bool wsSendText(WebSocket* ws, MsgType type, const std::string& payload);
void sendFullState(WebSocket* ws, const GameState& state);
void sendSystemMsg(WebSocket* ws, const std::string& msg);
void broadcastToRoom(Room& room, MsgType type, const std::vector<u8>& payload);
ReceivedMsg recvMessage(WebSocket* ws);

// 消息处理
void handleLobbyMessage(WebSocket* ws, const ReceivedMsg& msg);
void startGame(Room& room);
void broadcastStateDelta(Room& room);
void processPendingExplosions(Room& room);
bool canAct(const Room& room, i32 playerIdx, WebSocket* ws);
void handleRoomMessage(const std::string& roomId, Room& room, i32 playerIdx,
                       WebSocket* ws, const ReceivedMsg& msg);