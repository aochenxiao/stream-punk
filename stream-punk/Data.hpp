# pragma once
# include "StreamPunk.hpp"
# include <array>
# include <forward_list>
# include <bitset>
# include <optional>
# include <filesystem>
# include <atomic>
# include <variant>
# include <tuple>
# include <boost/predef.h>

constexpr inline u32 currDataVer = makeVersion(0, 0, 1, 0);

// 机器特性描述
#pragma pack(push, 1)  // 1字节对齐
struct StreamPunkMachineInfo {
    enum E_os {
        e_os_unknow = 0,
        e_os_win    ,
        e_os_mac    ,
        e_os_linux  ,
        e_os_ios    ,
        e_os_android,
        e_os_bsd    ,
    };

    enum E_cpu {
        e_cpu_unknow = 0,
        e_cpu_x86   ,
        e_cpu_X86_64,
        e_cpu_arm   ,
        e_cpu_arm64 ,
        e_cpu_ppc   ,
    };

    enum E_compiler {
        e_compiler_unknow = 0,
        e_compiler_msvc ,
        e_compiler_gcc  ,
        e_compiler_clang,
        e_compiler_intel,
    };

    u8 endian:1         ;   // 字节序：0=小端, 1=大端
    u8 os:7             ;   // 操作系统 (E_os)
    u8 cpu              ;   // CPU架构类型 (E_cpu)
    u8 wordByteCount    ;   // 程序字长的字节数
    u8 compiler         ;   // 编译器类型 (E_compiler)
    
    u32 compilerVer     ;   // 编译器版本
    u32 dataVer         ;   // 数据版本

    void init() {
        std::memset(this, 0, sizeof(StreamPunkMachineInfo));

        if constexpr (std::endian::native == std::endian::little) {
            endian = 0;
        }
        else if constexpr (std::endian::native == std::endian::big) {
            endian = 1;
        }

        wordByteCount = sizeof(void*);

        if      constexpr (BOOST_OS_WINDOWS ) os = e_os_win;
        else if constexpr (BOOST_OS_MACOS   ) os = e_os_mac;
        else if constexpr (BOOST_OS_LINUX   ) os = e_os_linux;
        else if constexpr (BOOST_OS_IOS     ) os = e_os_ios;
# if defined(BOOST_OS_ANDROID)
        else if constexpr (BOOST_OS_ANDROID ) os = e_os_android;
# endif
        else if constexpr (BOOST_OS_BSD     ) os = e_os_bsd;
        else os = e_os_unknow;

        if      constexpr (BOOST_ARCH_X86_64) cpu = e_cpu_X86_64;
        else if constexpr (BOOST_ARCH_X86   ) cpu = e_cpu_x86;
# if defined(BOOST_ARCH_ARM64)
        else if constexpr (BOOST_ARCH_ARM64 ) cpu = e_cpu_arm64;
# endif
        else if constexpr (BOOST_ARCH_ARM   ) cpu = e_cpu_arm;
        else if constexpr (BOOST_ARCH_PPC   ) cpu = e_cpu_ppc;
        else cpu = e_cpu_unknow;

        uint32_t compVerRaw = 0;
        if      constexpr (BOOST_COMP_MSVC  ) {
            compiler = e_compiler_msvc;
            compVerRaw = BOOST_COMP_MSVC;
        }
        else if constexpr (BOOST_COMP_CLANG ) {
            compiler = e_compiler_clang;
            compVerRaw = BOOST_COMP_CLANG;
        }
        else if constexpr (BOOST_COMP_GNUC  ) {
            compiler = e_compiler_gcc;
            compVerRaw = BOOST_COMP_GNUC;
        }
        else if constexpr (BOOST_COMP_INTEL ) {
            compiler = e_compiler_intel;
            compVerRaw = BOOST_COMP_INTEL;
        }
        else {
            compiler = e_compiler_unknow;
        }
        if (compVerRaw != 0) {
            uint32_t major = compVerRaw / 10000000;
            uint32_t minor = (compVerRaw % 10000000) / 100000;
            uint32_t patch = (compVerRaw % 100000) / 1000;
            compilerVer = (major << 24) | (minor << 16) | (patch << 8);
        }
        dataVer = currDataVer;
    }
};
#pragma pack(pop)
inline O& operator<<(O& s, const StreamPunkMachineInfo& v) { s.s.write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s, StreamPunkMachineInfo& v) { s.s.read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }


/*
    这是单元测试的一部分 也是一个使用示例
*/

struct AllBasicTypes : public Base {
#define Xt_AllBasicTypes(X__) \
    X__(bl  , b     , false) \
    X__(i8  , i8_v  , 0) \
    X__(u8  , u8_v  , 0) \
    X__(i16 , i16_v , 0) \
    X__(u16 , u16_v , 0) \
    X__(i32 , i32_v , 0) \
    X__(u32 , u32_v , 0) \
    X__(i64 , i64_v , 0) \
    X__(u64 , u64_v , 0) \
    X__(f32 , f     , 0.0f) \
    X__(f64 , d     , 0.0) \
    X__(ch  , c     , 0) \
    X__(ch8 , c8    , 0) \
    X__(ch16, c16   , 0) \
    X__(ch32, c32   , 0) \

    AllBasicTypes() = default;
    UseData(AllBasicTypes);
};

struct TemplateContainer : public Base {

#define Xt_TemplateContainer(X__) \
    X__(std::string, s, "") \
    X__(std::u8string, u8s, u8"") \
    X__(std::vector<int>, vec, {}) \
    X__(std::deque<double>, deq, {}) \
    X__(std::list<std::string>, lst, {}) \
    X__(std::forward_list<u16>, shortForwardList, {}) \
    X__(std::set<unsigned>, uintSet, {}) \
    X__(std::unordered_set<std::string>, stringHashSet, {}) \
    X__(std::map<int DH std::string>, intStringMap, {}) \
    X__(std::unordered_map<std::string DH float>, stringFloatHashMap, {})
	
    TemplateContainer() = default;
    UseData(TemplateContainer);

};

struct ComplexTemplateNesting : public Base {

 	using NestedVector = std::vector<std::vector<std::vector<u64>>>;
 	using ArrayOfVectors = std::vector<std::vector<f32>>;
 	using MapOfVectors = std::map<std::string, std::vector<u32>>;
    using SetOfVectors = std::set<std::vector<i64>>;

 	using NestedTuple = std::tuple<u16,std::tuple<f64, std::string>,std::vector<std::tuple<i32, f32>>>;
 	using OptionalCollection = std::vector<std::optional<std::string >>;
 	using VariantVector = std::vector<std::variant<u8, f64, std::string>>;
 		
#define Xt_ComplexTemplateNesting(X__) \
 	X__(NestedVector, nestedVectors, {}) \
 	X__(ArrayOfVectors, arrayVectors, {}) \
 	X__(MapOfVectors, mapVectors, {}) \
 	X__(SetOfVectors, setVecs, {}) \

 	/*
    X__(NestedTuple, complexTuple, {}) \
 	X__(OptionalCollection, optStrings, {}) \
 	X__(VariantVector, varVec, {}) \
    */
 		    
 	ComplexTemplateNesting() = default;
 	UseData(ComplexTemplateNesting);
};

struct ComprehensiveContainer : public Base {
#define Xt_ComprehensiveContainer(X__) \
    X__(std::vector<Sptr<AllBasicTypes>>, vec_sptr_all_basic, {}) \
    X__(std::deque<Uptr<TemplateContainer>>, deq_uptr_template_container, {}) \
    X__(std::list<std::string>, list_string, {}) \
    X__(std::forward_list<Sptr<ComplexTemplateNesting>>, flist_sptr_complex, {}) \
    X__(std::set<int>, set_int, {}) \
    X__(std::unordered_set<std::string>, uset_string, {}) \
    X__(std::map<std::string DH Sptr<AllBasicTypes>>, map_str_sptr_all_basic, {}) \
    X__(std::unordered_map<int DH Uptr<TemplateContainer>>, umap_int_uptr_template_container, {}) \
    X__(Wptr<ComprehensiveContainer>, self_wptr, {}) \
    X__(Sptr<ComprehensiveContainer>, self_sptr, {}) \

    ComprehensiveContainer() = default;
    UseData(ComprehensiveContainer);
};

struct PointerContainer : public Base {
#define Xt_PointerContainer(X__) \
    X__(int*, raw_ptr, nullptr) \
    X__(Sptr<int>, shared_ptr_int, {}) \
    X__(Uptr<int>, unique_ptr_int, {})

    PointerContainer() = default;
    UseData(PointerContainer);
};

struct Child : public AllBasicTypes {
#define Xt_Child(X__) \
    X__(int, child_field, 100)

    Child() = default;
    UseDataBase(Child, AllBasicTypes);
};

struct SelfReferential : public Base {
#define Xt_SelfReferential(X__) \
    X__(Sptr<SelfReferential>, self_ptr, {})

    SelfReferential() = default;
    UseData(SelfReferential);
};

struct TemplateAndPointer : public Base {
#define Xt_TemplateAndPointer(X__) \
    X__(std::vector<int*>, v_raw_ptr, {}) \
    X__(std::map<std::string DH Sptr<int>>, m_str_shared_ptr, {})

    TemplateAndPointer() = default;
    UseData(TemplateAndPointer);
};

struct InheritanceAndSelfReference : public Child {
#define Xt_InheritanceAndSelfReference(X__) \
    X__(Sptr<InheritanceAndSelfReference>, self_ptr, {})

    InheritanceAndSelfReference() = default;
    UseDataBase(InheritanceAndSelfReference, Child);
};

struct MegaComplexClass : public Child {
#define Xt_MegaComplexClass(X__) \
	X__(std::vector<Sptr<TemplateAndPointer>>, complex_vector, {}) \
	X__(SelfReferential*, raw_self_ref_ptr, nullptr) \
	X__(Sptr<MegaComplexClass>, self_ptr, {})

	MegaComplexClass() = default;
	UseDataBase(MegaComplexClass, Child);
};

struct SuperComplexContainer : public Base {
    // 从最内层向外逐层定义别名
    using PathSet = std::set<std::filesystem::path>;
    using PathSetDeque = std::deque<PathSet>;
    using ArrayType = std::array<char, 8>;
    using VariantElement = std::variant<ArrayType, PathSetDeque>;
    using VariantList = std::list<VariantElement>;
    using ComplexTuple = std::tuple<
        std::shared_ptr<SelfReferential>,  // 假设Sptr=std::shared_ptr
        std::unique_ptr<PointerContainer>, // 假设Uptr=std::unique_ptr
        VariantList
    >;
    using OptionalTuple = std::optional<ComplexTuple>;
    using ComplexVector = std::vector<OptionalTuple>;
    using ComplexMap = std::map<std::string, ComplexVector>; // 关键：作为成员类型

#define Xt_SuperComplexContainer(X__) \
    X__(std::bitset<64>, bits, {}) \
    X__(std::optional<int>, opt_int, {}) \
    X__(std::filesystem::path, file_path, {}) \
    X__(int, atomic_placeholder, 0) \
    X__(Sptr<Test>, sptr_test, {}) \
    X__(Wptr<Test>, wptr_test, {}) \
    X__(Uptr<Test>, uptr_test, {}) \
    X__(std::tuple<double DH std::string DH bool>, tuple_member, {}) \
    X__(std::map<std::string DH std::unordered_map<int DH double>>, map_of_umaps, {}) \
    X__(std::variant<int DH std::string DH Sptr<AllBasicTypes>>, variant_member, {}) \
    X__(std::deque<std::list<std::forward_list<int>>>, deq_list_flist, {}) \
    X__(std::vector<std::array<std::string DH 5>>, vec_arr_str, {}) \
    X__(std::set<std::vector<std::string>>, set_of_usets, {}) \
    X__(ComplexMap, kitchen_sink, {})

	SuperComplexContainer() = default;
	UseData(SuperComplexContainer);
};

// 注意, 禁止关于Base的菱形继承!
struct Test :public Base {

# define Xt_Test(X__) \
X__(std::string, name     ,{}) \
X__(std::string, pwd      ,{}) \
X__(std::string, gateWay  ,{}) \
X__(std::string, mask     ,{}) \
X__(std::string, ip       ,{}) \
X__(std::string, dns1     ,{}) \
X__(std::string, dns2     ,{}) \

    Test() = default;
    UseData(Test);
};

struct MQTT :public Base {
# define Xt_MQTT(X__) \
X__(std::string, host, {}) \
X__(std::string, user, {}) \
X__(std::string, pwd , {}) \

    MQTT() = default;
    UseData(MQTT);
}; // struct MQTT


// 包含各种指针类型的类
struct PointerDemo : public Base {
#define Xt_PointerDemo(X__) \
    X__(Test*,         rawPtr,       nullptr) \
    X__(Sptr<MQTT>,    sharedPtr,    {}) \
    X__(Uptr<Test>,    uniquePtr,    {}) \
    X__(Wptr<PointerDemo>, weakSelf, {})  // 自引用weak_ptr 

    PointerDemo() = default;
    PointerDemo& operator=(PointerDemo const& v_) = default;
    PointerDemo& operator=(PointerDemo&& v_) = default;
    PointerDemo(PointerDemo const& v_) = default;
    PointerDemo(PointerDemo&& v_) = default;

    UseData(PointerDemo);
};

// 包含容器和指针的类
struct ContainerDemo : public Base {
    /*
        X__(std::map<std::string, Uptr<MQTT>>, mqttConfigs, {}) 会报错
        原因是宏的逗号识别问题.
        解决方法:
            1.使用using 给带逗号的类型起一个别名
            2.使用DH 代替 逗号','
    */
    using mqttUptrMap = std::map<std::string, Uptr<MQTT>>;

#define Xt_ContainerDemo(X__) \
    X__(std::vector<Sptr<Test>>     , testPtrs, {}) \
    X__(Sptr<ContainerDemo>         , selfContainer, {}) \
    X__(std::unordered_set<Base*>   , allObjects, {}) \
    X__(mqttUptrMap                 , mqttConfigs, {}) \
    X__(std::map<std::string DH Uptr<MQTT>> , mqttConfigs2, {}) \
    
    ContainerDemo() = default;
    UseData(ContainerDemo);
};



// 更复杂的嵌套结构
struct NetworkSystem : public Base {
#define Xt_NetworkSystem(X__) \
    X__(Sptr<ContainerDemo>,  mainContainer,   {}) \
    X__(std::vector<Wptr<Test>>, activeTests,   {}) \
    X__(std::list<Uptr<MQTT>>,  mqttInstances,  {}) \
    X__(std::deque<Sptr<PointerDemo>>, demos,  {})

    NetworkSystem() = default;
    UseData(NetworkSystem);
};


// 基础设备类
struct Device : public Base {
#define Xt_Device(X__) \
    X__(std::string, deviceId, "unknown") \
    X__(std::string, manufacturer, "") \
    X__(std::chrono::system_clock::time_point, lastSeen, {})

    Device() = default;
    UseData(Device);
};

// 网络设备 - 继承自Device
struct NetworkDevice : public Device {
#define Xt_NetworkDevice(X__) \
    X__(std::string, ipAddress, "0.0.0.0") \
    X__(std::string, macAddress, "00:00:00:00:00:00") \
    X__(u16, port, 0)

    NetworkDevice() = default;
    UseDataBase(NetworkDevice,Device);
};

// 传感器设备 - 继承自Device
struct Sensor : public Device {
#define Xt_Sensor(X__) \
    X__(f64, currentValue, 0.0) \
    X__(f64, minValue, 0.0) \
    X__(f64, maxValue, 100.0) \
    X__(std::chrono::milliseconds, samplingInterval, std::chrono::milliseconds(1000))

    Sensor() = default;
    UseDataBase(Sensor,Device);

    // 虚函数示例
    virtual f64 getNormalizedValue() const {
        return (currentValue - minValue) / (maxValue - minValue);
    }
};

// 温度传感器 - 继承自Sensor
struct TemperatureSensor : public Sensor {
#define Xt_TemperatureSensor(X__) \
    X__(bool, isCelsius, true) \
    X__(f64, calibrationOffset, 0.0)

    TemperatureSensor() = default;
    UseDataBase(TemperatureSensor, Sensor);

    // 重写虚函数
    f64 getNormalizedValue() const override {
        f64 value = currentValue;
        if (!isCelsius) {
            // 转换为摄氏温度
            value = (value - 32) * 5 / 9;
        }
        return (value - minValue) / (maxValue - minValue);
    }
};

// 智能设备集合类
struct SmartHomeSystem : public Base {
#define Xt_SmartHomeSystem(X__) \
    X__(std::vector<Sptr<Device>>, allDevices, {}) \
    X__(std::map<std::string DH Sptr<Sensor>>, sensors, {}) \
    X__(Sptr<TemperatureSensor>, mainThermostat, nullptr) \
    X__(Sptr<NetworkSystem>, network, nullptr)

    SmartHomeSystem() = default;
    UseData(SmartHomeSystem);
};

// 复杂的多态容器类
struct MultiLevelContainer : public Base {
#define Xt_MultiLevelContainer(X__) \
    X__(Sptr<Base>, baseObj, nullptr) \
    X__(std::vector<Sptr<Device>>, deviceList, {}) \
    X__(std::map<std::string DH Sptr<Sensor>>, sensorMap, {}) \
    X__(Sptr<MultiLevelContainer>, selfRef, nullptr)

    MultiLevelContainer() = default;
    UseData(MultiLevelContainer);
};


struct SptrTest :public Base {
# define Xt_SptrTest(X__) \
    X__(Sptr<std::vector<Sptr<Device>>>, test1, nullptr) \

    SptrTest() = default;
    UseData(SptrTest);
};


struct MousePosition : public Base {
# define Xt_MousePosition(X__) \
    X__(i32, x, 0) \
    X__(i32, y, 0) \

    MousePosition() = default;
    UseData(MousePosition);
};

