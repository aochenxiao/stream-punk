// 示例 09：SPOI 全语言跨语言数据互查（C++ 服务器端）
// 展示：C++ 服务器托管游戏状态数据，通过 TCP 接收各语言客户端发送的 SPOI 查询指令，
//       执行查询后将结果序列化返回。支持多客户端顺序连接。

#include "../../../include/stream-punk/StreamPunk.hpp"
#include "../../../include/stream-punk/StreamPunkJson.hpp"
#include "../../../include/stream-punk/StreamPunkSPOI.hpp"
#include "../../../include/stream-punk/StreamPunkSPOIRange.hpp"
#include "../../../include/stream-punk/StreamPunkSPOIShadow.hpp"
#include "../../../include/stream-punk/StreamPunkSPOIExecutor.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

using namespace sp;

// ===== 数据定义 =====
namespace sp {
    struct CrossPlayer;
    struct CrossGameState;
}

template<> struct TypeDesc<sp::CrossPlayer> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};
template<> struct TypeDesc<sp::CrossGameState> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};

namespace sp {

struct CrossPlayer : public Base {
    #define Xt_CrossPlayer(X__) \
        X__(std::string, name,  "") \
        X__(i32,         hp,    100) \
        X__(i32,         level, 1) \
        X__(i32,         gold,  0)
    UseData(CrossPlayer);
};
UseSPOI(CrossPlayer, Xt_CrossPlayer);

struct CrossGameState : public Base {
    #define Xt_CrossGameState(X__) \
        X__(std::vector<CrossPlayer>, players,    {}) \
        X__(i32,                      tick,       0) \
        X__(std::string,              serverName, "")
    UseData(CrossGameState);
};
UseSPOI(CrossGameState, Xt_CrossGameState);
UseSPOIShadow(CrossGameState, Xt_CrossGameState);

} // namespace sp

// ===== TCP 辅助函数 =====

bool sendWithLength(SOCKET sock, const std::vector<u8>& data) {
    u32 len = static_cast<u32>(data.size());
    if (::send(sock, reinterpret_cast<const char*>(&len), sizeof(len), 0) != sizeof(len)) return false;
    if (!data.empty()) {
        if (::send(sock, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0) != static_cast<int>(data.size())) return false;
    }
    return true;
}

std::vector<u8> recvWithLength(SOCKET sock) {
    u32 len = 0;
    if (recv(sock, reinterpret_cast<char*>(&len), sizeof(len), MSG_WAITALL) != sizeof(len)) return {};
    std::vector<u8> data(len);
    if (len > 0) {
        if (recv(sock, reinterpret_cast<char*>(data.data()), static_cast<int>(len), MSG_WAITALL) != static_cast<int>(len)) return {};
    }
    return data;
}

std::vector<u8> executeSpoiQuery(sp::CrossGameState& state, const std::vector<u8>& queryData) {
    std::string queryStr(queryData.begin(), queryData.end());
    std::stringstream queryStream(queryStr);
    std::stringstream resultStream;
    try {
        sp::SpoiExecutor executor(queryStream);
        executor.execute(state, resultStream);
    } catch (const std::exception& e) {
        sp::SpoiResult errResult;
        errResult.resultType = static_cast<u8>(sp::ResultType::e_error);
        std::string errMsg = e.what();
        errResult.data = std::vector<u8>(errMsg.begin(), errMsg.end());
        std::stringstream errSS;
        O o(errSS);
        o << errResult;
        auto errStr = errSS.str();
        return std::vector<u8>(errStr.begin(), errStr.end());
    }
    auto resultStr = resultStream.str();
    return std::vector<u8>(resultStr.begin(), resultStr.end());
}

// ===== 主函数 =====

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    std::cout << "=== SPOI 全语言跨语言数据互查 — C++ 服务器 ===\n\n";

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup 失败\n";
        return 1;
    }

    // 创建游戏状态数据
    sp::CrossGameState state;
    state.tick = 42;
    state.serverName = "CrossLangServer";

    sp::CrossPlayer p1; p1.name = "Alice"; p1.hp = 80; p1.level = 10; p1.gold = 500;
    sp::CrossPlayer p2; p2.name = "Bob";   p2.hp = 30; p2.level = 5;  p2.gold = 200;
    sp::CrossPlayer p3; p3.name = "Carol"; p3.hp = 60; p3.level = 8;  p3.gold = 300;
    sp::CrossPlayer p4; p4.name = "Dave";  p4.hp = 90; p4.level = 12; p4.gold = 400;
    sp::CrossPlayer p5; p5.name = "Eve";   p5.hp = 15; p5.level = 3;  p5.gold = 100;
    state.players.push_back(p1);
    state.players.push_back(p2);
    state.players.push_back(p3);
    state.players.push_back(p4);
    state.players.push_back(p5);

    std::cout << "游戏状态已初始化：\n";
    std::cout << "  服务器名称: " << state.serverName << "\n";
    std::cout << "  tick: " << state.tick << "\n";
    std::cout << "  玩家数: " << state.players.size() << "\n";
    for (auto& p : state.players) {
        std::cout << "    " << p.name << ": hp=" << p.hp << " level=" << p.level << " gold=" << p.gold << "\n";
    }

    // 创建 TCP 服务器
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "socket 创建失败\n";
        WSACleanup();
        return 1;
    }

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

    std::cout << "\n服务器正在监听端口 9999，等待客户端连接...\n";

    int clientNum = 0;
    while (true) {
        SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "accept 失败\n";
            break;
        }

        ++clientNum;
        std::cout << "\n[客户端 #" << clientNum << "] 已连接\n";

        // 重置游戏状态（每个客户端使用独立状态）
        state.players.clear();
        sp::CrossPlayer pp1; pp1.name = "Alice"; pp1.hp = 80; pp1.level = 10; pp1.gold = 500;
        sp::CrossPlayer pp2; pp2.name = "Bob";   pp2.hp = 30; pp2.level = 5;  pp2.gold = 200;
        sp::CrossPlayer pp3; pp3.name = "Carol"; pp3.hp = 60; pp3.level = 8;  pp3.gold = 300;
        sp::CrossPlayer pp4; pp4.name = "Dave";  pp4.hp = 90; pp4.level = 12; pp4.gold = 400;
        sp::CrossPlayer pp5; pp5.name = "Eve";   pp5.hp = 15; pp5.level = 3;  pp5.gold = 100;
        state.players.push_back(pp1);
        state.players.push_back(pp2);
        state.players.push_back(pp3);
        state.players.push_back(pp4);
        state.players.push_back(pp5);

        int queryCount = 0;
        while (true) {
            std::vector<u8> queryData = recvWithLength(clientSocket);
            if (queryData.empty()) {
                std::cout << "[客户端 #" << clientNum << "] 已断开连接\n";
                break;
            }

            ++queryCount;
            std::vector<u8> resultData = executeSpoiQuery(state, queryData);

            if (!sendWithLength(clientSocket, resultData)) {
                std::cerr << "[客户端 #" << clientNum << "] 发送结果失败\n";
                break;
            }
        }

        closesocket(clientSocket);
    }

    closesocket(listenSocket);
    WSACleanup();
    std::cout << "\n服务器已关闭。\n";
    return 0;
}