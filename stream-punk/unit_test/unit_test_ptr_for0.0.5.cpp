#include "unit_test.hpp"
# include "Data.hpp"


REG_TEST(testBasicTypes) {
    // 创建各种基本类型数据
    u8 a = 10;
    u16 b = 65535;
    i32 c = -123456;
    f64 d = 3.1415926535;
    bl e = true;
    ch f = 'X';

    // 序列化
    std::stringstream ss;
    O o{ ss };
    o << a << b << c << d << e << f;

    // 反序列化
    u8 a2;
    u16 b2;
    i32 c2;
    f64 d2;
    bl e2;
    ch f2;
    I i{ ss };
    i >> a2 >> b2 >> c2 >> d2 >> e2 >> f2;

    // 验证
    ck_assert(a == a2);
    ck_assert(b == b2);
    ck_assert(c == c2);
    ck_assert(d == d2);
    ck_assert(e == e2);
    ck_assert(f == f2);
}

REG_TEST(testContainers) {
    // 创建容器数据
    std::vector<i32> vec = { 1, 2, 3, 4, 5 };
    std::map<std::string, u32> map = { {"one", 1}, {"two", 2}, {"three", 3} };
    std::unordered_set<std::string> set = { "apple", "banana", "cherry" };
    std::bitset<32> bits(0xDEADBEEF);

    // 序列化
    std::stringstream ss;
    O o{ ss };
    o << vec << map << set << bits;

    // 反序列化
    std::vector<i32> vec2;
    std::map<std::string, u32> map2;
    std::unordered_set<std::string> set2;
    std::bitset<32> bits2;
    I i{ ss };
    i >> vec2 >> map2 >> set2 >> bits2;

    // 验证
    ck_assert(vec == vec2);
    ck_assert(map == map2);
    ck_assert(set == set2);
    ck_assert(bits == bits2);
}

REG_TEST(unit_test_PointerDemo) {
    // 准备源对象数据
    auto testObj = std::make_unique<Test>();
    testObj->name = "test1";

    auto mqttObj = std::make_shared<MQTT>();
    mqttObj->host = "mqtt.example.com";

    // 创建原始对象 p1
    auto p1 = std::make_shared<PointerDemo>();
    p1->rawPtr = testObj.get();
    p1->sharedPtr = mqttObj;
    p1->uniquePtr = std::make_unique<Test>();
    p1->uniquePtr->name = "unique_test";
    p1->weakSelf = p1;  // 自引用

    // 验证初始状态
    ck_assert(p1->rawPtr == testObj.get());
    ck_assert(p1->sharedPtr->host == "mqtt.example.com");
    ck_assert(p1->uniquePtr != nullptr);
    ck_assert(p1->uniquePtr->name == "unique_test");
    ck_assert(!p1->weakSelf.expired());
    ck_assert(p1->weakSelf.lock().get() == p1.get());

    // ===== 测试移动构造函数 =====
    PointerDemo p2(std::move(*p1));  // 移动构造

    // 验证移动后目标对象
    ck_assert(p2.rawPtr == testObj.get());
    ck_assert(p2.sharedPtr == mqttObj);
    ck_assert(p2.sharedPtr.use_count() == 2); // 原始引用 + p2 的引用
    ck_assert(p2.uniquePtr->name == "unique_test");

    // 自引用这时就会有问题 需要定义这个类的人自己写移动构造函数
    //p2.weakSelf = p2;
    //ck_assert(p2.weakSelf.lock().get() == &p2);

    // 验证移动后源对象情况
        // 默认移动时 原生函数不会被置空
    ck_assert(p1->rawPtr != nullptr);
    ck_assert(p1->sharedPtr == nullptr);
    ck_assert(p1->uniquePtr == nullptr);
    ck_assert(p1->weakSelf.expired());

    // ===== 测试移动赋值运算符 =====
    PointerDemo p3;
    p3 = std::move(p2);  // 移动赋值

    // 验证移动后目标对象
    ck_assert(p3.rawPtr == testObj.get());
    ck_assert(p3.sharedPtr == mqttObj);
    ck_assert(p3.sharedPtr.use_count() == 2); // 原始引用 + p3 的引用
    ck_assert(p3.uniquePtr->name == "unique_test");

    // 栈分配的 不用更新自引用
    //p3.weakSelf = std::shared_ptr<PointerDemo>(&p3, [](auto) {});
    //ck_assert(p3.weakSelf.lock().get() == &p3);

    // 验证移动后源对象被置空
        // 默认移动时 原生函数不会被置空
    ck_assert(p2.rawPtr != nullptr);
    ck_assert(p2.sharedPtr == nullptr);
    ck_assert(p2.uniquePtr == nullptr);
    ck_assert(p2.weakSelf.expired());

    // 确保原始数据对象未受影响
    ck_assert(testObj->name == "test1");
    ck_assert(mqttObj->host == "mqtt.example.com");

    // 验证智能指针计数最终状态
    ck_assert(mqttObj.use_count() == 2); // 原始引用 + p3 的引用
}

REG_TEST(testPointerTypes) {
    // 创建指针数据
    auto testPtr = std::make_shared<Test>();
    testPtr->name = "Test Object";
    testPtr->ip = "192.168.1.1";

    auto mqttPtr = std::make_shared<MQTT>();
    mqttPtr->host = "mqtt.example.com";
    mqttPtr->user = "admin";

    auto demoPtr = std::make_shared<PointerDemo>();
    demoPtr->rawPtr = testPtr.get();
    demoPtr->sharedPtr = mqttPtr;
    demoPtr->uniquePtr = std::make_unique<Test>();
    demoPtr->uniquePtr->name = "Unique Test";
    demoPtr->weakSelf = demoPtr; // 自引用

    // 序列化
    std::stringstream ss;
    O o{ ss };
    o << demoPtr;

    // 反序列化
    Sptr<PointerDemo> demoPtr2;
    I i{ ss };
    i >> demoPtr2;

    // 验证
    ck_assert(demoPtr2);
    ck_assert(demoPtr2->rawPtr != demoPtr->rawPtr); // 地址不同
    ck_assert(demoPtr2->rawPtr->name == "Test Object");

    ck_assert(demoPtr2->sharedPtr);
    ck_assert(demoPtr2->sharedPtr->host == "mqtt.example.com");

    ck_assert(demoPtr2->uniquePtr);
    ck_assert(demoPtr2->uniquePtr->name == "Unique Test");

    // 验证自引用
    auto self = demoPtr2->weakSelf.lock();
    ck_assert(self == demoPtr2);
}

REG_TEST(testContainerWithPointers) {

    // 创建容器包含指针的数据
    auto container = std::make_shared<ContainerDemo>();

    // 添加Test指针
    auto test1 = std::make_shared<Test>();
    test1->name = "Test 1";
    container->testPtrs.push_back(test1);

    auto test2 = std::make_shared<Test>();
    test2->name = "Test 2";
    container->testPtrs.push_back(test2);

    // 添加MQTT配置
    auto mqtt1 = std::make_unique<MQTT>();
    mqtt1->host = "broker1.example.com";
    container->mqttConfigs["primary"] = std::move(mqtt1);

    auto mqtt2 = std::make_unique<MQTT>();
    mqtt2->host = "broker2.example.com";
    container->mqttConfigs["secondary"] = std::move(mqtt2);

    // 添加原始指针
    container->allObjects.insert(test1.get());
    container->allObjects.insert(test2.get());

    // 自引用
    container->selfContainer = container;

    // 序列化
    std::stringstream ss;
    O o{ ss };
    o << container;

    // 反序列化
    Sptr<ContainerDemo> container2;
    I i{ ss };
    i >> container2;

    // 验证
    ck_assert(container2);
    ck_assert(container2->testPtrs.size() == 2);
    ck_assert(container2->testPtrs[0]->name == "Test 1");
    ck_assert(container2->testPtrs[1]->name == "Test 2");

    ck_assert(container2->mqttConfigs.size() == 2);
    ck_assert(container2->mqttConfigs["primary"]->host == "broker1.example.com");
    ck_assert(container2->mqttConfigs["secondary"]->host == "broker2.example.com");

    ck_assert(container2->allObjects.size() == 2);
    for (auto ptr : container2->allObjects) {
        ck_assert(dynamic_cast<Test*>(ptr));
    }

    // 验证自引用
    ck_assert(container2->selfContainer == container2);
}

REG_TEST(testComplexNestedStructure) {
    // 创建复杂嵌套结构
    auto system = std::make_shared<NetworkSystem>();

    // 创建主容器
    system->mainContainer = std::make_shared<ContainerDemo>();

    // 添加活动测试
    auto activeTest = std::make_shared<Test>();
    activeTest->name = "Active Test";
    system->activeTests.push_back(activeTest);

    // 添加MQTT实例
    auto mqtt1 = std::make_unique<MQTT>();
    mqtt1->host = "system.broker.com";
    system->mqttInstances.push_back(std::move(mqtt1));

    // 添加演示对象
    auto demo1 = std::make_shared<PointerDemo>();
    demo1->uniquePtr = std::make_unique<Test>();
    demo1->uniquePtr->name = "Demo Test";
    system->demos.push_back(demo1);

    // 序列化
    std::stringstream ss;
    O o{ ss };
    o << system;

    // 反序列化
    Sptr<NetworkSystem> system2;
    I i{ ss };
    i >> system2;

    // 验证
    ck_assert(system2);
    ck_assert(system2->mainContainer);
    ck_assert(system2->activeTests.size() == 1);
    ck_assert(system2->activeTests[0].lock()->name == "Active Test");
    ck_assert(system2->mqttInstances.size() == 1);
    ck_assert(system2->mqttInstances.front()->host == "system.broker.com");
    ck_assert(system2->demos.size() == 1);
    ck_assert(system2->demos.front()->uniquePtr->name == "Demo Test");
}

REG_TEST(testChronoSupport) {
    using namespace std::chrono;

    // 创建时间数据
    auto duration = 123s + 456ms;
    auto timePoint = system_clock::now();

    // 序列化
    std::stringstream ss;
    O o{ ss };
    o << duration << timePoint;

    // 反序列化
    milliseconds duration2;
    system_clock::time_point timePoint2;
    I i{ ss };
    i >> duration2 >> timePoint2;

    // 验证
    ck_assert(duration_cast<milliseconds>(duration) == duration2);

    // 由于序列化/反序列化可能会有微小精度损失，所以比较在微秒内相等
    auto diff = duration_cast<microseconds>(timePoint - timePoint2);
    ck_assert(std::abs(diff.count()) < 100);
}