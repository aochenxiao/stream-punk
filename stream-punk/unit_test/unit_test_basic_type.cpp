# include "unit_test.hpp"

// 测试辅助函数
template <typename T>
bool compare_floating(T a, T b) {
    if (std::isnan(a) && std::isnan(b)) return true;
    if (std::isinf(a) && std::isinf(b) && std::signbit(a) == std::signbit(b)) return true;

    T tolerance = std::numeric_limits<T>::epsilon() * 10;
    return std::abs(a - b) <= tolerance;
}

// 测试单个基础类型
template <typename T>
void test_basic_type(T value, const char* type_name) {
    std::stringstream stream;
    printType<T>();
    // 序列化
    O o{ stream };
    o << value;
    // 验证写入大小
    ck_assert_uint_eq(stream.str().size(), sizeof(T));
    // 反序列化
    stream.seekg(0);
    T result;
    I i{ stream };
    i >> result;
    // 验证读取位置
    ck_assert_uint_eq(static_cast<size_t>(stream.tellg()), sizeof(T));

    // 特殊处理浮点数
    if constexpr (std::is_floating_point_v<T>) {
        if (std::isnan(value)) {
            ck_assert(std::isnan(result));
        }
        else if (std::isinf(value)) {
            ck_assert(std::isinf(result));
            ck_assert(std::signbit(value) == std::signbit(result));
        }
        else {
            ck_assert(compare_floating(value, result));
        }
    }
    else {
        ck_assert(value == result);
    }
}

// 测试单个基础类型的边界值
template <typename T>
void test_boundary_values() {
    test_basic_type(std::numeric_limits<T>::min(), typeid(T).name());
    test_basic_type(std::numeric_limits<T>::max(), typeid(T).name());
    if constexpr (std::is_signed_v<T>) {
        test_basic_type(std::numeric_limits<T>::lowest(), typeid(T).name());
    }
}

// 测试特定类型的多种值
template <typename T>
void test_type_values(std::initializer_list<T> values, const char* type_name) {
    std::stringstream stream;
    printType<T>();

    std::vector testValue(values.begin(), values.end());

    // 序列化
    O o{ stream };
    for (const auto& value : values) {
        o << value;
    }
    std::vector<T> inputValue(values.size());
    I i{ stream };
    for (auto& value : inputValue) {
        i >> value;
    }
    for (size_t j = 0; j < inputValue.size(); ++j) {
        auto& value = testValue[j];
        auto& result = inputValue[j];
        // 特殊处理浮点数
        if constexpr (std::is_floating_point_v<T>) {
            if (std::isnan(value)) {
                ck_assert(std::isnan(result));
            }
            else if (std::isinf(value)) {
                ck_assert(std::isinf(result));
                ck_assert(std::signbit(value) == std::signbit(result));
            }
            else {
                ck_assert(compare_floating(value, result));
            }
        }
        else {
            ck_assert(value == result);
        }
    }
}

// 测试用例：无符号整数类型
REG_TEST(u8_boundary) {
    test_boundary_values<u8>();
}

REG_TEST(u8_values) {
    test_type_values<u8>({ 0, 1, 127, 128, 255 }, "u8");
}

REG_TEST(u16_boundary) {
    test_boundary_values<u16>();
}

REG_TEST(u16_values) {
    test_type_values<u16>({ 0, 255, 256, 1000, 65535 }, "u16");
}

REG_TEST(u32_boundary) {
    test_boundary_values<u32>();
}

REG_TEST(u32_values) {
    test_type_values<u32>({ 0, 65535, 65536, 1000000, 4294967295 }, "u32");
}

REG_TEST(u64_boundary) {
    test_boundary_values<u64>();
}

REG_TEST(u64_values) {
    test_type_values<u64>({
        0,
        65535,
        65536,
        4294967295,
        4294967296,
        18446744073709551615ULL
        }, "u64");
}


// 测试用例：有符号整数类型
REG_TEST(test_signed_types) {
    test_boundary_values<i8>();
    test_boundary_values<i16>();
    test_boundary_values<i32>();
    test_boundary_values<i64>();

    test_type_values<i8>({ -128, -127, -1, 0, 1, 127 }, "i8");
    test_type_values<i16>({ -32768, -1000, 0, 1000, 32767 }, "i16");
    test_type_values<i32>({ -2147483648, -1000000, 0, 1000000, 2147483647 }, "i32");
}

// 测试用例：浮点类型
REG_TEST(test_float_types) {
    test_boundary_values<f32>();
    test_boundary_values<f64>();

    // 测试正常值
    test_type_values<f32>({
        0.0f, 1.0f, -1.0f,
        0.1f, -0.1f, 123.456f, -123.456f,
        std::numeric_limits<f32>::min(),
        std::numeric_limits<f32>::denorm_min()
        }, "f32");

    test_type_values<f64>({
        0.0, 1.0, -1.0,
        0.1, -0.1, 123.456789, -123.456789,
        std::numeric_limits<f64>::min(),
        std::numeric_limits<f64>::denorm_min()
        }, "f64");

    // 测试特殊值
    test_basic_type(std::numeric_limits<f32>::infinity(), "f32_inf");
    test_basic_type(-std::numeric_limits<f32>::infinity(), "f32_neg_inf");
    test_basic_type(std::numeric_limits<f32>::quiet_NaN(), "f32_nan");

    test_basic_type(std::numeric_limits<f64>::infinity(), "f64_inf");
    test_basic_type(-std::numeric_limits<f64>::infinity(), "f64_neg_inf");
    test_basic_type(std::numeric_limits<f64>::quiet_NaN(), "f64_nan");
}

// 测试用例：布尔类型
REG_TEST(test_bool_type) {
    test_basic_type(true, "bool_true");
    test_basic_type(false, "bool_false");

    // 测试多次写入/读取
    std::stringstream stream;
    O o{ stream };
    o << true << false << true << false;

    ck_assert_uint_eq(stream.str().size(), 4 * sizeof(bl));

    stream.seekg(0);
    I i{ stream };
    bl b1, b2, b3, b4;
    i >> b1 >> b2 >> b3 >> b4;

    ck_assert(b1 == true);
    ck_assert(b2 == false);
    ck_assert(b3 == true);
    ck_assert(b4 == false);
}

// 测试用例：混合类型序列化
REG_TEST(test_mixed_types) {
    std::stringstream stream;

    // 写入混合类型数据
    O o(stream);
    o << static_cast<u8>(65)   // 'A'
        << static_cast<u16>(1024)
        << static_cast<i32>(-50000)
        << static_cast<f32>(3.14159f)
        << static_cast<f64>(2.718281828459045)
        << true
        << static_cast<u64>(123456789012345);

    // 验证总大小
    size_t expected_size = sizeof(u8) + sizeof(u16) + sizeof(i32) +
        sizeof(f32) + sizeof(f64) + sizeof(bl) + sizeof(u64);
    ck_assert_uint_eq(stream.str().size(), expected_size);

    // 读取并验证数据
    stream.seekg(0);
    I i(stream);

    u8 c; u16 s; i32 n; f32 f; f64 d; bl b; u64 l;
    i >> c >> s >> n >> f >> d >> b >> l;

    ck_assert_uint_eq(c, 65);
    ck_assert_uint_eq(s, 1024);
    ck_assert_int_eq(n, -50000);
    ck_assert(compare_floating(f, 3.14159f));
    ck_assert(compare_floating(d, 2.718281828459045));
    ck_assert(b == true);
    ck_assert_uint_eq(l, 123456789012345);
}

// 测试用例：数据对齐
REG_TEST(test_alignment) {
    std::stringstream stream;

    // 写入数据
    O o(stream);
    o << static_cast<u8>('X')
        << static_cast<u32>(0xCAFEBABE);

    // 验证写入位置
    ck_assert_uint_eq(static_cast<size_t>(stream.tellp()), sizeof(u8) + sizeof(u32));

    // 读取并验证
    stream.seekg(0);
    I i(stream);
    u8 c; u32 v;
    i >> c >> v;

    ck_assert(c == 'X');
    ck_assert(v == 0xCAFEBABE);
}

// 测试用例：空流读取
REG_TEST(test_empty_stream) {
    std::stringstream empty_stream;
    I i(empty_stream);

    u8 value;
    try {
        i >> value;
        throw std::exception("空流测试 按预期抛异常");
    }
    catch (const std::exception& e) {
        // 期望抛出异常
        std::cout << e.what() << std::endl;
    }
    catch (...) {
        ck_abort_msg("Unexpected exception type thrown~~~~~~~~~~");
    }
}