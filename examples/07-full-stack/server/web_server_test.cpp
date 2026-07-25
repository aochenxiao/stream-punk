#include "web_server.hpp"
#include <cmath>
#include <random>

// 原有的全量测试
std::string test1() {
    AllBasicTypes d1;
    d1.b = true;
    d1.i8_v = -127;
    d1.u8_v = 255;
    d1.i16_v = -32767;
    d1.u16_v = 65535;
    d1.i32_v = -2147483647;
    d1.u32_v = 4294967295;
    d1.i64_v = -9223372036854775807LL;
    d1.u64_v = 18446744073709551615ULL;
    d1.f = 3.14159f;
    d1.d = 2.718281828459045;
    d1.c = 'A';
    d1.c8 = u8'C';
    d1.c16 = u'D';
    d1.c32 = U'\u0045'; // 'E'

    TemplateContainer d2;
    d2.s = "hello StreamPunk str!";
    d2.u8s = u8"hello StreamPunk u8Str!";
    d2.vec = { 1, 2, 3, 4, 5 };
    d2.deq = { 1.1, 2.2, 3.3, 4.4, 5.5 };
    d2.lst = { "first", "second", "third" };
    d2.shortForwardList = { 100, 200, 300 };
    d2.uintSet = { 11, 22, 33 };
    d2.stringHashSet = { "uno", "dos", "tres" };
    d2.intStringMap = { {1, "eins"}, {2, "zwei"}, {3, "drei"} };
    d2.stringFloatHashMap = { { "a", 1.23f }, { "b", 4.56f }, { "c", 7.89f } };

    PointerContainer d3;
    d3.raw_ptr = new int(123);
    d3.shared_ptr_int = std::make_shared<int>(456);
    d3.unique_ptr_int = std::make_unique<int>(789);

    PointerContainer d3_1;
    d3_1.raw_ptr = d3.raw_ptr;
    d3_1.shared_ptr_int = d3.shared_ptr_int;
    d3_1.unique_ptr_int = std::make_unique<int>(789);

    Child d4;
    d4.b = false;
    d4.c = 'X';
    d4.c8 = u8'Z';
    d4.c16 = u'W';
    d4.c32 = U'\u0056';
    d4.i8_v = -100;
    d4.u8_v = 200;
    d4.i16_v = -30000;
    d4.u16_v = 60000;
    d4.i32_v = -2000000000;
    d4.u32_v = 4000000000;
    d4.i64_v = -9000000000000000000LL;
    d4.u64_v = 18000000000000000000ULL;
    d4.f = 1.2345f;
    d4.d = 5.4321;
    d4.child_field = 999;

    auto d5 = std::make_shared<SelfReferential>();
    d5->self_ptr = d5;

    TemplateAndPointer d6;
    d6.v_raw_ptr.push_back(new int(101));
    d6.v_raw_ptr.push_back(new int(202));
    d6.m_str_shared_ptr["one"] = std::make_shared<int>(1);
    d6.m_str_shared_ptr["two"] = std::make_shared<int>(2);

    auto d7 = std::make_shared<InheritanceAndSelfReference>();
    d7->child_field = 777;
    d7->self_ptr = d7;

    auto d8 = std::make_shared<MegaComplexClass>();
    d8->child_field = 888;
    auto tap1 = std::make_shared<TemplateAndPointer>();
    tap1->v_raw_ptr.push_back(new int(303));
    tap1->m_str_shared_ptr["three"] = std::make_shared<int>(3);
    d8->complex_vector.push_back(tap1);
    d8->raw_self_ref_ptr = new SelfReferential();
    d8->raw_self_ref_ptr->self_ptr = nullptr;
    d8->self_ptr = nullptr;

    SuperComplexContainer d9;
    d9.bits = 0b1010101010101010101010101010101010101010101010101010101010101000;
    d9.opt_int = 42;
    d9.file_path = "C:/temp/test.txt";
    auto test_sptr = std::make_shared<Test>();
    test_sptr->name = "shared_test";
    d9.sptr_test = test_sptr;
    d9.wptr_test = test_sptr;
    d9.uptr_test = std::make_unique<Test>();
    d9.uptr_test->name = "unique_test";
    d9.tuple_member = std::make_tuple(3.14, "tuple_string", true);
    d9.map_of_umaps["key1"][1] = 1.1;
    d9.map_of_umaps["key1"][2] = 2.2;
    auto d9var = std::make_shared<AllBasicTypes>();
    d9var->u8_v = 2;
    d9var->c = 'a';
    d9.variant_member = d9var;
    d9.deq_list_flist = { {{1,2},{3,4}}, {{{5,6}}} };
    d9.vec_arr_str = { {"a","b","c","d","e"}, {"f","g","h","i","j"} };
    d9.set_of_usets = { {"set1_vec1"}, {"set2_vec1", "set2_vec2"} };
    SuperComplexContainer::ComplexTuple inner_tuple;
    std::get<0>(inner_tuple) = std::make_shared<SelfReferential>();
    std::get<1>(inner_tuple) = std::make_unique<PointerContainer>();
    d9.kitchen_sink["main_entry"].push_back(std::move(inner_tuple));

    auto d10 = std::make_shared<PointerDemo>();
    d10->rawPtr = new Test(); d10->rawPtr->name = "raw_test_in_d10";
    d10->sharedPtr = std::make_shared<MQTT>(); d10->sharedPtr->host = "mqtt.example.com";
    d10->uniquePtr = std::make_unique<Test>(); d10->uniquePtr->name = "unique_test_in_d10";
    d10->weakSelf = d10;

    auto d11 = std::make_shared<ContainerDemo>();
    auto test_in_d11 = std::make_shared<Test>(); test_in_d11->name = "test_in_d11";
    d11->testPtrs.push_back(test_in_d11);
    d11->selfContainer = d11;
    d11->mqttConfigs["config1"] = std::make_unique<MQTT>(); d11->mqttConfigs["config1"]->user = "user1";
    d11->mqttConfigs2["config2"] = std::make_unique<MQTT>(); d11->mqttConfigs2["config2"]->user = "user2";

    NetworkSystem d12;
    d12.mainContainer = d11;
    d12.activeTests.push_back(test_in_d11);
    d12.mqttInstances.push_back(std::make_unique<MQTT>()); d12.mqttInstances.back()->host = "mqtt.local";
    d12.demos.push_back(d10);

    auto d13 = std::make_shared<SmartHomeSystem>();
    auto dev1 = std::make_shared<NetworkDevice>();
    dev1->deviceId = "net-001";
    dev1->ipAddress = "192.168.1.1";
    auto dev2 = std::make_shared<TemperatureSensor>();
    dev2->deviceId = "temp-001";
    dev2->currentValue = 25.5;
    d13->allDevices.push_back(dev1);
    d13->allDevices.push_back(dev2);
    d13->sensors["living_room"] = dev2;
    d13->mainThermostat = dev2;

    MultiLevelContainer d14;
    d14.baseObj = std::make_shared<AllBasicTypes>();
    d14.deviceList.push_back(dev1);
    d14.sensorMap["bedroom"] = std::make_shared<Sensor>();
    d14.selfRef = std::make_shared<MultiLevelContainer>();

    std::ostringstream oss;
    SpObjProtocolOutput o{ oss };
    ComplexTemplateNesting d15;
    d15.nestedVectors = { {{1, 2}, {3, 4}}, {{5, 6}} };
    d15.arrayVectors = { std::vector<f32>{1.1f, 2.2f}, std::vector<f32>{3.3f, 4.4f} };
    d15.mapVectors = { {"key1", {10, 20}}, {"key2", {30, 40}} };
    d15.setVecs = { {100, 200}, {300, 400} };

    ComprehensiveContainer d16;
    d16.vec_sptr_all_basic.push_back(std::make_shared<AllBasicTypes>());
    d16.deq_uptr_template_container.push_back(std::make_unique<TemplateContainer>());
    d16.list_string = { "alpha", "beta", "gamma" };
    d16.flist_sptr_complex.push_front(std::make_shared<ComplexTemplateNesting>());
    d16.set_int = { 1, 2, 3 };
    d16.uset_string = { "x", "y", "z" };
    d16.map_str_sptr_all_basic["key"] = std::make_shared<AllBasicTypes>();
    d16.umap_int_uptr_template_container[42] = std::make_unique<TemplateContainer>();
    d16.self_wptr.reset();
    auto& ab = d16.vec_sptr_all_basic[0];
    ab->b = false;
    ab->i8_v = -1;
    ab->u8_v = 2;
    ab->i16_v = -2;
    ab->u16_v = 3;
    ab->i32_v = 4;
    ab->u32_v = 5;
    ab->i64_v = -6;
    ab->u64_v = 7;
    ab->f = 123.456f;
    ab->d = 789.123;
    ab->c = 'a';
    ab->c8 = u8'c';
    ab->c16 = u'd';
    ab->c32 = U'e';
    if (!d16.deq_uptr_template_container.empty()) {
        d16.deq_uptr_template_container[0]->vec = { 10, 20, 30 };
        d16.deq_uptr_template_container[0]->deq = { 1.1, 2.2 };
        d16.deq_uptr_template_container[0]->lst = { "deq_uptr_template_container_0", "deq_uptr_template_container_1" };
    }
    if (!d16.flist_sptr_complex.empty()) {
        auto& ct = d16.flist_sptr_complex.front();
        ct->nestedVectors = { {{7,8},{9,10}} };
        ct->arrayVectors = { std::vector<f32>{5.5f, 6.6f} };
        ct->mapVectors = { {"mkey", {100,200}} };
        ct->setVecs = { {111,222} };
    }
    if (d16.map_str_sptr_all_basic.count("key")) {
        d16.map_str_sptr_all_basic["key"]->b = false;
        d16.map_str_sptr_all_basic["key"]->c = 'M';
        d16.map_str_sptr_all_basic["key"]->i32_v = 54321;
    }

    o << d1;
    o << d2;
    o << d3;
    o << d3_1;
    o << d15;
    o << d16;
    o << d4;
    o << d5;
    o << d6;
    o << d7;
    o << d8;
    o << d9;
    o << d10;
    o << d11;
    o << d12;
    o << d13;
    o << d14;
    o << d15;
    o << d16;

    std::string serialized_str = oss.str();
    std::cout << "[test1] Total serialized size: " << serialized_str.size() << " bytes" << std::endl;
    return serialized_str;
}

// 项目 1：基础类型验证数据
std::string genBasicTypes() {
    AllBasicTypes d;
    d.b = true;
    d.i8_v = -128;
    d.u8_v = 255;
    d.i16_v = -32768;
    d.u16_v = 65535;
    d.i32_v = -2147483647 - 1;  // INT32_MIN 边界
    d.u32_v = 4294967295;        // UINT32_MAX（32 位无符号最大值）
    d.i64_v = INT64_MIN + 1;
    d.u64_v = UINT64_MAX;
    d.f = 3.141592653589793f;
    d.d = 2.718281828459045;
    d.c = 'Z';
    d.c8 = u8'\u007E'; // '~'
    d.c16 = u'\u03A9'; // 希腊字母 Ω
    d.c32 = U'\u2603'; // 雪人符号 ☃

    TemplateContainer tc;
    tc.s = "Test String 测试字符串";
    tc.u8s = u8"UTF8字符串 テスト";
    tc.vec = { 10, 20, 30, -40, 50 };
    tc.deq = { 1.5, 2.5, 3.5 };
    tc.lst = { "aaa", "bbb", "ccc" };
    tc.shortForwardList = { 1, 2, 3 };
    tc.uintSet = { 100, 200, 300 };
    tc.stringHashSet = { "A", "B", "C" };
    tc.intStringMap = { {1, "one"}, {2, "two"}, {3, "three"} };
    tc.stringFloatHashMap = { {"pi", 3.14f}, {"e", 2.71f} };

    std::ostringstream oss;
    SpObjProtocolOutput o{ oss };
    o << d;
    o << tc;

    std::string result = oss.str();
    std::cout << "[genBasicTypes] Size: " << result.size() << " bytes" << std::endl;
    return result;
}

// 项目 2：传感器实时更新数据（逐个序列化，避免注册新类型）
std::string genSensorUpdate(int tick) {
    static std::mt19937 rng(42);
    static std::uniform_real_distribution<double> tempDist(20.0, 30.0);
    static std::uniform_real_distribution<double> humDist(40.0, 80.0);
    static std::uniform_real_distribution<double> pressDist(1000.0, 1020.0);

    TemperatureSensor ts;
    ts.deviceId = "sensor-" + std::to_string(tick % 3 + 1);
    ts.manufacturer = "StreamPunk";
    ts.lastSeen = std::chrono::system_clock::now();
    ts.currentValue = tempDist(rng);
    ts.minValue = 0.0;
    ts.maxValue = 100.0;
    ts.samplingInterval = std::chrono::milliseconds(500);
    ts.isCelsius = true;
    ts.calibrationOffset = 0.0;

    Sensor hs;
    hs.deviceId = "sensor-humidity-" + std::to_string(tick % 3 + 1);
    hs.manufacturer = "StreamPunk";
    hs.lastSeen = std::chrono::system_clock::now();
    hs.currentValue = humDist(rng);
    hs.minValue = 0.0;
    hs.maxValue = 100.0;
    hs.samplingInterval = std::chrono::milliseconds(500);

    Sensor ps;
    ps.deviceId = "sensor-pressure-" + std::to_string(tick % 3 + 1);
    ps.manufacturer = "StreamPunk";
    ps.lastSeen = std::chrono::system_clock::now();
    ps.currentValue = pressDist(rng);
    ps.minValue = 900.0;
    ps.maxValue = 1100.0;
    ps.samplingInterval = std::chrono::milliseconds(500);

    // 逐个序列化，TS 端逐个反序列化
    std::ostringstream oss;
    SpObjProtocolOutput o{ oss };
    o << ts;
    o << hs;
    o << ps;
    // 追加 tick 值（作为 u32 原始数据，方便 TS 端读取）
    u32 tickVal = static_cast<u32>(tick);
    o.o.w->write(reinterpret_cast<const char*>(&tickVal), sizeof(tickVal));

    return oss.str();
}

// 项目 2/3：实时批量数据（多种类型混合）
std::string genRealtimeBatch(int tick) {
    static std::mt19937 rng(tick + 12345);
    static std::uniform_real_distribution<double> valDist(0.0, 100.0);

    SmartHomeSystem home;
    auto dev1 = std::make_shared<NetworkDevice>();
    dev1->deviceId = "router-" + std::to_string(tick);
    dev1->manufacturer = "StreamPunk";
    dev1->lastSeen = std::chrono::system_clock::now();
    dev1->ipAddress = "192.168.1." + std::to_string(tick % 254 + 1);
    dev1->macAddress = std::string("AA:BB:CC:DD:EE:") + (tick % 256 < 16 ? "0" : "") + 
                       std::to_string(tick % 256);
    dev1->port = 8080 + tick;

    auto dev2 = std::make_shared<TemperatureSensor>();
    dev2->deviceId = "temp-main";
    dev2->manufacturer = "StreamPunk";
    dev2->lastSeen = std::chrono::system_clock::now();
    dev2->currentValue = 20.0 + (valDist(rng) * 0.15);
    dev2->minValue = -20.0;
    dev2->maxValue = 60.0;
    dev2->samplingInterval = std::chrono::milliseconds(500);
    dev2->isCelsius = true;
    dev2->calibrationOffset = 0.1;

    home.allDevices.push_back(dev1);
    home.allDevices.push_back(dev2);
    home.sensors["living_room"] = dev2;
    home.mainThermostat = dev2;
    home.network = nullptr;

    std::ostringstream oss;
    SpObjProtocolOutput o{ oss };
    o << home;

    return oss.str();
}

// 项目 3：SmartHomeSystem 完整对象
std::string genHomeSystem() {
    SmartHomeSystem home;

    auto dev1 = std::make_shared<NetworkDevice>();
    dev1->deviceId = "router-001";
    dev1->manufacturer = "Cisco";
    dev1->lastSeen = std::chrono::system_clock::now();
    dev1->ipAddress = "192.168.1.1";
    dev1->macAddress = "00:11:22:33:44:55";
    dev1->port = 8080;

    auto dev2 = std::make_shared<TemperatureSensor>();
    dev2->deviceId = "temp-living";
    dev2->manufacturer = "Bosch";
    dev2->lastSeen = std::chrono::system_clock::now();
    dev2->currentValue = 23.5;
    dev2->minValue = -10.0;
    dev2->maxValue = 50.0;
    dev2->samplingInterval = std::chrono::milliseconds(1000);
    dev2->isCelsius = true;
    dev2->calibrationOffset = 0.2;

    auto dev3 = std::make_shared<TemperatureSensor>();
    dev3->deviceId = "temp-bedroom";
    dev3->manufacturer = "Honeywell";
    dev3->lastSeen = std::chrono::system_clock::now();
    dev3->currentValue = 21.0;
    dev3->minValue = -10.0;
    dev3->maxValue = 50.0;
    dev3->samplingInterval = std::chrono::milliseconds(1000);
    dev3->isCelsius = true;
    dev3->calibrationOffset = -0.1;

    auto dev4 = std::make_shared<Sensor>();
    dev4->deviceId = "hum-living";
    dev4->manufacturer = "Aqara";
    dev4->lastSeen = std::chrono::system_clock::now();
    dev4->currentValue = 65.0;
    dev4->minValue = 0.0;
    dev4->maxValue = 100.0;
    dev4->samplingInterval = std::chrono::milliseconds(2000);

    auto dev5 = std::make_shared<Sensor>();
    dev5->deviceId = "light-balcony";
    dev5->manufacturer = "Philips";
    dev5->lastSeen = std::chrono::system_clock::now();
    dev5->currentValue = 850.0;
    dev5->minValue = 0.0;
    dev5->maxValue = 2000.0;
    dev5->samplingInterval = std::chrono::milliseconds(5000);

    home.allDevices.push_back(dev1);
    home.allDevices.push_back(dev2);
    home.allDevices.push_back(dev3);
    home.allDevices.push_back(dev4);
    home.allDevices.push_back(dev5);
    home.sensors["living_room"] = dev2;
    home.sensors["bedroom"] = dev3;
    home.sensors["living_room_hum"] = dev4;
    home.sensors["balcony_light"] = dev5;
    home.mainThermostat = dev2;

    auto net = std::make_shared<NetworkSystem>();
    home.network = net;

    std::ostringstream oss;
    SpObjProtocolOutput o{ oss };
    o << home;

    std::string result = oss.str();
    std::cout << "[genHomeSystem] Size: " << result.size() << " bytes" << std::endl;
    return result;
}