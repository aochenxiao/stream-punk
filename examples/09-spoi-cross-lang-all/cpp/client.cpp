// 示例 09：SPOI 全语言跨语言数据互查（C++ 客户端）
// 展示：C++ 客户端通过 TCP 向 C++ 服务器发送 SPOI 查询指令，接收并展示查询结果。
// 使用 StreamPunk 原生 SPOI API 构建查询和解析结果。

#include "../../../include/stream-punk/StreamPunk.hpp"
#include "../../../include/stream-punk/StreamPunkJson.hpp"
#include "../../../include/stream-punk/StreamPunkSPOI.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <iomanip>

#pragma comment(lib, "ws2_32.lib")

using namespace sp;

// ===== TCP 通信 =====

bool sendWithLength(SOCKET sock, const std::vector<u8>& data) {
    u32 len = static_cast<u32>(data.size());
    if (send(sock, reinterpret_cast<const char*>(&len), sizeof(len), 0) != sizeof(len)) return false;
    if (!data.empty()) {
        if (send(sock, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0) != static_cast<int>(data.size())) return false;
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

// ===== SPOI 查询构建器 =====
// 使用 StreamPunk 原生 API 构建 SpoiStream 二进制

class SpoiQueryBuilder {
    SpoiStream stream;
    std::vector<u32> _currentPath;

public:
    // 构建比较表达式
    static SpoiCmpExpr cmpExpr(u32 memberIdx, u8 cmpOp, const std::vector<u8>& value) {
        SpoiCmpExpr expr;
        expr.memberIdx = memberIdx;
        expr.cmpOp = cmpOp;
        expr.value = value;
        return expr;
    }

    static std::vector<u8> cmpExprBytes(u32 memberIdx, u8 cmpOp, const std::vector<u8>& value) {
        auto expr = cmpExpr(memberIdx, cmpOp, value);
        std::stringstream ss;
        O o(ss);
        o << expr;
        auto str = ss.str();
        return std::vector<u8>(str.begin(), str.end());
    }

    // 序列化 i32 为 LE 字节
    static std::vector<u8> i32le(i32 v) {
        return std::vector<u8>(reinterpret_cast<u8*>(&v), reinterpret_cast<u8*>(&v) + sizeof(v));
    }

    // 序列化 u32 为 LE 字节
    static std::vector<u8> u32le(u32 v) {
        return std::vector<u8>(reinterpret_cast<u8*>(&v), reinterpret_cast<u8*>(&v) + sizeof(v));
    }

    // 序列化字符串（长度前缀 + UTF-8）
    static std::vector<u8> strBytes(const std::string& s) {
        std::vector<u8> result;
        u32 len = static_cast<u32>(s.size());
        auto lenBytes = u32le(len);
        result.insert(result.end(), lenBytes.begin(), lenBytes.end());
        result.insert(result.end(), s.begin(), s.end());
        return result;
    }

    // 添加指令
    void addInst(u8 op, const std::vector<u32>& path, const std::vector<u8>& operand) {
        SpoiInstruction inst;
        inst.op = op;
        inst.path = path;
        inst.operand = operand;
        stream.instructions.push_back(std::move(inst));
    }

    // 从 players 开始管道
    SpoiQueryBuilder& fromPlayers() {
        addInst(static_cast<u8>(SpoiOp::e_filter), {0}, cmpExprBytes(1, 5, i32le(0)));
        return *this;
    }

    // filter
    SpoiQueryBuilder& filter(u32 field, u8 cmpOp, i32 value) {
        addInst(static_cast<u8>(SpoiOp::e_filter), {}, cmpExprBytes(field, cmpOp, i32le(value)));
        return *this;
    }

    // filter by string
    SpoiQueryBuilder& filterStr(u32 field, u8 cmpOp, const std::string& value) {
        addInst(static_cast<u8>(SpoiOp::e_filter), {}, cmpExprBytes(field, cmpOp, strBytes(value)));
        return *this;
    }

    // sort
    SpoiQueryBuilder& sort(u32 field, bool ascending) {
        std::vector<u8> operand;
        auto fb = u32le(field);
        operand.insert(operand.end(), fb.begin(), fb.end());
        operand.push_back(ascending ? 1 : 0);
        addInst(static_cast<u8>(SpoiOp::e_sort), {}, operand);
        return *this;
    }

    // reverse
    SpoiQueryBuilder& reverse() {
        addInst(static_cast<u8>(SpoiOp::e_reverse), {}, {});
        return *this;
    }

    // take
    SpoiQueryBuilder& take(u32 n) {
        addInst(static_cast<u8>(SpoiOp::e_take), {}, u32le(n));
        return *this;
    }

    // drop
    SpoiQueryBuilder& drop(u32 n) {
        addInst(static_cast<u8>(SpoiOp::e_drop), {}, u32le(n));
        return *this;
    }

    // count
    SpoiQueryBuilder& count() {
        addInst(static_cast<u8>(SpoiOp::e_count), {}, {});
        return *this;
    }

    // any
    SpoiQueryBuilder& any(u32 field, u8 cmpOp, i32 value) {
        addInst(static_cast<u8>(SpoiOp::e_any), {}, cmpExprBytes(field, cmpOp, i32le(value)));
        return *this;
    }

    // find
    SpoiQueryBuilder& find(u32 field, u8 cmpOp, i32 value) {
        addInst(static_cast<u8>(SpoiOp::e_find), {}, cmpExprBytes(field, cmpOp, i32le(value)));
        return *this;
    }

    // find by string
    SpoiQueryBuilder& findStr(u32 field, const std::string& value) {
        addInst(static_cast<u8>(SpoiOp::e_find), {}, cmpExprBytes(field, 0, strBytes(value)));
        return *this;
    }

    // set
    SpoiQueryBuilder& set(const std::vector<u32>& path, i32 value) {
        addInst(static_cast<u8>(SpoiOp::e_set), path, i32le(value));
        return *this;
    }

    // add
    SpoiQueryBuilder& add(const std::vector<u32>& path, i32 delta) {
        addInst(static_cast<u8>(SpoiOp::e_add), path, i32le(delta));
        return *this;
    }

    // 构建二进制
    std::vector<u8> build() {
        addInst(static_cast<u8>(SpoiOp::e_exec), {}, {});
        std::stringstream ss;
        O o(ss);
        o << stream;
        auto str = ss.str();
        return std::vector<u8>(str.begin(), str.end());
    }
};

// ===== 结果解析 =====

void printResult(const std::vector<u8>& data) {
    if (data.empty()) {
        std::cout << "(空结果)" << std::endl;
        return;
    }

    SpoiResult result;
    std::string dataStr(data.begin(), data.end());
    std::stringstream dss(dataStr);
    I i(dss);
    i >> result;

    switch (static_cast<ResultType>(result.resultType)) {
        case ResultType::e_count: {
            if (result.data.size() >= 4) {
                i32 count = *reinterpret_cast<const i32*>(result.data.data());
                std::cout << "计数结果: " << count << std::endl;
            }
            break;
        }
        case ResultType::e_bool: {
            std::cout << "布尔结果: " << (result.data[0] ? "true" : "false") << std::endl;
            break;
        }
        case ResultType::e_vector: {
            // data 内部格式：[varint count][elements...]
            std::string innerStr(result.data.begin(), result.data.end());
            std::stringstream innerSS(innerStr);
            u32 count = readVarint(innerSS);
            std::cout << "向量结果: " << count << " 个元素" << std::endl;
            for (u32 idx = 0; idx < count; idx++) {
                // 读取字符串长度
                u32 nameLen = 0;
                innerSS.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
                std::string name(nameLen, '\0');
                innerSS.read(name.data(), nameLen);
                // 读取 hp, level, gold
                i32 hp = 0, level = 0, gold = 0;
                innerSS.read(reinterpret_cast<char*>(&hp), sizeof(hp));
                innerSS.read(reinterpret_cast<char*>(&level), sizeof(level));
                innerSS.read(reinterpret_cast<char*>(&gold), sizeof(gold));
                std::cout << "    [" << idx << "] Player{name='" << name << "', hp=" << hp
                          << ", level=" << level << ", gold=" << gold << "}" << std::endl;
            }
            break;
        }
        case ResultType::e_single: {
            std::string innerStr(result.data.begin(), result.data.end());
            std::stringstream innerSS(innerStr);
            u32 nameLen = 0;
            innerSS.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
            std::string name(nameLen, '\0');
            innerSS.read(name.data(), nameLen);
            i32 hp = 0, level = 0, gold = 0;
            innerSS.read(reinterpret_cast<char*>(&hp), sizeof(hp));
            innerSS.read(reinterpret_cast<char*>(&level), sizeof(level));
            innerSS.read(reinterpret_cast<char*>(&gold), sizeof(gold));
            std::cout << "单个结果: Player{name='" << name << "', hp=" << hp
                      << ", level=" << level << ", gold=" << gold << "}" << std::endl;
            break;
        }
        case ResultType::e_optional: {
            if (result.data.size() > 0 && result.data[0] != 0) {
                std::string innerStr(result.data.begin() + 1, result.data.end());
                std::stringstream innerSS(innerStr);
                u32 nameLen = 0;
                innerSS.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
                std::string name(nameLen, '\0');
                innerSS.read(name.data(), nameLen);
                i32 hp = 0, level = 0, gold = 0;
                innerSS.read(reinterpret_cast<char*>(&hp), sizeof(hp));
                innerSS.read(reinterpret_cast<char*>(&level), sizeof(level));
                innerSS.read(reinterpret_cast<char*>(&gold), sizeof(gold));
                std::cout << "可选结果: 有值 → Player{name='" << name << "', hp=" << hp
                          << ", level=" << level << ", gold=" << gold << "}" << std::endl;
            } else {
                std::cout << "可选结果: 空" << std::endl;
            }
            break;
        }
        case ResultType::e_error: {
            std::string errMsg(result.data.begin(), result.data.end());
            std::cout << "错误: " << errMsg << std::endl;
            break;
        }
        default:
            std::cout << "未知结果类型: " << static_cast<int>(result.resultType) << std::endl;
            break;
    }
}

// ===== 常量 =====
constexpr u32 PLAYER_NAME  = 0;
constexpr u32 PLAYER_HP    = 1;
constexpr u32 PLAYER_LEVEL = 2;
constexpr u32 PLAYER_GOLD  = 3;
constexpr u8  CMP_EQ = 0, CMP_NE = 1, CMP_LT = 2, CMP_GT = 3, CMP_LE = 4, CMP_GE = 5;

// ===== 主程序 =====

int main() {
    std::cout << "=== SPOI 跨语言数据互查 — C++ 客户端 ===\n" << std::endl;

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9999);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "无法连接到服务器 127.0.0.1:9999\n请确保 C++ 服务器已启动！" << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "已连接到服务器 127.0.0.1:9999\n" << std::endl;

    int testNum = 0;

    // 查询 1: 统计玩家总数
    std::cout << "--- 查询 " << ++testNum << ": 统计玩家总数 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().count().build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // 查询 2: 过滤 hp > 50
    std::cout << "--- 查询 " << ++testNum << ": 过滤 hp > 50 的玩家 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_GT, 50).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // 查询 3: 过滤 level >= 8，取前 2 个
    std::cout << "--- 查询 " << ++testNum << ": 过滤 level >= 8，取前 2 个 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_LEVEL, CMP_GE, 8).take(2).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // 查询 4: 查找名为 "Alice" 的玩家
    std::cout << "--- 查询 " << ++testNum << ": 查找名为 \"Alice\" 的玩家 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().findStr(PLAYER_NAME, "Alice").build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // 查询 5: 按 hp 降序排列，取前 3 个
    std::cout << "--- 查询 " << ++testNum << ": 按 hp 降序排列，取前 3 个 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().sort(PLAYER_HP, false).take(3).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // 查询 6: 检查是否有 hp < 20 的玩家
    std::cout << "--- 查询 " << ++testNum << ": 检查是否有 hp < 20 的玩家 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().any(PLAYER_HP, CMP_LT, 20).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // 查询 7: 统计 hp > 0 的玩家数
    std::cout << "--- 查询 " << ++testNum << ": 统计 hp > 0 的玩家数 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_GT, 0).count().build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // 查询 8: 复杂链式查询
    std::cout << "--- 查询 " << ++testNum << ": 复杂链式查询（filter + sort + reverse + take） ---" << std::endl;
    std::cout << "    (hp > 30 → 按 level 排序 → 反转 → 取前 2)" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers()
            .filter(PLAYER_HP, CMP_GT, 30)
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .take(2).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // 查询 9: 写操作 — 设置 hp
    std::cout << "--- 查询 " << ++testNum << ": 写操作 — 将玩家[0]的 hp 设置为 99 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.set({0, 0, PLAYER_HP}, 99).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // 查询 10: 验证写操作
    std::cout << "--- 查询 " << ++testNum << ": 验证写操作 — 查找 Alice 的 hp 是否变为 99 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().findStr(PLAYER_NAME, "Alice").build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // 查询 11: 写操作 — 增加金币
    std::cout << "--- 查询 " << ++testNum << ": 写操作 — 给玩家[0]增加 100 金币 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.add({0, 0, PLAYER_GOLD}, 100).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // 查询 12: 验证金币增加
    std::cout << "--- 查询 " << ++testNum << ": 验证写操作 — 查找 Alice 的金币是否变为 600 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().findStr(PLAYER_NAME, "Alice").build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // 查询 13: filter + drop
    std::cout << "--- 查询 " << ++testNum << ": filter(hp > 20) + drop(2) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_GT, 20).drop(2).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // ================================================================
    //  进阶查询：难度逐级上升，刁钻组合验证库正确性
    // ================================================================
    std::cout << "\n========== 进阶查询 ==========\n" << std::endl;

    // ---- 等级 1: 多条件组合过滤 ----
    std::cout << "--- 查询 " << ++testNum << ": 【L1】多条件 AND 过滤 (hp>30 AND level>5) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_GT, 30).filter(PLAYER_LEVEL, CMP_GT, 5).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L1】互斥条件过滤 (hp>30 AND level<=5) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_GT, 30).filter(PLAYER_LEVEL, CMP_LE, 5).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L1】三条件过滤 (hp>20 AND level>3 AND gold>200) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_GT, 20).filter(PLAYER_LEVEL, CMP_GT, 3).filter(PLAYER_GOLD, CMP_GT, 200).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // ---- 等级 2: 边界条件 ----
    std::cout << "--- 查询 " << ++testNum << ": 【L2】空结果 — 过滤 hp>9000 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_GT, 9000).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L2】TAKE 超出范围 — TAKE(100) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().take(100).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L2】DROP 超出范围 — DROP(100) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().drop(100).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L2】空管道 COUNT — DROP(100) + COUNT ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().drop(100).count().build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L2】空管道 FIND — 无匹配时查找 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_GT, 9000).find(PLAYER_HP, CMP_GT, 0).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L2】空管道 ANY — 无匹配时检查存在性 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_GT, 9000).any(PLAYER_HP, CMP_GT, 0).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // ---- 等级 3: 复杂管道操作 ----
    std::cout << "--- 查询 " << ++testNum << ": 【L3】SORT + DROP + TAKE 链 ---" << std::endl;
    std::cout << "    (按 level 升序 → 跳过前 2 → 取前 2)" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().sort(PLAYER_LEVEL, true).drop(2).take(2).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L3】SORT + REVERSE + DROP + TAKE + COUNT ---" << std::endl;
    std::cout << "    (按 hp 降序 → 反转 → 跳过 1 → 取 2 → 计数)" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().sort(PLAYER_HP, false).reverse().drop(1).take(2).count().build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L3】管道中间再过滤 — SORT + REVERSE + DROP + TAKE + FILTER ---" << std::endl;
    std::cout << "    (按 level 升序 → 反转 → 跳过 1 → 取 3 → 过滤 hp>40)" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().sort(PLAYER_LEVEL, true).reverse().drop(1).take(3).filter(PLAYER_HP, CMP_GT, 40).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // ---- 等级 4: 字符串操作 ----
    std::cout << "--- 查询 " << ++testNum << ": 【L4】字符串 NE 过滤 — name != \"Alice\" ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filterStr(PLAYER_NAME, CMP_NE, "Alice").build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L4】字符串比较过滤 — name < \"Carol\" ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filterStr(PLAYER_NAME, CMP_LT, "Carol").build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L4】查找不存在的名字 — FIND \"Zoe\" ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().findStr(PLAYER_NAME, "Zoe").build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // ---- 等级 5: 写后查询（先写再查，同一查询内完成） ----
    std::cout << "--- 查询 " << ++testNum << ": 【L5】SET + ADD + 查询 — 先改 hp=50，再加 30，再查 Alice ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.set({0, 0, PLAYER_HP}, 50).add({0, 0, PLAYER_HP}, 30).fromPlayers().findStr(PLAYER_NAME, "Alice").build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L5】多次写后查询 — SET Alice hp=999, SET Bob gold=9999，查 gold>9000 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.set({0, 0, PLAYER_HP}, 999).set({0, 1, PLAYER_GOLD}, 9999).fromPlayers().filter(PLAYER_GOLD, CMP_GT, 9000).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L5】负值 ADD — Alice 金币 -300，再查 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.add({0, 0, PLAYER_GOLD}, -300).fromPlayers().findStr(PLAYER_NAME, "Alice").build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // ---- 等级 6: 全比较运算符验证 ----
    std::cout << "--- 查询 " << ++testNum << ": 【L6】全比较运算符 — EQ(hp, 60) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_EQ, 60).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L6】全比较运算符 — NE(hp, 60) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_NE, 60).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L6】全比较运算符 — LT(hp, 60) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_LT, 60).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L6】全比较运算符 — GT(hp, 60) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_GT, 60).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L6】全比较运算符 — LE(hp, 60) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_LE, 60).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L6】全比较运算符 — GE(hp, 60) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_GE, 60).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // ---- 等级 7: 极限链 — 10 步操作 ----
    std::cout << "--- 查询 " << ++testNum << ": 【L7】极限链 — 10 步操作 ---" << std::endl;
    std::cout << "    (FILTER → SORT(level) → REVERSE → DROP(1) → TAKE(4) → FILTER(hp>20) → SORT(hp,desc) → REVERSE → TAKE(2) → COUNT)" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers()
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .drop(1)
            .take(4)
            .filter(PLAYER_HP, CMP_GT, 20)
            .sort(PLAYER_HP, false)
            .reverse()
            .take(2)
            .count()
            .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // ================================================================
    //  进阶查询 L8-L12：难度逐级上升，刁钻组合验证库正确性
    // ================================================================
    std::cout << "\n========== 高阶查询 L8-L12 ==========\n" << std::endl;

    // ---- 等级 8: 管道操作边缘情况 ----
    std::cout << "--- 查询 " << ++testNum << ": 【L8】REVERSE x2 — 应与原始顺序相同 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().reverse().reverse().build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L8】TAKE(0) — 取0个元素（空向量） ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().take(0).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L8】DROP(0) — 丢弃0个（应返回全部） ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().drop(0).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L8】SORT 覆盖 — SORT(level,asc) + SORT(hp,desc) ---" << std::endl;
    std::cout << "    (以最后一次排序为准)" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().sort(PLAYER_LEVEL, true).sort(PLAYER_HP, false).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L8】REVERSE x3 — 等同于单次 REVERSE ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().reverse().reverse().reverse().build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L8】DROP 到只剩 1 个 + TAKE(1) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().drop(4).take(1).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // ---- 等级 9: 数值边界与极端值 ----
    std::cout << "--- 查询 " << ++testNum << ": 【L9】FILTER hp < 0 — 无玩家 hp 为负 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_LT, 0).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L9】SET hp=0, FILTER hp EQ 0 — 零值精确匹配 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.set({0, 0, PLAYER_HP}, 0).fromPlayers().filter(PLAYER_HP, CMP_EQ, 0).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L9】ADD 负值使金币变负, FILTER gold < 0 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.add({0, 4, PLAYER_GOLD}, -200).fromPlayers().filter(PLAYER_GOLD, CMP_LT, 0).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L9】互斥条件 — FILTER hp>0, FILTER hp<=0（必然空） ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_GT, 0).filter(PLAYER_HP, CMP_LE, 0).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L9】FILTER level = 0 — 不存在 level=0 的玩家 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_LEVEL, CMP_EQ, 0).build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L9】FILTER hp >= 0（全部通过） + COUNT ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers().filter(PLAYER_HP, CMP_GE, 0).count().build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // ---- 等级 10: 写操作与管道混合 ----
    std::cout << "--- 查询 " << ++testNum << ": 【L10】多次 SET 后管道查询 — 改 3 个玩家 hp，然后 FILTER + SORT ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.set({0, 0, PLAYER_HP}, 10)
                      .set({0, 1, PLAYER_HP}, 20)
                      .set({0, 2, PLAYER_HP}, 30)
                      .fromPlayers()
                      .filter(PLAYER_HP, CMP_GT, 15)
                      .sort(PLAYER_HP, true)
                      .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L10】SET + ADD 同一字段后查询 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.set({0, 0, PLAYER_GOLD}, 100)
                      .add({0, 0, PLAYER_GOLD}, 50)
                      .fromPlayers()
                      .findStr(PLAYER_NAME, "Alice")
                      .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L10】ADD 全部玩家 level+1, 然后 FILTER + COUNT ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.add({0, 0, PLAYER_LEVEL}, 1)
                      .add({0, 1, PLAYER_LEVEL}, 1)
                      .add({0, 2, PLAYER_LEVEL}, 1)
                      .add({0, 3, PLAYER_LEVEL}, 1)
                      .add({0, 4, PLAYER_LEVEL}, 1)
                      .fromPlayers()
                      .filter(PLAYER_LEVEL, CMP_GT, 5)
                      .count()
                      .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L10】SET 不存在索引 [0,99] — 应静默忽略，无玩家 hp>9000 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.set({0, 99, PLAYER_HP}, 9999)
                      .fromPlayers()
                      .filter(PLAYER_HP, CMP_GT, 9000)
                      .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L10】写入后管道操作 — SET hp=55, SORT hp, REVERSE, TAKE(2) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.set({0, 0, PLAYER_HP}, 55)
                      .fromPlayers()
                      .sort(PLAYER_HP, true)
                      .reverse()
                      .take(2)
                      .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // ---- 等级 11: 交叉字段查询 ----
    std::cout << "--- 查询 " << ++testNum << ": 【L11】FILTER(hp>30) + SORT(gold) + ANY(level>8) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers()
                      .filter(PLAYER_HP, CMP_GT, 30)
                      .sort(PLAYER_GOLD, true)
                      .any(PLAYER_LEVEL, CMP_GT, 8)
                      .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L11】FILTER(gold>200) + FILTER(hp>50) + SORT(level) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers()
                      .filter(PLAYER_GOLD, CMP_GT, 200)
                      .filter(PLAYER_HP, CMP_GT, 50)
                      .sort(PLAYER_LEVEL, true)
                      .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L11】FILTER(gold>200) + SORT(hp) + FIND(level=12) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers()
                      .filter(PLAYER_GOLD, CMP_GT, 200)
                      .sort(PLAYER_HP, true)
                      .find(PLAYER_LEVEL, CMP_EQ, 12)
                      .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L11】FILTER(hp>50) + SORT(level) + REVERSE + ANY(gold>300) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers()
                      .filter(PLAYER_HP, CMP_GT, 50)
                      .sort(PLAYER_LEVEL, true)
                      .reverse()
                      .any(PLAYER_GOLD, CMP_GT, 300)
                      .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L11】全字段三条件 — hp>25 AND level>4 AND gold>150 ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers()
                      .filter(PLAYER_HP, CMP_GT, 25)
                      .filter(PLAYER_LEVEL, CMP_GT, 4)
                      .filter(PLAYER_GOLD, CMP_GT, 150)
                      .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    // ---- 等级 12: 极限组合压力 ----
    std::cout << "--- 查询 " << ++testNum << ": 【L12】15步极限链 — SORT→REVERSE→DROP→TAKE→FILTER→SORT→REVERSE→TAKE→FILTER→SORT→REVERSE→DROP→TAKE→COUNT ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers()
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .drop(1)
            .take(4)
            .filter(PLAYER_HP, CMP_GT, 20)
            .sort(PLAYER_HP, false)
            .reverse()
            .take(3)
            .filter(PLAYER_GOLD, CMP_GT, 100)
            .sort(PLAYER_GOLD, true)
            .reverse()
            .drop(1)
            .take(2)
            .count()
            .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L12】写入全部 5 个玩家, 然后复杂链查询 ---" << std::endl;
    std::cout << "    (SET 5个玩家hp → fromPlayers → FILTER hp>30 → SORT hp → REVERSE → TAKE(3))" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.set({0, 0, PLAYER_HP}, 100)
                      .set({0, 1, PLAYER_HP}, 200)
                      .set({0, 2, PLAYER_HP}, 150)
                      .set({0, 3, PLAYER_HP}, 50)
                      .set({0, 4, PLAYER_HP}, 175)
                      .fromPlayers()
                      .filter(PLAYER_HP, CMP_GT, 30)
                      .sort(PLAYER_HP, true)
                      .reverse()
                      .take(3)
                      .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L12】SORT+REVERSE 循环 3 次 — 稳定性测试 ---" << std::endl;
    std::cout << "    (SORT(level,asc)→REVERSE→SORT(hp,desc)→REVERSE→SORT(gold,asc)→REVERSE)" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers()
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .sort(PLAYER_HP, false)
            .reverse()
            .sort(PLAYER_GOLD, true)
            .reverse()
            .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L12】过滤到单元素 + 全操作 — FILTER(hp>85)→SORT→REVERSE→DROP(0)→TAKE(1) ---" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.fromPlayers()
            .filter(PLAYER_HP, CMP_GT, 85)
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .drop(0)
            .take(1)
            .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "--- 查询 " << ++testNum << ": 【L12】极限混合 — 写 + 读 + 排序 + 反转 + 过滤 + 计数 ---" << std::endl;
    std::cout << "    (SET hp=60→ADD gold=50→fromPlayers→FILTER hp>30→SORT level→REVERSE→DROP(1)→TAKE(3)→FILTER gold>100→SORT hp→REVERSE→TAKE(2)→COUNT)" << std::endl;
    {
        SpoiQueryBuilder q;
        auto query = q.set({0, 0, PLAYER_HP}, 60)
                      .add({0, 0, PLAYER_GOLD}, 50)
                      .fromPlayers()
                      .filter(PLAYER_HP, CMP_GT, 30)
                      .sort(PLAYER_LEVEL, true)
                      .reverse()
                      .drop(1)
                      .take(3)
                      .filter(PLAYER_GOLD, CMP_GT, 100)
                      .sort(PLAYER_HP, false)
                      .reverse()
                      .take(2)
                      .count()
                      .build();
        sendWithLength(sock, query);
        printResult(recvWithLength(sock));
        std::cout << std::endl;
    }

    std::cout << "=== 所有查询完成 ===" << std::endl;

    closesocket(sock);
    WSACleanup();
    return 0;
}