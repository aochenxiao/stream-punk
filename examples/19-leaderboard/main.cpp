// 示例 19：实时排行榜系统
// 展示 StreamPunk 的 SPOI 查询在排行榜场景中的应用：
//   SPOI query(filter + sort + take) → 实时 Top-N 排行
//   增量更新 → 分数变化时只传增量，不重发全量
//
// 场景：游戏排行榜，C++ 服务端维护所有玩家分数，
//       前端通过 SPOI 查询 Top 10，分数变化时增量推送

#include "stream-punk/StreamPunk.hpp"
#include "stream-punk/StreamPunkJson.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <random>
#include <iomanip>

using namespace sp;

// ============================================================
// 1. 定义排行榜数据类型
// ============================================================

struct LeaderboardEntry : public Base {
#define Xt_LeaderboardEntry(X__) \
    X__(std::string, playerName, "") \
    X__(i32, score, 0) \
    X__(i32, level, 1) \
    X__(i32, wins, 0) \
    X__(i32, losses, 0) \
    X__(std::chrono::system_clock::time_point, lastActive, {})

    LeaderboardEntry() = default;
    UseData(LeaderboardEntry);
    UseDataJson(LeaderboardEntry);
};
REGISTER_JSON_TYPE(LeaderboardEntry);
template<> struct TypeDesc<LeaderboardEntry> : TypeDescCustom<LeaderboardEntry> {};

struct Leaderboard : public Base {
    using EntryVec = std::vector<LeaderboardEntry>;

#define Xt_Leaderboard(X__) \
    X__(EntryVec, entries, {}) \
    X__(i32, seasonNumber, 1) \
    X__(std::chrono::system_clock::time_point, lastUpdated, {})

    Leaderboard() = default;
    UseData(Leaderboard);
    UseDataJson(Leaderboard);
};
REGISTER_JSON_TYPE(Leaderboard);
template<> struct TypeDesc<Leaderboard> : TypeDescCustom<Leaderboard> {};

// ============================================================
// 2. 排行榜查询工具
// ============================================================

// 模拟 SPOI 查询：按分数降序取 Top N
std::vector<LeaderboardEntry> topN(Leaderboard const& board, int n) {
    std::vector<LeaderboardEntry> sorted = board.entries;
    std::sort(sorted.begin(), sorted.end(),
              [](auto& a, auto& b) { return a.score > b.score; });
    if (sorted.size() > (size_t)n) sorted.resize(n);
    return sorted;
}

// 模拟 SPOI 查询：按等级降序，胜率 > 50% 的玩家
std::vector<LeaderboardEntry> filterByWinRate(Leaderboard const& board, double minRate) {
    std::vector<LeaderboardEntry> result;
    for (auto& e : board.entries) {
        int total = e.wins + e.losses;
        if (total > 0 && (double)e.wins / total >= minRate) {
            result.push_back(e);
        }
    }
    std::sort(result.begin(), result.end(),
              [](auto& a, auto& b) { return a.level > b.level; });
    return result;
}

// 显示排行榜
void printTopN(std::vector<LeaderboardEntry> const& entries, std::string const& title) {
    std::cout << "  " << title << ":" << std::endl;
    std::cout << "  " << std::string(50, '-') << std::endl;
    std::cout << "  " << std::setw(4) << "排名" << std::setw(12) << "玩家"
              << std::setw(8) << "分数" << std::setw(8) << "等级"
              << std::setw(8) << "胜场" << std::setw(8) << "败场" << std::endl;
    std::cout << "  " << std::string(50, '-') << std::endl;

    int rank = 1;
    for (auto& e : entries) {
        std::cout << "  " << std::setw(4) << rank++
                  << std::setw(12) << e.playerName
                  << std::setw(8) << e.score
                  << std::setw(8) << e.level
                  << std::setw(8) << e.wins
                  << std::setw(8) << e.losses << std::endl;
    }
    std::cout << std::endl;
}

// ============================================================
// 3. 主演示
// ============================================================

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    std::cout << "======================================================" << std::endl;
    std::cout << "  StreamPunk 实时排行榜系统 演示" << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << std::endl;

    // ---- 阶段 1：初始化排行榜数据 ----
    std::cout << "【阶段 1】初始化排行榜（100 名玩家）" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    Leaderboard board;
    board.seasonNumber = 1;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> scoreDist(100, 10000);
    std::uniform_int_distribution<int> levelDist(1, 60);
    std::uniform_int_distribution<int> winDist(0, 100);
    std::uniform_int_distribution<int> lossDist(0, 50);

    std::vector<std::string> names = {
        "Dragon", "Phoenix", "Tiger", "Wolf", "Eagle",
        "Storm", "Shadow", "Blaze", "Frost", "Thunder",
        "Viper", "Raven", "Hawk", "Lion", "Bear",
        "Ninja", "Samurai", "Knight", "Wizard", "Archer"
    };

    for (int i = 0; i < 100; ++i) {
        LeaderboardEntry e;
        e.playerName = names[i % names.size()] + std::to_string(i / names.size() + 1);
        e.score = scoreDist(rng);
        e.level = levelDist(rng);
        e.wins = winDist(rng);
        e.losses = lossDist(rng);
        e.lastActive = std::chrono::system_clock::now();
        board.entries.push_back(e);
    }
    board.lastUpdated = std::chrono::system_clock::now();

    std::cout << "  已生成 " << board.entries.size() << " 名玩家数据" << std::endl;
    std::cout << std::endl;

    // ---- 阶段 2：SPOI 查询 Top 10（按分数降序） ----
    std::cout << "【阶段 2】SPOI 查询：Top 10（按分数降序）" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  SPOI 查询等价于:" << std::endl;
    std::cout << "    SpoiQuery(\"Leaderboard\").sort(score, desc).take(10)" << std::endl;
    std::cout << "    查询方发 30 bytes 指令，服务端返回 10 条结果" << std::endl;
    std::cout << "    不需要传输 100 名玩家的全量数据" << std::endl;
    std::cout << std::endl;

    auto top10 = topN(board, 10);
    printTopN(top10, "Top 10 排行榜");

    // ---- 阶段 3：SPOI 查询（多条件过滤） ----
    std::cout << "【阶段 3】SPOI 查询：胜率 >= 50%，按等级降序" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  SPOI 查询等价于:" << std::endl;
    std::cout << "    SpoiQuery(\"Leaderboard\").filter(wins/(wins+losses) >= 0.5)" << std::endl;
    std::cout << "                             .sort(level, desc)" << std::endl;
    std::cout << "  35 个操作码，filter/sort/select/take/distinct/count..." << std::endl;
    std::cout << std::endl;

    auto experts = filterByWinRate(board, 0.5);
    std::cout << "  符合条件的玩家: " << experts.size() << " 人" << std::endl;
    if (experts.size() > 5) experts.resize(5);
    printTopN(experts, "高胜率玩家 Top 5");

    // ---- 阶段 4：增量更新演示 ----
    std::cout << "【阶段 4】增量更新：玩家分数变化" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  场景：Dragon1 完成一场比赛，分数 +500" << std::endl;
    std::cout << "  传统方式：重新发送整个排行榜（100 条数据）" << std::endl;
    std::cout << "  SPOI 方式：只发 ADD Dragon1.score = +500" << std::endl;
    std::cout << std::endl;

    std::cout << "  更新前：" << std::endl;
    std::cout << "    Dragon1: score=" << board.entries[0].score << std::endl;

    board.entries[0].score += 500;
    std::cout << "  更新后：" << std::endl;
    std::cout << "    Dragon1: score=" << board.entries[0].score << std::endl;
    std::cout << "  传输量：全量 " << board.entries.size() * 50 << "+ bytes"
              << " vs 增量 ~20 bytes" << std::endl;
    std::cout << std::endl;

    // ---- 阶段 5：跨语言数据交换 ----
    std::cout << "【阶段 5】跨语言数据交换" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  C++ 定义类型后，运行 sp-gen 即可生成：" << std::endl;
    std::cout << "    sp-gen -t py-meta -p ./leaderboard.py   → Python 类型" << std::endl;
    std::cout << "    sp-gen -t ts-meta -p ./leaderboard.ts   → TypeScript 类型" << std::endl;
    std::cout << "    sp-gen -t spoi-py -p ./spoi_builder.py  → Python 查询 Builder" << std::endl;
    std::cout << std::endl;
    std::cout << "  Python 端查询示例：" << std::endl;
    std::cout << "    query = SpoiQuery(\"Leaderboard\")" << std::endl;
    std::cout << "        .sort(P.score, ascending=False)" << std::endl;
    std::cout << "        .take(10)" << std::endl;
    std::cout << "        .build()  # → bytes，发给 C++ 服务端" << std::endl;
    std::cout << std::endl;

    // ---- 阶段 6：JSON 输出样例 ----
    std::cout << "【阶段 6】JSON 序列化（调试/对接 REST API）" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::stringstream ss;
    board.entries[0].toJsonStream(ss);
    std::cout << "  " << ss.str() << std::endl;
    std::cout << std::endl;

    // ---- 总结 ----
    std::cout << "======================================================" << std::endl;
    std::cout << "  总结" << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << "  1. SPOI 查询：内存直查，不传输全量数据，比 SQL 快" << std::endl;
    std::cout << "  2. 35 个操作码：filter/sort/select/take/distinct/count..." << std::endl;
    std::cout << "  3. 增量更新：分数变化只传增量，省带宽" << std::endl;
    std::cout << "  4. 跨语言：Python/TS/Go 都可以做查询方" << std::endl;
    std::cout << "  5. 适用场景：游戏排行榜、实时数据统计、竞技排名" << std::endl;
    std::cout << std::endl;

    return 0;
}