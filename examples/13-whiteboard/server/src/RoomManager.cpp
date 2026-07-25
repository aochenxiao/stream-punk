#include "../include/RoomManager.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

// ===== 全局状态 =====
std::map<std::string, Room> rooms;
std::mutex roomsMutex;
std::vector<std::unique_ptr<WebSocket>> allClients;

// ===== 通信底层 =====

bool wsSendBinary(WebSocket* ws, MsgType type, const std::vector<sp::u8>& payload) {
    sp::u32 totalLen = 1 + static_cast<sp::u32>(payload.size());
    std::vector<sp::u8> packet;
    packet.reserve(4 + totalLen);
    packet.push_back(static_cast<sp::u8>(totalLen & 0xFF));
    packet.push_back(static_cast<sp::u8>((totalLen >> 8) & 0xFF));
    packet.push_back(static_cast<sp::u8>((totalLen >> 16) & 0xFF));
    packet.push_back(static_cast<sp::u8>((totalLen >> 24) & 0xFF));
    packet.push_back(static_cast<sp::u8>(type));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return ws->sendBinary(packet);
}

bool wsSendText(WebSocket* ws, MsgType type, const std::string& payload) {
    std::vector<sp::u8> p(payload.begin(), payload.end());
    return wsSendBinary(ws, type, p);
}

void sendFullState(WebSocket* ws, const sp::WhiteboardState& state) {
    // StreamPunk 亮点：UseData 自动序列化，一行代码完成全量状态打包
    std::stringstream ss;
    sp::O o(ss);
    o << state;
    auto str = ss.str();
    std::vector<sp::u8> payload(str.begin(), str.end());
    wsSendBinary(ws, MsgType::FullState, payload);
}

void broadcastToRoom(Room& room, MsgType type, const std::vector<sp::u8>& payload) {
    for (auto client : room.clients) {
        wsSendBinary(client, type, payload);
    }
}

void broadcastToRoomExcept(Room& room, MsgType type, const std::vector<sp::u8>& payload, WebSocket* exclude) {
    for (auto client : room.clients) {
        if (client != exclude) {
            wsSendBinary(client, type, payload);
        }
    }
}

// ===== SPOI Shadow 增量同步 =====
// StreamPunk 亮点：UseSPOIShadow 自动追踪字段变更，
// 仅序列化变更部分，不是全量 State

void broadcastStateDelta(Room& room) {
    std::stringstream deltaStream;
    {
        auto shadow = sp::spoi(room.state, deltaStream);
        shadow.strokes = room.state.strokes;
    }
    auto deltaStr = deltaStream.str();
    std::vector<sp::u8> payload(deltaStr.begin(), deltaStr.end());
    broadcastToRoom(room, MsgType::SpoiDelta, payload);
}

// ===== 房间管理 =====

void handleJoinRoom(WebSocket* ws, const std::string& roomId, const std::string& userName) {
    std::cout << "[DEBUG handleJoinRoom] acquiring lock..." << std::endl;
    std::lock_guard<std::mutex> lock(roomsMutex);
    std::cout << "[DEBUG handleJoinRoom] lock acquired, roomId=" << roomId << std::endl;

    auto& room = rooms[roomId];
    if (room.roomId.empty()) {
        room.roomId = roomId;
    }

    sp::u32 color = USER_COLORS[room.clients.size() % 8];
    room.clients.push_back(ws);
    room.userNames.push_back(userName);
    room.userColors.push_back(color);

    std::cout << "[房间 " << roomId << "] " << userName << " 加入 ("
              << room.clients.size() << "人在线)\n" << std::flush;

    std::cout << "[DEBUG handleJoinRoom] calling sendFullState..." << std::endl;
    sendFullState(ws, room.state);
    std::cout << "[DEBUG handleJoinRoom] sendFullState returned" << std::endl;

    std::string joinMsg = "{\"type\":\"joined\",\"roomId\":\"" + roomId +
        "\",\"userIndex\":" + std::to_string(room.clients.size() - 1) +
        ",\"color\":" + std::to_string(color) + "}";
    wsSendText(ws, MsgType::SystemMsg, joinMsg);

    std::string userList = "{\"type\":\"userList\",\"users\":[";
    for (size_t i = 0; i < room.userNames.size(); i++) {
        if (i > 0) userList += ",";
        userList += "{\"name\":\"" + room.userNames[i] +
            "\",\"color\":" + std::to_string(room.userColors[i]) + "}";
    }
    userList += "]}";
    for (auto c : room.clients) {
        wsSendText(c, MsgType::SystemMsg, userList);
    }
}

void handleDrawStroke(Room& room, WebSocket* ws, const std::vector<sp::u8>& data) {
    std::stringstream ss(std::string(data.begin(), data.end()));
    sp::I i(ss);
    sp::Stroke newStroke;
    i >> newStroke;

    room.state.strokes.push_back(newStroke);

    std::cout << "[房间 " << room.roomId << "] 新笔画 tool="
              << static_cast<int>(newStroke.tool)
              << " points=" << newStroke.points.size() << "\n" << std::flush;

    // StreamPunk 亮点：SPOI Shadow 增量同步 —
    // 只发送新增的这一个笔画，不是全量 strokes 数组
    std::stringstream deltaStream;
    {
        auto shadow = sp::spoi(room.state, deltaStream);
        shadow.strokes.append(newStroke);  // 增量追加
    }
    auto deltaStr = deltaStream.str();
    std::vector<sp::u8> payload(deltaStr.begin(), deltaStr.end());
    broadcastToRoomExcept(room, MsgType::SpoiDelta, payload, ws);
}

void handleClearBoard(Room& room, WebSocket* ws) {
    room.state.strokes.clear();
    std::cout << "[房间 " << room.roomId << "] 画板已清空\n" << std::flush;

    std::stringstream deltaStream;
    {
        auto shadow = sp::spoi(room.state, deltaStream);
        shadow.strokes = {};
    }
    auto deltaStr = deltaStream.str();
    std::vector<sp::u8> payload(deltaStr.begin(), deltaStr.end());
    broadcastToRoom(room, MsgType::SpoiDelta, payload);
}

void handleDeleteStroke(Room& room, WebSocket* ws, const std::vector<sp::u8>& data) {
    if (data.size() < 4) return;
    sp::u32 index = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);

    if (index >= room.state.strokes.size()) return;
    room.state.strokes.erase(room.state.strokes.begin() + index);

    std::cout << "[房间 " << room.roomId << "] 删除笔画 idx=" << index << "\n" << std::flush;

    std::stringstream deltaStream;
    {
        auto shadow = sp::spoi(room.state, deltaStream);
        shadow.strokes.remove(index);
    }
    auto deltaStr = deltaStream.str();
    std::vector<sp::u8> payload(deltaStr.begin(), deltaStr.end());
    broadcastToRoom(room, MsgType::SpoiDelta, payload);
}

void handleCursorMove(Room& room, WebSocket* ws, const std::vector<sp::u8>& data) {
    if (data.size() < 17) return;

    int senderIdx = -1;
    for (size_t i = 0; i < room.clients.size(); i++) {
        if (room.clients[i] == ws) { senderIdx = static_cast<int>(i); break; }
    }
    if (senderIdx < 0) return;

    std::stringstream ss(std::string(data.begin(), data.end()));
    sp::I i(ss);
    sp::f64 cx = 0, cy = 0;
    i >> cx >> cy;

    std::string cursorMsg = "{\"type\":\"cursor\",\"userIndex\":" +
        std::to_string(senderIdx) +
        ",\"x\":" + std::to_string(static_cast<int>(cx)) +
        ",\"y\":" + std::to_string(static_cast<int>(cy)) + "}";
    broadcastToRoomExcept(room, MsgType::SystemMsg,
        std::vector<sp::u8>(cursorMsg.begin(), cursorMsg.end()), ws);
}

void removeClientFromRoom(WebSocket* ws) {
    std::lock_guard<std::mutex> lock(roomsMutex);

    for (auto it = rooms.begin(); it != rooms.end(); ) {
        auto& room = it->second;
        for (size_t i = 0; i < room.clients.size(); i++) {
            if (room.clients[i] == ws) {
                std::string name = room.userNames[i];
                room.clients.erase(room.clients.begin() + i);
                room.userNames.erase(room.userNames.begin() + i);
                room.userColors.erase(room.userColors.begin() + i);

                std::cout << "[房间 " << room.roomId << "] " << name << " 离开 ("
                          << room.clients.size() << "人在线)\n" << std::flush;

                std::string userList = "{\"type\":\"userList\",\"users\":[";
                for (size_t j = 0; j < room.userNames.size(); j++) {
                    if (j > 0) userList += ",";
                    userList += "{\"name\":\"" + room.userNames[j] +
                        "\",\"color\":" + std::to_string(room.userColors[j]) + "}";
                }
                userList += "]}";
                for (auto c : room.clients) {
                    wsSendText(c, MsgType::SystemMsg, userList);
                }
                break;
            }
        }

        if (room.clients.empty()) {
            std::cout << "[房间 " << it->first << "] 已关闭\n" << std::flush;
            it = rooms.erase(it);
        } else {
            ++it;
        }
    }
}