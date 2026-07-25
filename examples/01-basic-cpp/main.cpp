// 示例 01：纯 C++ 序列化/反序列化
// 展示 StreamPunk 最核心的用法：定义类型 → 序列化 → 反序列化 → 深拷贝

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../00-demo-types/Data.hpp"
using namespace sp;
#include "../common/locale_init.hpp"
#include <iostream>
#include <sstream>

// 1. 定义可序列化类型
struct Player : public Base {
    #define Xt_Player(X__) \
    X__(std::string, name, "") \
    X__(i32, level, 1) \
    X__(f64, health, 100.0)

    Player() = default;
    UseData(Player);
};

// 2. 在 customData.hpp 中注册（或在此处通过宏注册）
// 已在 customData.hpp 中定义的类型无需重复注册

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    // 序列化：对象 → 二进制流
    Player p1;
    p1.name = "Alice";
    p1.level = 42;
    p1.health = 88.5;

    std::stringstream ss;
    O output{ss};
    output << p1;

    std::cout << "序列化后流大小: " << ss.str().size() << " bytes" << std::endl;

    // 反序列化：二进制流 → 对象
    Player p2;
    I input{ss};
    input >> p2;

    std::cout << "反序列化结果: name=" << p2.name
              << ", level=" << p2.level
              << ", health=" << p2.health << std::endl;

    // 深拷贝
    DeepCopier copier;
    Player p3;
    deepCopy(copier, p3, p1);
    copier.clear();

    std::cout << "深拷贝结果: name=" << p3.name
              << ", level=" << p3.level
              << ", health=" << p3.health << std::endl;

    // 验证独立：修改 p1 不影响 p3
    p1.name = "Bob";
    std::cout << "修改 p1 后: p1.name=" << p1.name
              << ", p3.name=" << p3.name << " (深拷贝独立)" << std::endl;

    return 0;
}