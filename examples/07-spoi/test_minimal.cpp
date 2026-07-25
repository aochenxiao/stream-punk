// 最小测试：验证 UseData 对 map/set 的支持
#include "../../include/stream-punk/StreamPunk.hpp"
#include "../00-demo-types/Data.hpp"
using namespace sp;
#include <map>
#include <set>

namespace sp {
    struct TestPlayer : public Base {
        #define Xt_TestPlayer(X__) \
            X__(std::string, name, "") \
            X__(i32, hp, 100)
        UseData(TestPlayer);
    };

    struct TestState : public Base {
        #define Xt_TestState(X__) \
            X__(std::vector<TestPlayer>, players, {}) \
            X__(std::map<std::string, TestPlayer>, playerMap, {}) \
            X__(std::set<i32>, activeLevels, {})
        UseData(TestState);
    };
}

int main() {
    sp::TestState state;
    return 0;
}