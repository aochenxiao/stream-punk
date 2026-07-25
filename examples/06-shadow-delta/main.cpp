// 示例 06：SPOI 影子模式增量更新（已从 Delta 迁移到 SPOI）
// 展示：创建 SPOI 影子对象 → 追踪变化 → 只传输增量数据

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../00-demo-types/Data.hpp"
using namespace sp;
#include "../common/locale_init.hpp"
#include "../../include/stream-punk/StreamPunkSPOIShadow.hpp"
#include <iostream>
#include <sstream>

namespace sp {

// 定义一个游戏状态
struct GameState : public Base {
    #define Xt_GameState(X__) \
    X__(i32, score, 0) \
    X__(f64, playerX, 0.0) \
    X__(f64, playerY, 0.0) \
    X__(i32, hp, 100) \
    X__(std::string, currentMap, "")

    GameState() = default;
    UseData(GameState);
};
UseSPOIShadow(GameState, Xt_GameState);

} // namespace sp

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    // 1. 创建游戏状态
    sp::GameState state;
    state.score = 0;
    state.playerX = 100.0;
    state.playerY = 200.0;
    state.hp = 100;
    state.currentMap = "level_1";

    // 2. 创建 SPOI 影子（增量追踪器）
    std::stringstream spoiStream;
    sp::spoi(state, spoiStream);

    std::cout << "初始状态已建立 SPOI 影子追踪" << std::endl;

    // 3. 修改状态 — 影子自动追踪变化
    state.score = 10;           // 只有这个变了
    state.playerX = 150.0;      // 这个也变了
    // playerY, hp, currentMap 没变

    std::cout << "修改了 score(0→10) 和 playerX(100.0→150.0)" << std::endl;
    std::cout << "SPOI 增量数据大小: " << spoiStream.str().size() << " bytes" << std::endl;

    // 4. 对比全量序列化
    std::stringstream fullStream;
    O fullOut{fullStream};
    fullOut << state;
    std::cout << "全量数据大小: " << fullStream.str().size() << " bytes" << std::endl;
    std::cout << "节省: " << (1.0 - (double)spoiStream.str().size() / fullStream.str().size()) * 100 << "%" << std::endl;

    std::cout << std::endl;
    std::cout << "--- SPOI 增量更新工作流程 ---" << std::endl;
    std::cout << "1. 首次同步：发送全量数据 + 创建 SPOI 影子" << std::endl;
    std::cout << "2. 每次修改：影子自动追踪变化字段，生成 SpoiInstruction 流" << std::endl;
    std::cout << "3. 同步时：只发送增量数据（变化的部分）" << std::endl;
    std::cout << "4. 接收端：用 SPOIExecutor 应用增量更新到本地副本" << std::endl;
    std::cout << std::endl;
    std::cout << "适用场景：实时游戏状态同步、协作编辑、物联网数据推送" << std::endl;
    std::cout << "注：此示例已从 Delta API 迁移到 SPOI API" << std::endl;

    return 0;
}