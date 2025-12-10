#include "unit_test.hpp"

// 定义没有显式底层类型的枚举
enum class NoUnderlying { ZERO, ONE, TWO };
enum class NegativeValues { MINUS_ONE = -1, MINUS_TWO = -2 };

// Arrange
const NoUnderlying originalNoUnderlying = NoUnderlying::TWO;
const NegativeValues originalNegative = NegativeValues::MINUS_TWO;


// 添加不带显式底层类型的枚举测试
REG_TEST(test_enum_no_underlying_type) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // Act
    out << originalNoUnderlying << originalNegative;

    NoUnderlying deserializedNoUnderlying{};
    NegativeValues deserializedNegative{};
    in >> deserializedNoUnderlying >> deserializedNegative;

    // Assert
    ck_assert_int_eq(static_cast<int>(deserializedNoUnderlying),
        static_cast<int>(originalNoUnderlying));
    ck_assert_int_eq(static_cast<int>(deserializedNegative),
        static_cast<int>(originalNegative));
}

// 测试枚举大小兼容性
REG_TEST(test_enum_size_compatibility) {
    // 验证不同大小枚举的序列化兼容性
    static_assert(sizeof(Sz) >= sizeof(NoUnderlying),
        "Sz must be large enough to hold enum values");

    // 测试大值枚举
    enum class LargeEnum { VALUE = 0x7FFFFFFF };

    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    const LargeEnum originalLarge = LargeEnum::VALUE;
    out << originalLarge;

    LargeEnum deserializedLarge{};
    in >> deserializedLarge;

    ck_assert_int_eq(static_cast<int>(deserializedLarge),
        static_cast<int>(originalLarge));
}

// 测试匿名枚举
REG_TEST(test_anonymous_enum) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // 定义匿名枚举
    enum { ANON_VALUE1 = 100, ANON_VALUE2 = 200 };

    // 序列化匿名枚举值
    out << ANON_VALUE1 << ANON_VALUE2;

    // 反序列化到有类型枚举
    enum class NamedEnum { VALUE1, VALUE2 };
    NamedEnum deserialized1{}, deserialized2{};
    in >> deserialized1 >> deserialized2;

    // 验证值正确传输
    ck_assert_int_eq(static_cast<int>(deserialized1), ANON_VALUE1);
    ck_assert_int_eq(static_cast<int>(deserialized2), ANON_VALUE2);
}

// 测试枚举默认构造
REG_TEST(test_enum_default_construction) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // 定义枚举
    enum class DefaultEnum { DEFAULT = 0, OTHER = 1 };

    // 默认构造的枚举值
    DefaultEnum defaultConstructed{};

    // 序列化默认值
    out << defaultConstructed;

    DefaultEnum deserialized{};
    in >> deserialized;

    // 验证默认值正确序列化/反序列化
    ck_assert_int_eq(static_cast<int>(deserialized),
        static_cast<int>(DefaultEnum::DEFAULT));
}

// 测试枚举位运算兼容性
REG_TEST(test_enum_bitwise_operations) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // 定义标志位枚举
    enum class Flags {
        NONE = 0,
        READ = 1 << 0,
        WRITE = 1 << 1,
        EXECUTE = 1 << 2
    };

    // 组合标志
    Flags combined = static_cast<Flags>(
        static_cast<int>(Flags::READ) |
        static_cast<int>(Flags::WRITE)
        );

    // 序列化组合值
    out << combined;

    Flags deserialized{};
    in >> deserialized;

    // 验证位运算值正确传输
    ck_assert_int_eq(static_cast<int>(deserialized),
        static_cast<int>(combined));

    // 验证位标志
    ck_assert(static_cast<int>(deserialized) & static_cast<int>(Flags::READ));
    ck_assert(static_cast<int>(deserialized) & static_cast<int>(Flags::WRITE));
    ck_assert(!(static_cast<int>(deserialized) & static_cast<int>(Flags::EXECUTE)));
}