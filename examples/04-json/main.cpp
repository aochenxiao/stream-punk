// 示例 04：JSON 序列化/反序列化
// 展示：StreamPunk 的零依赖 JSON 支持

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../00-demo-types/Data.hpp"
using namespace sp;
#include "../common/locale_init.hpp"
#include "../../include/stream-punk/StreamPunkJson.hpp"
#include <iostream>

// 定义一个带 JSON 支持的类型
struct User : public Base {
    #define Xt_User(X__) \
    X__(std::string, name, "") \
    X__(i32, age, 0) \
    X__(std::string, email, "")

    User() = default;
    UseData(User);
    UseDataJson(User);
};
REGISTER_JSON_TYPE(User);

struct Config : public Base {
    #define Xt_Config(X__) \
    X__(std::string, host, "localhost") \
    X__(u16, port, 8080) \
    X__(bool, debug, false)

    Config() = default;
    UseData(Config);
    UseDataJson(Config);
};
REGISTER_JSON_TYPE(Config);

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    // --- 序列化为 JSON ---
    User user;
    user.name = "Alice";
    user.age = 30;
    user.email = "alice@example.com";

    JsonVal j;
    user.toJson(j);
    std::string json = j.dump();
    std::cout << "User JSON: " << json << std::endl;

    Config config;
    config.host = "api.example.com";
    config.port = 443;
    config.debug = true;

    JsonVal j2;
    config.toJson(j2);
    std::string configJson = j2.dump();
    std::cout << "Config JSON: " << configJson << std::endl;

    // --- 从 JSON 反序列化 ---
    User restored;
    JsonVal parsed = JsonVal::parse(json);
    restored.fromJson(parsed);
    std::cout << "Restored: name=" << restored.name
              << ", age=" << restored.age
              << ", email=" << restored.email << std::endl;

    Config restoredConfig;
    JsonVal parsed2 = JsonVal::parse(configJson);
    restoredConfig.fromJson(parsed2);
    std::cout << "Restored Config: host=" << restoredConfig.host
              << ", port=" << restoredConfig.port
              << ", debug=" << (restoredConfig.debug ? "true" : "false") << std::endl;

    return 0;
}