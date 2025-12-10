# include "unit_test.hpp"
# include "Data.hpp"

using namespace std;

// 测试基本类型深拷贝
REG_TEST(test_primitives) {
    DeepCopier dc;

    u8 a = 10, a_copy;
    deepCopy(dc, a_copy, a);
    ck_assert_int_eq(a_copy, 10);

    f64 b = 3.14159, b_copy;
    deepCopy(dc, b_copy, b);
    ck_assert_double_eq(b_copy, 3.14159/*,0.00001*/);
}

// 容器测试（vector/list/map）
REG_TEST(test_containers) {
    DeepCopier dc;

    // Vector
    vector<u32> nums = { 1, 2, 3, 4 };
    vector<u32> nums_copy;
    deepCopy(dc, nums_copy, nums);
    ck_assert_uint_eq(nums_copy.size(), 4);
    ck_assert_int_eq(nums_copy[0], 1);
    ck_assert_int_eq(nums_copy[3], 4);

    // List
    list<string> strings = { "one", "two" };
    list<string> strings_copy;
    deepCopy(dc, strings_copy, strings);
    ck_assert_uint_eq(strings_copy.size(), 2);
    ck_assert_str_eq(strings_copy.front().c_str(), "one");

    // Map
    map<string, u16> kv = { {"a", 1}, {"b", 2} };
    map<string, u16> kv_copy;
    deepCopy(dc, kv_copy, kv);
    ck_assert_uint_eq(kv_copy.size(), 2);
    ck_assert_int_eq(kv_copy.at("a"), 1);
}

// 智能指针基本测试
REG_TEST(test_smart_pointers) {
    DeepCopier dc;

    // Shared pointer
    auto sptr_orig = make_shared<u32>(42);
    shared_ptr<u32> sptr_copy;
    deepCopy(dc, sptr_copy, sptr_orig);
    ck_assert_ptr_nonnull(sptr_copy.get());
    ck_assert_int_eq(*sptr_copy, 42);

    // Unique pointer
    auto uptr_orig = make_unique<f64>(2.718);
    unique_ptr<f64> uptr_copy;
    deepCopy(dc, uptr_copy, uptr_orig);
    ck_assert_ptr_nonnull(uptr_copy.get());
    ck_assert_double_eq(*uptr_copy, 2.718/*, 0.0001*/);
}

inline void asdfasdfas(DeepCopier& dc, std::map<Base*, int>& dstV, std::map<Base*, int>const& srcV) {
    //detail::deepCopyMap(dc, dstV, srcV);
    if (std::addressof(dstV) == std::addressof(srcV)) {
        return;
    }
    dstV.clear();
    auto hint = dstV.cend();
    for (const auto& [srcKey, srcVal] : srcV) {
        Base* newKey;
        deepCopy(dc, newKey, srcKey);
        typename std::map<Base*, int>::value_type::second_type newVal;
        deepCopy(dc, newVal, srcVal);
        hint = dstV.emplace_hint(hint, std::move(newKey), std::move(newVal));
    }
}


// 自赋值和空指针测试
REG_TEST(test_deepCopy_edge_cases) {
    DeepCopier dc;

    // 自赋值测试
    auto self_ptr = make_shared<i32>(100);
    deepCopy(dc, self_ptr, self_ptr);
    ck_assert_int_eq(*self_ptr, 100);

    // 空指针测试
    shared_ptr<f64> null_sptr;
    shared_ptr<f64> null_copy;
    deepCopy(dc, null_copy, null_sptr);
    ck_assert_ptr_null(null_copy.get());

    // 空容器测试
    vector<u8> empty_vec;
    vector<u8> empty_copy;
    deepCopy(dc, empty_copy, empty_vec);
    ck_assert_uint_eq(empty_copy.size(), 0);
}

// DeepCopier内部状态测试
REG_TEST(test_deepcopier_state) {
    DeepCopier dc;

    auto ptr1 = make_shared<u32>(10);
    auto ptr2 = make_shared<u32>(20);

    // 第一次深拷贝
    shared_ptr<u32> copy1;
    deepCopy(dc, copy1, ptr1);
    ck_assert_int_eq(*copy1, 10);

    // 第二次深拷贝应该使用缓存
    shared_ptr<u32> copy1a;
    deepCopy(dc, copy1a, ptr1);
    ck_assert_ptr_eq(copy1.get(), copy1a.get());

    // 不同的指针
    shared_ptr<u32> copy2;
    deepCopy(dc, copy2, ptr2);
    ck_assert_int_eq(*copy2, 20);
    ck_assert_ptr_ne(copy1.get(), copy2.get());

    // 清除后重新拷贝
    dc.clear();
    shared_ptr<u32> copy1b;
    deepCopy(dc, copy1b, ptr1);
    ck_assert_ptr_ne(copy1.get(), copy1b.get());
}


// 测试基础类型深拷贝
REG_TEST(test_deep_copy_basic_types) {
    // Arrange
    u8 srcU8 = 42;
    i32 srcI32 = -123;
    f64 srcF64 = 3.14159;
    std::string srcStr = "Hello, DeepCopy!";
    bl srcBool = true;

    u8 dstU8;
    i32 dstI32;
    f64 dstF64;
    std::string dstStr;
    bl dstBool;

    DeepCopier dc;

    // Act
    deepCopy(dc, dstU8, srcU8);
    deepCopy(dc, dstI32, srcI32);
    deepCopy(dc, dstF64, srcF64);
    deepCopy(dc, dstStr, srcStr);
    deepCopy(dc, dstBool, srcBool);

    // Assert
    ck_assert_uint_eq(srcU8, dstU8);
    ck_assert_int_eq(srcI32, dstI32);
    ck_assert(dstF64 == srcF64);
    ck_assert_str_eq(srcStr.c_str(), dstStr.c_str());
    ck_assert(dstBool);
}

// 测试容器深拷贝
REG_TEST(test_deep_copy_containers) {
    // Arrange
    std::vector<int> srcVec = { 1, 2, 3, 4, 5 };
    std::list<std::string> srcList = { "apple", "banana", "cherry" };
    std::map<int, std::string> srcMap = { {1, "one"}, {2, "two"}, {3, "three"} };

    std::vector<int> dstVec;
    std::list<std::string> dstList;
    std::map<int, std::string> dstMap;

    DeepCopier dc;

    // Act
    deepCopy(dc, dstVec, srcVec);
    deepCopy(dc, dstList, srcList);
    deepCopy(dc, dstMap, srcMap);

    // Assert
    ck_assert_uint_eq(srcVec.size(), dstVec.size());
    for (size_t i = 0; i < srcVec.size(); ++i) {
        ck_assert_int_eq(srcVec[i], dstVec[i]);
    }

    ck_assert_uint_eq(srcList.size(), dstList.size());
    auto srcIt = srcList.begin();
    auto dstIt = dstList.begin();
    while (srcIt != srcList.end()) {
        ck_assert_str_eq(srcIt->c_str(), dstIt->c_str());
        ++srcIt;
        ++dstIt;
    }

    ck_assert_uint_eq(srcMap.size(), dstMap.size());
    for (const auto& [key, value] : srcMap) {
        auto it = dstMap.find(key);
        ck_assert(it != dstMap.end());
        ck_assert_str_eq(value.c_str(), it->second.c_str());
    }
}

// 测试智能指针深拷贝
REG_TEST(test_deep_copy_smart_pointers) {
    // Arrange
    auto srcSptr = std::make_shared<int>(42);
    auto srcUptr = std::make_unique<double>(3.14);
    auto srcShared = std::make_shared<Test>();
    srcShared->name = "Test Object";

    Sptr<int> dstSptr;
    Uptr<double> dstUptr;
    Sptr<Test> dstShared;

    DeepCopier dc;

    // Act
    deepCopy(dc, dstSptr, srcSptr);
    deepCopy(dc, dstUptr, srcUptr);
    deepCopy(dc, dstShared, srcShared);

    // Assert
    ck_assert(dstSptr.get() != nullptr);
    ck_assert_int_eq(*srcSptr, *dstSptr);
    ck_assert_ptr_ne(srcSptr.get(), dstSptr.get());

    ck_assert(dstUptr.get() != nullptr);
    ck_assert(dstUptr != srcUptr);

    ck_assert(dstShared.get()!=nullptr);
    ck_assert_str_eq(srcShared->name.c_str(), dstShared->name.c_str());
    ck_assert_ptr_ne(srcShared.get(), dstShared.get());
}

// 测试自定义类深拷贝
REG_TEST(test_deep_copy_custom_classes) {
    // Arrange
    Test srcTest;
    srcTest.name = "Test Object";
    srcTest.pwd = "secret";
    srcTest.gateWay = "192.168.1.1";

    MQTT srcMqtt;
    srcMqtt.host = "mqtt.example.com";
    srcMqtt.user = "admin";
    srcMqtt.pwd = "mqttpassword";

    Test dstTest;
    MQTT dstMqtt;

    DeepCopier dc;

    // Act
    deepCopy(dc, dstTest, srcTest);
    deepCopy(dc, dstMqtt, srcMqtt);

    // Assert
    ck_assert_str_eq(srcTest.name.c_str(), dstTest.name.c_str());
    ck_assert_str_eq(srcTest.pwd.c_str(), dstTest.pwd.c_str());
    ck_assert_str_eq(srcTest.gateWay.c_str(), dstTest.gateWay.c_str());

    ck_assert_str_eq(srcMqtt.host.c_str(), dstMqtt.host.c_str());
    ck_assert_str_eq(srcMqtt.user.c_str(), dstMqtt.user.c_str());
    ck_assert_str_eq(srcMqtt.pwd.c_str(), dstMqtt.pwd.c_str());
}

// 测试指针结构深拷贝
REG_TEST(test_deep_copy_pointer_demo) {
    // Arrange
    auto testObj = std::make_shared<Test>();
    testObj->name = "Pointer Target";

    auto src = std::make_shared<PointerDemo>();
    src->rawPtr = testObj.get();
    src->sharedPtr = std::make_shared<MQTT>();
    src->sharedPtr->host = "mqtt.server";
    src->uniquePtr = std::make_unique<Test>();
    src->uniquePtr->name = "Unique Object";

    // 创建自引用weak_ptr
    src->weakSelf = src;

    Sptr<PointerDemo> dst;

    DeepCopier dc;

    // Act
    deepCopy(dc, dst, src);

    // Assert
    // 验证原始指针被深拷贝
    ck_assert(dst->rawPtr!=nullptr);
    ck_assert_str_eq(testObj->name.c_str(), dst->rawPtr->name.c_str());
    ck_assert_ptr_ne(src->rawPtr, dst->rawPtr);

    // 验证shared_ptr被深拷贝
    ck_assert(dst->sharedPtr!=nullptr);
    ck_assert_str_eq(src->sharedPtr->host.c_str(), dst->sharedPtr->host.c_str());
    ck_assert_ptr_ne(src->sharedPtr.get(), dst->sharedPtr.get());

    // 验证unique_ptr被深拷贝
    ck_assert(dst->uniquePtr!=nullptr);
    ck_assert_str_eq(src->uniquePtr->name.c_str(), dst->uniquePtr->name.c_str());

    // 验证weak_ptr关系被保留
    ck_assert(!dst->weakSelf.expired());
    auto dstPtr = dst.get();
    auto dstSelfPtr = dst->weakSelf.lock().get();
    ck_assert_ptr_eq(dstPtr, dstSelfPtr);
}

// 测试复杂容器深拷贝
REG_TEST(test_deep_copy_container_demo) {
    // Arrange
    ContainerDemo src;

    // 创建共享指针向量
    for (int i = 0; i < 3; ++i) {
        auto testObj = std::make_shared<Test>();
        testObj->name = "Test Object " + std::to_string(i);
        src.testPtrs.push_back(testObj);
    }

    // 添加自引用容器
    src.selfContainer = std::make_shared<ContainerDemo>();

    // 添加原始指针到集合
    Test rawTest;
    rawTest.name = "Raw Test Object";
    src.allObjects.insert(&rawTest);

    // 添加MQTT配置
    src.mqttConfigs["server1"] = std::make_unique<MQTT>();
    src.mqttConfigs["server1"]->host = "mqtt1.example.com";

    ContainerDemo dst;

    DeepCopier dc;

    // Act
    deepCopy(dc, dst, src);

    // Assert
    // 验证共享指针向量
    ck_assert_uint_eq(src.testPtrs.size(), dst.testPtrs.size());
    for (size_t i = 0; i < src.testPtrs.size(); ++i) {
        ck_assert(dst.testPtrs[i]!=nullptr);
        ck_assert_str_eq(src.testPtrs[i]->name.c_str(),
            dst.testPtrs[i]->name.c_str());
        ck_assert_ptr_ne(src.testPtrs[i].get(), dst.testPtrs[i].get());
    }

    // 验证自引用容器
    ck_assert(dst.selfContainer!=nullptr);
    ck_assert_ptr_ne(src.selfContainer.get(), dst.selfContainer.get());

    // 验证原始指针集合
    ck_assert_uint_eq(src.allObjects.size(), dst.allObjects.size());
    for (auto ptr : dst.allObjects) {
        ck_assert(ptr!=nullptr);
        ck_assert_str_eq("Raw Test Object", dynamic_cast<Test*>(ptr)->name.c_str());
    }

    // 验证MQTT配置
    auto it = dst.mqttConfigs.find("server1");
    ck_assert(it != dst.mqttConfigs.end());
    ck_assert_str_eq("mqtt1.example.com", it->second->host.c_str());
}

// 测试多级嵌套深拷贝
REG_TEST(test_deep_copy_network_system) {
    // Arrange
    NetworkSystem src;

    // 添加主容器
    src.mainContainer = std::make_shared<ContainerDemo>();
    src.mainContainer->testPtrs.push_back(std::make_shared<Test>());

    // 添加活跃测试
    auto activeTest = std::make_shared<Test>();
    activeTest->name = "Active Test";
    src.activeTests.push_back(activeTest);

    // 添加MQTT实例
    src.mqttInstances.push_back(std::make_unique<MQTT>());
    src.mqttInstances.front()->host = "mqtt.network";

    // 添加指针演示对象
    auto demo = std::make_shared<PointerDemo>();
    demo->uniquePtr = std::make_unique<Test>();
    demo->uniquePtr->name = "Nested Unique";
    src.demos.push_back(demo);

    NetworkSystem dst;

    DeepCopier dc;

    // Act
    deepCopy(dc, dst, src);

    // Assert
    // 验证主容器
    ck_assert(dst.mainContainer!=nullptr);
    ck_assert_uint_eq(1, dst.mainContainer->testPtrs.size());

    // 验证活跃测试
    ck_assert_uint_eq(1, dst.activeTests.size());
    ck_assert(!dst.activeTests.front().expired());
    auto lockedActive = dst.activeTests.front().lock();
    ck_assert_str_eq("Active Test", lockedActive->name.c_str());

    // 验证MQTT实例
    ck_assert_uint_eq(1, dst.mqttInstances.size());
    ck_assert(dst.mqttInstances.front()!=nullptr);
    ck_assert_str_eq("mqtt.network", dst.mqttInstances.front()->host.c_str());

    // 验证指针演示对象
    ck_assert_uint_eq(1, dst.demos.size());
    auto demoCopy = dst.demos.front();
    ck_assert(demoCopy!=nullptr);
    ck_assert(demoCopy->uniquePtr!=nullptr);
    ck_assert_str_eq("Nested Unique", demoCopy->uniquePtr->name.c_str());
}

// 测试设备继承体系深拷贝
REG_TEST(test_deep_copy_device_inheritance) {
    // Arrange
    TemperatureSensor srcSensor;
    srcSensor.deviceId = "temp-sensor-001";
    srcSensor.currentValue = 25.5;
    srcSensor.isCelsius = true;

    TemperatureSensor dstSensor;

    DeepCopier dc;

    // Act
    deepCopy(dc, dstSensor, srcSensor);

    // Assert
    ck_assert_str_eq(srcSensor.deviceId.c_str(), dstSensor.deviceId.c_str());
    ck_assert(srcSensor.currentValue == dstSensor.currentValue);
    ck_assert(srcSensor.isCelsius == dstSensor.isCelsius);
}

// 测试多态深拷贝
REG_TEST(test_deep_copy_polymorphism) {
    // Arrange
    auto srcSensor = std::make_shared<TemperatureSensor>();
    srcSensor->deviceId = "temp-sensor-poly";
    srcSensor->currentValue = 30.0;

    Sptr<Base> srcBase = srcSensor;
    Sptr<Base> dstBase;

    DeepCopier dc;

    // Act
    deepCopy(dc, dstBase, srcBase);

    // Assert
    ck_assert(dstBase!=nullptr);
    ck_assert(typeid(*dstBase) == typeid(TemperatureSensor));

    auto dstSensor = std::dynamic_pointer_cast<TemperatureSensor>(dstBase);
    ck_assert_str_eq(srcSensor->deviceId.c_str(), dstSensor->deviceId.c_str());
    ck_assert(srcSensor->currentValue == dstSensor->currentValue);
}

// 测试复杂系统深拷贝
REG_TEST(test_deep_copy_complex_systems) {
    // Arrange
    SmartHomeSystem srcSystem;
    srcSystem.mainThermostat = std::make_shared<TemperatureSensor>();
    srcSystem.mainThermostat->deviceId = "main-thermostat";

    srcSystem.sensors["living-room"] = std::make_shared<TemperatureSensor>();
    srcSystem.sensors["living-room"]->currentValue = 22.5;

    SmartHomeSystem dstSystem;

    DeepCopier dc;

    // Act
    deepCopy(dc, dstSystem, srcSystem);

    // Assert
    // 验证主恒温器
    ck_assert(dstSystem.mainThermostat!=nullptr);
    ck_assert_str_eq("main-thermostat", dstSystem.mainThermostat->deviceId.c_str());

    // 验证传感器映射
    ck_assert_uint_eq(1, dstSystem.sensors.size());
    auto it = dstSystem.sensors.find("living-room");
    ck_assert(it != dstSystem.sensors.end());
    ck_assert(it->second->currentValue == 22.5);

    // 验证网络系统
    ck_assert(dstSystem.network == nullptr);
}

// 测试自引用和循环引用
REG_TEST(test_deep_copy_circular_references) {
    // Arrange
    auto container1 = std::make_shared<ContainerDemo>();
    auto container2 = std::make_shared<ContainerDemo>();

    // 创建循环引用
    container1->selfContainer = container2;
    container2->selfContainer = container1;

    Sptr<ContainerDemo> dstContainer1;
    Sptr<ContainerDemo> dstContainer2;

    DeepCopier dc;

    // Act
    deepCopy(dc, dstContainer1, container1);
    deepCopy(dc, dstContainer2, container2);

    // Assert
    ck_assert(dstContainer1!=nullptr);
    ck_assert(dstContainer2!=nullptr);

    // 验证循环引用被正确复制
    ck_assert(dstContainer1->selfContainer!=nullptr);
    ck_assert(dstContainer2->selfContainer!=nullptr);

    // 验证新容器与原始容器不同
    ck_assert_ptr_ne(container1.get(), dstContainer1.get());
    ck_assert_ptr_ne(container2.get(), dstContainer2.get());

    // 验证新容器之间的循环关系
    ck_assert_ptr_eq(dstContainer1->selfContainer.get(), dstContainer2.get());
    ck_assert_ptr_eq(dstContainer2->selfContainer.get(), dstContainer1.get());
}

// 测试空指针深拷贝
REG_TEST(test_deep_copy_null_pointers) {
    // Arrange
    Test* srcRawPtr = nullptr;
    Sptr<Test> srcSptr;
    Uptr<Test> srcUptr;
    Wptr<Test> srcWptr;

    Test* dstRawPtr = nullptr;
    Sptr<Test> dstSptr;
    Uptr<Test> dstUptr;
    Wptr<Test> dstWptr;

    DeepCopier dc;

    // Act
    deepCopy(dc, dstRawPtr, srcRawPtr);
    deepCopy(dc, dstSptr, srcSptr);
    deepCopy(dc, dstUptr, srcUptr);
    deepCopy(dc, dstWptr, srcWptr);

    // Assert
    ck_assert_ptr_null(dstRawPtr);
    ck_assert(!dstSptr);
    ck_assert(!dstUptr);
    ck_assert(dstWptr.expired());
}

// 测试多级容器深拷贝
REG_TEST(test_deep_copy_multilevel_containers) {
    // Arrange
    MultiLevelContainer src;
    src.baseObj = std::make_shared<Test>();

    // 添加设备
    for (int i = 0; i < 2; ++i) {
        auto device = std::make_shared<Device>();
        device->deviceId = "device-" + std::to_string(i);
        src.deviceList.push_back(device);
    }

    // 添加传感器
    auto sensor = std::make_shared<TemperatureSensor>();
    sensor->deviceId = "sensor-001";
    src.sensorMap["kitchen"] = sensor;

    // 添加自引用
    src.selfRef = std::make_shared<MultiLevelContainer>();

    MultiLevelContainer dst;

    DeepCopier dc;

    // Act
    deepCopy(dc, dst, src);

    // Assert
    // 验证基础对象
    ck_assert(dst.baseObj!=nullptr);
    ck_assert(typeid(*dst.baseObj) == typeid(Test));

    // 验证设备列表
    ck_assert_uint_eq(2, dst.deviceList.size());
    for (size_t i = 0; i < dst.deviceList.size(); ++i) {
        ck_assert(dst.deviceList[i]!=nullptr);
        std::string s("device-");
        s += std::to_string(i);
        ck_assert_str_eq(s.c_str(), dst.deviceList[i]->deviceId.c_str());
    }

    // 验证传感器映射
    auto it = dst.sensorMap.find("kitchen");
    ck_assert(it != dst.sensorMap.end());
    ck_assert_str_eq("sensor-001", it->second->deviceId.c_str());

    // 验证自引用
    ck_assert(dst.selfRef!=nullptr);
    ck_assert_ptr_ne(src.selfRef.get(), dst.selfRef.get());
}



