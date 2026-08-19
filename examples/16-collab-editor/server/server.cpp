// 在线文本协作 WebSocket 服务器
// 使用 ixwebsocket + StreamPunk 二进制序列化

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <algorithm>

#include "../Data.hpp"
#include <stream-punk/StreamPunkSPOIExecutor.hpp>
using namespace sp;

// ==================== 日志 ====================
inline std::string nowStr() {
    auto t = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count() % 1000;
    auto tt = std::chrono::system_clock::to_time_t(t);
    std::tm tm;
    localtime_s(&tm, &tt);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03lld", tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
    return buf;
}
#define LOG(tag, ...) printf("[%s] %s ", nowStr().c_str(), tag); printf(__VA_ARGS__); printf("\n"); fflush(stdout)

// ==================== 消息类型 ====================
enum class MsgType : u8 {
    Join = 0x01,
    JoinResponse = 0x02,
    SpoiOps = 0x03,       // SPOI 指令流（增量编辑）
    CursorUpdate = 0x04,
    UserList = 0x05,
};

// ==================== 协作文档（SPOI 可寻址的根对象） ====================
struct CollabDoc : public Base {
    #define Xt_CollabDoc(X__) \
    X__(std::string, content, "")
    CollabDoc() = default;
    UseData(CollabDoc);
};

// ==================== 用户颜色 ====================
static const std::vector<std::string> USER_COLORS = {
    "#FF6B6B", "#4ECDC4", "#45B7D1", "#96CEB4", "#FFEAA7",
    "#DDA0DD", "#98D8C8", "#F7DC6F", "#BB8FCE", "#85C1E9",
    "#F8C471", "#82E0AA", "#F1948A", "#85929E", "#AED6F1",
    "#FAD7A0", "#D7BDE2", "#A3E4D7", "#F5B7B1", "#A9CCE3"
};

// ==================== 协作引擎 ====================
struct CollabUser {
    std::weak_ptr<ix::WebSocket> ws;
    int userId;
    std::string userName;
    std::string color;
    int cursorPos = 0;
};

class CollabEngine {
public:
    CollabDoc doc;          // 权威文档（SPOI 根对象）
    int nextUserId = 1;
    int docVersion = 0;
    std::vector<CollabUser> users;
    std::mutex mtx;

    void broadcast(const std::vector<u8>& data, int excludeUserId = -1) {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& u : users) {
            if (u.userId == excludeUserId) continue;
            auto sp = u.ws.lock();
            if (sp) {
                sp->sendBinary(ix::IXWebSocketSendData(
                    reinterpret_cast<const char*>(data.data()), data.size()));
            }
        }
    }

    void sendTo(std::shared_ptr<ix::WebSocket> ws, const std::vector<u8>& data) {
        ws->sendBinary(ix::IXWebSocketSendData(
            reinterpret_cast<const char*>(data.data()), data.size()));
    }

    std::vector<u8> packMsg(MsgType type, const std::string& payload) {
        std::vector<u8> msg;
        msg.push_back(static_cast<u8>(type));
        msg.insert(msg.end(), payload.begin(), payload.end());
        return msg;
    }

    std::vector<u8> packMsg(MsgType type, const std::vector<u8>& payload) {
        std::vector<u8> msg;
        msg.push_back(static_cast<u8>(type));
        msg.insert(msg.end(), payload.begin(), payload.end());
        return msg;
    }

    template<typename T>
    std::vector<u8> packType(MsgType type, T& obj) {
        std::stringstream ss;
        O o{ss};
        o << obj;
        return packMsg(type, ss.str());
    }

    void handleJoin(std::shared_ptr<ix::WebSocket> ws, const std::vector<u8>& payload) {
        std::stringstream ss;
        ss.write(reinterpret_cast<const char*>(payload.data()), payload.size());
        JoinRequest req;
        I i{ss};
        i >> req;

        int userId = nextUserId++;
        std::string color = USER_COLORS[(userId - 1) % USER_COLORS.size()];

        {
            std::lock_guard<std::mutex> lock(mtx);
            CollabUser user;
            user.ws = ws;
            user.userId = userId;
            user.userName = req.userName;
            user.color = color;
            users.push_back(user);
        }

        LOG("JOIN", "User #%d '%s' (total: %zu)", userId, req.userName.c_str(), users.size());

        JoinResponse resp;
        resp.userId = userId;
        resp.document = doc.content;
        {
            std::lock_guard<std::mutex> lock(mtx);
            for (auto& u : users) {
                CursorInfo ci;
                ci.userId = u.userId;
                ci.userName = u.userName;
                ci.position = u.cursorPos;
                ci.color = u.color;
                resp.users.push_back(ci);
            }
        }
        sendTo(ws, packType(MsgType::JoinResponse, resp));
        broadcastUserList();
    }

    // 收到 SPOI 指令流（增量编辑）：payload = [userId i32 LE][SPOI 指令流]
    // 应用到权威文档，并把完整 payload（含 userId）广播给其他用户
    void handleSpoiOps(const std::vector<u8>& payload) {
        if (payload.size() < 4) return;
        i32 senderUserId = 0;
        std::memcpy(&senderUserId, payload.data(), 4);
        std::vector<u8> ops(payload.begin() + 4, payload.end());
        {
            std::lock_guard<std::mutex> lock(mtx);
            std::stringstream ss;
            ss.write(reinterpret_cast<const char*>(ops.data()), ops.size());
            try {
                SpoiExecutor exec(ss);
                exec >> doc;   // 增量应用：INSERT/REMOVE/REPLACE/MOVE/APPEND
                docVersion++;
                LOG("SPOI", "user#%d applied %zuB of ops, doc now %zu chars (v%d)",
                    senderUserId, ops.size(), doc.content.size(), docVersion);
            } catch (std::exception const& e) {
                LOG("SPOI", "apply error: %s", e.what());
            }
        }
        broadcast(packMsg(MsgType::SpoiOps, payload), senderUserId);
    }

    void handleCursorUpdate(const std::vector<u8>& payload) {
        std::stringstream ss;
        ss.write(reinterpret_cast<const char*>(payload.data()), payload.size());
        CursorInfo ci;
        I i{ss};
        i >> ci;

        {
            std::lock_guard<std::mutex> lock(mtx);
            for (auto& u : users) {
                if (u.userId == ci.userId) {
                    u.cursorPos = ci.position;
                    break;
                }
            }
        }

        broadcast(packType(MsgType::CursorUpdate, ci), ci.userId);
    }

    void broadcastUserList() {
        UserListUpdate ulu;
        {
            std::lock_guard<std::mutex> lock(mtx);
            for (auto& u : users) {
                if (u.ws.expired()) continue;
                CursorInfo ci;
                ci.userId = u.userId;
                ci.userName = u.userName;
                ci.position = u.cursorPos;
                ci.color = u.color;
                ulu.users.push_back(ci);
            }
        }
        if (!ulu.users.empty()) {
            broadcast(packType(MsgType::UserList, ulu));
        }
    }

    void handleDisconnect(std::shared_ptr<ix::WebSocket> ws) {
        std::lock_guard<std::mutex> lock(mtx);
        size_t before = users.size();
        for (auto it = users.begin(); it != users.end(); ++it) {
            auto sp = it->ws.lock();
            if (sp == ws) {
                LOG("LEAVE", "User #%d '%s' left (total: %zu)", it->userId, it->userName.c_str(), before - 1);
                users.erase(it);
                break;
            }
        }
        // Clean up stale
        for (auto it = users.begin(); it != users.end();) {
            if (it->ws.expired()) {
                it = users.erase(it);
            } else {
                ++it;
            }
        }
        if (before != users.size()) {
            broadcastUserList();
        }
    }
};

// ==================== 主函数 ====================
int main() {
    INIT_StreamPunk();
    ix::initNetSystem();

    SetConsoleOutputCP(65001);
    LOG("INIT", "=== Collaborative Text Editor Server ===");
    LOG("INIT", "Using ixwebsocket + StreamPunk");

    CollabEngine engine;

    engine.doc.content = "Welcome to the collaborative text editor!\n\n"
                         "Start typing to edit this document together with others.\n\n"
                         "All changes are synchronized in real-time.\n";

    ix::WebSocketServer server(9005, "0.0.0.0");

    server.setOnConnectionCallback(
        [&engine](std::weak_ptr<ix::WebSocket> weakWs,
                  std::shared_ptr<ix::ConnectionState> connectionState) {
            auto ws = weakWs.lock();
            if (!ws) return;

            LOG("CONNECT", "New connection from %s", connectionState->getRemoteIp().c_str());

            ws->setOnMessageCallback(
                [&engine, weakWs](const ix::WebSocketMessagePtr& msg) {
                    if (msg->type == ix::WebSocketMessageType::Message) {
                        const auto& data = msg->str;
                        if (data.empty()) return;

                        u8 msgType = static_cast<u8>(data[0]);
                        std::vector<u8> payload(data.begin() + 1, data.end());

                        auto ws = weakWs.lock();
                        if (!ws) return;

                        switch (static_cast<MsgType>(msgType)) {
                        case MsgType::Join:
                            engine.handleJoin(ws, payload);
                            break;
                        case MsgType::SpoiOps:
                            // payload = [userId i32 LE][SPOI 指令流]
                            engine.handleSpoiOps(payload);
                            break;
                        case MsgType::CursorUpdate:
                            engine.handleCursorUpdate(payload);
                            break;
                        default:
                            break;
                        }
                    } else if (msg->type == ix::WebSocketMessageType::Close ||
                               msg->type == ix::WebSocketMessageType::Error) {
                        auto ws = weakWs.lock();
                        if (ws) engine.handleDisconnect(ws);
                    }
                });
        });

    auto res = server.listen();
    if (!res.first) {
        LOG("ERROR", "Failed to listen: %s", res.second.c_str());
        return 1;
    }
    server.start();

    LOG("READY", "Listening on ws://0.0.0.0:9005");
    LOG("READY", "Type 'quit' to stop");

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit" || line == "exit") break;
    }

    server.stop();
    ix::uninitNetSystem();
    return 0;
}