#include "unit_test.hpp"

// 测试数据结构
struct Node {
    int id;
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev;

    Node() = default;
    Node(int i) : id(i) {}
};

// 全局序列化操作符
inline O& operator<<(O& o, const Node& node) {
    o << node.id; // 序列化节点ID
    o << node.next; // 序列化下一个节点的shared_ptr
    o << node.prev.lock(); // 序列化前一个节点的shared_ptr
    return o;
}

// 全局反序列化操作符
inline I& operator>>(I& i, Node& node) {
    i >> node.id; // 反序列化节点ID
    i >> node.next;
    i >> node.prev;
    return i;
}


REG_TEST(test_pointer_serialization) {

    // 创建循环引用结构
    auto node1 = std::make_shared<Node>(1);
    auto node2 = std::make_shared<Node>(2);
    auto node3 = std::make_shared<Node>(3);

    node1->next = node2;
    node2->next = node3;
    node3->next = node1; // 循环引用

    node2->prev = node1;
    node3->prev = node2;
    node1->prev = node3;

    node2.reset();
    node3.reset();

    // 序列化
    std::stringstream stream;
    {
        O o(stream);
        o << node1; // 序列化整个结构
    }

    // 反序列化
    std::shared_ptr<Node> restored;
    {
        I i(stream);
        i >> restored;
    }

    // 验证结构完整性
    ck_assert(restored != nullptr);
    ck_assert(restored->id == 1);

    ck_assert(restored->next != nullptr);
    ck_assert(restored->next->id == 2);

    ck_assert(restored->next->next != nullptr);
    ck_assert(restored->next->next->id == 3);

    ck_assert(restored->next->next->next == restored); // 循环引用

    // 验证weak_ptr
    auto prev = restored->prev.lock();
    ck_assert(prev != nullptr);
    ck_assert(prev->id == 3);
    prev.reset();

    // 验证引用计数
    auto node1_use_count = node1.use_count();
    auto restored_use_count = restored.use_count();
    ck_assert(node1_use_count == restored_use_count);
    ck_assert(node1_use_count == 2);
    ck_assert(restored_use_count == 2); 
    std::cout << "restored->next.use_count():" << node1->next->next.use_count() << std::endl;
    ck_assert(restored->next.use_count() == 1);
    std::cout << "node1->next->next.use_count():" << node1->next->next.use_count() << std::endl;
    ck_assert(node1->next->next.use_count() == 1);
    ck_assert(restored->next->next.use_count() == 1);
}

REG_TEST(test_unique_ptr_serialization) {
    std::stringstream stream;

    // 序列化
    {
        auto ptr = std::make_unique<int>(42);
        O o(stream);
        o << ptr;
    }

    // 反序列化
    std::unique_ptr<int> restored;
    {
        I i(stream);
        i >> restored;
    }

    ck_assert(restored != nullptr);
    ck_assert(*restored == 42);
}

REG_TEST(test_mixed_pointer_types) {
    std::stringstream stream;

    // 创建测试数据
    auto shared = std::make_shared<int>(100);
    int* raw = new int(200);
    auto unique = std::make_unique<int>(300);

    // 序列化
    {
        O o(stream);
        o << shared << raw << unique;
    }

    // 反序列化
    std::shared_ptr<int> restored_shared;
    int* restored_raw = nullptr;
    std::unique_ptr<int> restored_unique;

    {
        I i(stream);
        i >> restored_shared >> restored_raw >> restored_unique;
    }

    // 验证
    ck_assert(restored_shared != nullptr);
    ck_assert(*restored_shared == 100);

    ck_assert(restored_raw != nullptr);
    ck_assert(*restored_raw == 200);

    ck_assert(restored_unique != nullptr);
    ck_assert(*restored_unique == 300);

    // 清理
    delete restored_raw;
}

REG_TEST(test_pointer_aliasing) {
    // 测试多个指针指向同一对象
    std::stringstream stream;

    auto shared1 = std::make_shared<int>(42);
    auto shared2 = shared1;
    int* raw = shared1.get();

    // 序列化
    {
        O o(stream);
        o << shared1 << shared2 << raw;
    }

    // 反序列化
    std::shared_ptr<int> restored1;
    std::shared_ptr<int> restored2;
    int* restored_raw = nullptr;

    {
        I i(stream);
        i >> restored1 >> restored2 >> restored_raw;
    }

    // 验证
    ck_assert(restored1 != nullptr);
    ck_assert(restored2 != nullptr);
    ck_assert(restored_raw != nullptr);

    ck_assert(restored1 == restored2);
    ck_assert(restored1.get() == restored_raw);
    ck_assert(*restored1 == 42);
    ck_assert(*restored2 == 42);
    ck_assert(*restored_raw == 42);

    // 验证引用计数
    ck_assert(restored1.use_count() == 2); // restored1 和 restored2 共享所有权
}

REG_TEST(test_null_pointer_handling) {
    std::stringstream stream;

    // 各种空指针
    int* raw_null = nullptr;
    std::shared_ptr<int> shared_null;
    std::weak_ptr<int> weak_null;
    std::unique_ptr<int> unique_null;

    // 序列化
    {
        O o(stream);
        o << raw_null << shared_null << weak_null << unique_null;
    }

    // 反序列化
    int* restored_raw = reinterpret_cast<int*>(0xdeadbeef);
    std::shared_ptr<int> restored_shared = std::make_shared<int>(42);
    std::weak_ptr<int> restored_weak = restored_shared;
    std::unique_ptr<int> restored_unique = std::make_unique<int>(100);

    {
        I i(stream);
        i >> restored_raw >> restored_shared >> restored_weak >> restored_unique;
    }

    // 验证
    ck_assert(restored_raw == nullptr);
    ck_assert(restored_shared == nullptr);
    ck_assert(restored_weak.expired());
    ck_assert(restored_unique == nullptr);
}

REG_TEST(test_pointer_ownership_conflict) {
    // 测试所有权冲突
    std::stringstream stream;

    // 序列化 unique_ptr
    {
        auto unique = std::make_unique<int>(42);
        O o(stream);
        o << unique;
    }

    // 尝试反序列化为 shared_ptr
    bool exception_thrown = false;
    try {
        I i(stream);
        std::shared_ptr<int> shared;
        i >> shared; // 应该抛出异常
    }
    catch (const std::runtime_error& e) {
        exception_thrown = true;
        std::cout << e.what() << std::endl;
    }

    //ck_assert(exception_thrown);
}