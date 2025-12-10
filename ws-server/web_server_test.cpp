#include "web_server.hpp"

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
    //d1.wc = L'B';
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
    //d4.wc = L'Y';
    d4.c8 = u8'Z';
    d4.c16 = u'W';
    d4.c32 = U'\u0056'; // 'V'
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
    //ab->wc = L'b';
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
    std::cout << "[WebSocket] Sending serialized data (" << serialized_str.size() << " bytes):" << std::endl << std::noshowbase;
    for (size_t i = 0; i < serialized_str.size(); ++i) {
        if (i % 16 == 0) {
            std::cout << std::hex << std::setw(8) << std::setfill('0') << i << ": ";
        }
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(serialized_str[i])) << " ";
        if (i % 16 == 15 || i == serialized_str.size() - 1) {
            std::cout << std::dec << std::endl;
        }
    }
    std::cout << std::endl;
    return serialized_str;
}

//void test2() {
//    std::istringstream iss(std::string(reinterpret_cast<const char*>(in), len));
//    I i(iss);
//
//    AllBasicTypes d1;
//    TemplateContainer d2;
//    PointerContainer d3;
//    PointerContainer d3_1;
//    Child d4;
//    auto d5 = std::make_shared<SelfReferential>();
//    TemplateAndPointer d6;
//    auto d7 = std::make_shared<InheritanceAndSelfReference>();
//    auto d8 = std::make_shared<MegaComplexClass>();
//    SuperComplexContainer d9;
//    auto d10 = std::make_shared<PointerDemo>();
//    auto d11 = std::make_shared<ContainerDemo>();
//    NetworkSystem d12;
//    auto d13 = std::make_shared<SmartHomeSystem>();
//    MultiLevelContainer d14;
//    ComplexTemplateNesting d15;
//    ComprehensiveContainer d16;
//
//}







