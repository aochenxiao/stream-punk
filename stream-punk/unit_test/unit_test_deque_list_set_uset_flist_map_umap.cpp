# include "unit_test.hpp"


// ======================= 容器序列化单元测试 =====================

// 测试用例：deque序列化
REG_TEST(test_deque_serialization) {
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 整型deque
    std::deque<int> int_deque = { 1, 2, 3, 4, 5 };
    o << int_deque;

    // 字符串deque
    std::deque<std::string> str_deque = { "a", "bc", "def" };
    o << str_deque;

    // 反序列化验证
    stream.seekg(0);

    std::deque<int> int_deque_out;
    i >> int_deque_out;
    ck_assert(int_deque_out == int_deque);

    std::deque<std::string> str_deque_out;
    i >> str_deque_out;
    ck_assert(str_deque_out == str_deque);
}
END_TEST

// 测试用例：list序列化


// 自定义类型链表
struct Point {
    int x, y;
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};
// 注册序列化支持
O& operator<<(O& o, const Point& p) {
    o << p.x << p.y;
    return o;
}
I& operator>>(I& i, Point& p) {
    i >> p.x >> p.y;
    return i;
}
REG_TEST(test_list_serialization) {
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 双向链表
    std::list<double> double_list = { 1.1, 2.2, 3.3 };
    o << double_list;

    std::list<Point> point_list = { {1,2}, {3,4}, {5,6} };
    o << point_list;

    // 反序列化验证
    stream.seekg(0);

    std::list<double> double_list_out;
    i >> double_list_out;
    ck_assert(double_list_out == double_list);

    std::list<Point> point_list_out;
    i >> point_list_out;
    ck_assert(point_list_out == point_list);
}
END_TEST

// 测试用例：set序列化
REG_TEST(test_set_serialization) {
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 整型set
    std::set<int> int_set = { 3, 1, 4, 1, 5 }; // 重复元素会被忽略
    o << int_set;

    // 字符串set
    std::set<std::string> str_set = { "apple", "banana", "cherry" };
    o << str_set;

    // 自定义比较函数的set
    auto case_insensitive = [](const std::string& a, const std::string& b) {
        return std::lexicographical_compare(
            a.begin(), a.end(), b.begin(), b.end(),
            [](char c1, char c2) { return std::tolower(c1) < std::tolower(c2); }
        );
        };
    std::set<std::string, decltype(case_insensitive)> custom_set(case_insensitive);
    custom_set = { "Apple", "banana", "CHERRY" };
    o << custom_set;

    // 反序列化验证
    stream.seekg(0);

    std::set<int> int_set_out;
    i >> int_set_out;
    ck_assert(int_set_out == int_set);

    std::set<std::string> str_set_out;
    i >> str_set_out;
    ck_assert(str_set_out == str_set);

    std::set<std::string, decltype(case_insensitive)> custom_set_out(case_insensitive);
    i >> custom_set_out;

    // 验证自定义比较函数仍然有效
    auto it = custom_set_out.find("APPLE");
    ck_assert(it != custom_set_out.end());
    ck_assert(*it == "Apple");
}
END_TEST


// 自定义哈希函数的unordered_set
struct Color {
    std::string name;
    int r, g, b;

    bool operator==(const Color& other) const {
        return name == other.name;
    }
};

struct ColorHash {
    size_t operator()(const Color& c) const {
        return std::hash<std::string>()(c.name);
    }
};
// 注册序列化支持
O& operator<<(O& o, const Color& c) {
    o << c.name << c.r << c.g << c.b;
    return o;
}
I& operator>>(I& i, Color& c) {
    i >> c.name >> c.r >> c.g >> c.b;
    return i;
}

// 测试用例：unordered_set序列化
REG_TEST(test_unordered_set_serialization) {
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 整型unordered_set
    std::unordered_set<int> int_uset = { 5, 2, 8, 3 };
    o << int_uset;

    // 字符串unordered_set
    std::unordered_set<std::string> str_uset = { "red", "green", "blue" };
    o << str_uset;

    std::unordered_set<Color, ColorHash> custom_uset;
    custom_uset.insert({ "red", 255, 0, 0 });
    custom_uset.insert({ "green", 0, 255, 0 });
    o << custom_uset;

    // 反序列化验证
    stream.seekg(0);

    std::unordered_set<int> int_uset_out;
    i >> int_uset_out;
    ck_assert(int_uset_out == int_uset);

    std::unordered_set<std::string> str_uset_out;
    i >> str_uset_out;
    ck_assert(str_uset_out == str_uset);

    std::unordered_set<Color, ColorHash> custom_uset_out;
    i >> custom_uset_out;
    ck_assert(custom_uset_out.size() == 2);
    ck_assert(custom_uset_out.find({ "red", 0, 0, 0 }) != custom_uset_out.end());
}
END_TEST

// 测试用例：forward_list序列化
REG_TEST(test_forward_list_serialization) {
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 整型forward_list
    std::forward_list<int> int_flist = { 10, 20, 30 };
    o << int_flist;

    // 字符串forward_list
    std::forward_list<std::string> str_flist;
    str_flist.assign({ "one", "two", "three" });
    o << str_flist;

    // 空forward_list
    std::forward_list<int> empty_flist;
    o << empty_flist;

    // 反序列化验证
    stream.seekg(0);

    std::forward_list<int> int_flist_out;
    i >> int_flist_out;
    ck_assert(std::equal(int_flist.begin(), int_flist.end(), int_flist_out.begin()));

    std::forward_list<std::string> str_flist_out;
    i >> str_flist_out;
    ck_assert(std::equal(str_flist.begin(), str_flist.end(), str_flist_out.begin()));

    std::forward_list<int> empty_flist_out;
    i >> empty_flist_out;
    ck_assert(empty_flist_out.empty());
}
END_TEST

// 测试用例：map序列化
REG_TEST(test_map_serialization) {
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 整型map
    std::map<int, std::string> int_map = {
        {1, "one"},
        {2, "two"},
        {3, "three"}
    };
    o << int_map;

    // 字符串map
    std::map<std::string, double> str_map = {
        {"pi", 3.14159},
        {"e", 2.71828}
    };
    o << str_map;

    // 自定义比较函数的map
    struct CaseInsensitiveCompare {
        bool operator()(const std::string& a, const std::string& b) const {
            return std::lexicographical_compare(
                a.begin(), a.end(), b.begin(), b.end(),
                [](char c1, char c2) { return std::tolower(c1) < std::tolower(c2); }
            );
        }
    };

    std::map<std::string, int, CaseInsensitiveCompare> custom_map;
    custom_map["Apple"] = 10;
    custom_map["banana"] = 20;
    o << custom_map;

    // 反序列化验证
    stream.seekg(0);

    std::map<int, std::string> int_map_out;
    i >> int_map_out;
    ck_assert(int_map_out == int_map);

    std::map<std::string, double> str_map_out;
    i >> str_map_out;
    ck_assert(str_map_out == str_map);

    std::map<std::string, int, CaseInsensitiveCompare> custom_map_out;
    i >> custom_map_out;

    // 验证自定义比较函数仍然有效
    ck_assert(custom_map_out.find("APPLE") != custom_map_out.end());
    ck_assert(custom_map_out["apple"] == 10);
}
END_TEST


// 自定义类型的unordered_map
struct Employee {
    int id;
    std::string name;
    double salary;

    bool operator==(const Employee& other) const {
        return id == other.id;
    }
};
struct EmployeeHash {
    size_t operator()(const Employee& e) const {
        return std::hash<int>()(e.id);
    }
};
// 注册序列化支持
O& operator<<(O& o, const Employee& e) {
    o << e.id << e.name << e.salary;
    return o;
}
I& operator>>(I& i, Employee& e) {
    i >> e.id >> e.name >> e.salary;
    return i;
}
// 测试用例：unordered_map序列化
REG_TEST(test_unordered_map_serialization) {
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 整型unordered_map
    std::unordered_map<int, std::string> int_umap = {
        {100, "hundred"},
        {200, "two hundred"}
    };
    o << int_umap;

    // 字符串unordered_map
    std::unordered_map<std::string, int> str_umap = {
        {"Alice", 30},
        {"Bob", 25}
    };
    o << str_umap;

    std::unordered_map<Employee, std::string, EmployeeHash> custom_umap;
    custom_umap[{1, "John", 50000.0}] = "Manager";
    custom_umap[{2, "Jane", 60000.0}] = "Director";
    o << custom_umap;

    // 反序列化验证
    stream.seekg(0);

    std::unordered_map<int, std::string> int_umap_out;
    i >> int_umap_out;
    ck_assert(int_umap_out == int_umap);

    std::unordered_map<std::string, int> str_umap_out;
    i >> str_umap_out;
    ck_assert(str_umap_out == str_umap);

    std::unordered_map<Employee, std::string, EmployeeHash> custom_umap_out;
    i >> custom_umap_out;

    // 验证自定义类型
    auto it = custom_umap_out.find({ 1, "", 0.0 });
    ck_assert(it != custom_umap_out.end());
    ck_assert(it->second == "Manager");
}
END_TEST

// 测试用例：bitset序列化
REG_TEST(test_bitset_serialization) {
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 小bitset
    std::bitset<8> small_bitset(0b10101010);
    o << small_bitset;

    // 大bitset
    std::bitset<1000> large_bitset;
    large_bitset.set(5);
    large_bitset.set(9);
    o << large_bitset;

    // 全0 bitset
    std::bitset<16> zero_bitset(0);
    o << zero_bitset;

    // 全1 bitset
    std::bitset<32> full_bitset;
    full_bitset.set();
    o << full_bitset;

    // 反序列化验证
    stream.seekg(0);

    std::bitset<8> small_bitset_out;
    i >> small_bitset_out;
    ck_assert(small_bitset_out == small_bitset);

    std::bitset<1000> large_bitset_out;
    i >> large_bitset_out;
    ck_assert(large_bitset_out == large_bitset);

    std::bitset<16> zero_bitset_out;
    i >> zero_bitset_out;
    ck_assert(zero_bitset_out == zero_bitset);

    std::bitset<32> full_bitset_out;
    i >> full_bitset_out;
    ck_assert(full_bitset_out == full_bitset);
}
END_TEST

// 测试用例：空容器序列化
REG_TEST(test_empty_container_serialization) {
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 空容器
    std::deque<int> empty_deque;
    std::list<double> empty_list;
    std::set<int> empty_set;
    std::unordered_set<std::string> empty_uset;
    std::forward_list<char> empty_flist;
    std::map<int, int> empty_map;
    std::unordered_map<std::string, double> empty_umap;
    std::bitset<0> empty_bitset;

    o << empty_deque;
    auto x1 = stream.str();
    o << empty_list  ;
    auto x2 = stream.str();
    o << empty_set   ;
    auto x3 = stream.str();
    o << empty_uset  ;
    auto x4 = stream.str();
    o << empty_flist ;
    auto x5 = stream.str();
    o << empty_map   ;
    auto x6 = stream.str();
    o << empty_umap  ;
    auto x7 = stream.str();
    o << empty_bitset;
    auto x8 = stream.str();

    // 验证写入大小
    // 每个容器写入一个size_t(0)，bitset写入0字节（但先写入size=0）
    size_t expected_size = sizeof(Sz) * 8 + sizeof(u64); // 8个容器 * 每个容器一个大小字段
    ck_assert_uint_eq(stream.str().size(), expected_size);

    // 反序列化验证
    stream.seekg(0);

    std::deque<int> empty_deque_out;
    i >> empty_deque_out;
    ck_assert(empty_deque_out.empty());

    std::list<double> empty_list_out;
    i >> empty_list_out;
    ck_assert(empty_list_out.empty());

    std::set<int> empty_set_out;
    i >> empty_set_out;
    ck_assert(empty_set_out.empty());

    std::unordered_set<std::string> empty_uset_out;
    i >> empty_uset_out;
    ck_assert(empty_uset_out.empty());

    std::forward_list<char> empty_flist_out;
    i >> empty_flist_out;
    ck_assert(empty_flist_out.empty());

    std::map<int, int> empty_map_out;
    i >> empty_map_out;
    ck_assert(empty_map_out.empty());

    std::unordered_map<std::string, double> empty_umap_out;
    i >> empty_umap_out;
    ck_assert(empty_umap_out.empty());

    std::bitset<0> empty_bitset_out;
    i >> empty_bitset_out;
    ck_assert(empty_bitset_out.size() == 0);
}
END_TEST

// 测试用例：容器嵌套
REG_TEST(test_nested_container_serialization) {
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 嵌套容器结构
    std::map<std::string, std::vector<std::set<int>>> complex_structure;
    complex_structure["group1"] = { {1, 2, 3}, {4, 5} };
    complex_structure["group2"] = { {6}, {7, 8, 9} };

    o << complex_structure;

    // 反序列化验证
    stream.seekg(0);

    std::map<std::string, std::vector<std::set<int>>> complex_structure_out;
    i >> complex_structure_out;

    // 验证结构
    ck_assert(complex_structure_out.size() == 2);

    const auto& group1 = complex_structure_out["group1"];
    ck_assert(group1.size() == 2);
    ck_assert(group1[0] == std::set<int>({ 1, 2, 3 }));
    ck_assert(group1[1] == std::set<int>({ 4, 5 }));

    const auto& group2 = complex_structure_out["group2"];
    ck_assert(group2.size() == 2);
    ck_assert(group2[0] == std::set<int>({ 6 }));
    ck_assert(group2[1] == std::set<int>({ 7, 8, 9 }));
}
END_TEST

// 测试用例：大容量容器
REG_TEST(test_large_container_serialization) {
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 创建大容量容器
    const size_t large_size = 10000;

    // 大列表
    std::list<int> large_list;
    for (int i = 0; i < large_size; i++) {
        large_list.push_back(i);
    }
    o << large_list;

    // 大map
    std::map<int, int> large_map;
    for (int i = 0; i < large_size; i++) {
        large_map[i] = i * 2;
    }
    o << large_map;

    // 反序列化验证
    stream.seekg(0);

    // 验证列表
    std::list<int> large_list_out;
    i >> large_list_out;

    ck_assert(large_list_out.size() == large_size);
    int counter = 0;
    for (auto it = large_list_out.begin(); it != large_list_out.end(); ++it, ++counter) {
        ck_assert(*it == counter);
    }

    // 验证map
    std::map<int, int> large_map_out;
    i >> large_map_out;

    ck_assert(large_map_out.size() == large_size);
    for (int i = 0; i < large_size; i++) {
        ck_assert(large_map_out[i] == i * 2);
    }
}
END_TEST

// 测试用例：序列化兼容性
REG_TEST(test_container_compatibility) {
    // 测试不同容器类型之间的序列化兼容性
    std::stringstream stream;
    O o(stream);
    I i(stream);

    // 写入deque
    std::deque<int> deque_in = { 10, 20, 30 };
    o << deque_in;

    // 尝试作为vector读取
    stream.seekg(0);
    std::vector<int> vector_out;
    i >> vector_out;

    // 验证数据相同
    ck_assert(vector_out.size() == deque_in.size());
    for (size_t j = 0; j < deque_in.size(); j++) {
        ck_assert(vector_out[j] == deque_in[j]);
    }

    // 写入set
    stream.str("");
    stream.clear();
    std::set<int> set_in = { 1, 2, 3 };
    o << set_in;

    // 尝试作为unordered_set读取
    stream.seekg(0);
    std::unordered_set<int> uset_out;
    i >> uset_out;

    // 验证数据相同
    ck_assert(uset_out.size() == set_in.size());
    for (int num : set_in) {
        ck_assert(uset_out.find(num) != uset_out.end());
    }
}
END_TEST