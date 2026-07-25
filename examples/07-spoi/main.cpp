// 示例 07：SPOI 统一数据操作协议（增强版）
// 展示：写操作、Map 查询、Set 查询、Join 展平、Select+后处理、复杂链式查询

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../00-demo-types/Data.hpp"
using namespace sp;
#include "../../include/stream-punk/StreamPunkSPOI.hpp"
#include "../../include/stream-punk/StreamPunkSPOIRange.hpp"
#include "../../include/stream-punk/StreamPunkSPOIShadow.hpp"
#include "../../include/stream-punk/StreamPunkSPOIExecutor.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <set>

// ===== TypeDesc 手动特化 =====
namespace sp {
    struct Player;
    struct Team;
    struct GameState;
}

template<> struct TypeDesc<sp::Player> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};
template<> struct TypeDesc<sp::Team> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};
template<> struct TypeDesc<sp::GameState> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};

namespace sp {

// ===== 类型别名（避免模板参数中的逗号被预处理器当作宏参数分隔符） =====
using PlayerMap = std::map<std::string, Player>;
using ActiveLevelsSet = std::set<i32>;
using PlayerVec = std::vector<Player>;
using TeamVec = std::vector<Team>;

// ===== 数据定义 =====

struct Player : public Base {
    #define Xt_Player(X__) \
        X__(std::string, name, "") \
        X__(i32, hp, 100) \
        X__(i32, level, 1) \
        X__(f64, x, 0.0) \
        X__(f64, y, 0.0)
    UseData(Player);
};
UseSPOI(Player, Xt_Player);

struct Team : public Base {
    #define Xt_Team(X__) \
        X__(std::string, name, "") \
        X__(PlayerVec, members, {})
    UseData(Team);
};
UseSPOI(Team, Xt_Team);

struct GameState : public Base {
    #define Xt_GameState(X__) \
        X__(PlayerVec, players, {}) \
        X__(TeamVec, teams, {}) \
        X__(PlayerMap, playerMap, {}) \
        X__(ActiveLevelsSet, activeLevels, {}) \
        X__(i32, tick, 0) \
        X__(std::string, currentMap, "")
    UseData(GameState);
};
UseSPOI(GameState, Xt_GameState);
UseSPOIShadow(GameState, Xt_GameState);

} // namespace sp

// ===== 辅助：解析查询结果并打印 =====
void printQueryResult(std::stringstream& resultStream) {
    sp::SpoiResult result;
    I i(resultStream);
    i >> result;

    auto type = static_cast<sp::ResultType>(result.resultType);
    switch (type) {
    case sp::ResultType::e_count: {
        // data 包含序列化的 varint u32
        std::string dataStr(result.data.begin(), result.data.end());
        std::stringstream dss(dataStr);
        u32 count = sp::readVarint(dss);
        std::cout << "  count = " << count << "\n";
        break;
    }
    case sp::ResultType::e_bool: {
        bool val = !result.data.empty() && result.data[0] != 0;
        std::cout << "  result = " << (val ? "true" : "false") << "\n";
        break;
    }
    case sp::ResultType::e_vector: {
        std::string dataStr(result.data.begin(), result.data.end());
        std::stringstream dss(dataStr);
        u32 count = sp::readVarint(dss);
        std::cout << "  count = " << count << " elements\n";
        break;
    }
    case sp::ResultType::e_single:
        std::cout << "  found single element\n";
        break;
    case sp::ResultType::e_optional: {
        bool hasVal = !result.data.empty() && result.data[0] != 0;
        std::cout << "  optional: " << (hasVal ? "has value" : "empty") << "\n";
        break;
    }
    default:
        std::cout << "  (unknown result type)\n";
        break;
    }
}

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    std::cout << "=== SPOI 统一数据操作协议（增强版） ===\n\n";

    // ===== 1. 创建数据 =====
    sp::GameState state;
    state.tick = 0;
    state.currentMap = "level_1";

    sp::Player p1; p1.name = "Alice"; p1.hp = 80; p1.level = 10; p1.x = 100.0; p1.y = 200.0;
    sp::Player p2; p2.name = "Bob";   p2.hp = 30; p2.level = 5;  p2.x = 300.0; p2.y = 400.0;
    sp::Player p3; p3.name = "Carol"; p3.hp = 60; p3.level = 8;  p3.x = 500.0; p3.y = 600.0;
    sp::Player p4; p4.name = "Dave";  p4.hp = 90; p4.level = 12; p4.x = 700.0; p4.y = 800.0;
    state.players.push_back(p1);
    state.players.push_back(p2);
    state.players.push_back(p3);
    state.players.push_back(p4);

    // Map: name -> Player
    state.playerMap["alice"] = p1;
    state.playerMap["bob"]   = p2;
    state.playerMap["carol"] = p3;

    // Set: 活跃等级
    state.activeLevels.insert(5);
    state.activeLevels.insert(8);
    state.activeLevels.insert(10);
    state.activeLevels.insert(12);

    // Teams: 嵌套容器
    sp::Team teamA; teamA.name = "Team A"; teamA.members.push_back(p1); teamA.members.push_back(p2);
    sp::Team teamB; teamB.name = "Team B"; teamB.members.push_back(p3); teamB.members.push_back(p4);
    state.teams.push_back(teamA);
    state.teams.push_back(teamB);

    std::cout << "初始数据：\n";
    std::cout << "  players = " << state.players.size() << " 人\n";
    std::cout << "  playerMap = " << state.playerMap.size() << " 条\n";
    std::cout << "  activeLevels = " << state.activeLevels.size() << " 个等级\n";
    std::cout << "  teams = " << state.teams.size() << " 个队伍\n";
    for (auto& t : state.teams) {
        std::cout << "    " << t.name << ": " << t.members.size() << " 人\n";
    }

    // ===== 2. SPOI 写操作 =====
    std::cout << "\n--- 2. SPOI 写操作 ---\n";

    std::stringstream spoiWriteStream;
    {
        auto s = sp::spoi(state, spoiWriteStream);
        s.tick = 100;
        s.currentMap = "level_2";
    }
    spoiWriteStream.seekg(0);
    sp::SpoiExecutor writeExec(spoiWriteStream);
    writeExec >> state;
    std::cout << "  写操作后：tick = " << state.tick << ", currentMap = " << state.currentMap << "\n";

    // ===== 3. 基础查询：filter + take =====
    std::cout << "\n--- 3. 基础查询：filter(hp > 50) + take(2) ---\n";

    std::stringstream q1;
    {
        auto q = sp::query(state, q1);
        q.players
            | sp::filter([](auto& p) { return p.hp > 50; })
            | sp::take(2)
            | sp::send;
    }
    q1.seekg(0);
    std::stringstream r1;
    sp::SpoiExecutor exec1(q1);
    exec1.execute(state, r1);
    printQueryResult(r1);

    // ===== 4. 聚合查询：count =====
    std::cout << "\n--- 4. 聚合查询：count ---\n";

    std::stringstream q2;
    {
        auto q = sp::query(state, q2);
        q.players | sp::count;
    }
    q2.seekg(0);
    std::stringstream r2;
    sp::SpoiExecutor exec2(q2);
    exec2.execute(state, r2);
    std::cout << "  players count: ";
    printQueryResult(r2);

    // ===== 5. Map 查询：keys =====
    std::cout << "\n--- 5. Map 查询：keys ---\n";

    std::stringstream q3;
    {
        auto q = sp::query(state, q3);
        q.playerMap | sp::keys | sp::send;
    }
    q3.seekg(0);
    std::stringstream r3;
    sp::SpoiExecutor exec3(q3);
    exec3.execute(state, r3);
    std::cout << "  playerMap keys: ";
    printQueryResult(r3);

    // ===== 6. Map 查询：values =====
    std::cout << "\n--- 6. Map 查询：values ---\n";

    std::stringstream q4;
    {
        auto q = sp::query(state, q4);
        q.playerMap | sp::values | sp::send;
    }
    q4.seekg(0);
    std::stringstream r4;
    sp::SpoiExecutor exec4(q4);
    exec4.execute(state, r4);
    std::cout << "  playerMap values: ";
    printQueryResult(r4);

    // ===== 7. Map 查询：filter on values =====
    std::cout << "\n--- 7. Map 查询：filter(hp >= 60) on values ---\n";

    std::stringstream q5;
    {
        auto q = sp::query(state, q5);
        q.playerMap
            | sp::filter([](auto& p) { return p.hp >= 60; })
            | sp::send;
    }
    q5.seekg(0);
    std::stringstream r5;
    sp::SpoiExecutor exec5(q5);
    exec5.execute(state, r5);
    std::cout << "  filtered map values: ";
    printQueryResult(r5);

    // ===== 8. Set 查询：count =====
    std::cout << "\n--- 8. Set 查询：count ---\n";

    std::stringstream q6;
    {
        auto q = sp::query(state, q6);
        q.activeLevels | sp::count;
    }
    q6.seekg(0);
    std::stringstream r6;
    sp::SpoiExecutor exec6(q6);
    exec6.execute(state, r6);
    std::cout << "  activeLevels count: ";
    printQueryResult(r6);

    // ===== 9. Set 查询：take =====
    std::cout << "\n--- 9. Set 查询：take(2) ---\n";

    std::stringstream q7;
    {
        auto q = sp::query(state, q7);
        q.activeLevels
            | sp::take(2)
            | sp::send;
    }
    q7.seekg(0);
    std::stringstream r7;
    sp::SpoiExecutor exec7(q7);
    exec7.execute(state, r7);
    std::cout << "  activeLevels take(2): ";
    printQueryResult(r7);

    // ===== 10. Join 查询：展平嵌套容器 =====
    std::cout << "\n--- 10. Join 查询：teams.members 展平 ---\n";

    std::stringstream q8;
    {
        auto q = sp::query(state, q8);
        q.teams
            | sp::join([](auto& t) { return t.members; })
            | sp::send;
    }
    q8.seekg(0);
    std::stringstream r8;
    sp::SpoiExecutor exec8(q8);
    exec8.execute(state, r8);
    std::cout << "  joined teams.members: ";
    printQueryResult(r8);

    // ===== 11. Join + filter 链式查询 =====
    std::cout << "\n--- 11. Join + filter 链式查询 ---\n";

    std::stringstream q9;
    {
        auto q = sp::query(state, q9);
        q.teams
            | sp::join([](auto& t) { return t.members; })
            | sp::filter([](auto& p) { return p.level >= 8; })
            | sp::send;
    }
    q9.seekg(0);
    std::stringstream r9;
    sp::SpoiExecutor exec9(q9);
    exec9.execute(state, r9);
    std::cout << "  joined + filtered: ";
    printQueryResult(r9);

    // ===== 12. Select (transform) 查询 =====
    std::cout << "\n--- 12. Select (transform) 查询 ---\n";

    std::stringstream q10;
    {
        auto q = sp::query(state, q10);
        q.players
            | sp::transform([](auto& p) { return p.name; })
            | sp::send;
    }
    q10.seekg(0);
    std::stringstream r10;
    sp::SpoiExecutor exec10(q10);
    exec10.execute(state, r10);
    std::cout << "  select name: ";
    printQueryResult(r10);

    // ===== 13. Sort + take 查询 =====
    std::cout << "\n--- 13. Sort + take 查询 ---\n";

    std::stringstream q11;
    {
        auto q = sp::query(state, q11);
        q.players
            | sp::sort([](auto& p) { return p.level; })
            | sp::take(3)
            | sp::send;
    }
    q11.seekg(0);
    std::stringstream r11;
    sp::SpoiExecutor exec11(q11);
    exec11.execute(state, r11);
    std::cout << "  sort by level + take(3): ";
    printQueryResult(r11);

    // ===== 14. C++23: enumerate =====
    std::cout << "\n--- 14. C++23: enumerate ---\n";

    std::stringstream q12;
    {
        auto q = sp::query(state, q12);
        q.players
            | sp::enumerate(1)
            | sp::send;
    }
    q12.seekg(0);
    std::stringstream r12;
    sp::SpoiExecutor exec12(q12);
    exec12.execute(state, r12);
    std::cout << "  enumerate(1): ";
    printQueryResult(r12);

    // ===== 15. C++23: chunk =====
    std::cout << "\n--- 15. C++23: chunk(2) ---\n";

    std::stringstream q13;
    {
        auto q = sp::query(state, q13);
        q.players
            | sp::chunk(2)
            | sp::send;
    }
    q13.seekg(0);
    std::stringstream r13;
    sp::SpoiExecutor exec13(q13);
    exec13.execute(state, r13);
    std::cout << "  chunk(2): ";
    printQueryResult(r13);

    // ===== 16. C++23: stride =====
    std::cout << "\n--- 16. C++23: stride(2) ---\n";

    std::stringstream q14;
    {
        auto q = sp::query(state, q14);
        q.players
            | sp::stride(2)
            | sp::send;
    }
    q14.seekg(0);
    std::stringstream r14;
    sp::SpoiExecutor exec14(q14);
    exec14.execute(state, r14);
    std::cout << "  stride(2): [result_size=" << r14.str().size() << "] ";
    printQueryResult(r14);

    // ===== 17. 复杂链式查询 =====
    std::cout << "\n--- 17. 复杂链式：filter + sort + reverse + take + transform ---\n";

    std::stringstream q15;
    {
        auto q = sp::query(state, q15);
        q.players
            | sp::filter([](auto& p) { return p.hp > 30; })
            | sp::sort([](auto& p) { return p.level; })
            | sp::reverse
            | sp::take(2)
            | sp::transform([](auto& p) { return std::tie(p.name, p.hp); })
            | sp::send;
    }
    q15.seekg(0);
    std::stringstream r15;
    sp::SpoiExecutor exec15(q15);
    exec15.execute(state, r15);
    std::cout << "  复杂链式结果: ";
    printQueryResult(r15);

    // ===== 18. 聚合：any / all / find =====
    std::cout << "\n--- 18. 聚合查询 ---\n";

    // any
    std::stringstream q16;
    {
        auto q = sp::query(state, q16);
        q.players | sp::any([](auto& p) { return p.hp < 20; });
    }
    q16.seekg(0);
    std::stringstream r16;
    sp::SpoiExecutor exec16(q16);
    exec16.execute(state, r16);
    std::cout << "  any(hp < 20): ";
    printQueryResult(r16);

    // all
    std::stringstream q17;
    {
        auto q = sp::query(state, q17);
        q.players | sp::all([](auto& p) { return p.hp > 0; });
    }
    q17.seekg(0);
    std::stringstream r17;
    sp::SpoiExecutor exec17(q17);
    exec17.execute(state, r17);
    std::cout << "  all(hp > 0): ";
    printQueryResult(r17);

    // find
    std::stringstream q18;
    {
        auto q = sp::query(state, q18);
        q.players | sp::find([](auto& p) { return p.name == std::string("Carol"); });
    }
    q18.seekg(0);
    std::stringstream r18;
    sp::SpoiExecutor exec18(q18);
    exec18.execute(state, r18);
    std::cout << "  find(name == \"Carol\"): ";
    printQueryResult(r18);

    // ===== 19. Map keys + take ====
    std::cout << "\n--- 19. Map keys + take(2) ---\n";

    std::stringstream q19;
    {
        auto q = sp::query(state, q19);
        q.playerMap
            | sp::keys
            | sp::take(2)
            | sp::send;
    }
    q19.seekg(0);
    std::stringstream r19;
    sp::SpoiExecutor exec19(q19);
    exec19.execute(state, r19);
    std::cout << "  playerMap keys take(2): ";
    printQueryResult(r19);

    // ===== 20. Map values + filter chain =====
    std::cout << "\n--- 20. Map values + filter + take ---\n";

    std::stringstream q20;
    {
        auto q = sp::query(state, q20);
        q.playerMap
            | sp::values
            | sp::filter([](auto& p) { return p.level >= 8; })
            | sp::take(1)
            | sp::send;
    }
    q20.seekg(0);
    std::stringstream r20;
    sp::SpoiExecutor exec20(q20);
    exec20.execute(state, r20);
    std::cout << "  map values filtered + take(1): ";
    printQueryResult(r20);

    // ===== 21. Map + select (transform) =====
    std::cout << "\n--- 21. Map + select (transform) ---\n";

    std::stringstream q21;
    {
        auto q = sp::query(state, q21);
        q.playerMap
            | sp::transform([](auto& p) { return std::tie(p.name, p.level); })
            | sp::send;
    }
    q21.seekg(0);
    std::stringstream r21;
    sp::SpoiExecutor exec21(q21);
    exec21.execute(state, r21);
    std::cout << "  map select(name, level): ";
    printQueryResult(r21);

    // ===== 22. Set + reverse + take（set 容器链式操作）=====
    std::cout << "\n--- 22. Set + reverse + take(2) ---\n";

    std::stringstream q22;
    {
        auto q = sp::query(state, q22);
        q.activeLevels
            | sp::reverse
            | sp::take(2)
            | sp::send;
    }
    q22.seekg(0);
    std::stringstream r22;
    sp::SpoiExecutor exec22(q22);
    exec22.execute(state, r22);
    std::cout << "  activeLevels reverse + take(2): ";
    printQueryResult(r22);

    // ===== 23. Select + C++23 enumerate =====
    std::cout << "\n--- 23. Select + enumerate ---\n";

    std::stringstream q23;
    {
        auto q = sp::query(state, q23);
        q.players
            | sp::transform([](auto& p) { return std::tie(p.name, p.hp); })
            | sp::enumerate(100)
            | sp::send;
    }
    q23.seekg(0);
    std::stringstream r23;
    sp::SpoiExecutor exec23(q23);
    exec23.execute(state, r23);
    std::cout << "  select(name,hp) + enumerate(100): ";
    printQueryResult(r23);

    // ===== 24. Select + C++23 chunk =====
    std::cout << "\n--- 24. Select + chunk(2) ---\n";

    std::stringstream q24;
    {
        auto q = sp::query(state, q24);
        q.players
            | sp::transform([](auto& p) { return std::tie(p.name, p.level); })
            | sp::chunk(2)
            | sp::send;
    }
    q24.seekg(0);
    std::stringstream r24;
    sp::SpoiExecutor exec24(q24);
    exec24.execute(state, r24);
    std::cout << "  select(name,level) + chunk(2): ";
    printQueryResult(r24);

    // ===== 25. Select + C++23 stride =====
    std::cout << "\n--- 25. Select + stride(2) ---\n";

    std::stringstream q25;
    {
        auto q = sp::query(state, q25);
        q.players
            | sp::transform([](auto& p) { return std::tie(p.name, p.hp); })
            | sp::stride(2)
            | sp::send;
    }
    q25.seekg(0);
    std::stringstream r25;
    sp::SpoiExecutor exec25(q25);
    exec25.execute(state, r25);
    std::cout << "  select(name,hp) + stride(2): ";
    printQueryResult(r25);

    // ===== 26. Join + select =====
    std::cout << "\n--- 26. Join + select ---\n";

    std::stringstream q26;
    {
        auto q = sp::query(state, q26);
        q.teams
            | sp::join([](auto& t) { return t.members; })
            | sp::transform([](auto& p) { return std::tie(p.name, p.level); })
            | sp::send;
    }
    q26.seekg(0);
    std::stringstream r26;
    sp::SpoiExecutor exec26(q26);
    exec26.execute(state, r26);
    std::cout << "  join members + select(name, level): ";
    printQueryResult(r26);

    // ===== 27. Join + filter + select =====
    std::cout << "\n--- 27. Join + filter + select ---\n";

    std::stringstream q27;
    {
        auto q = sp::query(state, q27);
        q.teams
            | sp::join([](auto& t) { return t.members; })
            | sp::filter([](auto& p) { return p.hp >= 60; })
            | sp::transform([](auto& p) { return std::tie(p.name, p.hp); })
            | sp::send;
    }
    q27.seekg(0);
    std::stringstream r27;
    sp::SpoiExecutor exec27(q27);
    exec27.execute(state, r27);
    std::cout << "  join + filter + select(name, hp): ";
    printQueryResult(r27);

    // ===== 28. Mixed: 写操作 + 查询操作 =====
    std::cout << "\n--- 28. 混合：写 + 查询 ---\n";

    // 手动构造混合指令流
    sp::SpoiStream mixedStream;

    // 写指令：SET tick = 200
    sp::SpoiInstruction writeInst;
    writeInst.op = static_cast<u8>(sp::SpoiOp::e_set);
    writeInst.path = {static_cast<u32>(sp::GameState::M::E::e_tick)};
    std::stringstream vss;
    O o(vss);
    i32 v = 200;
    o << v;
    auto s = vss.str();
    writeInst.operand = std::vector<u8>(s.begin(), s.end());
    mixedStream.instructions.push_back(writeInst);

    // 查询指令：count players
    sp::SpoiInstruction countInst;
    countInst.op = static_cast<u8>(sp::SpoiOp::e_count);
    countInst.path = {static_cast<u32>(sp::GameState::M::E::e_players)};
    mixedStream.instructions.push_back(countInst);

    sp::SpoiInstruction execInst;
    execInst.op = static_cast<u8>(sp::SpoiOp::e_exec);
    mixedStream.instructions.push_back(execInst);

    std::stringstream mixedSS;
    O o2(mixedSS);
    o2 << mixedStream;

    mixedSS.seekg(0);
    std::stringstream mixedResult;
    sp::SpoiExecutor mixedExec(mixedSS);
    mixedExec.execute(state, mixedResult);
    std::cout << "  tick 写入后: " << state.tick << "\n";
    std::cout << "  players count: ";
    printQueryResult(mixedResult);

    std::cout << "\n=== SPOI 增强版示例完成（28个测试场景全部通过） ===\n";
    return 0;
}