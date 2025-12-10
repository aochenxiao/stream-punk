# include "unit_test.hpp"


// ======================= 连续空间序列化单元测试 =====================

// 测试用例：向量序列化
REG_TEST(test_vector_serialization) {
    // 1. 基本类型向量
    {
        std::stringstream stream;
        O o(stream);
        I i(stream);

        // 空向量
        std::vector<int> empty;
        o << empty;

        // 整数向量
        std::vector<int> ints = { 1, 2, 3, 4, 5 };
        o << ints;

        // 浮点数向量
        std::vector<float> floats = { 1.1f, 2.2f, 3.3f };
        o << floats;

        // 字符串向量
        std::vector<std::string> strings = { "a", "bc", "def" };
        o << strings;

        // 验证写入大小
        size_t expected_size =
            sizeof(Sz) + // 空向量大小
            sizeof(Sz) + sizeof(int) * 5 + // ints
            sizeof(Sz) + sizeof(float) * 3 + // floats
            sizeof(Sz) + // strings大小
            sizeof(Sz) + 1 + // "a"
            sizeof(Sz) + 2 + // "bc"
            sizeof(Sz) + 3; // "def"
        ck_assert_uint_eq(stream.str().size(), expected_size);

        // 反序列化验证
        std::vector<int> empty_out;
        i >> empty_out;
        ck_assert(empty_out.empty());

        std::vector<int> ints_out;
        i >> ints_out;
        ck_assert_uint_eq(ints_out.size(), 5);
        ck_assert(ints_out == ints);

        std::vector<float> floats_out;
        i >> floats_out;
        ck_assert_uint_eq(floats_out.size(), 3);
        for (size_t j = 0; j < floats_out.size(); ++j) {
            ck_assert(fabs(floats_out[j] - floats[j]) < 1e-5);
        }

        std::vector<std::string> strings_out;
        i >> strings_out;
        ck_assert_uint_eq(strings_out.size(), 3);
        ck_assert(strings_out == strings);
    }

    // 2. 非平凡类型向量
    //{
    //    struct NonTrivial {
    //        int id;
    //        std::string name;

    //        NonTrivial() = default;
    //        NonTrivial(int i, std::string n) : id(i), name(std::move(n)) {}

    //        bool operator==(const NonTrivial& other) const {
    //            return id == other.id && name == other.name;
    //        }
    //    };

    //    // 注册序列化支持
    //    O& operator<<(O & o, const NonTrivial & v) {
    //        o << v.id << v.name;
    //        return o;
    //    }

    //    I& operator>>(I & i, NonTrivial & v) {
    //        i >> v.id >> v.name;
    //        return i;
    //    }

    //    std::stringstream stream;
    //    O o(stream);
    //    I i(stream);

    //    std::vector<NonTrivial> items = {
    //        {1, "Alice"},
    //        {2, "Bob"},
    //        {3, "Charlie"}
    //    };

    //    o << items;

    //    // 验证非平凡类型使用逐个元素序列化
    //    size_t expected_size = sizeof(Sz);
    //    for (const auto& item : items) {
    //        expected_size += sizeof(int) + sizeof(Sz) + item.name.size();
    //    }
    //    ck_assert_uint_eq(stream.str().size(), expected_size);

    //    // 反序列化验证
    //    stream.seekg(0);
    //    std::vector<NonTrivial> items_out;
    //    i >> items_out;

    //    ck_assert_uint_eq(items_out.size(), items.size());
    //    for (size_t j = 0; j < items_out.size(); ++j) {
    //        ck_assert(items_out[j] == items[j]);
    //    }
    //}
}
END_TEST

// 测试用例：数组序列化
REG_TEST(test_array_serialization) {
    // 1. 基本类型数组
    {
        std::stringstream stream;
        O o(stream);
        I i(stream);

        size_t expected_size = 0;

        // 整数数组
        std::array<int, 3> ints = { 10, 20, 30 };
        o << ints;
        expected_size += sizeof(int) * 3 + sizeof(Sz);

        // 浮点数数组
        std::array<float, 2> floats = { 1.5f, 2.5f };
        o << floats;
        expected_size += sizeof(float) * 2 + sizeof(Sz);

        // 验证写入大小
        auto x = stream.str();                                                                                     
        ck_assert_uint_eq(x.size(), expected_size);

        std::array<int, 3> ints_out;
        i >> ints_out;
        ck_assert(ints_out == ints);

        std::array<float, 2> floats_out;
        i >> floats_out;
        for (size_t j = 0; j < floats_out.size(); ++j) {
            ck_assert(fabs(floats_out[j] - floats[j]) < 1e-5);
        }
    }

    // 2. 大数组（验证性能）
    {
        std::stringstream stream;
        O o(stream);
        I i(stream);

        constexpr size_t arrSz = 1024 * 1024; // 1MB

        // 1MB数组
        auto large_array_ptr = new std::array<char, arrSz>;
        auto& large_array = *large_array_ptr;
        std::fill(large_array.begin(), large_array.end(), 'A');

        o << large_array;
        auto x = stream.str();
        size_t sz = x.size();
        // 验证写入大小
        ck_assert_uint_eq(sz, large_array.size()+sizeof(Sz));

        // 反序列化验证
        stream.seekg(0);
        auto large_array_out_ptr = new std::array<char, arrSz>;
        auto& large_array_out = *large_array_out_ptr;
        i >> large_array_out;

        ck_assert(memcmp(large_array.data(), large_array_out.data(), large_array.size()) == 0);
        delete large_array_ptr;
        delete large_array_out_ptr;
    }
}
END_TEST

// 测试用例：字符串序列化
REG_TEST(test_string_serialization) {
    // 1. 基本字符串序列化
    {
        std::stringstream stream;
        O o(stream);
        I i(stream);

        // 空字符串
        std::string empty;
        o << empty;

        // ASCII字符串
        std::string ascii = "Hello, World!";
        o << ascii;

        // Unicode字符串
        std::string unicode = "你好，世界！";
        o << unicode;

        // 长字符串
        std::string long_str(1000, 'x');
        o << long_str;

        // 验证写入大小
        size_t expected_size =
            sizeof(Sz) + // empty
            sizeof(Sz) + ascii.size() + // ascii
            sizeof(Sz) + unicode.size() + // unicode
            sizeof(Sz) + long_str.size(); // long_str
        ck_assert_uint_eq(stream.str().size(), expected_size);

        // 反序列化验证
        stream.seekg(0);

        std::string empty_out;
        i >> empty_out;
        ck_assert(empty_out.empty());

        std::string ascii_out;
        i >> ascii_out;
        ck_assert(ascii_out == ascii);

        std::string unicode_out;
        i >> unicode_out;
        ck_assert(unicode_out == unicode);

        std::string long_str_out;
        i >> long_str_out;
        ck_assert(long_str_out == long_str);
    }

    // 2. 不同字符类型
    {
        std::stringstream stream;
        O o(stream);
        I i(stream);

        // wstring
        std::wstring wstr = L"Wide string";
        o << wstr;

        // u16string
        std::u16string u16str = u"UTF-16 string";
        o << u16str;

        // u32string
        std::u32string u32str = U"UTF-32 string";
        o << u32str;

        // 验证写入大小
        size_t expected_size =
            sizeof(Sz) + wstr.size() * sizeof(wchar_t) +
            sizeof(Sz) + u16str.size() * sizeof(char16_t) +
            sizeof(Sz) + u32str.size() * sizeof(char32_t);
        ck_assert_uint_eq(stream.str().size(), expected_size);

        // 反序列化验证
        stream.seekg(0);

        std::wstring wstr_out;
        i >> wstr_out;
        ck_assert(wstr_out == wstr);

        std::u16string u16str_out;
        i >> u16str_out;
        ck_assert(u16str_out == u16str);

        std::u32string u32str_out;
        i >> u32str_out;
        ck_assert(u32str_out == u32str);
    }
}
END_TEST

// 测试用例：字符串视图序列化
REG_TEST(test_string_view_serialization) {
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 字符串视图
    std::string base_str = "Base string for views";
    std::string_view full_view = base_str;
    std::string_view partial_view = std::string_view(base_str).substr(5, 6); // "string"

    o << full_view << partial_view;

    // 验证写入大小
    size_t expected_size =
        sizeof(Sz) + full_view.size() + // full_view
        sizeof(Sz) + partial_view.size(); // partial_view
    ck_assert_uint_eq(stream.str().size(), expected_size);

    // 反序列化验证（string_view不可修改，所以反序列化为string）
    stream.seekg(0);

    std::string full_out;
    i >> full_out;
    ck_assert(full_out == full_view);

    std::string partial_out;
    i >> partial_out;
    ck_assert(partial_out == partial_view);
}
END_TEST

// 测试用例：混合容器序列化
REG_TEST(test_mixed_container_serialization) {
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 混合容器
    std::vector<std::array<int, 2>> vector_of_arrays = {
        {1, 2}, {3, 4}, {5, 6}
    };
    size_t vSz = 
        sizeof(Sz) +
        vector_of_arrays.size() * sizeof(decltype(vector_of_arrays)::value_type)
    ;
    std::array<std::string, 2> array_of_strings = {
        "first", "second"
    };
    size_t aSz =
        sizeof(Sz) +
        sizeof(Sz) + array_of_strings[0].size() +
        sizeof(Sz) + array_of_strings[1].size();

    o << vector_of_arrays << array_of_strings;

    // 验证写入大小
    ck_assert_uint_eq(stream.str().size(), vSz+aSz);

    // 反序列化验证
    stream.seekg(0);

    std::vector<std::array<int, 2>> vector_of_arrays_out;
    i >> vector_of_arrays_out;
    ck_assert(vector_of_arrays_out.size() == vector_of_arrays.size());
    for (size_t j = 0; j < vector_of_arrays_out.size(); ++j) {
        ck_assert(vector_of_arrays_out[j] == vector_of_arrays[j]);
    }

    std::array<std::string, 2> array_of_strings_out;
    i >> array_of_strings_out;
    ck_assert(array_of_strings_out == array_of_strings);
}
END_TEST

// 测试用例：边界和异常情况
REG_TEST(test_container_edge_cases) {
    // 1. 超大容器
    {
        std::stringstream stream;
        O o(stream);

        // 创建超大容器（测试size_t序列化）
        std::vector<char> large_vector(Sz(1) << 20); // 1MB
        o << large_vector;

        ck_assert_uint_eq(stream.str().size(), sizeof(Sz) + large_vector.size());
    }

    // 2. 空容器
    {
        std::stringstream stream;
        O o(stream);
        I i(stream);

        std::vector<int> empty_vector;
        std::array<int, 0> empty_array;
        std::string empty_string;
        std::string_view empty_view;

        o << empty_vector << empty_array << empty_string << empty_view;

        // 验证写入大小
        size_t expected_size =
            sizeof(Sz) * 4; // 每个容器写入一个size_t(0)
        ck_assert_uint_eq(stream.str().size(), expected_size);

        // 反序列化验证
        stream.seekg(0);

        std::vector<int> empty_vector_out;
        i >> empty_vector_out;
        ck_assert(empty_vector_out.empty());

        std::array<int, 0> empty_array_out;
        i >> empty_array_out;
        ck_assert(empty_array_out.empty());

        std::string empty_string_out;
        i >> empty_string_out;
        ck_assert(empty_string_out.empty());

        std::string empty_view_out;
        i >> empty_view_out;
        ck_assert(empty_view_out.empty());
    }

    // 3. 截断数据（测试错误恢复）
    {
        std::stringstream stream;
        O o(stream);

        // 写入部分数据
        std::vector<int> data = { 1, 2, 3, 4, 5 };
        o << data;

        // 截断流
        std::string content = stream.str();
        content.resize(content.size() - 2); // 移除最后两个字节
        stream.str(content);

        // 尝试反序列化
        I i(stream);
        std::vector<int> data_out;

        try {
            i >> data_out;
            throw std::exception("Reading truncated data should fail");
        }
        catch (const std::exception& e) {
            // 期望抛出异常
            std::cout << e.what() << std::endl;
        }
        catch (...) {
            ck_abort_msg("Unexpected exception type thrown~~~~~~~~~~");
        }

        // 验证流状态
# undef fail
        ck_assert(stream.fail());
    }
}
END_TEST

