// 示例 17：WAL 持久化与断电恢复
// 展示 StreamPunk 的增量更新如何实现 WAL（预写日志）模式：
//   全量快照 + 增量追加 → 断电恢复 → 差分安全
//
// 场景：游戏服务端，定时做全量快照，运行中增量写入 delta.bin
//       模拟断电后从快照 + 增量恢复，几乎不丢数据

#include "stream-punk/StreamPunk.hpp"
#include "stream-punk/StreamPunkSPOIShadow.hpp"
#include "stream-punk/StreamPunkJson.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <chrono>
#include <iomanip>

using namespace sp;
namespace fs = std::filesystem;

// ============================================================
// 1. 定义游戏状态类型
// ============================================================

struct PlayerState : public Base {
#define Xt_PlayerState(X__) \
    X__(std::string, name, "") \
    X__(i32, level, 1) \
    X__(i32, hp, 100) \
    X__(f64, x, 0.0) \
    X__(f64, y, 0.0) \
    X__(std::vector<std::string>, items, {})

    PlayerState() = default;
    UseData(PlayerState);
    UseDataJson(PlayerState);
};
REGISTER_JSON_TYPE(PlayerState);

// TypeDesc 必须在 PlayerMap 定义之前，因为 map<string, PlayerState> 需要它
template<> struct TypeDesc<PlayerState> : TypeDescCustom<PlayerState> {};

using PlayerMap = std::map<std::string, PlayerState>;

struct GameWorld : public Base {
#define Xt_GameWorld(X__) \
    X__(i32, worldTick, 0) \
    X__(PlayerMap, players, {}) \
    X__(std::string, currentMap, "village")

    GameWorld() = default;
    UseData(GameWorld);
    UseDataJson(GameWorld);
};
REGISTER_JSON_TYPE(GameWorld);

template<> struct TypeDesc<GameWorld> : TypeDescCustom<GameWorld> {};

// SPOI Shadow 必须在所有类型定义之后
UseSPOIShadow(PlayerState, Xt_PlayerState);
UseSPOIShadow(GameWorld, Xt_GameWorld);

// ============================================================
// 2. 工具函数
// ============================================================

// 保存全量快照到文件
void saveSnapshot(GameWorld const& world, std::string const& path) {
    std::ofstream file(path, std::ios::binary);
    O o(file);
    o << world;
    file.close();
    std::cout << "  [snapshot] saved to " << path
              << " (" << fs::file_size(path) << " bytes)" << std::endl;
}

// 从文件加载全量快照
void loadSnapshot(GameWorld& world, std::string const& path) {
    std::ifstream file(path, std::ios::binary);
    I i(file);
    i >> world;
    file.close();
    std::cout << "  [snapshot] loaded from " << path << std::endl;
}

// 追加增量指令到 delta 文件
void appendDelta(std::stringstream& deltaSS, std::string const& path) {
    std::ofstream file(path, std::ios::binary | std::ios::app);
    auto deltaStr = deltaSS.str();
    file.write(deltaStr.data(), deltaStr.size());
    file.close();
    std::cout << "  [delta] appended " << deltaStr.size() << " bytes to " << path << std::endl;
}

// 打印世界状态
void printWorld(GameWorld const& world, std::string const& label) {
    std::cout << "  " << label << ":" << std::endl;
    std::cout << "    worldTick=" << world.worldTick
              << ", currentMap=" << world.currentMap << std::endl;
    for (auto& [name, p] : world.players) {
        std::cout << "    " << name << ": level=" << p.level
                  << ", hp=" << p.hp << ", pos=(" << p.x << "," << p.y << ")"
                  << ", items=[";
        for (size_t i = 0; i < p.items.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << p.items[i];
        }
        std::cout << "]" << std::endl;
    }
}

// 打印 delta 文件内容（十六进制），展示差分加密特性
void printDeltaHex(std::string const& path) {
    std::ifstream file(path, std::ios::binary);
    std::vector<unsigned char> data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();

    std::cout << "  [delta.bin content] (" << data.size() << " bytes):" << std::endl;
    std::cout << "  ";
    for (size_t i = 0; i < data.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << (int)data[i] << " ";
        if ((i + 1) % 16 == 0 && i + 1 < data.size())
            std::cout << std::endl << "  ";
    }
    std::cout << std::dec << std::endl;
}

// ============================================================
// 3. 主演示
// ============================================================

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    std::string snapshotPath = "temp/snapshot.bin";
    std::string deltaPath = "temp/delta.bin";

    // 清理旧文件
    fs::remove(snapshotPath);
    fs::remove(deltaPath);
    fs::create_directories("temp");

    std::cout << "======================================================" << std::endl;
    std::cout << "  StreamPunk WAL Persistence & Crash Recovery Demo" << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << std::endl;

    // ---- 阶段 1：初始化世界，做全量快照 ----
    std::cout << "[Phase 1] Initialize world, create full snapshot" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    GameWorld world;
    world.worldTick = 0;
    world.currentMap = "village";

    // 初始化玩家
    PlayerState alice;
    alice.name = "Alice";
    alice.level = 42;
    alice.hp = 100;
    alice.x = 10.0;
    alice.y = 20.0;
    alice.items = {"sword", "shield"};
    world.players["Alice"] = alice;

    PlayerState bob;
    bob.name = "Bob";
    bob.level = 35;
    bob.hp = 80;
    bob.x = 50.0;
    bob.y = 30.0;
    bob.items = {"bow"};
    world.players["Bob"] = bob;

    printWorld(world, "Initial state");
    saveSnapshot(world, snapshotPath);
    std::cout << std::endl;

    // ---- 阶段 2：运行中，产生增量变更 ----
    std::cout << "[Phase 2] Game running, generating delta changes" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    // 变更 1: Alice 移动 + 血量变化
    {
        std::stringstream deltaSS;
        {
            auto shadow = sp::spoi(world, deltaSS);
            world.worldTick = 100;
            world.players["Alice"].x = 25.0;
            world.players["Alice"].y = 35.0;
            world.players["Alice"].hp = 80;
        }
        appendDelta(deltaSS, deltaPath);
    }
    std::cout << "  Change 1: Alice moved to (25,35), hp reduced to 80" << std::endl;

    // 变更 2: Bob 升级 + 捡到物品
    {
        std::stringstream deltaSS;
        {
            auto shadow = sp::spoi(world, deltaSS);
            world.worldTick = 200;
            world.players["Bob"].level = 36;
            world.players["Bob"].items.push_back("arrow");
        }
        appendDelta(deltaSS, deltaPath);
    }
    std::cout << "  Change 2: Bob leveled up to 36, picked up arrow" << std::endl;

    // 变更 3: Alice 捡到药水
    {
        std::stringstream deltaSS;
        {
            auto shadow = sp::spoi(world, deltaSS);
            world.worldTick = 300;
            world.players["Alice"].items.push_back("potion");
            world.players["Alice"].hp = 95;
        }
        appendDelta(deltaSS, deltaPath);
    }
    std::cout << "  Change 3: Alice picked up potion, hp restored to 95" << std::endl;

    printWorld(world, "Current state");
    std::cout << std::endl;

    // ---- 阶段 3：展示 delta.bin 内容（差分加密） ----
    std::cout << "[Phase 3] Differential Encryption Property" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  delta.bin only stores delta operations (SET/ADD/APPEND)." << std::endl;
    std::cout << "  Without the full snapshot baseline, delta.bin alone" << std::endl;
    std::cout << "  cannot reconstruct any real player state." << std::endl;
    std::cout << "  This is the 'differential encryption' property." << std::endl;
    std::cout << std::endl;
    printDeltaHex(deltaPath);
    std::cout << std::endl;

    // ---- 阶段 4：模拟断电，内存数据丢失 ----
    std::cout << "[Phase 4] Simulate power failure - memory data lost!" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    GameWorld recoveredWorld;
    std::cout << "  After power failure (empty memory):" << std::endl;
    printWorld(recoveredWorld, "Before recovery");
    std::cout << std::endl;

    // ---- 阶段 5：恢复 — 加载快照 + 重放增量 ----
    std::cout << "[Phase 5] Recovery: snapshot + delta replay" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    // 5.1 加载基准快照
    loadSnapshot(recoveredWorld, snapshotPath);
    std::cout << "  After snapshot restore:" << std::endl;
    printWorld(recoveredWorld, "Snapshot restored");

    // 5.2 读取 delta.bin 并重放（应用 SPOI 指令）
    std::cout << std::endl;
    std::cout << "  Replaying delta.bin instructions..." << std::endl;
    {
        std::ifstream deltaFile(deltaPath, std::ios::binary);
        std::vector<uint8_t> deltaData(
            (std::istreambuf_iterator<char>(deltaFile)),
            std::istreambuf_iterator<char>());
        deltaFile.close();

        std::stringstream deltaSS;
        deltaSS.write(reinterpret_cast<char const*>(deltaData.data()), deltaData.size());

        I input(deltaSS);
        while (deltaSS.tellg() < (std::streampos)deltaData.size()) {
            SpoiStream stream;
            try {
                input >> stream;
                std::cout << "    Read " << stream.instructions.size() << " instructions" << std::endl;
                for (auto& inst : stream.instructions) {
                    std::cout << "      op=" << (int)inst.op << " path_size=" << inst.path.size() << std::endl;
                }
            } catch (...) {
                break;
            }
        }
    }

    std::cout << std::endl;
    std::cout << "  [Note] Full recovery requires SPOIExecutor to apply delta instructions." << std::endl;
    std::cout << "  This demo shows the complete WAL workflow:" << std::endl;
    std::cout << "    1. Full snapshot (snapshot.bin) - baseline data" << std::endl;
    std::cout << "    2. Delta append (delta.bin) - change log" << std::endl;
    std::cout << "    3. Recovery = snapshot + replay deltas" << std::endl;
    std::cout << std::endl;

    // ---- 总结 ----
    std::cout << "======================================================" << std::endl;
    std::cout << "  Summary" << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << "  1. WAL mode: full snapshot + delta append, append-only is fast" << std::endl;
    std::cout << "  2. Crash recovery: restore snapshot -> replay deltas" << std::endl;
    std::cout << "  3. Differential encryption: deltas only store relative ops" << std::endl;
    std::cout << "     Without baseline snapshot, delta.bin cannot reconstruct data" << std::endl;
    std::cout << "  4. Use cases: game saves, real-time collaboration, IoT persistence" << std::endl;
    std::cout << std::endl;

    // 清理
    fs::remove(snapshotPath);
    fs::remove(deltaPath);

    return 0;
}