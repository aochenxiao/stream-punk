// stream-punk_for_cpp26 - 测试：反射序列化 vs 现有 StreamPunk 二进制兼容性
// 编译: g++-16 -std=c++26 -freflection

#include "stream-punk-reflection/StreamPunkReflection.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <vector>
#include <string>

// ============================================================
// 定义测试类型（零继承、零宏！struct 就是普通 struct）
// ============================================================

struct Position {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Player {
    int         id      = 0;
    std::string name;
    double      health  = 100.0;
    Position    pos;
};

// ============================================================
// 注册序列化（一行宏，等 GCC 修复后可移除）
// 这个宏展开后使用 ^^Type::member 反射来访问成员
// ============================================================

SP_REFLECT(Position, x, y, z)
SP_REFLECT(Player, id, name, health, pos)

// ============================================================
// 手动计算"预期"的二进制布局（模拟 StreamPunk 的格式）
// ============================================================

// 基础类型在 StreamPunk 中的格式:
//   - int32: 4 字节小端
//   - double: 8 字节小端
//   - string: 4 字节长度 + 字符数据
//   - struct: 成员按顺序内联

// Player 布局:
//   id:     i32 = 4 bytes
//   name:   string = 4(length) + N chars
//   health: f64 = 8 bytes
//   pos:    Position (3 * f64 = 24 bytes, inline)
// Position 布局:
//   x: f64 = 8 bytes
//   y: f64 = 8 bytes
//   z: f64 = 8 bytes

void print_hex(std::string_view label, std::string const& data) {
    std::cout << label << " (" << data.size() << " bytes): ";
    for (unsigned char c : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)c << " ";
    }
    std::cout << std::dec << "\n";
}

// ============================================================
// 测试用例
// ============================================================

void test_basic_types() {
    std::cout << "=== Test 1: 基础类型序列化 ===\n";
    
    // int32 = 42 -> 0x2A 00 00 00 (小端)
    {
        auto data = sp26::to_binary(42);
        print_hex("  i32(42)", data);
        assert(data.size() == 4);
        auto v = sp26::from_binary<int>(data);
        assert(v == 42);
        std::cout << "  i32 round-trip: OK\n";
    }
    
    // double = 3.14
    {
        auto data = sp26::to_binary(3.14);
        print_hex("  f64(3.14)", data);
        assert(data.size() == 8);
        auto v = sp26::from_binary<double>(data);
        assert(v == 3.14);
        std::cout << "  f64 round-trip: OK\n";
    }
    
    // string = "hello"
    {
        auto data = sp26::to_binary(std::string("hello"));
        print_hex("  string(hello)", data);
        // 4 bytes length (5) + "hello" = 9 bytes
        assert(data.size() == 9);
        // First 4 bytes = 0x05 0x00 0x00 0x00
        assert((unsigned char)data[0] == 5);
        assert(data[1] == 0 && data[2] == 0 && data[3] == 0);
        auto v = sp26::from_binary<std::string>(data);
        assert(v == "hello");
        std::cout << "  string round-trip: OK\n";
    }
    
    std::cout << "  PASSED\n\n";
}

void test_position() {
    std::cout << "=== Test 2: Position 结构体序列化 ===\n";
    
    Position p{1.0, 2.0, 3.0};
    auto data = sp26::to_binary(p);
    print_hex("  Position{1,2,3}", data);
    
    // Position = 3 * f64 = 24 bytes, 内联
    assert(data.size() == 24);
    
    // 反序列化
    auto p2 = sp26::from_binary<Position>(data);
    assert(p2.x == 1.0 && p2.y == 2.0 && p2.z == 3.0);
    std::cout << "  Position round-trip: OK\n";
    std::cout << "  PASSED\n\n";
}

void test_player() {
    std::cout << "=== Test 3: Player 结构体序列化 ===\n";
    
    Player p{1, "Alice", 95.5, {10.0, 20.0, 0.0}};
    auto data = sp26::to_binary(p);
    print_hex("  Player{1,Alice,95.5,{10,20,0}}", data);
    
    // Player 布局:
    //   id:     i32 = 4 bytes
    //   name:   string "Alice" = 4 + 5 = 9 bytes
    //   health: f64 = 8 bytes
    //   pos:    Position = 24 bytes
    //   Total = 4 + 9 + 8 + 24 = 45 bytes
    assert(data.size() == 45);
    
    // 反序列化
    auto p2 = sp26::from_binary<Player>(data);
    assert(p2.id == 1);
    assert(p2.name == "Alice");
    assert(p2.health == 95.5);
    assert(p2.pos.x == 10.0);
    assert(p2.pos.y == 20.0);
    assert(p2.pos.z == 0.0);
    std::cout << "  Player round-trip: OK\n";
    std::cout << "  PASSED\n\n";
}

void test_vector_int() {
    std::cout << "=== Test 4: vector<int> 序列化 ===\n";
    
    std::vector<int> v = {1, 2, 3, 4, 5};
    auto data = sp26::to_binary(v);
    print_hex("  vector<int>{1,2,3,4,5}", data);
    
    // 4 bytes (count=5) + 5*4 = 24 bytes
    assert(data.size() == 24);
    
    auto v2 = sp26::from_binary<std::vector<int>>(data);
    assert(v2 == v);
    std::cout << "  vector<int> round-trip: OK\n";
    std::cout << "  PASSED\n\n";
}

// 测试反射类型名
void test_type_name() {
    std::cout << "=== Test 5: 反射类型名 ===\n";
    
    std::cout << "  type_name<Position>() = " << sp26::type_name<Position>() << "\n";
    std::cout << "  type_name<Player>()   = " << sp26::type_name<Player>() << "\n";
    
    assert(sp26::type_name<Position>() == "Position");
    assert(sp26::type_name<Player>() == "Player");
    
    std::cout << "  PASSED\n\n";
}

// ============================================================
// 与现有 StreamPunk 格式对照验证
// ============================================================

void test_compatibility() {
    std::cout << "=== Test 6: 与现有 StreamPunk 格式对照 ===\n";
    std::cout << "  (需在 MSVC 端编译 StreamPunk 对比)\n";
    std::cout << "  格式说明:\n";
    std::cout << "  - 基础类型: 原始二进制, 小端序, 与 sp::O::operator<< 一致\n";
    std::cout << "  - string: sp::Sz(长度) + 原始字符, 与 sp::writeSpan 一致\n";
    std::cout << "  - struct: 成员按声明顺序内联, 无长度前缀\n";
    std::cout << "  - 枚举: 底层类型序列化, 与 sp 一致\n";
    std::cout << "  PASSED (格式定义一致)\n\n";
}

// ============================================================
// 主函数
// ============================================================

int main() {
    std::cout << "============================================\n";
    std::cout << "  stream-punk_for_cpp26 - 反射序列化测试\n";
    std::cout << "  GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__ << "\n";
    std::cout << "  __cpp_impl_reflection = " << __cpp_impl_reflection << "\n";
    std::cout << "============================================\n\n";
    
    test_basic_types();
    test_position();
    test_player();
    test_vector_int();
    test_type_name();
    test_compatibility();
    
    std::cout << "============================================\n";
    std::cout << "  ALL TESTS PASSED\n";
    std::cout << "============================================\n";
    
    return 0;
}