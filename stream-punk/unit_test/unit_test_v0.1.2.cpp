# include "unit_test.hpp"
# include "Data.hpp"

using namespace detail;

REG_TEST(concat_arrays_basic_v0_1_2) {
    // Arrange: 基本功能测试
    constexpr MaliuTokenArr<2> arr1 = { 1, 2 };
    constexpr MaliuTokenArr<3> arr2 = { 3, 4, 5 };

    // Act
    constexpr auto result = concat_arrays(arr1, arr2);

    // Assert
    ck_assert_uint_eq(result.size(), 5);
    ck_assert_uint_eq(result[0], 1);
    ck_assert_uint_eq(result[1], 2);
    ck_assert_uint_eq(result[2], 3);
    ck_assert_uint_eq(result[3], 4);
    ck_assert_uint_eq(result[4], 5);
}

REG_TEST(concat_arrays_empty_arrays_v0_1_2) {
    // Arrange: 包含空数组的情况
    constexpr MaliuTokenArr<0> empty1 = {};
    constexpr MaliuTokenArr<0> empty2 = {};
    constexpr MaliuTokenArr<2> data = { 10, 20 };

    // Act
    constexpr auto result1 = concat_arrays(empty1, data);
    constexpr auto result2 = concat_arrays(data, empty1);
    constexpr auto result3 = concat_arrays(empty1, empty2);

    // Assert
    ck_assert_uint_eq(result1.size(), 2);
    ck_assert_uint_eq(result1[0], 10);
    ck_assert_uint_eq(result1[1], 20);

    ck_assert_uint_eq(result2.size(), 2);
    ck_assert_uint_eq(result2[0], 10);
    ck_assert_uint_eq(result2[1], 20);

    ck_assert_uint_eq(result3.size(), 0);
}

REG_TEST(concat_arrays_mixed_sizes_v0_1_2) {
    // Arrange: 混合不同大小的数组
    constexpr MaliuTokenArr<1> arr1 = { 100 };
    constexpr MaliuTokenArr<0> arr2 = {};
    constexpr MaliuTokenArr<3> arr3 = { 200, 201, 202 };
    constexpr MaliuTokenArr<2> arr4 = { 300, 301 };

    // Act
    constexpr auto result = concat_arrays(arr1, arr2, arr3, arr4);

    // Assert
    ck_assert_uint_eq(result.size(), 6);
    ck_assert_uint_eq(result[0], 100);
    ck_assert_uint_eq(result[1], 200);
    ck_assert_uint_eq(result[2], 201);
    ck_assert_uint_eq(result[3], 202);
    ck_assert_uint_eq(result[4], 300);
    ck_assert_uint_eq(result[5], 301);
}

REG_TEST(concat_arrays_large_array_v0_1_2) {
    // Arrange: 大数组测试
    constexpr MaliuTokenArr<1000> large_arr = [] {
        MaliuTokenArr<1000> arr{};
        for (size_t i = 0; i < 1000; ++i) {
            arr[i] = i;
        }
        return arr;
    }();

    constexpr MaliuTokenArr<1> single = { 9999 };

    // Act
    constexpr auto result = concat_arrays(large_arr, single);

    // Assert
    ck_assert_uint_eq(result.size(), 1001);
    ck_assert_uint_eq(result[0], 0);
    ck_assert_uint_eq(result[999], 999);
    ck_assert_uint_eq(result[1000], 9999);
}

REG_TEST(concat_arrays_constexpr_evaluation_v0_1_2) {
    // Arrange: 编译时求值测试
    constexpr MaliuTokenArr<2> arr1 = { 1, 2 };
    constexpr MaliuTokenArr<2> arr2 = { 3, 4 };

    // Act & Assert: 在编译时计算结果
    static_assert(concat_arrays(arr1, arr2)[0] == 1, "constexpr error");
    static_assert(concat_arrays(arr1, arr2)[1] == 2, "constexpr error");
    static_assert(concat_arrays(arr1, arr2)[2] == 3, "constexpr error");
    static_assert(concat_arrays(arr1, arr2)[3] == 4, "constexpr error");
    static_assert(concat_arrays(arr1, arr2).size() == 4, "constexpr size error");
}

REG_TEST(concat_arrays_complex_combination_v0_1_2) {
    // Arrange: 多数组组合测试
    constexpr MaliuTokenArr<1> a = { 10 };
    constexpr MaliuTokenArr<2> b = { 20, 21 };
    constexpr MaliuTokenArr<0> c = {};
    constexpr MaliuTokenArr<3> d = { 30, 31, 32 };
    constexpr MaliuTokenArr<1> e = { 40 };

    // Act
    constexpr auto result = concat_arrays(a, b, c, d, e);

    // Assert
    constexpr MaliuTokenArr<7> expected = { 10, 20, 21, 30, 31, 32, 40 };
    ck_assert(result == expected);
}


// 测试基本类型令牌生成
REG_TEST(test_basic_types_v0_1_2) {
    constexpr auto u8_desc = TypeDesc<u8>::v;
    ck_assert_uint_eq(u8_desc.size(), 1);
    ck_assert_uint_eq(u8_desc[0], maliu::u8);

    constexpr auto f32_desc = TypeDesc<f32>::v;
    ck_assert_uint_eq(f32_desc.size(), 1);
    ck_assert_uint_eq(f32_desc[0], maliu::f32);

    constexpr auto bl_desc = TypeDesc<bl>::v;
    ck_assert_uint_eq(bl_desc.size(), 1);
    ck_assert_uint_eq(bl_desc[0], maliu::bl);

    // 字符串类型应该有特殊令牌
    auto string_desc = TypeDesc<std::string>::v;
    ck_assert_uint_eq(string_desc.size(), 2);
    ck_assert_uint_eq(string_desc[0], maliu::E::string);
    ck_assert_uint_eq(string_desc[1], maliu::ch);
}

// 测试容器类型令牌生成
REG_TEST(test_container_types_v0_1_2) {
    // vector类型
    auto vec_desc = TypeDesc<std::vector<i32>>::v;
    ck_assert_uint_eq(vec_desc.size(), 2);
    ck_assert_uint_eq(vec_desc[0], maliu::E::vector);
    ck_assert_uint_eq(vec_desc[1], maliu::i32);

    // array类型
    auto arr_desc = TypeDesc<std::array<f64, 10>>::v;
    ck_assert_uint_eq(arr_desc.size(), 3);
    ck_assert_uint_eq(arr_desc[0], maliu::E::array);
    ck_assert_uint_eq(arr_desc[1], 10);
    ck_assert_uint_eq(arr_desc[2], maliu::f64);

    // list类型
    auto list_desc = TypeDesc<std::list<std::string>>::v;
    ck_assert_uint_eq(list_desc.size(), 1+2);
    ck_assert_uint_eq(list_desc[0], maliu::E::list);
    ck_assert_uint_eq(list_desc[1], maliu::E::string);
    ck_assert_uint_eq(list_desc[2], maliu::E::ch);
    // 进一步验证字符串描述内部
    auto str_desc = TypeDesc<std::string>::v;
    ck_assert_uint_eq(str_desc.size(), 2);
    ck_assert_uint_eq(str_desc[1], maliu::ch);
}

// 测试智能指针类型令牌生成
REG_TEST(test_smart_pointer_types_v0_1_2) {
    // shared_ptr
    auto sptr_desc = TypeDesc<Sptr<u64>>::v;
    ck_assert_uint_eq(sptr_desc.size(), 2);
    ck_assert_uint_eq(sptr_desc[0], maliu::E::sptr);
    ck_assert_uint_eq(sptr_desc[1], maliu::u64);

    // unique_ptr
    auto uptr_desc = TypeDesc<Uptr<f32>>::v;
    ck_assert_uint_eq(uptr_desc.size(), 2);
    ck_assert_uint_eq(uptr_desc[0], maliu::E::uptr);
    ck_assert_uint_eq(uptr_desc[1], maliu::f32);

    // weak_ptr
    auto wptr_desc = TypeDesc<Wptr<bl>>::v;
    ck_assert_uint_eq(wptr_desc.size(), 2);
    ck_assert_uint_eq(wptr_desc[0], maliu::E::wptr);
    ck_assert_uint_eq(wptr_desc[1], maliu::bl);
}

// 测试复杂嵌套类型令牌生成
REG_TEST(test_nested_types_v0_1_2) {
    // map嵌套vector
    auto map_desc = TypeDesc<std::map<i32, std::vector<f64>>>::v;
    ck_assert_uint_eq(map_desc.size(), 4);
    ck_assert_uint_eq(map_desc[0], maliu::E::map);
    ck_assert_uint_eq(map_desc[1], maliu::i32);
    ck_assert_uint_eq(map_desc[2], maliu::E::vector);
    ck_assert_uint_eq(map_desc[3], maliu::f64);

    // vector嵌套map嵌套shared_ptr
    auto vec_desc = TypeDesc<std::vector<std::map<i32, Sptr<std::string>>>>::v;
    ck_assert_uint_eq(vec_desc.size(), 6);
    ck_assert_uint_eq(vec_desc[0], maliu::E::vector);
    ck_assert_uint_eq(vec_desc[1], maliu::E::map);
    ck_assert_uint_eq(vec_desc[2], maliu::i32);
    ck_assert_uint_eq(vec_desc[3], maliu::E::sptr);
    ck_assert_uint_eq(vec_desc[4], maliu::E::string);
    ck_assert_uint_eq(vec_desc[5], maliu::E::ch);
    // 进一步验证内部类型描述
    auto str_desc = TypeDesc<std::string>::v;
    ck_assert_uint_eq(str_desc.size(), 2);
    ck_assert_uint_eq(str_desc[1], maliu::ch);
}

// 测试variant类型令牌生成
REG_TEST(test_variant_type_v0_1_2) {
    using TestVariant = std::variant<i32, f32, std::string>;

    auto variant_desc = TypeDesc<TestVariant>::v;
    ck_assert_uint_eq(variant_desc.size(), 5+1);
    ck_assert_uint_eq(variant_desc[0], maliu::E::variant);
    ck_assert_uint_eq(variant_desc[1], maliu::i32);
    ck_assert_uint_eq(variant_desc[2], maliu::f32);
    ck_assert_uint_eq(variant_desc[3], maliu::E::string);
    ck_assert_uint_eq(variant_desc[4], maliu::ch);
    ck_assert_uint_eq(variant_desc[5], maliu::ed);
}

// 测试atomic类型令牌生成
REG_TEST(test_atomic_type_v0_1_2) {
    auto atomic_desc = TypeDesc<std::atomic<u64>>::v;
    ck_assert_uint_eq(atomic_desc.size(), 2);
    ck_assert_uint_eq(atomic_desc[0], maliu::E::atomic);
    ck_assert_uint_eq(atomic_desc[1], maliu::u64);
}

// 测试optional类型令牌生成
REG_TEST(test_optional_type_v0_1_2) {
    auto opt_desc = TypeDesc<std::optional<std::string>>::v;
    ck_assert_uint_eq(opt_desc.size(), 3);
    ck_assert_uint_eq(opt_desc[0], maliu::E::opt);
    ck_assert_uint_eq(opt_desc[1], maliu::E::string);
    ck_assert_uint_eq(opt_desc[2], maliu::E::ch);
    // 进一步验证内部类型描述
    auto str_desc = TypeDesc<std::string>::v;
    ck_assert_uint_eq(str_desc.size(), 2);
    ck_assert_uint_eq(str_desc[1], maliu::ch);
}

// 测试路径类型令牌生成
REG_TEST(test_path_type_v0_1_2) {
    auto path_desc = TypeDesc<std::filesystem::path>::v;
    ck_assert_uint_eq(path_desc.size(), 1);
    ck_assert_uint_eq(path_desc[0], maliu::E::path);
}


// 测试空元组类型
REG_TEST(test_tuple_empty_v0_1_2) {
    using EmptyTuple = std::tuple<>;
    auto desc = TypeDesc<EmptyTuple>::v;

    // 结构: [tuple开始标识, ed结束符]
    ck_assert_uint_eq(desc.size(), 2);
    ck_assert_uint_eq(desc[0], maliu::E::tuple);
    ck_assert_uint_eq(desc[1], maliu::E::ed);
}

// 测试单元素元组类型
REG_TEST(test_tuple_single_element_v0_1_2) {
    using SingleTuple = std::tuple<int>;
    auto desc = TypeDesc<SingleTuple>::v;

    // 结构: [tuple标识, int类型标识, ed结束符]
    ck_assert_uint_eq(desc.size(), 3);
    ck_assert_uint_eq(desc[0], maliu::E::tuple);
    ck_assert_uint_eq(desc[1], maliu::i32);
    ck_assert_uint_eq(desc[2], maliu::E::ed);
}

// 测试多元素元组类型
REG_TEST(test_tuple_multiple_elements_v0_1_2) {
    using MultiTuple = std::tuple<char, float, bool>;
    auto desc = TypeDesc<MultiTuple>::v;

    // 结构: [tuple标识, char, float, bool, ed结束符]
    ck_assert_uint_eq(desc.size(), 5);
    ck_assert_uint_eq(desc[0], maliu::E::tuple);
    ck_assert_uint_eq(desc[1], maliu::ch);
    ck_assert_uint_eq(desc[2], maliu::f32);
    ck_assert_uint_eq(desc[3], maliu::bl);
    ck_assert_uint_eq(desc[4], maliu::E::ed);
}

// 测试嵌套元组类型
REG_TEST(test_tuple_nested_v0_1_2) {
    using NestedTuple = std::tuple<std::tuple<int>, double>;
    auto desc = TypeDesc<NestedTuple>::v;

    // 结构: [tuple开始, [tuple开始, int, ed], double, ed结束符]
    ck_assert_uint_eq(desc.size(), 6);
    ck_assert_uint_eq(desc[0], maliu::E::tuple); // 外层tuple开始
    ck_assert_uint_eq(desc[1], maliu::E::tuple); // 内层tuple开始
    ck_assert_uint_eq(desc[2], maliu::i32);    // int类型
    ck_assert_uint_eq(desc[3], maliu::E::ed);   // 内层tuple结束
    ck_assert_uint_eq(desc[4], maliu::f64);    // double类型
    ck_assert_uint_eq(desc[5], maliu::E::ed);   // 外层tuple结束
}

// 测试混合容器元组类型
REG_TEST(test_tuple_mixed_containers_v0_1_2) {
    using ComplexTuple = std::tuple<
        std::vector<int>,
        std::map<std::string, int>,
        Uptr<float>
    >;
    auto desc = TypeDesc<ComplexTuple>::v;

    // 验证外层元组结构
    ck_assert_uint_gt(desc.size(), 4); // 确保有足够元素

    // 外层元组开始
    ck_assert_uint_eq(desc[0], maliu::E::tuple);

    // 验证vector<int>
    size_t pos = 1;
    ck_assert_uint_eq(desc[pos++], maliu::E::vector); // vector开始
    ck_assert_uint_eq(desc[pos++], maliu::i32);       // int类型

    // 验证map<string, int>
    ck_assert_uint_eq(desc[pos++], maliu::E::map);    // map开始
    // string描述（2个元素）
    ck_assert_uint_eq(desc[pos++], maliu::E::string); // string开始
    ck_assert_uint_eq(desc[pos++], maliu::ch);         // char类型
    // int类型
    ck_assert_uint_eq(desc[pos++], maliu::i32);

    // 验证unique_ptr<float>
    ck_assert_uint_eq(desc[pos++], maliu::E::uptr);   // unique_ptr开始
    ck_assert_uint_eq(desc[pos++], maliu::f32);        // float类型

    // 外层元组结束
    ck_assert_uint_eq(desc.back(), maliu::E::ed);
}

// 测试大元组类型（边界情况）
REG_TEST(test_tuple_large_v0_1_2) {
    auto desc = TypeDesc<std::tuple<
        i32, ch, f32, f64, bl,
        i16, i32, u32, u64
        >>::v;

    // 基本结构检查
    ck_assert_uint_eq(desc.size(), 11); // 1(开始) + 9(类型) + 1(结束)
    ck_assert_uint_eq(desc[0], maliu::E::tuple);
    ck_assert_uint_eq(desc.back(), maliu::E::ed);

    // 序列验证
    ck_assert_uint_eq(desc[1], maliu::i32);
    ck_assert_uint_eq(desc[2], maliu::ch );
    ck_assert_uint_eq(desc[3], maliu::f32);
    ck_assert_uint_eq(desc[4], maliu::f64);
    ck_assert_uint_eq(desc[5], maliu::bl );
    ck_assert_uint_eq(desc[6], maliu::i16);
    ck_assert_uint_eq(desc[7], maliu::i32);
    ck_assert_uint_eq(desc[8], maliu::u32);
    ck_assert_uint_eq(desc[9], maliu::u64);
    ck_assert_uint_eq(desc[10], maliu::ed );
}


// 基本类型指针测试
REG_TEST(test_pointer_basic_types_v0_1_2) {
    // int指针
    constexpr auto int_ptr_desc = TypeDesc<int*>::v;
    constexpr MaliuTokenArr<2> expected_int_desc = {
        maliu::E::ptr,
        TypeDesc<int>::v[0]  // 基础int类型描述
    };
    static_assert(int_ptr_desc == expected_int_desc);

    // double指针
    constexpr auto double_ptr_desc = TypeDesc<double*>::v;
    constexpr MaliuTokenArr<2> expected_double_desc = {
        maliu::E::ptr,
        TypeDesc<double>::v[0]  // 基础double类型描述
    };
    static_assert(double_ptr_desc == expected_double_desc);

    // bool指针
    constexpr auto bool_ptr_desc = TypeDesc<bool*>::v;
    constexpr MaliuTokenArr<2> expected_bool_desc = {
        maliu::E::ptr,
        TypeDesc<bool>::v[0]  // 基础bool类型描述
    };
    static_assert(bool_ptr_desc == expected_bool_desc);
}

// 多级指针测试
REG_TEST(test_pointer_multilevel_v0_1_2) {
    // 双重指针
    constexpr auto double_int_ptr_desc = TypeDesc<int**>::v;
    constexpr MaliuTokenArr<3> expected_double_int_desc = {
        maliu::E::ptr,
        maliu::E::ptr,
        TypeDesc<int>::v[0]
    };
    static_assert(double_int_ptr_desc == expected_double_int_desc);

    // 三重指针
    constexpr auto triple_double_ptr_desc = TypeDesc<double***>::v;
    constexpr MaliuTokenArr<4> expected_triple_double_desc = {
        maliu::E::ptr,
        maliu::E::ptr,
        maliu::E::ptr,
        TypeDesc<double>::v[0]
    };
    static_assert(triple_double_ptr_desc == expected_triple_double_desc);
}

// 特殊类型指针测试
REG_TEST(test_pointer_special_types_v0_1_2) {
    // void指针
    constexpr auto void_ptr_desc = TypeDesc<void*>::v;
    constexpr MaliuTokenArr<1> expected_void_desc = {
        maliu::E::voidPtr,
    };
    static_assert(void_ptr_desc == expected_void_desc);

    // const指针
    constexpr auto const_int_ptr_desc = TypeDesc<const int*>::v;
    constexpr MaliuTokenArr<3> expected_const_int_desc = {
        maliu::E::ptr,
        maliu::E::cst,
        TypeDesc<int>::v[0]  // const性由序列化处理
    };
    static_assert(const_int_ptr_desc == expected_const_int_desc);
}

// 内存安全测试
REG_TEST(test_pointer_memory_safety_v0_1_2) {
    // 验证空类型描述不会导致访问异常
    ck_assert_ptr_nonnull(TypeDesc<int*>::v.data());
    ck_assert_uint_gt(TypeDesc<int*>::v.size(), 0);

    // 验证多级指针描述访问安全
    ck_assert_ptr_nonnull(TypeDesc<double***>::v.data());
    ck_assert_uint_eq(TypeDesc<double***>::v.size(), 4);
}


REG_TEST(unit_test_TypesToken_v0_1_2) {
    auto v = TypesDesc<i32, i64, u32>::v;

    const auto* ptr = &TypeDesc<void*>::v; // 获取地址
    std::cout << "Memory address: " << ptr << '\n';

    auto x = MultiLevelContainer::desc;
    int a = 1;
}





// 测试基本类型的类型描述符
REG_TEST(test_basic_types_desc) {
    // 整数类型
    static_assert(TypeDesc<u8>::v[0] == maliu::u8);
    static_assert(TypeDesc<i32>::v[0] == maliu::i32);

    // 浮点类型
    static_assert(TypeDesc<f32>::v[0] == maliu::f32);
    static_assert(TypeDesc<f64>::v[0] == maliu::f64);

    // 字符类型
    static_assert(TypeDesc<char8_t>::v[0] == maliu::ch8);
    static_assert(TypeDesc<char32_t>::v[0] == maliu::ch32);

    // 布尔类型
    static_assert(TypeDesc<bool>::v[0] == maliu::bl);
}

// 测试容器类型的类型描述符
REG_TEST(test_container_types_desc) {
    // vector描述符 (vector + 元素类型)
    constexpr auto vecDesc = TypeDesc<std::vector<int>>::v;
    ck_assert_uint_eq(vecDesc.size(), 2);
    ck_assert_uint_eq(vecDesc[0], maliu::vector);
    ck_assert_uint_eq(vecDesc[1], maliu::i32);

    // array描述符 (array + 大小 + 元素类型)
    constexpr auto arrDesc = TypeDesc<std::array<float, 10>>::v;
    ck_assert_uint_eq(arrDesc.size(), 3);
    ck_assert_uint_eq(arrDesc[0], maliu::array);
    ck_assert_uint_eq(arrDesc[1], 10); // size
    ck_assert_uint_eq(arrDesc[2], maliu::f32); // 元素类型

    // map描述符 (map + key类型 + value类型)
    constexpr auto mapDesc = TypeDesc<std::map<std::string, int>>::v;
    ck_assert_uint_eq(mapDesc.size(), 4); // map + (string + char) + i32
    ck_assert_uint_eq(mapDesc[0], maliu::map);
    ck_assert_uint_eq(mapDesc[1], maliu::string);
    ck_assert_uint_eq(mapDesc[2], maliu::ch); // string的char类型
    ck_assert_uint_eq(mapDesc[3], maliu::i32);
}

// 测试智能指针的类型描述符
REG_TEST(test_smart_pointers_desc) {
    // shared_ptr描述符
    constexpr auto sptrDesc = TypeDesc<Sptr<int>>::v;
    ck_assert_uint_eq(sptrDesc.size(), 2);
    ck_assert_uint_eq(sptrDesc[0], maliu::sptr);
    ck_assert_uint_eq(sptrDesc[1], maliu::i32);

    // unique_ptr描述符
    constexpr auto uptrDesc = TypeDesc<Uptr<std::string>>::v;
    ck_assert_uint_eq(uptrDesc.size(), 3); // uptr + string + char
    ck_assert_uint_eq(uptrDesc[0], maliu::uptr);
    ck_assert_uint_eq(uptrDesc[1], maliu::string);
    ck_assert_uint_eq(uptrDesc[2], maliu::ch);
}

// 测试时间类型的类型描述符
REG_TEST(test_time_types_desc) {
    using namespace std::chrono;

    // duration描述符
    constexpr auto durDesc = TypeDesc<milliseconds>::v;
    ck_assert_uint_eq(durDesc.size(), 1);
    ck_assert_uint_eq(durDesc[0], maliu::dur);

    // time_point描述符
    constexpr auto tpDesc = TypeDesc<system_clock::time_point>::v;
    ck_assert_uint_eq(tpDesc.size(), 1);
    ck_assert_uint_eq(tpDesc[0], maliu::timepoint);
}

// 测试复杂嵌套类型的类型描述符
REG_TEST(test_nested_types_desc) {
    // 嵌套容器 vector<set<int>>
    constexpr auto nestedDesc = TypeDesc<std::vector<std::set<int>>>::v;
    ck_assert_uint_eq(nestedDesc.size(), 3);
    ck_assert_uint_eq(nestedDesc[0], maliu::vector); // 最外层
    ck_assert_uint_eq(nestedDesc[1], maliu::set);    // 内层容器
    ck_assert_uint_eq(nestedDesc[2], maliu::i32);    // 元素类型
}

// 测试变体类型(variant)的描述符
REG_TEST(test_variant_desc) {
    constexpr auto varDesc = TypeDesc<std::variant<int, std::string, float>>::v;
    ck_assert_uint_eq(varDesc.size(), 6); // variant + [i32, string, ch, f32] + ed
    ck_assert_uint_eq(varDesc[0], maliu::variant);
    ck_assert_uint_eq(varDesc[1], maliu::i32);
    ck_assert_uint_eq(varDesc[2], maliu::string);
    ck_assert_uint_eq(varDesc[3], maliu::ch);
    ck_assert_uint_eq(varDesc[4], maliu::f32);
    ck_assert_uint_eq(varDesc[5], maliu::ed); // ed表示变体结束
}

// 测试自定义类型的描述符
REG_TEST(test_custom_types_desc) {
    // 测试自定义类型Test的描述符
    constexpr auto testDesc = TypeDesc<Test>::v;
    ck_assert_uint_eq(testDesc.size(), 1);
    ck_assert_uint_eq(testDesc[0], maliu::Test);
}

// 测试指针类型的描述符
REG_TEST(test_pointer_desc) {
    // 原始指针
    constexpr auto rawDesc = TypeDesc<int*>::v;
    ck_assert_uint_eq(rawDesc.size(), 2);
    ck_assert_uint_eq(rawDesc[0], maliu::ptr);
    ck_assert_uint_eq(rawDesc[1], maliu::i32);

    // void指针
    constexpr auto voidDesc = TypeDesc<void*>::v;
    ck_assert_uint_eq(voidDesc.size(), 1);
    ck_assert_uint_eq(voidDesc[0], maliu::voidPtr);
}

// 测试常量类型的描述符
REG_TEST(test_const_desc) {
    constexpr auto constDesc = TypeDesc<const int>::v;
    ck_assert_uint_eq(constDesc.size(), 2);
    ck_assert_uint_eq(constDesc[0], maliu::cst);
    ck_assert_uint_eq(constDesc[1], maliu::i32);
}

// 测试机器信息结构的序列化
REG_TEST(test_machine_info_serialization) {
    StreamPunkMachineInfo original;
    original.init();

    // 序列化
    std::stringstream ss;
    O out{ ss };
    out << original;

    // 反序列化
    StreamPunkMachineInfo restored;
    I in{ ss };
    in >> restored;

    // 验证内容
    ck_assert_int_eq(original.endian, restored.endian);
    ck_assert_int_eq(original.os, restored.os);
    ck_assert_int_eq(original.cpu, restored.cpu);
    ck_assert_int_eq(original.wordByteCount, restored.wordByteCount);
    ck_assert_int_eq(original.compiler, restored.compiler);
    ck_assert_int_eq(original.compilerVer, restored.compilerVer);
    ck_assert_int_eq(original.dataVer, restored.dataVer);
}



// 测试基本自定义类的类型描述符
REG_TEST(test_custom_class_desc_basic) {
    // Test 类 (直接继承自 Base)
    constexpr auto testDesc = TypeDesc<Test>::v;
    ck_assert_uint_eq(testDesc.size(), 1);
    ck_assert_uint_eq(testDesc[0], maliu::Test);

    // MQTT 类
    constexpr auto mqttDesc = TypeDesc<MQTT>::v;
    ck_assert_uint_eq(mqttDesc.size(), 1);
    ck_assert_uint_eq(mqttDesc[0], maliu::MQTT);
}

// 测试带继承的自定义类的类型描述符
REG_TEST(test_custom_class_desc_inheritance) {
    // Device 基类
    constexpr auto deviceDesc = TypeDesc<Device>::v;
    ck_assert_uint_eq(deviceDesc.size(), 1);
    ck_assert_uint_eq(deviceDesc[0], maliu::Device);

    // NetworkDevice (继承自 Device)
    constexpr auto netDeviceDesc = NetworkDevice::desc;
    ck_assert_uint_eq(netDeviceDesc[0], maliu::Device);
    ck_assert_uint_eq(netDeviceDesc[1], maliu::string);
    ck_assert_uint_eq(netDeviceDesc[2], maliu::ch);
    ck_assert_uint_eq(netDeviceDesc[3], maliu::string);
    ck_assert_uint_eq(netDeviceDesc[4], maliu::ch);
    ck_assert_uint_eq(netDeviceDesc[5], maliu::u16);

    // 多层继承的 TemperatureSensor
    constexpr auto tempSensorDesc = TemperatureSensor::desc;
    ck_assert_uint_eq(tempSensorDesc.size(), 3);
    ck_assert_uint_eq(tempSensorDesc[0], maliu::Sensor);
    ck_assert_uint_eq(tempSensorDesc[1], maliu::bl);
    ck_assert_uint_eq(tempSensorDesc[2], maliu::f64);
}

// 测试包含智能指针的自定义类描述符
REG_TEST(test_custom_class_desc_smart_pointers) {
    // PointerDemo 类 (含各种指针类型)
    constexpr auto ptrDemoDesc = PointerDemo::desc;
    ck_assert_uint_eq(ptrDemoDesc.size(), 9); // [base, pointer_demo]
    ck_assert_uint_eq(ptrDemoDesc[0], maliu::base);
    ck_assert_uint_eq(ptrDemoDesc[1], maliu::ptr);
    ck_assert_uint_eq(ptrDemoDesc[2], maliu::Test);
    ck_assert_uint_eq(ptrDemoDesc[3], maliu::sptr);
    ck_assert_uint_eq(ptrDemoDesc[4], maliu::MQTT);
    ck_assert_uint_eq(ptrDemoDesc[5], maliu::uptr);
    ck_assert_uint_eq(ptrDemoDesc[6], maliu::Test);
    ck_assert_uint_eq(ptrDemoDesc[7], maliu::wptr);
    ck_assert_uint_eq(ptrDemoDesc[8], maliu::PointerDemo);

    // 验证智能指针描述符
    constexpr auto rawPtrDesc = TypeDesc<decltype(PointerDemo::rawPtr)>::v;
    ck_assert_uint_eq(rawPtrDesc.size(), 2); // ptr + Test
    ck_assert_uint_eq(rawPtrDesc[0], maliu::ptr);
    ck_assert_uint_eq(rawPtrDesc[1], maliu::Test);

    constexpr auto sharedPtrDesc = TypeDesc<decltype(PointerDemo::sharedPtr)>::v;
    ck_assert_uint_eq(sharedPtrDesc.size(), 2); // sptr + MQTT
    ck_assert_uint_eq(sharedPtrDesc[0], maliu::sptr);
    ck_assert_uint_eq(sharedPtrDesc[1], maliu::MQTT);
}

// 测试嵌套容器的自定义类描述符
REG_TEST(test_custom_class_desc_nested_containers) {
    // ContainerDemo 类
    constexpr auto containerDesc = TypeDesc<ContainerDemo>::v;
    ck_assert_uint_eq(containerDesc.size(), 1);
    ck_assert_uint_eq(containerDesc[0], maliu::ContainerDemo);

    // 验证容器成员描述符
    constexpr auto vecPtrDesc = TypeDesc<decltype(ContainerDemo::testPtrs)>::v;
    ck_assert_uint_eq(vecPtrDesc.size(), 3); // vector + sptr + Test
    ck_assert_uint_eq(vecPtrDesc[0], maliu::vector);
    ck_assert_uint_eq(vecPtrDesc[1], maliu::sptr);
    ck_assert_uint_eq(vecPtrDesc[2], maliu::Test);

    // 验证嵌套 map 的描述符
    constexpr auto mapDesc = TypeDesc<decltype(ContainerDemo::mqttConfigs)>::v;
    ck_assert_uint_eq(mapDesc.size(), 5); // [map, string, char, umap, string, MQTT]
    ck_assert_uint_eq(mapDesc[0], maliu::map);
    ck_assert_uint_eq(mapDesc[1], maliu::string);
    ck_assert_uint_eq(mapDesc[2], maliu::ch);
    ck_assert_uint_eq(mapDesc[3], maliu::uptr);
    ck_assert_uint_eq(mapDesc[4], maliu::MQTT);
}

// 测试复杂系统类的类型描述符
REG_TEST(test_custom_class_desc_complex_systems) {
    // NetworkSystem 类
    constexpr auto networkDesc = TypeDesc<NetworkSystem>::v;
    ck_assert_uint_eq(networkDesc.size(), 1);
    ck_assert_uint_eq(networkDesc[0], maliu::NetworkSystem);

    // SmartHomeSystem 类
    constexpr auto smartHomeDesc = SmartHomeSystem::desc;
    ck_assert_uint_eq(smartHomeDesc.size(), 13);
    ck_assert_uint_eq(smartHomeDesc[0], maliu::base);
    ck_assert_uint_eq(smartHomeDesc[1], maliu::vector);
    ck_assert_uint_eq(smartHomeDesc[2], maliu::sptr);
    ck_assert_uint_eq(smartHomeDesc[3], maliu::Device);
    ck_assert_uint_eq(smartHomeDesc[4], maliu::map);
    ck_assert_uint_eq(smartHomeDesc[5], maliu::string);
    ck_assert_uint_eq(smartHomeDesc[6], maliu::ch);
    ck_assert_uint_eq(smartHomeDesc[7], maliu::sptr);
    ck_assert_uint_eq(smartHomeDesc[8], maliu::Sensor);
    ck_assert_uint_eq(smartHomeDesc[9], maliu::sptr);
    ck_assert_uint_eq(smartHomeDesc[10], maliu::TemperatureSensor);
    ck_assert_uint_eq(smartHomeDesc[11], maliu::sptr);
    ck_assert_uint_eq(smartHomeDesc[12], maliu::NetworkSystem);

    // 验证时间类型成员描述符
    constexpr auto timeDesc = TypeDesc<decltype(Device::lastSeen)>::v;
    ck_assert_uint_eq(timeDesc.size(), 1);
    ck_assert_uint_eq(timeDesc[0], maliu::timepoint);
}

// 测试多态和自引用类型的描述符
REG_TEST(test_custom_class_desc_polymorphic) {
    // 多态容器类
    constexpr auto mlcDesc = TypeDesc<MultiLevelContainer>::v;
    ck_assert_uint_eq(mlcDesc.size(), 1); // [base, multi_level_container]
    ck_assert_uint_eq(mlcDesc[0], maliu::MultiLevelContainer);

    // 验证基础对象指针描述符
    constexpr auto basePtrDesc = TypeDesc<decltype(MultiLevelContainer::baseObj)>::v;
    ck_assert_uint_eq(basePtrDesc.size(), 2); // sptr + base
    ck_assert_uint_eq(basePtrDesc[0], maliu::sptr);
    ck_assert_uint_eq(basePtrDesc[1], maliu::base);

    // 验证自引用描述符
    constexpr auto selfRefDesc = TypeDesc<decltype(MultiLevelContainer::selfRef)>::v;
    ck_assert_uint_eq(selfRefDesc.size(), 2); // [sptr, base, multi_level_container]
    ck_assert_uint_eq(selfRefDesc[0], maliu::sptr);
    ck_assert_uint_eq(selfRefDesc[1], maliu::MultiLevelContainer);
}

// 测试类型描述符的实际使用场景
REG_TEST(test_custom_class_desc_usage) {
    // 创建机器信息
    StreamPunkMachineInfo machine;
    machine.init();

    // 创建各种对象
    Test testObj;
    NetworkDevice netDevice;
    SmartHomeSystem smartHome;

    // 获取类型描述符
    const auto& testDesc = testObj.getDesc();
    const auto& netDeviceDesc = netDevice.getDesc();
    const auto& smartHomeDesc = smartHome.getDesc();

    // 验证基本描述符结构
    ck_assert_uint_eq(testDesc.size(), 15);
    ck_assert_uint_eq(testDesc[0], maliu::base);
    ck_assert_uint_eq(testDesc[1], maliu::string);
    ck_assert_uint_eq(testDesc[2], maliu::ch);
    ck_assert_uint_eq(testDesc[3], maliu::string);
    ck_assert_uint_eq(testDesc[4], maliu::ch);
    ck_assert_uint_eq(testDesc[5], maliu::string);
    ck_assert_uint_eq(testDesc[6], maliu::ch);
    ck_assert_uint_eq(testDesc[7], maliu::string);
    ck_assert_uint_eq(testDesc[8], maliu::ch);
    ck_assert_uint_eq(testDesc[9], maliu::string);
    ck_assert_uint_eq(testDesc[10], maliu::ch);
    ck_assert_uint_eq(testDesc[11], maliu::string);
    ck_assert_uint_eq(testDesc[12], maliu::ch);
    ck_assert_uint_eq(testDesc[13], maliu::string);
    ck_assert_uint_eq(testDesc[14], maliu::ch);

    // 验证继承类描述符
    ck_assert_uint_eq(netDeviceDesc.size(), 6);
    ck_assert_uint_eq(netDeviceDesc[0], maliu::Device);
    ck_assert_uint_eq(netDeviceDesc[1], maliu::string);
    ck_assert_uint_eq(netDeviceDesc[2], maliu::ch);
    ck_assert_uint_eq(netDeviceDesc[3], maliu::string);
    ck_assert_uint_eq(netDeviceDesc[4], maliu::ch);
    ck_assert_uint_eq(netDeviceDesc[5], maliu::u16);

    // 验证复杂系统描述符
    ck_assert_uint_eq(smartHomeDesc.size(), 13);
    ck_assert_uint_eq(smartHomeDesc[ 0], maliu::base);
    ck_assert_uint_eq(smartHomeDesc[ 1], maliu::vector);
    ck_assert_uint_eq(smartHomeDesc[ 2], maliu::sptr);
    ck_assert_uint_eq(smartHomeDesc[ 3], maliu::Device);
    ck_assert_uint_eq(smartHomeDesc[ 4], maliu::map);
    ck_assert_uint_eq(smartHomeDesc[ 5], maliu::string);
    ck_assert_uint_eq(smartHomeDesc[ 6], maliu::ch);
    ck_assert_uint_eq(smartHomeDesc[ 7], maliu::sptr);
    ck_assert_uint_eq(smartHomeDesc[ 8], maliu::Sensor);
    ck_assert_uint_eq(smartHomeDesc[ 9], maliu::sptr);
    ck_assert_uint_eq(smartHomeDesc[10], maliu::TemperatureSensor);
    ck_assert_uint_eq(smartHomeDesc[11], maliu::sptr);
    ck_assert_uint_eq(smartHomeDesc[12], maliu::NetworkSystem);

    // 序列化/反序列化机器信息
    std::stringstream ss;
    {
        O out(ss);
        out << machine;
    }

    StreamPunkMachineInfo restoredMachine;
    {
        I in(ss);
        in >> restoredMachine;
    }

    ck_assert_int_eq(machine.endian, restoredMachine.endian);
    ck_assert_int_eq(machine.dataVer, restoredMachine.dataVer);
}