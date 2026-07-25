#pragma once

#include "../WhiteboardData.hpp"
#include "WebSocket.hpp"
#include <winsock2.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>

// ===== 消息协议常量 =====
enum class MsgType : sp::u8 {
    // 服务器 → 客户端
    FullState   = 0x01,  // 全量 WhiteboardState（UseData 二进制）
    SpoiDelta   = 0x02,  // SPOI 增量指令（仅变更部分）
    SystemMsg   = 0x04,  // 系统消息（JSON 文本：用户加入/离开）

    // 客户端 → 服务器
    DrawStroke  = 0x10,  // 新笔画（Stroke 二进制）
    ClearBoard  = 0x11,  // 清空画板
    DeleteStroke= 0x12,  // 删除笔画（u32 index）
    CursorMove  = 0x13,  // 光标位置（f64 x, f64 y）
};

// ===== 房间结构 =====
struct Room {
    std::string roomId;
    sp::WhiteboardState state;
    std::vector<WebSocket*> clients;
    std::vector<std::string> userNames;
    std::vector<sp::u32> userColors;
};

// ===== 全局状态 =====
extern std::map<std::string, Room> rooms;
extern std::mutex roomsMutex;
extern std::vector<std::unique_ptr<WebSocket>> allClients;

// ===== 用户颜色 =====
constexpr sp::u32 USER_COLORS[] = {
    0xFFE53E3E,  // 红
    0xFF3182CE,  // 蓝
    0xFF38A169,  // 绿
    0xFFD69E2E,  // 黄
    0xFF805AD5,  // 紫
    0xFFDD6B20,  // 橙
    0xFFE53E8E,  // 粉
    0xFF00B5D8,  // 青
};

// ===== 通信函数 =====
bool wsSendBinary(WebSocket* ws, MsgType type, const std::vector<sp::u8>& payload);
bool wsSendText(WebSocket* ws, MsgType type, const std::string& payload);
void sendFullState(WebSocket* ws, const sp::WhiteboardState& state);
void broadcastToRoom(Room& room, MsgType type, const std::vector<sp::u8>& payload);
void broadcastToRoomExcept(Room& room, MsgType type, const std::vector<sp::u8>& payload, WebSocket* exclude);

// ===== 房间管理 =====
void handleJoinRoom(WebSocket* ws, const std::string& roomId, const std::string& userName);
void handleDrawStroke(Room& room, WebSocket* ws, const std::vector<sp::u8>& data);
void handleClearBoard(Room& room, WebSocket* ws);
void handleDeleteStroke(Room& room, WebSocket* ws, const std::vector<sp::u8>& data);
void handleCursorMove(Room& room, WebSocket* ws, const std::vector<sp::u8>& data);
void removeClientFromRoom(WebSocket* ws);
void broadcastStateDelta(Room& room);