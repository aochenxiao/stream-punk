#include "../include/MessageHandler.hpp"

#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <random>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

// ===== 全局状态 =====
std::map<std::string, Room> rooms;
std::mutex roomsMutex;
std::vector<std::unique_ptr<WebSocket>> allClients;

// ===== SPOI 指令处理 =====
void handleSpoiCommand(GameState& state, const std::vector<u8>& data) {
    std::string cmdStr(data.begin(), data.end());
    std::stringstream cmdStream(cmdStr);
    try {
        sp::SpoiExecutor executor(cmdStream);
        std::stringstream resultStream;
        executor.execute(state, resultStream);
    } catch (const std::exception& e) {
        std::cerr << "[SPOI 执行错误] " << e.what() << std::endl;
    }
}

// ===== WebSocket 通信 =====

bool wsSendBinary(WebSocket* ws, MsgType type, const std::vector<u8>& payload) {
    u32 totalLen = 1 + static_cast<u32>(payload.size());
    std::vector<u8> packet;
    packet.reserve(4 + totalLen);
    u32 len = totalLen;
    packet.push_back(static_cast<u8>(len & 0xFF));
    packet.push_back(static_cast<u8>((len >> 8) & 0xFF));
    packet.push_back(static_cast<u8>((len >> 16) & 0xFF));
    packet.push_back(static_cast<u8>((len >> 24) & 0xFF));
    packet.push_back(static_cast<u8>(type));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return ws->sendBinary(packet);
}

bool wsSendText(WebSocket* ws, MsgType type, const std::string& payload) {
    std::vector<u8> p(payload.begin(), payload.end());
    return wsSendBinary(ws, type, p);
}

void sendFullState(WebSocket* ws, const GameState& state) {
    std::cerr << "[CRASH-DEBUG] sendFullState 开始序列化..." << std::endl;
    std::stringstream ss;
    O o(ss);
    o << state;
    std::cerr << "[CRASH-DEBUG] sendFullState 序列化完成" << std::endl;
    auto str = ss.str();
    std::cerr << "[CRASH-DEBUG] sendFullState str.size=" << str.size() << std::endl;
    std::vector<u8> payload(str.begin(), str.end());
    std::cerr << "[CRASH-DEBUG] sendFullState 发送中..." << std::endl;
    wsSendBinary(ws, MsgType::FullState, payload);
    std::cerr << "[CRASH-DEBUG] sendFullState 发送完成" << std::endl;
}

void sendSystemMsg(WebSocket* ws, const std::string& msg) {
    wsSendText(ws, MsgType::SystemMsg, msg);
}

void broadcastToRoom(Room& room, MsgType type, const std::vector<u8>& payload) {
    for (auto client : room.clients) {
        wsSendBinary(client, type, payload);
    }
}

ReceivedMsg recvMessage(WebSocket* ws) {
    auto frame = ws->recvFrame();
    if (frame.empty()) return {MsgType::SpoiCommand, {}};
    if (frame.size() == 1 && frame[0] == 0) return {MsgType::SpoiCommand, {}};

    if (frame.size() < 5) {
        std::cerr << "[recvMessage] frame too small: " << frame.size() << std::endl;
        return {MsgType::SpoiCommand, {}};
    }

    u32 totalLen = frame[0] | (frame[1] << 8) | (frame[2] << 16) | (frame[3] << 24);
    std::cerr << "[recvMessage] frame.size=" << frame.size() << " totalLen=" << totalLen << std::endl;
    if (totalLen < 1 || totalLen > 1024 * 1024) return {MsgType::SpoiCommand, {}};

    if (5 + totalLen - 1 > frame.size()) {
        std::cerr << "[recvMessage] ERROR: payload end " << (5 + totalLen - 1)
                  << " > frame.size " << frame.size() << std::endl;
        return {MsgType::SpoiCommand, {}};
    }

    MsgType type = static_cast<MsgType>(frame[4]);
    std::cerr << "[recvMessage] type=" << static_cast<int>(type) << std::endl;
    
    ReceivedMsg result;
    result.type = type;
    
    std::cerr << "[recvMessage] before memcpy, count=" << (totalLen - 1) << std::endl;
    result.payload.resize(totalLen - 1);
    if (totalLen - 1 > 0) {
        std::memcpy(result.payload.data(), frame.data() + 5, totalLen - 1);
    }
    std::cerr << "[recvMessage] payload.size=" << result.payload.size() << std::endl;
    return result;
}

// ===== 消息处理 =====

void handleLobbyMessage(WebSocket* ws, const ReceivedMsg& msg) {
    if (msg.type != MsgType::LobbyAction) return;

    std::string json(msg.payload.begin(), msg.payload.end());
    std::cout << "[大厅] " << json << "\n" << std::flush;
    std::cerr << "[CRASH-DEBUG] handleLobbyMessage 解析 JSON..." << std::endl;

    auto findVal = [&](const std::string& key) -> std::string {
        auto pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        pos = json.find(":", pos);
        if (pos == std::string::npos) return "";
        pos++;
        while (pos < json.size() && std::isspace(static_cast<u8>(json[pos]))) pos++;
        if (pos >= json.size()) return "";
        if (json[pos] == '"') {
            pos++;
            auto end = json.find("\"", pos);
            if (end == std::string::npos) return "";
            return json.substr(pos, end - pos);
        }
        auto end = json.find_first_of(",}", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };

    std::string type = findVal("type");
    std::string roomId = findVal("roomId");
    std::string playerName = findVal("playerName");
    std::cerr << "[CRASH-DEBUG] handleLobbyMessage type=" << type << " roomId=" << roomId
              << " playerName=" << playerName << std::endl;

    if (type == "join") {
        if (roomId.empty()) roomId = "default";

        std::lock_guard<std::mutex> lock(roomsMutex);

        if (rooms.find(roomId) == rooms.end()) {
            rooms[roomId] = Room{};
            rooms[roomId].roomId = roomId;
            std::cout << "[房间] 创建房间: " << roomId << "\n" << std::flush;
        }

        auto& room = rooms[roomId];
        if (room.gameStarted) {
            sendSystemMsg(ws, R"({"type":"error","msg":"游戏已开始"})");
            return;
        }

        room.clients.push_back(ws);
        room.playerNames.push_back(playerName);
        room.ready.push_back(false);

        std::cout << "[房间 " << roomId << "] " << playerName << " 加入 (" << room.clients.size() << "人)\n" << std::flush;

        sendSystemMsg(ws, R"({"type":"joined","roomId":")" + roomId +
            R"(","playerIndex":)" + std::to_string(room.clients.size() - 1) + "}");

        std::string playerList = R"({"type":"playerList","players":[)";
        for (size_t i = 0; i < room.playerNames.size(); i++) {
            if (i > 0) playerList += ",";
            playerList += "\"" + room.playerNames[i] + "\"";
        }
        playerList += "]}";
        for (auto c : room.clients) {
            sendSystemMsg(c, playerList);
        }

        std::cerr << "[CRASH-DEBUG] handleLobbyMessage clients.size=" << room.clients.size() << std::endl;
        if (room.clients.size() >= 2) {
            std::cerr << "[CRASH-DEBUG] handleLobbyMessage 调用 startGame..." << std::endl;
            startGame(room);
            std::cerr << "[CRASH-DEBUG] handleLobbyMessage startGame 返回" << std::endl;
        }
    }
    std::cerr << "[CRASH-DEBUG] handleLobbyMessage 出口" << std::endl;
}

void startGame(Room& room) {
    std::cerr << "[CRASH-DEBUG] startGame 入口 room=" << room.roomId << std::endl;
    std::cout << "[房间 " << room.roomId << "] 游戏开始！\n" << std::flush;

    std::cerr << "[CRASH-DEBUG] startGame 调用 initGameState..." << std::endl;
    initGameState(room.state, room.playerNames);
    std::cerr << "[CRASH-DEBUG] startGame initGameState 完成" << std::endl;

    room.gameStarted = true;
    room.turnStart = std::chrono::steady_clock::now();

    std::cerr << "[CRASH-DEBUG] startGame 发送 gameStart 系统消息..." << std::endl;
    for (auto c : room.clients) {
        sendSystemMsg(c, R"({"type":"gameStart","currentTurn":)" +
            std::to_string(room.state.currentTurn) + "}");
    }
    std::cerr << "[CRASH-DEBUG] startGame 系统消息发送完成" << std::endl;

    std::cerr << "[CRASH-DEBUG] startGame 发送全量状态, clients=" << room.clients.size() << std::endl;
    for (auto c : room.clients) {
        std::cerr << "[CRASH-DEBUG] startGame sendFullState 开始..." << std::endl;
        sendFullState(c, room.state);
        std::cerr << "[CRASH-DEBUG] startGame sendFullState 完成" << std::endl;
    }
    std::cerr << "[CRASH-DEBUG] startGame 全部完成" << std::endl;
}

void broadcastStateDelta(Room& room) {
    std::stringstream deltaStream;
    {
        auto shadow = sp::spoi(room.state, deltaStream);
        shadow.worms = room.state.worms;
        shadow.terrain = room.state.terrain;
        shadow.explosions = room.state.explosions;
        shadow.phase = room.state.phase;
        shadow.currentTurn = room.state.currentTurn;
        shadow.wind = room.state.wind;
        shadow.turnTimeLeft = room.state.turnTimeLeft;
        shadow.winner = room.state.winner;
        shadow.trajectory = room.state.trajectory;
    }
    auto deltaStr = deltaStream.str();
    std::vector<u8> payload(deltaStr.begin(), deltaStr.end());
    broadcastToRoom(room, MsgType::SpoiDelta, payload);
}

void processPendingExplosions(Room& room) {
    auto now = std::chrono::steady_clock::now();
    for (auto it = room.pendingExplosions.begin(); it != room.pendingExplosions.end(); ) {
        if (now >= it->applyAt) {
            auto& result = it->result;

            for (auto& [idx, newHp] : result.wormDamage) {
                room.state.worms[idx].hp = newHp;
                if (newHp <= 0) room.state.worms[idx].alive = false;
            }

            for (auto& [x, newH] : result.terrainHoles) {
                if (x < room.state.terrain.size()) {
                    room.state.terrain[x] = newH;
                }
            }

            room.state.explosions.push_back(result.explosion);

            applyWormGravity(room.state.worms, room.state.terrain);

            std::cout << "[房间 " << room.roomId << "] 爆炸! 伤害=" << result.wormDamage.size()
                      << "只虫 弹坑=" << result.terrainHoles.size() << "点\n" << std::flush;

            advanceTurn(room.state);
            room.turnStart = std::chrono::steady_clock::now();

            broadcastStateDelta(room);
            it = room.pendingExplosions.erase(it);
        } else {
            ++it;
        }
    }
}

bool canAct(const Room& room, i32 playerIdx, WebSocket* ws) {
    if (!room.gameStarted || room.state.phase != "aiming") {
        sendSystemMsg(ws, R"({"type":"error","msg":"不是你的回合"})");
        return false;
    }
    if (room.state.currentTurn != playerIdx) {
        sendSystemMsg(ws, R"({"type":"error","msg":"不是你的回合"})");
        return false;
    }
    return true;
}

void handleRoomMessage(const std::string& roomId, Room& room, i32 playerIdx,
                       WebSocket* ws, const ReceivedMsg& msg) {
    std::cerr << "[CRASH-DEBUG] handleRoomMessage 入口 room=" << roomId
              << " player=" << playerIdx << " type=" << static_cast<int>(msg.type) << std::endl;

    if (msg.type != MsgType::SpoiCommand) {
        std::cerr << "[CRASH-DEBUG] handleRoomMessage 出口" << std::endl;
        return;
    }

    if (msg.payload.empty()) {
        std::cerr << "[CRASH-DEBUG] handleRoomMessage empty payload" << std::endl;
        return;
    }

    u8 cmd = msg.payload[0];
    std::cerr << "[CRASH-DEBUG] handleRoomMessage cmd=" << static_cast<int>(cmd) << std::endl;

    if (cmd == 0x01) { // AIM: f64 angle
        if (msg.payload.size() < 9) return;
        std::stringstream ss(std::string(msg.payload.begin() + 1, msg.payload.end()));
        I i(ss);
        f64 angle = 0;
        i >> angle;
        angle = clampAngle(angle);

        if (!canAct(room, playerIdx, ws)) return;

        auto& worm = room.state.worms[playerIdx];
        worm.angle = angle;
        std::cout << "[房间 " << roomId << "] " << room.playerNames[playerIdx]
                  << " 调整角度=" << angle << "\n" << std::flush;

        broadcastStateDelta(room);
    }
    else if (cmd == 0x02) { // MOVE: i8 direction
        if (msg.payload.size() < 2) return;
        i8 dir = static_cast<i8>(msg.payload[1]);
        if (dir != 1 && dir != -1) return;

        if (!canAct(room, playerIdx, ws)) return;

        auto& worm = room.state.worms[playerIdx];
        if (tryMoveWorm(worm, dir, room.state.terrain)) {
            std::cout << "[房间 " << roomId << "] " << room.playerNames[playerIdx]
                      << " 移动 dir=" << static_cast<int>(dir)
                      << " x=" << worm.x << " y=" << worm.y
                      << " moved=" << worm.movedThisTurn << "\n" << std::flush;
            broadcastStateDelta(room);
        }
    }
    else if (cmd == 0x03) { // FIRE: f64 power
        if (msg.payload.size() < 9) return;
        std::stringstream ss(std::string(msg.payload.begin() + 1, msg.payload.end()));
        I i(ss);
        f64 power = 0;
        i >> power;
        power = std::clamp(power, POWER_MIN, POWER_MAX);

        if (!canAct(room, playerIdx, ws)) return;

        room.state.explosions.clear();

        auto& worm = room.state.worms[playerIdx];
        worm.power = power;
        f64 worldAngle = toWorldAngle(worm.angle, worm.facingRight);

        std::cout << "[房间 " << roomId << "] " << room.playerNames[playerIdx]
                  << " 发射! 武器=" << worm.weapon
                  << " relAngle=" << worm.angle << " worldAngle=" << worldAngle
                  << " power=" << power << "\n" << std::flush;

        auto result = simulateProjectile(
            worm.x, worm.y + WORM_OFFSET_Y, worldAngle, power,
            room.state.terrain, room.state.wind,
            room.state.worms, playerIdx,
            worm.weapon);

        room.state.phase = "firing";

        {
            std::stringstream ts;
            O o(ts);
            o << result.trajectory;
            auto str = ts.str();
            std::vector<u8> payload(str.begin(), str.end());
            for (auto c : room.clients) {
                wsSendBinary(c, MsgType::Trajectory, payload);
            }
        }

        room.pendingExplosions.push_back(PendingExplosion{
            std::move(result),
            std::chrono::steady_clock::now() + std::chrono::milliseconds(1000),
            static_cast<size_t>(playerIdx)
        });
    }
    else if (cmd == 0x04) { // SWITCH WEAPON: i32 weapon
        if (msg.payload.size() < 5) return;
        std::stringstream ss(std::string(msg.payload.begin() + 1, msg.payload.end()));
        I i(ss);
        i32 weapon = 0;
        i >> weapon;
        weapon = std::clamp(weapon, 0, 2);

        auto& worm = room.state.worms[playerIdx];
        worm.weapon = weapon;

        std::cout << "[房间 " << roomId << "] " << room.playerNames[playerIdx]
                  << " 切换武器=" << weapon << "\n" << std::flush;

        broadcastStateDelta(room);
    }
    else {
        std::cerr << "[CRASH-DEBUG] handleRoomMessage unknown cmd=" << static_cast<int>(cmd) << std::endl;
    }

    std::cerr << "[CRASH-DEBUG] handleRoomMessage 出口" << std::endl;
}