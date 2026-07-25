// 坦克大战 WebSocket 服务端
// 使用 StreamPunk 进行二进制序列化

#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <thread>
#include <cmath>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ws2_32.lib")

#include "../Data.hpp"
using namespace sp;

// ==================== 常量 ====================
constexpr double PI = 3.141592653589793;
constexpr double TANK_SPEED = 200.0;       // 像素/秒
constexpr double TANK_ROTATE_SPEED = 4.0;  // 弧度/秒
constexpr double BULLET_SPEED = 400.0;     // 像素/秒
constexpr double BULLET_LIFETIME = 2.0;    // 秒
constexpr double CANVAS_W = 800.0;
constexpr double CANVAS_H = 600.0;
constexpr double TANK_SIZE = 20.0;
constexpr double TICK_DT = 1.0 / 30.0;     // 30 FPS

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

// ==================== WebSocket 帧协议 ====================
enum class WsOpcode : u8 { Text = 0x1, Binary = 0x2, Close = 0x8, Ping = 0x9, Pong = 0xA };
enum class MsgType : u8 { Input = 0x01, GameState = 0x02, Join = 0x03, PlayerId = 0x04 };

std::vector<u8> makeWsFrame(WsOpcode opcode, const std::vector<u8>& payload) {
    std::vector<u8> frame;
    frame.push_back(0x80 | static_cast<u8>(opcode));
    size_t len = payload.size();
    if (len <= 125) {
        frame.push_back(static_cast<u8>(len));
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<u8>((len >> 8) & 0xFF));
        frame.push_back(static_cast<u8>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i)
            frame.push_back(static_cast<u8>((len >> (i * 8)) & 0xFF));
    }
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

bool parseWsFrame(const std::vector<u8>& buf, size_t& consumed,
                  WsOpcode& opcode, std::vector<u8>& payload) {
    if (buf.size() < 2) return false;
    size_t pos = 0;
    u8 first = buf[pos++];
    opcode = static_cast<WsOpcode>(first & 0x0F);
    bool masked = (buf[pos] & 0x80) != 0;
    size_t len = buf[pos++] & 0x7F;
    if (len == 126) {
        if (buf.size() < pos + 2) return false;
        len = (static_cast<size_t>(buf[pos]) << 8) | buf[pos + 1];
        pos += 2;
    } else if (len == 127) {
        if (buf.size() < pos + 8) return false;
        len = 0;
        for (int i = 0; i < 8; ++i) len = (len << 8) | buf[pos + i];
        pos += 8;
    }
    u8 mask[4] = {};
    if (masked) {
        if (buf.size() < pos + 4) return false;
        for (int i = 0; i < 4; ++i) mask[i] = buf[pos + i];
        pos += 4;
    }
    if (buf.size() < pos + len) return false;
    payload.resize(len);
    for (size_t i = 0; i < len; ++i) payload[i] = buf[pos + i] ^ mask[i % 4];
    pos += len;
    consumed = pos;
    return true;
}

// ==================== HTTP 握手 ====================
std::string readHttpRequest(SOCKET sock) {
    std::string req;
    char buf[4096];
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n > 0) { buf[n] = '\0'; req = buf; }
    return req;
}

std::string extractHeader(const std::string& req, const std::string& key) {
    size_t pos = req.find(key + ": ");
    if (pos == std::string::npos) return "";
    pos += key.length() + 2;
    size_t end = req.find("\r\n", pos);
    return req.substr(pos, end - pos);
}

static const char* BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::vector<u8>& data) {
    std::string result;
    size_t i = 0;
    while (i < data.size()) {
        u32 triple = 0;
        int padding = 0;
        for (int j = 0; j < 3; ++j) {
            triple <<= 8;
            if (i < data.size()) triple |= data[i++];
            else ++padding;
        }
        result += BASE64_CHARS[(triple >> 18) & 0x3F];
        result += BASE64_CHARS[(triple >> 12) & 0x3F];
        result += (padding >= 2) ? '=' : BASE64_CHARS[(triple >> 6) & 0x3F];
        result += (padding >= 1) ? '=' : BASE64_CHARS[triple & 0x3F];
    }
    return result;
}

std::string sha1(const std::string& input) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) return {};
    if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) { CryptReleaseContext(hProv, 0); return {}; }
    CryptHashData(hHash, reinterpret_cast<const BYTE*>(input.data()), static_cast<DWORD>(input.size()), 0);
    DWORD hashLen = 20;
    BYTE hash[20] = {};
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return std::string(reinterpret_cast<char*>(hash), hashLen);
}

std::string makeWsAcceptKey(const std::string& clientKey) {
    std::string combined = clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    auto hash = sha1(combined);
    std::vector<u8> hashBytes(hash.begin(), hash.end());
    return base64Encode(hashBytes);
}

bool doHandshake(SOCKET sock, const std::string& req) {
    std::string key = extractHeader(req, "Sec-WebSocket-Key");
    if (key.empty()) return false;
    std::string accept = makeWsAcceptKey(key);
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    int sent = send(sock, response.c_str(), static_cast<int>(response.size()), 0);
    return sent > 0;
}

// ==================== 游戏逻辑 ====================
struct GameClient {
    SOCKET sock;
    int id;
    std::vector<u8> recvBuf;
    PlayerInput lastInput;
    bool connected = true;

    GameClient(SOCKET s, int cid) : sock(s), id(cid) {}
};

struct GameRoom {
    GameState state;
    std::vector<GameClient> clients;
    std::mutex mtx;
    int nextClientId = 1;
    double bulletCooldown[2] = {0, 0};
    static constexpr double COOLDOWN_TIME = 0.5;

    void broadcast(const std::vector<u8>& data) {
        auto frame = makeWsFrame(WsOpcode::Binary, data);
        for (auto& c : clients) {
            int sent = send(c.sock, reinterpret_cast<const char*>(frame.data()),
                            static_cast<int>(frame.size()), 0);
            if (sent <= 0) c.connected = false;
        }
    }

    void broadcastGameState() {
        std::stringstream ss;
        O o{ss};
        o << state;
        auto str = ss.str();
        std::vector<u8> payload;
        payload.push_back(static_cast<u8>(MsgType::GameState));
        payload.insert(payload.end(),
                       reinterpret_cast<const u8*>(str.data()),
                       reinterpret_cast<const u8*>(str.data()) + str.size());
        broadcast(payload);
    }

    void sendPlayerId(SOCKET sock, int playerId) {
        std::vector<u8> payload;
        payload.push_back(static_cast<u8>(MsgType::PlayerId));
        payload.push_back(static_cast<u8>(playerId));
        auto frame = makeWsFrame(WsOpcode::Binary, payload);
        send(sock, reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()), 0);
    }

    void addPlayer(SOCKET sock) {
        int pid = nextClientId++;
        int idx = static_cast<int>(clients.size());
        clients.emplace_back(sock, pid);
        sendPlayerId(sock, pid);

        // 初始化玩家位置
        PlayerState ps;
        ps.id = pid;
        ps.hp = 100;
        if (idx == 0) {
            ps.x = 150; ps.y = CANVAS_H / 2; ps.rotation = 0;
        } else {
            ps.x = CANVAS_W - 150; ps.y = CANVAS_H / 2; ps.rotation = PI;
        }
        state.players.push_back(ps);
        LOG("JOIN", "Player %d joined (total: %zu)", pid, clients.size());
    }

    void processInput(int clientIndex, const PlayerInput& input) {
        if (clientIndex < 0 || clientIndex >= static_cast<int>(clients.size())) return;
        clients[clientIndex].lastInput = input;
    }

    double clamp(double val, double lo, double hi) { return val < lo ? lo : (val > hi ? hi : val); }

    void fireBullet(int playerIndex) {
        if (playerIndex < 0 || playerIndex >= static_cast<int>(state.players.size())) return;
        auto& p = state.players[playerIndex];
        if (p.hp <= 0) return;
        if (bulletCooldown[playerIndex] > 0) return;

        Bullet b;
        b.x = p.x + std::cos(p.rotation) * TANK_SIZE;
        b.y = p.y + std::sin(p.rotation) * TANK_SIZE;
        b.vx = std::cos(p.rotation) * BULLET_SPEED;
        b.vy = std::sin(p.rotation) * BULLET_SPEED;
        b.ownerId = p.id;
        state.bullets.push_back(b);
        bulletCooldown[playerIndex] = COOLDOWN_TIME;
    }

    void update(double dt) {
        // 更新冷却时间
        for (int i = 0; i < 2; ++i) {
            if (bulletCooldown[i] > 0) bulletCooldown[i] -= dt;
        }

        // 处理玩家输入
        for (int i = 0; i < static_cast<int>(clients.size()) && i < static_cast<int>(state.players.size()); ++i) {
            auto& input = clients[i].lastInput;
            auto& p = state.players[i];
            if (p.hp <= 0) continue;

            // 旋转
            if (input.left)  p.rotation -= TANK_ROTATE_SPEED * dt;
            if (input.right) p.rotation += TANK_ROTATE_SPEED * dt;

            // 移动
            if (input.up) {
                p.x += std::cos(p.rotation) * TANK_SPEED * dt;
                p.y += std::sin(p.rotation) * TANK_SPEED * dt;
            }
            if (input.down) {
                p.x -= std::cos(p.rotation) * TANK_SPEED * dt;
                p.y -= std::sin(p.rotation) * TANK_SPEED * dt;
            }

            // 边界限制
            p.x = clamp(p.x, TANK_SIZE, CANVAS_W - TANK_SIZE);
            p.y = clamp(p.y, TANK_SIZE, CANVAS_H - TANK_SIZE);

            // 射击
            if (input.fire) fireBullet(i);
        }

        // 更新子弹
        for (auto it = state.bullets.begin(); it != state.bullets.end();) {
            it->x += it->vx * dt;
            it->y += it->vy * dt;

            // 边界检测
            bool outOfBounds = (it->x < 0 || it->x > CANVAS_W || it->y < 0 || it->y > CANVAS_H);
            if (outOfBounds) {
                it = state.bullets.erase(it);
                continue;
            }

            // 碰撞检测
            bool hit = false;
            for (auto& p : state.players) {
                if (p.hp <= 0) continue;
                if (p.id == it->ownerId) continue;
                double dx = it->x - p.x;
                double dy = it->y - p.y;
                if (std::sqrt(dx * dx + dy * dy) < TANK_SIZE) {
                    p.hp -= 34;
                    if (p.hp < 0) p.hp = 0;
                    hit = true;
                    break;
                }
            }
            if (hit) {
                it = state.bullets.erase(it);
            } else {
                ++it;
            }
        }
    }
};

// ==================== 主循环 ====================
int main() {
    INIT_StreamPunk();

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9002);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "Bind failed" << std::endl;
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    listen(listenSock, SOMAXCONN);
    LOG("INIT", "=== Tank Battle Server ===");
    LOG("INIT", "Listening on ws://localhost:9002");
    LOG("INIT", "========================");

    GameRoom room;
    std::vector<SOCKET> pendingHandshakes;

    u_long nonBlock = 1;
    ioctlsocket(listenSock, FIONBIO, &nonBlock);

    auto lastTick = std::chrono::steady_clock::now();
    bool running = true;

    while (running) {
        // 接受新连接
        SOCKET clientSock = accept(listenSock, nullptr, nullptr);
        if (clientSock != INVALID_SOCKET) {
            ioctlsocket(clientSock, FIONBIO, &nonBlock);
            pendingHandshakes.push_back(clientSock);
            LOG("ACCEPT", "new TCP connection, pending=%zu", pendingHandshakes.size());
        }

        // 处理握手
        for (auto it = pendingHandshakes.begin(); it != pendingHandshakes.end();) {
            std::string req = readHttpRequest(*it);
            if (!req.empty()) {
                if (doHandshake(*it, req)) {
                    std::lock_guard<std::mutex> lock(room.mtx);
                    if (room.clients.size() < 2) {
                        room.addPlayer(*it);
                    } else {
                        LOG("FULL", "Room full, rejecting connection");
                        closesocket(*it);
                    }
                } else {
                    closesocket(*it);
                }
                it = pendingHandshakes.erase(it);
            } else {
                ++it;
            }
        }

        // 接收消息
        {
            std::lock_guard<std::mutex> lock(room.mtx);
            for (auto& client : room.clients) {
                if (!client.connected) continue;
                char buf[65536];
                int n = recv(client.sock, buf, sizeof(buf), 0);
                if (n > 0) {
                    client.recvBuf.insert(client.recvBuf.end(),
                                          reinterpret_cast<u8*>(buf),
                                          reinterpret_cast<u8*>(buf) + n);
                    size_t consumed = 0;
                    WsOpcode opcode;
                    std::vector<u8> payload;
                    while (parseWsFrame(client.recvBuf, consumed, opcode, payload)) {
                        client.recvBuf.erase(client.recvBuf.begin(), client.recvBuf.begin() + consumed);
                        if (opcode == WsOpcode::Binary && payload.size() > 1) {
                            MsgType type = static_cast<MsgType>(payload[0]);
                            if (type == MsgType::Input) {
                                std::stringstream ss;
                                ss.write(reinterpret_cast<const char*>(&payload[1]), payload.size() - 1);
                                PlayerInput input;
                                I i{ss};
                                i >> input;
                                // 找到对应客户端索引
                                int idx = -1;
                                for (int j = 0; j < static_cast<int>(room.clients.size()); ++j) {
                                    if (room.clients[j].sock == client.sock) { idx = j; break; }
                                }
                                room.processInput(idx, input);
                            }
                        } else if (opcode == WsOpcode::Close) {
                            client.connected = false;
                        }
                    }
                } else if (n == 0 || WSAGetLastError() == WSAECONNRESET) {
                    client.connected = false;
                }
            }

            // 清理断线玩家
            for (auto it = room.clients.begin(); it != room.clients.end();) {
                if (!it->connected) {
                    LOG("DISCONNECT", "Player %d disconnected", it->id);
                    // 移除对应玩家状态
                    for (auto pit = room.state.players.begin(); pit != room.state.players.end();) {
                        if (pit->id == it->id) pit = room.state.players.erase(pit);
                        else ++pit;
                    }
                    closesocket(it->sock);
                    it = room.clients.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // 游戏更新
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - lastTick).count();
        if (dt >= TICK_DT) {
            lastTick = now;
            {
                std::lock_guard<std::mutex> lock(room.mtx);
                room.update(TICK_DT);
                room.broadcastGameState();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    closesocket(listenSock);
    WSACleanup();
    return 0;
}