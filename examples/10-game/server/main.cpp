// 示例 10：StreamWorms — C++ 游戏服务器
// 展示 StreamPunk 跨语言同步：
//   1. UseData 定义游戏类型 → 自动序列化/反序列化
//   2. SPOI 协议 → 增量同步、客户端操作指令
//   3. 全量状态同步 → 二进制序列化广播
//   4. 物理引擎 → C++ 权威计算

#define NOMINMAX
#include "include/MessageHandler.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    SetConsoleOutputCP(65001);
    std::cout << "=== StreamWorms — C++ 游戏服务器 ===\n";
    std::cout << "  展示 StreamPunk 跨语言对象同步\n";
    std::cout << "  WebSocket 端口: 9999\n\n";

    // 初始化 WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup 失败\n";
        return 1;
    }

    // 创建 TCP 服务器
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "socket 创建失败\n";
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9999);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "bind 失败\n";
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen 失败\n";
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "服务器就绪，等待客户端连接...\n\n";

    std::vector<WebSocket*> pendingHandshake;

    while (true) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenSocket, &readSet);
        int maxFd = static_cast<int>(listenSocket);

        for (auto& ws : allClients) {
            SOCKET s = ws->sock;
            FD_SET(s, &readSet);
            if (static_cast<int>(s) > maxFd) maxFd = static_cast<int>(s);
        }

        timeval tv{0, 50000}; // 50ms
        int sel = select(maxFd + 1, &readSet, nullptr, nullptr, &tv);

        // 1. 处理新连接
        if (sel > 0 && FD_ISSET(listenSocket, &readSet)) {
            SOCKET sock = accept(listenSocket, nullptr, nullptr);
            if (sock != INVALID_SOCKET) {
                u_long mode = 1;
                ioctlsocket(sock, FIONBIO, &mode);

                auto ws = std::make_unique<WebSocket>();
                ws->sock = sock;
                pendingHandshake.push_back(ws.get());
                allClients.push_back(std::move(ws));
                std::cout << "[连接] 新连接 (fd=" << sock << ")\n";
            }
        }

        // 2. 处理 WebSocket 握手
        for (auto it = pendingHandshake.begin(); it != pendingHandshake.end(); ) {
            WebSocket* ws = *it;
            if (sel > 0 && FD_ISSET(ws->sock, &readSet)) {
                if (ws->tryHandshake()) {
                    std::cout << "[WS] 握手成功 (fd=" << ws->sock << ")\n";
                    sendSystemMsg(ws, R"({"type":"welcome","msg":"Welcome to StreamWorms!"})");
                    it = pendingHandshake.erase(it);
                } else {
                    std::cout << "[WS] 握手失败 (fd=" << ws->sock << ")\n";
                    closesocket(ws->sock);
                    ws->sock = INVALID_SOCKET;
                    it = pendingHandshake.erase(it);
                }
            } else {
                ++it;
            }
        }

        // 3. 处理大厅消息（未加入房间的客户端）
        for (auto& ws : allClients) {
            if (!ws->handshakeDone || ws->sock == INVALID_SOCKET) continue;
            if (!(sel > 0 && FD_ISSET(ws->sock, &readSet))) continue;

            bool inRoom = false;
            for (auto& [rid, room] : rooms) {
                for (auto c : room.clients) {
                    if (c == ws.get()) { inRoom = true; break; }
                }
                if (inRoom) break;
            }
            if (inRoom) continue;

            std::cerr << "[CRASH-DEBUG] Step3 处理大厅消息 fd=" << ws->sock << std::endl;
            auto msg = recvMessage(ws.get());
            std::cerr << "[CRASH-DEBUG] Step3 recvMessage done, type=" << static_cast<int>(msg.type)
                      << " payload.size=" << msg.payload.size() << std::endl;
            if (msg.payload.empty() && !ws->handshakeDone) {
                std::cout << "[断开] 客户端 (fd=" << ws->sock << ")\n";
                closesocket(ws->sock);
                ws->sock = INVALID_SOCKET;
                ws->handshakeDone = false;
                continue;
            }
            if (msg.payload.empty()) continue;

            std::cerr << "[CRASH-DEBUG] Step3 调用 handleLobbyMessage..." << std::endl;
            handleLobbyMessage(ws.get(), msg);
            std::cerr << "[CRASH-DEBUG] Step3 handleLobbyMessage 返回" << std::endl;
        }

        // 4. 处理房间内客户端消息
        for (auto& [rid, room] : rooms) {
            processPendingExplosions(room);

            for (size_t i = 0; i < room.clients.size(); i++) {
                WebSocket* ws = room.clients[i];
                if (ws->sock == INVALID_SOCKET) {
                    room.clients.erase(room.clients.begin() + i);
                    room.playerNames.erase(room.playerNames.begin() + i);
                    room.ready.erase(room.ready.begin() + i);
                    i--;
                    continue;
                }
                if (!(sel > 0 && FD_ISSET(ws->sock, &readSet))) continue;

                std::cerr << "[CRASH-DEBUG] Step4 房间 " << rid << " 玩家 " << i << " socket 可读" << std::endl;
                auto msg = recvMessage(ws);
                std::cerr << "[CRASH-DEBUG] Step4 recvMessage: type=" << static_cast<int>(msg.type)
                          << " payload.size=" << msg.payload.size() << std::endl;
                if (msg.payload.empty()) continue;
                std::cerr << "[CRASH-DEBUG] Step4 调用 handleRoomMessage..." << std::endl;
                handleRoomMessage(rid, room, static_cast<i32>(i), ws, msg);
                std::cerr << "[CRASH-DEBUG] Step4 handleRoomMessage 返回" << std::endl;
            }
        }

        // 5. 清理断开连接和空房间
        allClients.erase(
            std::remove_if(allClients.begin(), allClients.end(),
                [](auto& ws) { return ws->sock == INVALID_SOCKET; }),
            allClients.end());

        for (auto it = rooms.begin(); it != rooms.end(); ) {
            auto& room = it->second;
            for (size_t i = 0; i < room.clients.size(); ) {
                if (room.clients[i]->sock == INVALID_SOCKET) {
                    std::cout << "[房间 " << it->first << "] 玩家 " << room.playerNames[i] << " 断开\n";
                    room.clients.erase(room.clients.begin() + i);
                    room.playerNames.erase(room.playerNames.begin() + i);
                    room.ready.erase(room.ready.begin() + i);
                } else {
                    i++;
                }
            }
            if (it->second.clients.empty()) {
                std::cout << "[房间 " << it->first << "] 已关闭\n";
                it = rooms.erase(it);
            } else {
                ++it;
            }
        }

        // 6. 推进游戏回合
        for (auto& [rid, room] : rooms) {
            if (room.gameStarted && room.state.phase == "aiming") {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - room.turnStart).count();
                room.state.turnTimeLeft = TURN_TIME - static_cast<i32>(elapsed);
                if (room.state.turnTimeLeft <= 0) {
                    std::cout << "[房间 " << rid << "] 回合超时\n" << std::flush;
                    std::cerr << "[CRASH-DEBUG] 回合超时 advanceTurn..." << std::endl;
                    advanceTurn(room.state);
                    std::cerr << "[CRASH-DEBUG] 回合超时 advanceTurn 完成" << std::endl;
                    room.turnStart = std::chrono::steady_clock::now();

                    std::cerr << "[CRASH-DEBUG] 回合超时 SPOI shadow..." << std::endl;
                    broadcastStateDelta(room);
                    std::cerr << "[CRASH-DEBUG] 回合超时 SPOI delta 广播完成" << std::endl;
                }
            }
        }
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}