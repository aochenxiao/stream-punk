#include "unit_test.hpp"
#include <variant>
#include <sstream>

// 测试 std::variant 序列化/反序列化
REG_TEST(test_variant_serialization) {
    using TestVariant = std::variant<int, std::string, double>;

    // 测试 int 类型存储
    {
        TestVariant v = 42;
        std::stringstream ss;
        O o(ss);
        o << v;

        TestVariant result;
        I i(ss);
        i >> result;

        ck_assert_int_eq(v.index(), result.index());
        ck_assert_int_eq(std::get<int>(v), std::get<int>(result));
    }

    // 测试 string 类型存储
    {
        TestVariant v = "variant_test";
        std::stringstream ss;
        O o(ss);
        o << v;

        TestVariant result;
        I i(ss);
        i >> result;

        ck_assert_int_eq(v.index(), result.index());
        ck_assert_str_eq(std::get<std::string>(v).c_str(),
            std::get<std::string>(result).c_str());
    }

    // 测试 double 类型存储
    {
        TestVariant v = 3.14159265;
        std::stringstream ss;
        O o(ss);
        o << v;

        TestVariant result;
        I i(ss);
        i >> result;

        ck_assert_int_eq(v.index(), result.index());
        const double tolerance = 0.000001;
        ck_assert(std::abs(std::get<double>(v) - std::get<double>(result)) < tolerance);
    }
}

// 测试 variant 异常行为（使用手动异常检查）
REG_TEST(test_variant_exceptions) {
    using TestVariant = std::variant<int, std::string>;

    std::stringstream bad_ss;
    bad_ss << "999";  // 无效的 index

    TestVariant v;
    I i(bad_ss);

    try {
        i >> v;
        ck_abort_msg("Expected std::runtime_error but no exception was thrown");
    }
    catch (const std::runtime_error& e) {
        const char* expected = "o >> std::variant error";
        ck_assert_str_eq(e.what(), expected);
    }
    catch (...) {
        ck_abort_msg("Unexpected exception type caught");
    }
}

// 测试移动语义
REG_TEST(test_variant_move_semantics) {
    using TestVariant = std::variant<int, std::string>;

    TestVariant source = "text";
    std::stringstream ss;
    O o(ss);
    o << source;  // 移动后序列化

    // 验证原对象仍然有效 (v.index() 应该保留)
    ck_assert_int_eq(source.index(), 1);

    // 验证序列化数据
    TestVariant result;
    I i(ss);
    i >> result;

    ck_assert_int_eq(source.index(), result.index());
    ck_assert_str_eq(std::get<std::string>(source).c_str(),
        std::get<std::string>(result).c_str());
}