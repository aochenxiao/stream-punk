// stream-punk_for_cpp26 - C++26 反射版数据定义
// 编译: g++-16 -std=c++26 -freflection
//
// 与旧版 (examples/00-demo-types/Data.hpp) 的对比:
//   - 零继承: 不需要 Base
//   - 零宏: 不需要 UseData / UseDataBase / DH
//   - 注册: 使用 SP_REFLECT 一行
//   - 反射: 使用 ^^ 和 [: ... :] 访问成员

#pragma once

#include "stream-punk-reflection/StreamPunkReflection.hpp"
#include <string>
#include <vector>
#include <deque>
#include <list>
#include <forward_list>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <array>
#include <bitset>
#include <optional>
#include <tuple>
#include <variant>
#include <memory>
#include <chrono>

namespace sp26 {

// ============================================================
// 类型别名（与旧版 sp 保持一致）
// ============================================================

template<typename T> using Sptr = std::shared_ptr<T>;
template<typename T> using Uptr = std::unique_ptr<T>;
template<typename T> using Wptr = std::weak_ptr<T>;
using Sz = u32;

// ============================================================
// 值语义类型
// ============================================================

// ---- 基础类型合集 ----
struct AllBasicTypes {
    bool    b      = false;
    i8      i8_v   = 0;
    u8      u8_v   = 0;
    i16     i16_v  = 0;
    u16     u16_v  = 0;
    i32     i32_v  = 0;
    u32     u32_v  = 0;
    i64     i64_v  = 0;
    u64     u64_v  = 0;
    f32     f      = 0.0f;
    f64     d      = 0.0;
    char    c      = 0;
    char8_t c8     = 0;
    char16_t c16   = 0;
    char32_t c32   = 0;
};

// ---- 模板容器合集 ----
struct TemplateContainer {
    std::string                      s;
    std::u8string                    u8s;
    std::vector<int>                 vec;
    std::deque<double>               deq;
    std::list<std::string>           lst;
    std::forward_list<u16>           shortForwardList;
    std::set<unsigned>               uintSet;
    std::unordered_set<std::string>  stringHashSet;
    std::map<int, std::string>       intStringMap;
    std::unordered_map<std::string, float> stringFloatHashMap;
};

// ---- 复杂嵌套模板 ----
struct ComplexTemplateNesting {
    using NestedVector    = std::vector<std::vector<std::vector<u64>>>;
    using ArrayOfVectors  = std::vector<std::vector<f32>>;
    using MapOfVectors    = std::map<std::string, std::vector<u32>>;
    using SetOfVectors    = std::set<std::vector<i64>>;

    using NestedTuple      = std::tuple<u16, std::tuple<f64, std::string>, std::vector<std::tuple<i32, f32>>>;
    using OptionalCollection = std::vector<std::optional<std::string>>;
    using VariantVector     = std::vector<std::variant<u8, f64, std::string>>;

    NestedVector       nestedVectors;
    ArrayOfVectors     arrayVectors;
    MapOfVectors       mapVectors;
    SetOfVectors       setVecs;
    NestedTuple        nestedTuple;
    OptionalCollection optCollection;
    VariantVector      variantVec;
};

// ---- Test / MQTT ----
struct Test {
    std::string name;
    std::string pwd;
    std::string gateWay;
    std::string mask;
    std::string ip;
    std::string dns1;
    std::string dns2;
};

struct MQTT {
    std::string host;
    std::string user;
    std::string pwd;
};

// ---- MousePosition ----
struct MousePosition {
    i32 x = 0;
    i32 y = 0;
};

// ---- ShadowTestData ----
struct ShadowTestData {
    std::vector<i32>            numbers;
    std::map<i32, std::string>  items;
    std::optional<i32>          optVal;
    MousePosition               pos;
};

// ---- 设备层级（继承 → 扁平化） ----
struct Device {
    std::string                              deviceId;
    std::string                              manufacturer;
    std::chrono::system_clock::time_point    lastSeen;
};

struct NetworkDevice {
    // Device 成员（扁平化）
    std::string                              deviceId;
    std::string                              manufacturer;
    std::chrono::system_clock::time_point    lastSeen;
    // NetworkDevice 自身成员
    std::string ipAddress;
    std::string macAddress;
    u16        port = 0;
};

struct Sensor {
    // Device 成员
    std::string                              deviceId;
    std::string                              manufacturer;
    std::chrono::system_clock::time_point    lastSeen;
    // Sensor 自身成员
    f64                                   currentValue     = 0.0;
    f64                                   minValue         = 0.0;
    f64                                   maxValue         = 100.0;
    std::chrono::milliseconds             samplingInterval = std::chrono::milliseconds(1000);
};

struct TemperatureSensor {
    // Device 成员
    std::string                              deviceId;
    std::string                              manufacturer;
    std::chrono::system_clock::time_point    lastSeen;
    // Sensor 成员
    f64                                   currentValue     = 0.0;
    f64                                   minValue         = 0.0;
    f64                                   maxValue         = 100.0;
    std::chrono::milliseconds             samplingInterval = std::chrono::milliseconds(1000);
    // TemperatureSensor 自身成员
    bool  isCelsius         = true;
    f64   calibrationOffset = 0.0;
};

// ---- Animal / Dog 层级（扁平化） ----
struct Animal {
    std::string species = "unknown";
    i32         age     = 0;
};

struct Dog {
    std::string species   = "unknown";
    i32         age       = 0;
    std::string breed     = "mixed";
    bool        isTrained = false;
};

struct WorkingDog {
    std::string species       = "unknown";
    i32         age           = 0;
    std::string breed         = "mixed";
    bool        isTrained     = false;
    std::string jobTitle      = "unemployed";
    i32         yearsOfService = 0;
};

// ---- Child（AllBasicTypes 继承 → 扁平化） ----
struct Child {
    // AllBasicTypes 成员
    bool    b      = false;
    i8      i8_v   = 0;
    u8      u8_v   = 0;
    i16     i16_v  = 0;
    u16     u16_v  = 0;
    i32     i32_v  = 0;
    u32     u32_v  = 0;
    i64     i64_v  = 0;
    u64     u64_v  = 0;
    f32     f      = 0.0f;
    f64     d      = 0.0;
    char    c      = 0;
    char8_t c8     = 0;
    char16_t c16   = 0;
    char32_t c32   = 0;
    // Child 自身成员
    i32 child_field = 100;
};

// ---- InheritanceAndSelfReference（扁平化） ----
struct InheritanceAndSelfReference {
    // AllBasicTypes 成员
    bool    b      = false;
    i8      i8_v   = 0;
    u8      u8_v   = 0;
    i16     i16_v  = 0;
    u16     u16_v  = 0;
    i32     i32_v  = 0;
    u32     u32_v  = 0;
    i64     i64_v  = 0;
    u64     u64_v  = 0;
    f32     f      = 0.0f;
    f64     d      = 0.0;
    char    c      = 0;
    char8_t c8     = 0;
    char16_t c16   = 0;
    char32_t c32   = 0;
    // Child 成员
    i32 child_field = 100;
    // InheritanceAndSelfReference 自身成员
    Sptr<InheritanceAndSelfReference> self_ptr;
};

// ============================================================
// 指针类型（含 shared_ptr / unique_ptr / weak_ptr）
// ============================================================

struct PointerContainer {
    i32*        raw_ptr = nullptr;
    Sptr<i32>   shared_ptr_int;
    Uptr<i32>   unique_ptr_int;
};

struct PointerDemo {
    Test*              rawPtr    = nullptr;
    Sptr<MQTT>         sharedPtr;
    Uptr<Test>         uniquePtr;
    Wptr<PointerDemo>  weakSelf;
};

struct TemplateAndPointer {
    std::vector<i32*>                     v_raw_ptr;
    std::map<std::string, Sptr<i32>>      m_str_shared_ptr;
};

struct SelfReferential {
    Sptr<SelfReferential> self_ptr;
};

struct ContainerDemo {
    std::vector<Sptr<Test>>               testPtrs;
    Sptr<ContainerDemo>                   selfContainer;
    std::map<std::string, Uptr<MQTT>>     mqttConfigs;
};

struct NetworkSystem {
    Sptr<ContainerDemo>              mainContainer;
    std::vector<Wptr<Test>>          activeTests;
    std::list<Uptr<MQTT>>            mqttInstances;
    std::deque<Sptr<PointerDemo>>    demos;
};

struct SmartHomeSystem {
    std::vector<Sptr<Device>>              allDevices;
    std::map<std::string, Sptr<Sensor>>    sensors;
    Sptr<TemperatureSensor>                mainThermostat;
    Sptr<NetworkSystem>                    network;
};

struct MultiLevelContainer {
    Sptr<AllBasicTypes>                  baseObj;
    std::vector<Sptr<Device>>            deviceList;
    std::map<std::string, Sptr<Sensor>>  sensorMap;
    Sptr<MultiLevelContainer>            selfRef;
};

struct SptrTest {
    Sptr<std::vector<Sptr<Device>>> test1;
};

struct ComprehensiveContainer {
    std::vector<Sptr<AllBasicTypes>>                  vec_sptr_all_basic;
    std::deque<Uptr<TemplateContainer>>               deq_uptr_template_container;
    std::list<std::string>                            list_string;
    std::forward_list<Sptr<ComplexTemplateNesting>>   flist_sptr_complex;
    std::set<i32>                                     set_int;
    std::unordered_set<std::string>                   uset_string;
    std::map<std::string, Sptr<AllBasicTypes>>        map_str_sptr_all_basic;
    std::unordered_map<i32, Uptr<TemplateContainer>>  umap_int_uptr_template_container;
    Wptr<ComprehensiveContainer>                      self_wptr;
    Sptr<ComprehensiveContainer>                      self_sptr;
};

// ---- MegaComplexClass（Child 继承 → 扁平化） ----
struct MegaComplexClass {
    // AllBasicTypes 成员
    bool    b      = false;
    i8      i8_v   = 0;
    u8      u8_v   = 0;
    i16     i16_v  = 0;
    u16     u16_v  = 0;
    i32     i32_v  = 0;
    u32     u32_v  = 0;
    i64     i64_v  = 0;
    u64     u64_v  = 0;
    f32     f      = 0.0f;
    f64     d      = 0.0;
    char    c      = 0;
    char8_t c8     = 0;
    char16_t c16   = 0;
    char32_t c32   = 0;
    // Child 成员
    i32 child_field = 100;
    // MegaComplexClass 自身成员
    std::vector<Sptr<TemplateAndPointer>> complex_vector;
    SelfReferential*                      raw_self_ref_ptr = nullptr;
    Sptr<MegaComplexClass>                self_ptr;
};

// ---- SuperComplexContainer ----
struct SuperComplexContainer {
    using PathSet         = std::set<std::string>;  // 简化: 不用 filesystem::path
    using PathSetDeque    = std::deque<PathSet>;
    using ArrayType       = std::array<char, 8>;
    using VariantElement  = std::variant<ArrayType, PathSetDeque>;
    using VariantList     = std::list<VariantElement>;
    using ComplexTuple    = std::tuple<Sptr<SelfReferential>, Uptr<PointerContainer>, VariantList>;
    using OptionalTuple   = std::optional<ComplexTuple>;
    using ComplexVector   = std::vector<OptionalTuple>;
    using ComplexMap      = std::map<std::string, ComplexVector>;

    std::bitset<64>                                        bits;
    std::optional<i32>                                     opt_int;
    std::string                                            file_path;
    i32                                                    atomic_placeholder = 0;
    Sptr<Test>                                             sptr_test;
    Wptr<Test>                                             wptr_test;
    Uptr<Test>                                             uptr_test;
    std::tuple<double, std::string, bool>                  tuple_member;
    std::map<std::string, std::unordered_map<i32, double>> map_of_umaps;
    std::variant<i32, std::string, Sptr<AllBasicTypes>>    variant_member;
    std::deque<std::list<std::forward_list<i32>>>          deq_list_flist;
    std::vector<std::array<std::string, 5>>                vec_arr_str;
    std::set<std::vector<std::string>>                     set_of_usets;
    ComplexMap                                             kitchen_sink;
};

} // namespace sp26

// ============================================================
// SP_REFLECT 注册（必须在 sp26 命名空间内，因为类型在此定义）
// ============================================================

namespace sp26 {

SP_REFLECT(AllBasicTypes,
    b, i8_v, u8_v, i16_v, u16_v, i32_v, u32_v, i64_v, u64_v, f, d, c, c8, c16, c32)

SP_REFLECT(TemplateContainer,
    s, u8s, vec, deq, lst, shortForwardList, uintSet, stringHashSet, intStringMap, stringFloatHashMap)

SP_REFLECT(ComplexTemplateNesting,
    nestedVectors, arrayVectors, mapVectors, setVecs, nestedTuple, optCollection, variantVec)

SP_REFLECT(Test, name, pwd, gateWay, mask, ip, dns1, dns2)
SP_REFLECT(MQTT, host, user, pwd)
SP_REFLECT(MousePosition, x, y)
SP_REFLECT(ShadowTestData, numbers, items, optVal, pos)

SP_REFLECT(Device, deviceId, manufacturer, lastSeen)
SP_REFLECT(NetworkDevice, deviceId, manufacturer, lastSeen, ipAddress, macAddress, port)
SP_REFLECT(Sensor, deviceId, manufacturer, lastSeen, currentValue, minValue, maxValue, samplingInterval)
SP_REFLECT(TemperatureSensor, deviceId, manufacturer, lastSeen, currentValue, minValue, maxValue, samplingInterval, isCelsius, calibrationOffset)

SP_REFLECT(Animal, species, age)
SP_REFLECT(Dog, species, age, breed, isTrained)
SP_REFLECT(WorkingDog, species, age, breed, isTrained, jobTitle, yearsOfService)

SP_REFLECT(Child,
    b, i8_v, u8_v, i16_v, u16_v, i32_v, u32_v, i64_v, u64_v, f, d, c, c8, c16, c32, child_field)

SP_REFLECT(InheritanceAndSelfReference,
    b, i8_v, u8_v, i16_v, u16_v, i32_v, u32_v, i64_v, u64_v, f, d, c, c8, c16, c32, child_field, self_ptr)

SP_REFLECT(PointerContainer, raw_ptr, shared_ptr_int, unique_ptr_int)
SP_REFLECT(PointerDemo, rawPtr, sharedPtr, uniquePtr, weakSelf)
SP_REFLECT(TemplateAndPointer, v_raw_ptr, m_str_shared_ptr)
SP_REFLECT(SelfReferential, self_ptr)
SP_REFLECT(ContainerDemo, testPtrs, selfContainer, mqttConfigs)
SP_REFLECT(NetworkSystem, mainContainer, activeTests, mqttInstances, demos)
SP_REFLECT(SmartHomeSystem, allDevices, sensors, mainThermostat, network)
SP_REFLECT(MultiLevelContainer, baseObj, deviceList, sensorMap, selfRef)
SP_REFLECT(SptrTest, test1)
SP_REFLECT(ComprehensiveContainer,
    vec_sptr_all_basic, deq_uptr_template_container, list_string, flist_sptr_complex,
    set_int, uset_string, map_str_sptr_all_basic, umap_int_uptr_template_container,
    self_wptr, self_sptr)

SP_REFLECT(MegaComplexClass,
    b, i8_v, u8_v, i16_v, u16_v, i32_v, u32_v, i64_v, u64_v, f, d, c, c8, c16, c32,
    child_field, complex_vector, raw_self_ref_ptr, self_ptr)

SP_REFLECT(SuperComplexContainer,
    bits, opt_int, file_path, atomic_placeholder, sptr_test, wptr_test, uptr_test,
    tuple_member, map_of_umaps, variant_member, deq_list_flist, vec_arr_str,
    set_of_usets, kitchen_sink)

} // namespace sp26