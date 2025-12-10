# include "unit_test.hpp"

# define std_eq(v1, v2) std::equal(v1.begin(), v1.end(), v2.begin(), v2.end())

// 测试基础功能：三种视图类型的序列化
REG_TEST(test_span_serialization_basic) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // Arrange
    const i32 arr[] = { 10, 20, 30, 40 };
    std::span<const i32> sp(arr);
    std::string_view sv = "TestView";
    std::initializer_list<f32> il = { 1.1f, 2.2f, 3.3f };

    // Act
    out << sp << sv << il;

    // Assert - 验证写入的数据结构
    std::array<i32, 4> arr_in;
    in >> arr_in;

    std::string sv_in;
    in >> sv_in;
    ck_assert(sv == sv_in);

    std::vector<f32> il_in;
    in >> il_in;
    ck_assert(std_eq(il, il_in));
}

// 测试空视图和单个元素的边界情况
REG_TEST(test_span_edge_cases) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // Arrange - 所有空视图
    std::span<const int> empty_sp;
    std::string_view empty_sv;
    std::initializer_list<double> empty_il;
    std::span<const char> single_sp{ "A", 1 };

    // Act
    out << empty_sp << empty_sv << empty_il << single_sp;

    // Assert - 验证空视图处理
    Sz sizes[4];
    in >> sizes[0] >> sizes[1] >> sizes[2] >> sizes[3];
    ck_assert_uint_eq(sizes[0], 0);  // empty_sp
    ck_assert_uint_eq(sizes[1], 0);  // empty_sv
    ck_assert_uint_eq(sizes[2], 0);  // empty_il
    ck_assert_uint_eq(sizes[3], 1);  // single_sp
}

// 测试不同字符类型 (char/wchar_t)
REG_TEST(test_char_type_variations) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // Arrange
    std::wstring_view wsv = L"WideView";
    std::span<const wchar_t> wspan{ wsv.data(), wsv.size() };
    std::initializer_list<wchar_t> wil = { L'A', L'B', L'Ç' };

    // Act
    out << wsv << wspan << wil;

    std::wstring wsv_in;
    std::vector<wchar_t> wspan_in;
    std::array<wchar_t, 3> wil_in;

    in >> wsv_in >> wspan_in >> wil_in;

    ck_assert(wsv == wsv_in);  // wsv
    ck_assert(std_eq(wspan,wspan_in));  // wspan
    ck_assert(std_eq(wil, wil_in));  // wil
}

// 测试span视图的不同访问方式
REG_TEST(test_span_access_methods) {
    std::stringstream ss;
    O out{ ss };

    // Arrange
    std::vector data_vec{ 1.5, 2.5, 3.5 };
    double data_arr[] = { 4.5, 5.5, 6.5 };

    std::span<const f64> vec_span(data_vec);
    std::span<const f64> arr_span(data_arr);
    std::span<const f64> ptr_span(data_vec.data(), data_vec.size());

    // Act & Assert
    out << vec_span << arr_span << ptr_span;

    // 验证流长度正确
    std::string result = ss.str();
    ck_assert_uint_eq(result.size(),
        3 * sizeof(Sz) +  // 3个size字段
        9 * sizeof(f64));  // 共9个double值
}

// 测试初始化列表的类型推导特性
REG_TEST(test_initializer_list_type_deduction) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // Arrange - 不同类型初始化列表
    auto int_list = { 1, 2, 3, 4 };         // std::initializer_list<int>
    auto mixed_list = { 5.0f, 6.0f, 7.5f };     // 应该推导为float或double？
    auto str_list = { "a", "bc", "def" };    // std::initializer_list<const char*>

    // Act
    out << int_list << mixed_list << str_list;

    // Assert - 验证元素计数
    std::vector<int> int_list_in;
    std::vector<f32> mixed_list_in;
    std::vector<char> str_list_in;
    in >> int_list_in >> mixed_list_in >> str_list_in;

    ck_assert(std_eq(int_list, int_list_in) );   // int_list
    ck_assert(std_eq(mixed_list, mixed_list_in) );   // mixed_list
    //ck_assert(std_eq(str_list, str_list_in) );   // str_list
}



REG_TEST(test_tuple_basic_serialization) {
    // Arrange - 基本类型组合
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    const auto original = std::make_tuple(42, 3.14, std::string("hello"), true);

    // Act - 序列化与反序列化
    out << original;
    std::remove_cv_t<decltype(original)> deserialized = std::make_tuple(0, 0.0, "", false);
    in >> deserialized;

    // Assert - 验证所有元素
    const auto& [orig_i, orig_d, orig_s, orig_b] = original;
    const auto& [deser_i, deser_d, deser_s, deser_b] = deserialized;

    ck_assert_int_eq(orig_i, deser_i);
    ck_assert_double_eq_tol(orig_d, deser_d, 0.001);
    ck_assert_str_eq(orig_s.data(), deser_s.data());
    ck_assert(orig_b == deser_b);
}

REG_TEST(test_tuple_empty) {
    // Arrange - 空元组
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    const std::tuple<> original{};
    std::tuple<> deserialized{};

    // 记录序列化前的状态
    const auto initial_stream_pos = ss.tellg();

    // Act - 序列化空元组
    out << original;
    const auto after_serialize_pos = ss.tellg();

    // 反序列化前准备
    ss.clear();
    ss.seekg(0, std::ios::beg);

    // Act - 反序列化空元组
    in >> deserialized;
    const auto after_deserialize_pos = ss.tellg();

    // Assert - 验证：
    // 1. 序列化操作未添加任何字节
    ck_assert_msg(initial_stream_pos == after_serialize_pos,
        "Empty tuple serialization should not write any data");

    // 2. 反序列化操作未消耗任何字节
    ck_assert_msg(after_deserialize_pos == 0,
        "Empty tuple deserialization should not read any data");

    // 3. 流状态保持正常
    ck_assert_msg(ss.good(),
        "Stream state should remain good after empty tuple operations");

    // 4. 流内容为空（双重验证）
    ck_assert_msg(ss.str().empty(),
        "Stream should be empty after empty tuple operations");
}

REG_TEST(test_tuple_nested_types) {
    // Arrange - 嵌套容器类型
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    using NestedTuple = std::tuple<std::vector<int>, std::map<int, std::string>>;
    const NestedTuple original{
        {1, 2, 3},
        {{1, "one"}, {2, "two"}}
    };

    // Act
    out << original;
    NestedTuple deserialized;
    in >> deserialized;

    // Assert
    const auto& [orig_vec, orig_map] = original;
    const auto& [deser_vec, deser_map] = deserialized;

    ck_assert(orig_vec == deser_vec);
    ck_assert(orig_map == deser_map);
}

REG_TEST(test_tuple_move_semantics) {
    // Arrange - 移动语义测试
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    auto source = std::make_tuple(std::make_unique<int>(42), std::string("movable"));

    // Act - 移动构造
    decltype(source) target;
    out << std::move(source);
    in >> target;


    // 验证反序列化结果
    auto& [tgt_ptr, tgt_str] = target;
    ck_assert_ptr_nonnull(tgt_ptr.get());
    ck_assert_int_eq(*tgt_ptr, 42);
    ck_assert_str_eq(tgt_str.data(), "movable");

    // Assert - 移动后源对象状态不会受移动所影响 这个测试必不通过
    auto& [ptr, str] = source;
    ck_assert(ptr == nullptr);
}

REG_TEST(test_tuple_mixed_constness) {
    // Arrange - 混合const/非const测试
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    const auto original = std::make_tuple(100, 200, 300);
    auto deserialized = std::make_tuple(0, 0, 0);

    // Act - 分步序列化
    const auto& [a, b, c] = original;
    out << a << b << c;  // 模拟分步序列化

    auto& [x, y, z] = deserialized;
    in >> x >> y >> z;  // 模拟分步反序列化

    // Assert
    ck_assert_int_eq(a, x);
    ck_assert_int_eq(b, y);
    ck_assert_int_eq(c, z);
}