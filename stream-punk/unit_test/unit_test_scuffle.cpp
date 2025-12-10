#include "unit_test.hpp"

// 测试基本数据类型
REG_TEST(test_basic_types) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // 各种基本类型值
    u8 a = 255;
    u16 b = 65535;
    u32 c = 4294967295;
    u64 d = 18446744073709551615ULL;
    i8 e = -128;
    i16 f = -32768;
    i32 g = -2147483648;
    i64 h = -9223372036854775807LL;
    f32 i = 3.1415926f;
    f64 j = 2.718281828459045;
    ch k = 'Z';
    bl l = true;
    bl m = false;

    // 序列化
    out << a << b << c << d << e << f << g << h << i << j << k << l << m;

    // 反序列化
    u8 a1; u16 b1; u32 c1; u64 d1;
    i8 e1; i16 f1; i32 g1; i64 h1;
    f32 i1; f64 j1; ch k1; bl l1; bl m1;

    in >> a1 >> b1 >> c1 >> d1 >> e1 >> f1 >> g1 >> h1 >> i1 >> j1 >> k1 >> l1 >> m1;

    // 验证
    ck_assert(a == a1);
    ck_assert(b == b1);
    ck_assert(c == c1);
    ck_assert(d == d1);
    ck_assert(e == e1);
    ck_assert(f == f1);
    ck_assert(g == g1);
    ck_assert(h == h1);
    ck_assert(i == i1);
    ck_assert(j == j1);
    ck_assert(k == k1);
    ck_assert(l == l1);
    ck_assert(m == m1);
}

// 测试std::array
void test_std_arrays() {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // 各种数组
    std::array<int, 5> arr1 = { 1, 2, 3, 4, 5 };
    std::array<float, 3> arr2 = { 1.1f, 2.2f, 3.3f };
    std::array<std::string, 2> arr3 = { "Hello", "World" };

    // 序列化
    out << arr1 << arr2 << arr3;

    // 反序列化
    std::array<int, 5> arr1_in;
    std::array<float, 3> arr2_in;
    std::array<std::string, 2> arr3_in;

    in >> arr1_in >> arr2_in >> arr3_in;

    // 验证
    ck_assert(arr1 == arr1_in);
    ck_assert(arr2 == arr2_in);
    ck_assert(arr3 == arr3_in);
}

// 测试字符串
REG_TEST(test_strings) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    std::string str1 = "Hello, World!";
    std::wstring wstr1 = L"宽字符字符串";
    std::string_view sv = "String View";

    // 序列化
    out << str1 << wstr1 << sv;

    // 反序列化
    std::string str1_in;
    std::wstring wstr1_in;
    std::string_view sv_in; // 注意：string_view不能直接反序列化为相同视图

    in >> str1_in;
    in >> wstr1_in;

    // 对于string_view，我们需要序列化数据然后创建视图
    std::string temp;
    in >> temp;
    sv_in = temp;

    // 验证
    ck_assert(str1 == str1_in);
    ck_assert(wstr1 == wstr1_in);
    ck_assert(sv == sv_in);
}

// 测试vector
REG_TEST(test_vectors) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // 各种vector
    std::vector<int> vec1 = { 1, 2, 3, 4, 5 };
    std::vector<std::vector<int>> vec2 = { {1, 2}, {3, 4}, {5, 6} };
    std::vector<std::string> vec3 = { "A", "B", "C" };

    // 序列化
    out << vec1 << vec2 << vec3;

    // 反序列化
    std::vector<int> vec1_in;
    std::vector<std::vector<int>> vec2_in;
    std::vector<std::string> vec3_in;

    in >> vec1_in >> vec2_in >> vec3_in;

    // 验证
    ck_assert(vec1 == vec1_in);
    ck_assert(vec2 == vec2_in);
    ck_assert(vec3 == vec3_in);
}

// 测试deque
REG_TEST(test_deques) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    std::deque<double> deq = { 1.1, 2.2, 3.3, 4.4 };
    std::deque<std::string> deq2 = { "Apple", "Banana", "Cherry" };

    // 序列化
    out << deq << deq2;

    // 反序列化
    std::deque<double> deq_in;
    std::deque<std::string> deq2_in;

    in >> deq_in >> deq2_in;

    // 验证
    ck_assert(deq == deq_in);
    ck_assert(deq2 == deq2_in);
}

// 测试map和unordered_map
REG_TEST(test_maps) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // 各种map
    std::map<int, std::string> map1 = {
        {1, "One"},
        {2, "Two"},
        {3, "Three"}
    };

    std::unordered_map<std::string, int> map2 = {
        {"Apple", 10},
        {"Banana", 20},
        {"Cherry", 30}
    };

    std::map<std::string, std::vector<int>> map3 = {
        {"A", {1, 2, 3}},
        {"B", {4, 5, 6}}
    };

    // 序列化
    out << map1 << map2 << map3;

    // 反序列化
    std::map<int, std::string> map1_in;
    std::unordered_map<std::string, int> map2_in;
    std::map<std::string, std::vector<int>> map3_in;

    in >> map1_in >> map2_in >> map3_in;

    // 验证
    ck_assert(map1 == map1_in);
    ck_assert(map2 == map2_in);
    ck_assert(map3 == map3_in);
}

// 测试set和unordered_set
REG_TEST(test_sets) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // 各种set
    std::set<int> set1 = { 1, 3, 5, 7, 9 };
    std::unordered_set<std::string> set2 = { "Cat", "Dog", "Elephant" };
    std::set<std::vector<int>> set3 = { {1,2}, {3,4}, {5,6} };

    // 序列化
    out << set1 << set2 << set3;

    // 反序列化
    std::set<int> set1_in;
    std::unordered_set<std::string> set2_in;
    std::set<std::vector<int>> set3_in;

    in >> set1_in >> set2_in >> set3_in;

    // 验证
    ck_assert(set1 == set1_in);
    ck_assert(set2 == set2_in);
    ck_assert(set3 == set3_in);
}

// 测试list和forward_list
REG_TEST(test_lists) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // 双向链表
    std::list<int> list1 = { 1, 2, 3, 4, 5 };
    std::list<std::list<int>> list2 = { {1,2}, {3,4}, {5,6} };

    // 单向链表
    std::forward_list<std::string> flist = { "Red", "Green", "Blue" };

    // 序列化
    out << list1 << list2 << flist;

    // 反序列化
    std::list<int> list1_in;
    std::list<std::list<int>> list2_in;
    std::forward_list<std::string> flist_in;

    in >> list1_in >> list2_in >> flist_in;

    // 验证
    ck_assert(list1 == list1_in);
    ck_assert(list2 == list2_in);
    ck_assert(std::equal(flist.begin(), flist.end(), flist_in.begin()));
}

// 测试bitset
REG_TEST(test_bitset) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // 各种bitset
    std::bitset<8> bs1(0b10101010);
    std::bitset<16> bs2(0b1111000011110000);
    std::bitset<100> bs3; // 大bitset

    // 设置大bitset中的一些位
    bs3.set(10);
    bs3.set(50);
    bs3.set(99);

    // 序列化
    out << bs1 << bs2 << bs3;

    // 反序列化
    std::bitset<8> bs1_in;
    std::bitset<16> bs2_in;
    std::bitset<100> bs3_in;

    in >> bs1_in >> bs2_in >> bs3_in;

    // 验证
    ck_assert(bs1 == bs1_in);
    ck_assert(bs2 == bs2_in);
    ck_assert(bs3 == bs3_in);
}

// 测试原生指针
REG_TEST(test_raw_pointers) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    int* ptr1 = new int(42);
    int* ptr2 = new int(100);
    int* null_ptr = nullptr;

    // 序列化
    out << ptr1 << ptr2 << null_ptr;

    // 反序列化
    int* ptr1_in = nullptr;
    int* ptr2_in = nullptr;
    int* null_ptr_in = nullptr;

    in >> ptr1_in >> ptr2_in >> null_ptr_in;

    // 验证
    ck_assert(*ptr1_in == 42);
    ck_assert(*ptr2_in == 100);
    ck_assert(null_ptr_in == nullptr);

    // 清理
    delete ptr1;
    delete ptr2;
}

// 测试shared_ptr
REG_TEST(test_shared_pointers) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    auto ptr1 = std::make_shared<int>(42);
    auto ptr2 = std::make_shared<std::string>("Hello");
    auto ptr3 = ptr1; // 共享所有权
    std::shared_ptr<int> null_ptr;

    // 序列化
    out << ptr1 << ptr2 << ptr3 << null_ptr;

    // 反序列化
    std::shared_ptr<int> ptr1_in;
    std::shared_ptr<std::string> ptr2_in;
    std::shared_ptr<int> ptr3_in;
    std::shared_ptr<int> null_ptr_in;

    in >> ptr1_in >> ptr2_in >> ptr3_in >> null_ptr_in;

    // 验证
    ck_assert(*ptr1_in == 42);
    ck_assert(*ptr2_in == "Hello");
    ck_assert(*ptr3_in == 42);
    ck_assert(null_ptr_in == nullptr);

    // 验证指针相等（同一个对象）
    ck_assert(ptr1_in == ptr3_in);
}

// 测试weak_ptr
REG_TEST(test_weak_pointers) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    auto shared = std::make_shared<int>(42);
    std::weak_ptr<int> weak = shared;
    std::weak_ptr<int> null_weak;

    // 序列化
    out << weak << null_weak;

    // 反序列化
    std::weak_ptr<int> weak_in;
    std::weak_ptr<int> null_weak_in;

    in >> weak_in >> null_weak_in;

    // 验证
    auto locked = weak_in.lock();
    ck_assert(locked != nullptr);
    ck_assert(*locked == 42);

    auto null_locked = null_weak_in.lock();
    ck_assert(null_locked == nullptr);
}

// 测试unique_ptr
REG_TEST(test_unique_pointers) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    auto ptr1 = std::make_unique<int>(42);
    auto ptr2 = std::make_unique<std::string>("World");
    std::unique_ptr<int> null_ptr;

    // 序列化
    out << ptr1 << ptr2 << null_ptr;

    // 反序列化
    std::unique_ptr<int> ptr1_in;
    std::unique_ptr<std::string> ptr2_in;
    std::unique_ptr<int> null_ptr_in;

    in >> ptr1_in >> ptr2_in >> null_ptr_in;

    // 验证
    ck_assert(*ptr1_in == 42);
    ck_assert(*ptr2_in == "World");
    ck_assert(null_ptr_in == nullptr);
}

// 树形结构测试
struct TreeNode {
    int value;
    std::vector<std::shared_ptr<TreeNode>> children;

    TreeNode() = default;
    TreeNode(int v) : value(v) {}
};

inline O& operator<<(O& o, const TreeNode& node) {
    o << node.value << node.children;
    return o;
}

inline I& operator>>(I& i, TreeNode& node) {
    i >> node.value >> node.children;
    return i;
}

REG_TEST(test_tree_structure) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // 创建树结构
    //      1
    //     /|\
    //    2 3 4
    //   / \
    //  5   6
    auto root = std::make_shared<TreeNode>(1);
    auto c1 = std::make_shared<TreeNode>(2);
    auto c2 = std::make_shared<TreeNode>(3);
    auto c3 = std::make_shared<TreeNode>(4);
    auto c11 = std::make_shared<TreeNode>(5);
    auto c12 = std::make_shared<TreeNode>(6);

    c1->children = { c11, c12 };
    root->children = { c1, c2, c3 };

    // 序列化
    out << root;

    // 反序列化
    std::shared_ptr<TreeNode> root_in;
    in >> root_in;

    // 验证结构
    ck_assert(root_in->value == 1);
    ck_assert(root_in->children.size() == 3);
    ck_assert(root_in->children[0]->value == 2);
    ck_assert(root_in->children[1]->value == 3);
    ck_assert(root_in->children[2]->value == 4);

    auto& child1 = root_in->children[0];
    ck_assert(child1->children.size() == 2);
    ck_assert(child1->children[0]->value == 5);
    ck_assert(child1->children[1]->value == 6);
}

// 图结构测试
struct GraphNode {
    int id;
    std::vector<std::weak_ptr<GraphNode>> neighbors;

    GraphNode() = default;
    GraphNode(int id) : id(id) {}
};

inline O& operator<<(O& o, const GraphNode& node) {
    o << node.id;

    // 序列化邻居的shared_ptr
    std::vector<std::shared_ptr<GraphNode>> sharedNeighbors;
    for (auto& weak : node.neighbors) {
        if (auto shared = weak.lock()) {
            sharedNeighbors.push_back(shared);
        }
    }
    o << sharedNeighbors;

    return o;
}

inline I& operator>>(I& i, GraphNode& node) {
    i >> node.id;

    std::vector<std::shared_ptr<GraphNode>> neighbors;
    i >> neighbors;

    for (auto& n : neighbors) {
        node.neighbors.push_back(n);
    }

    return i;
}

REG_TEST(test_graph_structure) {
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // 创建图结构
    // 1 ↔ 2
    // ↗ ↖
    // 3 ← 4
    auto n1 = std::make_shared<GraphNode>(1);
    auto n2 = std::make_shared<GraphNode>(2);
    auto n3 = std::make_shared<GraphNode>(3);
    auto n4 = std::make_shared<GraphNode>(4);

    n1->neighbors.push_back(n2);
    n2->neighbors.push_back(n1);
    n2->neighbors.push_back(n3);
    n3->neighbors.push_back(n4);
    n4->neighbors.push_back(n2);

    // 序列化
    out << n1 << n2 << n3 << n4;

    // 反序列化
    std::shared_ptr<GraphNode> n1_in, n2_in, n3_in, n4_in;
    in >> n1_in >> n2_in >> n3_in >> n4_in;

    // 验证结构
    ck_assert(n1_in->id == 1);
    ck_assert(n2_in->id == 2);
    ck_assert(n3_in->id == 3);
    ck_assert(n4_in->id == 4);

    // 验证边
    auto n1_nb = n1_in->neighbors[0].lock();
    ck_assert(n1_nb->id == 2);

    auto n2_nb1 = n2_in->neighbors[0].lock();
    auto n2_nb2 = n2_in->neighbors[1].lock();
    ck_assert((n2_nb1->id == 1 && n2_nb2->id == 3) ||
        (n2_nb1->id == 3 && n2_nb2->id == 1));

    auto n3_nb = n3_in->neighbors[0].lock();
    ck_assert(n3_nb->id == 4);

    auto n4_nb = n4_in->neighbors[0].lock();
    ck_assert(n4_nb->id == 2);
}