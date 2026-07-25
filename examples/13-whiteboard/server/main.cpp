// 示例 13：多人协同画板 — C++ 服务器
// 展示 StreamPunk 核心优势：
//   1. UseData — 定义一次类型，自动获得序列化/反序列化（见 WhiteboardData.hpp）
//   2. SPOI Shadow — 增量同步：新笔画只发 append，不清空时不重发全量
//   3. 二进制协议 — 紧凑高效，一个笔画仅传输坐标点数据
//   4. 跨语言 — 同样的 C++ 类型定义可通过 sp-gen 生成 JS 等价代码

#define NOMINMAX
#include "include/RoomManager.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

int main() {
    try {
    sp::SpRegistry reg;
    INIT_StreamPunk(&reg);

    SetConsoleOutputCP(65001);
    std::cout << "=== 多人协同画板 — C++ 服务器 ===" << std::endl;
    std::cout << "  展示 StreamPunk 增量同步（SPOI Shadow）" << std::endl;
    std::cout << "  WebSocket 端口: 9998" << std::endl << std::endl;

    // 初始化 WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup 失败" << std::endl;
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
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9998);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr),
             sizeof(serverAddr)) == SOCKET_ERROR) {
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

    std::cout << "服务器就绪，等待客户端连接..." << std::endl << std::endl;

    std::vector<WebSocket*> pendingHandshake;

    while (true) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenSocket, &readSet);
        int maxFd = static_cast<int>(listenSocket);

        for (auto& ws : allClients) {
            SOCKET s = ws->sock;
            if (s != INVALID_SOCKET) {
                FD_SET(s, &readSet);
                if (static_cast<int>(s) > maxFd) maxFd = static_cast<int>(s);
            }
        }

        timeval tv{0, 50000}; // 50ms tick
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
                std::cout << "[连接] 新连接 (fd=" << sock << ")" << std::endl;
            }
        }

        // 2. 处理 WebSocket 握手
        for (auto it = pendingHandshake.begin(); it != pendingHandshake.end(); ) {
            WebSocket* ws = *it;
            if (sel > 0 && FD_ISSET(ws->sock, &readSet)) {
                if (ws->tryHandshake()) {
                    std::cout << "[WS] 握手成功 (fd=" << ws->sock << ")" << std::endl;
                    std::cout << "[DEBUG] 发送欢迎消息..." << std::flush;
                    wsSendText(ws, MsgType::SystemMsg,
                        R"({"type":"welcome","msg":"Welcome to Collaborative Whiteboard!"})");
                    std::cout << "完成" << std::endl;
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

        // 3. 处理客户端消息
        for (auto& ws : allClients) {
            if (!ws->handshakeDone || ws->sock == INVALID_SOCKET) continue;
            if (!(sel > 0 && FD_ISSET(ws->sock, &readSet))) continue;

            std::cout << "[DEBUG] recvFrame 开始 fd=" << ws->sock << std::flush;
            auto frame = ws->recvFrame();
            std::cout << " size=" << frame.size() << " hex=" << std::hex;
            for (size_t i = 0; i < std::min(frame.size(), size_t(10)); i++)
                std::cout << (int)frame[i] << " ";
            std::cout << std::dec << std::endl;
            if (frame.empty()) continue;
            if (frame.size() == 1 && frame[0] == 0) {
                // 客户端断开
                std::cout << "[断开] 客户端 (fd=" << ws->sock << ")\n";
                removeClientFromRoom(ws.get());
                closesocket(ws->sock);
                ws->sock = INVALID_SOCKET;
                ws->handshakeDone = false;
                continue;
            }

            if (frame.size() < 5) continue;

            u32 totalLen = frame[0] | (frame[1] << 8) | (frame[2] << 16) | (frame[3] << 24);
            std::cout << "[DEBUG] totalLen=" << totalLen << std::endl << std::flush;
            if (totalLen < 1 || totalLen > 1024 * 1024) {
                std::cout << "[DEBUG] totalLen check FAILED, continue" << std::endl << std::flush;
                continue;
            }
            std::cout << "[DEBUG] check2: 5+totalLen-1=" << (5 + totalLen - 1) << " frame.size=" << frame.size() << std::endl << std::flush;
            if (5 + totalLen - 1 > frame.size()) {
                std::cout << "[DEBUG] frame.size check FAILED, continue" << std::endl << std::flush;
                continue;
            }
            std::cout << "[DEBUG] both checks passed" << std::endl << std::flush;
            MsgType type = static_cast<MsgType>(frame[4]);
            std::vector<u8> payload(frame.begin() + 5, frame.begin() + 5 + (totalLen - 1));
            std::cout << "[DEBUG] type=" << (int)type << " totalLen=" << totalLen
                      << " payloadSize=" << payload.size() << std::endl << std::flush;

            // 查找客户端所在房间
            Room* clientRoom = nullptr;
            {
                std::lock_guard<std::mutex> lock(roomsMutex);
                for (auto& [rid, room] : rooms) {
                    for (auto c : room.clients) {
                        if (c == ws.get()) { clientRoom = &room; break; }
                    }
                    if (clientRoom) break;
                }
            }

            if (clientRoom) {
                // 已在房间内 → 处理画板消息
                switch (type) {
                    case MsgType::DrawStroke:
                        handleDrawStroke(*clientRoom, ws.get(), payload);
                        break;
                    case MsgType::ClearBoard:
                        handleClearBoard(*clientRoom, ws.get());
                        break;
                    case MsgType::DeleteStroke:
                        handleDeleteStroke(*clientRoom, ws.get(), payload);
                        break;
                    case MsgType::CursorMove:
                        handleCursorMove(*clientRoom, ws.get(), payload);
                        break;
                    default:
                        break;
                }
            } else {
                // 未加入房间 → 处理大厅消息（JSON 文本）
                if (type == MsgType::SystemMsg) {
                    // 可能是加入房间请求
                    std::string json(payload.begin(), payload.end());

                    // 简易 JSON 解析
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

                    std::string action = findVal("type");
                    std::cout << "[DEBUG] action=" << action << std::endl;
                    if (action == "join") {
                        std::string roomId = findVal("roomId");
                        std::string userName = findVal("userName");
                        std::cout << "[DEBUG] join roomId=" << roomId << " userName=" << userName << std::endl;
                        if (roomId.empty()) roomId = "default";
                        if (userName.empty()) userName = "Anonymous";
                        std::cout << "[DEBUG] calling handleJoinRoom..." << std::endl;
                        handleJoinRoom(ws.get(), roomId, userName);
                        std::cout << "[DEBUG] handleJoinRoom returned" << std::endl;
                    }
                }
            }
        }

        // 4. 清理断开连接和空房间
        allClients.erase(
            std::remove_if(allClients.begin(), allClients.end(),
                [](auto& ws) { return ws->sock == INVALID_SOCKET; }),
            allClients.end());

        {
            std::lock_guard<std::mutex> lock(roomsMutex);
            for (auto it = rooms.begin(); it != rooms.end(); ) {
                auto& room = it->second;
                for (size_t i = 0; i < room.clients.size(); ) {
                    if (room.clients[i]->sock == INVALID_SOCKET) {
                        room.clients.erase(room.clients.begin() + i);
                        room.userNames.erase(room.userNames.begin() + i);
                        room.userColors.erase(room.userColors.begin() + i);
                    } else {
                        i++;
                    }
                }
                if (room.clients.empty()) {
                    it = rooms.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL] 异常: " << e.what() << "\n" << std::flush;
        WSACleanup();
        return 1;
    } catch (...) {
        std::cerr << "\n[FATAL] 未知异常\n" << std::flush;
        WSACleanup();
        return 1;
    }
}