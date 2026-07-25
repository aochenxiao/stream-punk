// 示例 08：SPOI 跨语言数据互查（C++ 服务器端）
// 展示：C++ 服务器托管游戏状态数据，通过 TCP 接收 Java 客户端发送的 SPOI 查询指令，
//       执行查询后将结果序列化返回。

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

// ===== 辅助函数 =====

// 发送带长度前缀的数据
bool sendWithLength(SOCKET sock, const std::vector<u8>& data) {
    u32 len = static_cast<u32>(data.size());
    // 发送 4 字节长度（小端）
    if (::send(sock, reinterpret_cast<const char*>(&len), sizeof(len), 0) != sizeof(len)) {
        return false;
    }
    // 发送数据
    if (!data.empty()) {
        if (::send(sock, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0) != static_cast<int>(data.size())) {
            return false;
        }
    }
    return true;
}

// 接收带长度前缀的数据
std::vector<u8> recvWithLength(SOCKET sock) {
    // 接收 4 字节长度
    u32 len = 0;
    if (recv(sock, reinterpret_cast<char*>(&len), sizeof(len), MSG_WAITALL) != sizeof(len)) {
        return {};
    }
    // 接收数据
    std::vector<u8> data(len);
    if (len > 0) {
        if (recv(sock, reinterpret_cast<char*>(data.data()), static_cast<int>(len), MSG_WAITALL) != static_cast<int>(len)) {
            return {};
        }
    }
    return data;
}

// 执行 SPOI 查询并返回结果
std::vector<u8> executeSpoiQuery(sp::CrossGameState& state, const std::vector<u8>& queryData) {
    // 将查询数据包装为 istream
    std::string queryStr(queryData.begin(), queryData.end());
    std::stringstream queryStream(queryStr);

    // 创建结果输出流
    std::stringstream resultStream;

    try {
        // 创建执行器并执行查询
        sp::SpoiExecutor executor(queryStream);
        executor.execute(state, resultStream);
    } catch (const std::exception& e) {
        // 返回错误结果
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

    std::cout << "=== SPOI 跨语言数据互查 — C++ 服务器 ===\n\n";

    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup 失败\n";
        return 1;
    }

    // ===== 创建游戏状态数据 =====
    sp::CrossGameState state;
    state.tick = 42;
    state.serverName = "CrossLangServer";

    sp::CrossPlayer p1; p1.name = "Alice"; p1.hp = 80; p1.level = 10; p1.gold = 500;
    sp::CrossPlayer p2; p2.name = "Bob";   p2.hp = 30; p2.level = 5;  p2.gold = 200;
    sp::CrossPlayer p3; p3.name = "Carol"; p3.hp = 60; p3.level = 8;  p3.gold = 350;
    sp::CrossPlayer p4; p4.name = "Dave";  p4.hp = 90; p4.level = 12; p4.gold = 800;
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

    // ===== 创建 TCP 服务器 =====
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

    // 接受连接并处理查询
    SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "accept 失败\n";
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "客户端已连接，开始处理查询...\n\n";

    int queryCount = 0;
    while (true) {
        // 接收查询数据
        std::vector<u8> queryData = recvWithLength(clientSocket);
        if (queryData.empty()) {
            std::cout << "客户端已断开连接。\n";
            break;
        }

        ++queryCount;
        std::cout << "收到查询 #" << queryCount << " (" << queryData.size() << " 字节)\n";

        // 执行查询
        std::vector<u8> resultData = executeSpoiQuery(state, queryData);

        // 发送结果
        if (!sendWithLength(clientSocket, resultData)) {
            std::cerr << "发送结果失败\n";
            break;
        }

        std::cout << "  返回结果 " << resultData.size() << " 字节\n";
    }

    // 清理
    closesocket(clientSocket);
    closesocket(listenSocket);
    WSACleanup();

    std::cout << "\n服务器已关闭。\n";
    return 0;
}