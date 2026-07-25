// Copyright (c) 2026 aochenxiao
// SPDX-License-Identifier: MIT

/*
    这个库汉语名称: 流水账
    英文名:StreamPunk

    基础功能:
        支持数据的序列化和反序列化
    基础功能对数据的态度是:
        1.不管是什么类型的数据, 只要你拥有数据定义的头文件, 你就应当知道它的数据结构，也应当知道序列化和反序列化的正确顺序。
        2.没考虑过private
    v0.0.1:
        完成了基础的类型支持.
    v0.0.2:
        实现了容器的支持.
    v0.0.3:
        指针类型的基本支持, 可以放入原始指针, Sptr, Uptr, Wptr.
        存在的问题:
            基类指针指向子类对象, 首次对该对象序列化时, 若放入的是基类指针, 对象只会被当做基类对象进行处理.
            反序列化时,用父类指针去取出,对象就会被新建成父类对象. 从而出错.
        解决方法:
            给所有类型进行编号,只要是有可能被序列化的类型,都要有一个编号.
                序列化时,先将编号写入流中,然后再写入对象.
                反序列化时,如果编号对应的类型是子类,则将其转换为子类指针.
                这样就可以避免父类指针指向子类对象的问题.
            但是,如果在序列化时,父类指针指向子类对象,该如何是好?
                所以,如果是自定义的struct/class,就要准备一个虚函数,获取当前类型.
    v0.0.4:
        支持chrono
            新建了StreamPunkTime类型128位数据保存时间,精度细至atto,时间跨度也足够.
    v0.0.5:
        引入Base解决版本0.0.3存在的问题, 代价是所有需要用到这个库的类, 都要继承Base
    v0.0.6 支持 optional filesystem::path atomic 
    v0.0.7 variant 
    v0.0.8 支持enum/enum class
    v0.0.9 
        去除对std::string_view的反序列化支持
        添加std::span<T>的序列化支持
        添加std::initializer_list 的序列化支持
        添加std::tuple的序列化/反序列化支持
    v0.1.0 支持深拷贝
    v0.1.1 实现机器特性描述
    v0.1.2 实现类型描述
        补充这些类型的序列化/反序列化:
            char8_t
            char16_t
            char32_t
    v0.2.0 实现与 TS     互通 (JS的互通依靠TS编译成JS从而实现)
        补充了遗漏的几个深拷贝模板函数
        取消了对wchar_t的支持
    v0.2.1
        开始用AI生成代码
        实现与 Python 互通
        实现与 Java   互通
        实现与 Go     互通
        实现与 Rust   互通
        实现与 Kotlin 互通
    v0.2.2
        实现数据的增删改查

    v0.9.0 实现数据的查询和修改
        实现了SPOI，跨语言，可以动态实时进行增删改查。
        支持数据快照+数据增量更新

    待办:
        图形化显示数据
            写一些应用范例，展示使用StreamPunk进行实时在线数据同步
        实现一个简易的同步机制，展示针对对象的同步。
        数据版本管理

    使用方法:
        所有自定义的类,要使用到StreamPunk的序列化/反序列化,就要继承Base.
        定义了之后,也要将名称和别名写到Xt_CustomType当中.

        深拷贝:
            执行深拷贝依靠 DeepCopier 的对象,拷贝完要手动执行clear()
    注意事项:
        本项目下的所有文件使用utf-8编码 无签名,代码页65001
        仅限Cpp20及以上标准
        程序初始化必须运行一次 INIT_StreamPunk();
        all_custom_creator_pfn() 是函数内静态变量，线程安全。

        自定义的类不建议多继承,菱形继承
        自定义的类不要用std::string_view std::span 等,只适合临时使用的类型做成员.
        std::string_view std::span 这种类可以做序列化,但不能做反序列化.
        不支持宽字节，原因：为支持跨平台 跨语言 。
        跨语言交互数据
            只使用ASCII可以使用std::string，用到了其他字符时，推荐定义为std::u8string。
            与TS、JS交互数据，定义为std::u16string对于TS、JS端可以避免转码。
        
        目前char* / char const* 会被当成堆空间的一个char的对象进行处理 而不是字符串.
        使用 o << std::move(obj);大部分情况里,obj的数据不会被移走, 而是被当做一个引用来处理.

        指针:
            1. 不要使用原始引用作为成员.
            2. 在反序列化时,遇到原始指针,StreamPunk只会给原始指针从堆中分配对象. 所以使用时,只适合让原始指针指向堆分配的对象.
            3. 同样, 在进行深拷贝时, 拷贝出来的原始指针, 指向的也是新创建的堆分配的对象.
            这意味着, 如果你用指针a指向了另一个对象中的成员b,那么,反序列化出来之后,这个指针a`,指向的就是一个独立对象c`, 与b`是相互独立的.
            综上所述, 对于需要类内引用的部分, 最好将其改为1.Sptr配合Wptr进行引用. 2.Uptr配合原始指针进行引用.
            4.void*指向一个变量或者对象,那么首次对这个对象或者变量序列化,就不能用void*,会出问题.
            更不能在指向栈内存的时候做序列化,因为反序列化时,它会被当成堆内存的对象.
        涉及到map的深拷贝,要求键和值的类型支持移动构造
        自定义类当中,默认大家都不使用private, 若使用private对成员进行保护,则需要注意给StreamPunk声明友元函数

        自定义模板, 在类型的描述符上, 注意参考现有代码使用SpToken, 为它设置相应描述符.
        不方便做到的, 你可以干脆将特化后的类型, 注册成自定义类.
*/

#pragma once

#include <ostream>
#include <istream>
#include <bit>
#include <type_traits>
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <string_view>

#include <deque>
#include <list>
#include <forward_list>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <bitset>
#include <memory>

#include <chrono>

#include <any>
#include <atomic>
#include <variant>
#include <optional>
#include <filesystem>

#include <algorithm>

#include <tuple>
#include <ranges>

#include <stdexcept>

/*
    Xt_CustomType 自定义类型注册表。

    这是用来注册自定义数据的头文件.
    独立出来让用户写
    只要项目能正确包含这个文件, 这个文件实际上放在哪个目录都随意.
    所有自定义类型, 都要有默认构造函数.
    
    应用程序应在包含本头文件之前 #define Xt_CustomType(X__) 来注册自己的类型。
    格式：X__(完整类型名, 唯一标识符)
    示例：
        #define Xt_CustomType(X__) \
        X__(::MyType, MyType) \
        X__(::OtherType, OtherType)

    注意：由于 Xt_CustomType 在 namespace sp 之前定义，用户可以在包含本头文件之前
    自由决定类型所在的命名空间。X_using_struct 会将自定义类型引入 sp 命名空间。
*/
#include "custom_type.hpp"
#ifndef Xt_CustomType
#define Xt_CustomType(X__) /* 空：未注册自定义类型 */
#endif

#define DH  ,

namespace sp {
// X 宏类型注册中心。
// 每个 Xt_* 定义一张类型表，通过传入不同的宏函数 X__ 产生不同产出。
// 例如：Xt_BasicType(X_enumMember) 生成枚举成员，Xt_BasicType(X_using) 生成类型别名。
//
// 常用 X 宏函数（按调用顺序）：
//   X_enumMember( type, name ) -> name ,          // 生成 E_type 枚举成员
//   X_using      ( type, name ) -> using name = type;  // 生成类型别名
//   X_DEF_TypeID_kind(type, name, kind) -> TypeID_t<name> 特化 // 注册类型 ID
//   X_reg_custom ( type, name ) -> 注册创建函数(typeID映射) + Schema类名  // INIT_StreamPunk()
//   X_CustomTypeDesc(type, name) -> TypeDesc 注册（通过 SFINAE 自动检测 _desc）
//
// UseData 宏内部的 X 宏函数（由 UseDataXtBase 定义）：
//   X_classMember(type, name, default, ...) -> 成员变量声明
//   X_enumClassMember(type, name, ...) -> 成员索引枚举
//   X_tupleMember(type, name, ...) -> std::tuple 类型列表
//   X_leftShiftName(type, name, ...) -> o << member 序列化
//   X_rightShiftName(type, name, ...) -> i >> member 反序列化
//   X_deepCopyFrom(type, name, ...) -> 深拷贝
//   X_comma_decltypeName(type, name, ...) -> , decltype(name) 用于类型列表
//   X_memberNameStr(type, name, ...) -> 成员名字符串
//   X_memberRefForTuple(type, name, ...) -> std::tie 成员引用

#define Xt_BasicType(X__) \
X__(::std::uint8_t , u8  ) \
X__(::std::uint16_t, u16 ) \
X__(::std::uint32_t, u32 ) \
X__(::std::uint64_t, u64 ) \
X__(::std::int8_t  , i8  ) \
X__(::std::int16_t , i16 ) \
X__(::std::int32_t , i32 ) \
X__(::std::int64_t , i64 ) \
X__(float          , f32 ) \
X__(double         , f64 ) \
X__(char           , ch  ) \
X__(char8_t        , ch8 ) \
X__(char16_t       , ch16) \
X__(char32_t       , ch32) \
X__(bool           , bl  ) \

#define Xt_template(X__) \
X__(::std::vector             , vector ) \
X__(::std::array              , array  ) \
X__(::std::string             , string ) \
X__(::std::bitset             , bitset ) \
X__(::std::deque              , deque  ) \
X__(::std::list               , list   ) \
X__(::std::forward_list       , flist  ) \
X__(::std::set                , set    ) \
X__(::std::unordered_set      , uset   ) \
X__(::std::map                , map    ) \
X__(::std::unordered_map      , umap   ) \
X__(::std::shared_ptr         , sptr   ) \
X__(::std::weak_ptr           , wptr   ) \
X__(::std::unique_ptr         , uptr   ) \
X__(::std::optional           , opt    ) \
X__(::std::filesystem::path   , path   ) \
X__(::std::atomic             , atomic ) \
X__(::std::variant            , variant) \
X__(::std::tuple              , tuple  ) \

#define Xt_Data_options(X__) \
X__( , e_op_position) \
X__( , e_op_select) \
X__( , e_op_deptr) \
X__( , e_op_ranges_insert_one) \

#define Xt_Type(X__) \
X__( , e_unknowType  ) \
Xt_Data_options(X__) \
X__( , bg) \
X__( , ed) \
Xt_template(X__) \
Xt_BasicType(X__) \
X__( , ptr) \
X__( , voidPtr) \
X__( , cst) \
X__( , dur) \
X__( , timepoint) \
X__( Base, Base) \
Xt_CustomType(X__) \
X__( , e_customType  ) \

#define X_enumMember( type, name, ...) name ,
namespace E_type { enum E { Xt_Type(X_enumMember) }; }
# undef X_enumMember

#define X_using(oldName, newName) using newName = oldName;
#define X_using_struct(oldName, newName) using newName = struct oldName;

Xt_BasicType(X_using);
Xt_CustomType(X_using_struct);


# undef X_using
# undef X_using_struct

// 表示长度的类型, 用来放入序列中
using Sz = u32;

using Imax = i64;
using Umax = u64;

using SpToken = Sz;
template<size_t N> using SpTokenArr = std::array<SpToken, N>;

// ===================== 错误类型层次 (v0.9.1) =====================
struct StreamPunkError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct SpVersionError : StreamPunkError { using StreamPunkError::StreamPunkError; };
struct SpTypeError : StreamPunkError { using StreamPunkError::StreamPunkError; };
struct SpDataError : StreamPunkError { using StreamPunkError::StreamPunkError; };
struct SpUninitializedError : StreamPunkError { using StreamPunkError::StreamPunkError; };

inline constexpr u32 makeVersion(u8 major, u8 minor = 0, u8 patch = 0, u8 custom = 0) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return (static_cast<u32>(major) << 24) | (static_cast<u32>(minor) << 16)
             | (static_cast<u32>(patch) << 8)  | static_cast<u32>(custom);
    } else {
        return (static_cast<u32>(custom) << 24) | (static_cast<u32>(patch) << 16)
             | (static_cast<u32>(minor) << 8)   | static_cast<u32>(major);
    }
}
inline constexpr u8 getVerMajor(u32 version) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return static_cast<u8>(version >> 24);
    } else {
        return static_cast<u8>(version & 0xFF);
    }
}
inline constexpr u8 getVerMinor(u32 version) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return static_cast<u8>(version >> 16);
    } else {
        return static_cast<u8>((version >> 8) & 0xFF);
    }
}
inline constexpr u8 getVerPatch(u32 version) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return static_cast<u8>(version >> 8);
    } else {
        return static_cast<u8>((version >> 16) & 0xFF);
    }
}
inline constexpr u8 getVerCustom(u32 version) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return static_cast<u8>(version & 0xFF);
    } else {
        return static_cast<u8>(version >> 24);
    }
}

inline constexpr u32 StreamPunkVer = makeVersion(0, 9, 0);

/*
用来放入流当中的指针类型 长度统一为64位
对于32位系统来说会浪费4字节本地空间 但是对于64位系统不可或缺
如果只考虑32位系统的数据使用,那可以改成u32,省点空间.
*/
using PtrValue = u64;

template<typename T> using Sptr = std::shared_ptr<T>;
template<typename T> using Wptr = std::weak_ptr  <T>;
template<typename T> using Uptr = std::unique_ptr<T>;

struct PtrRefInfo {
    Sz refCount = 0;
    enum Kind { e_raw, e_uptr, e_sptr } kind = e_raw;
    void* rawPtr = nullptr;               // e_raw 和 e_uptr 用此字段
    std::shared_ptr<void> sptr;           // e_sptr 用此字段
};
using DeepCopier = std::unordered_map<void*, PtrRefInfo>;

// ============================== 抽象 IO 接口 ==============================
// 解耦 O/I 与 std::iostream，允许用户对接自定义后端（内存 buffer、网络 socket 等）。

struct SpWriter {
    virtual void write(const void* data, size_t size) = 0;
    virtual ~SpWriter() = default;
};

struct SpReader {
    // 读取恰好 size 字节到 data。失败时抛异常，调用方无需检查返回值。
    virtual void read(void* data, size_t size) = 0;
    virtual ~SpReader() = default;
};

struct SpStreamWriter final : SpWriter {
    std::ostream& os;
    explicit SpStreamWriter(std::ostream& s) : os(s) {}
    void write(const void* data, size_t size) override {
        os.write(static_cast<const char*>(data), size);
    }
};

struct SpStreamReader final : SpReader {
    std::istream& is;
    explicit SpStreamReader(std::istream& s) : is(s) {}
    void read(void* data, size_t size) override {
        is.read(static_cast<char*>(data), size);
        if (!is.good()) {
            throw SpDataError("read failed: unexpected EOF or stream error");
        }
    }
};

/*
    这个流对象会在序列化和反序列化时,存一些上下文数据
    主要是为了避免重复序列化同一个指针,以及在反序列化时,避免重复创建对象
    使用时注意这一点.
*/
struct O {
    std::unique_ptr<SpStreamWriter> _ownedWriter;
    SpWriter* w = nullptr;
    std::unordered_set<PtrValue> ptrSet;

    explicit O(std::ostream& os)
        : _ownedWriter(std::make_unique<SpStreamWriter>(os))
        , w(_ownedWriter.get()) {}
    explicit O(SpWriter& writer) : w(&writer) {}

    void clear() {
        ptrSet.clear();
    }
}; // struct O

struct I {
    std::unique_ptr<SpStreamReader> _ownedReader;
    SpReader* r = nullptr;
    std::unordered_map<PtrValue, Sptr<void>> sptrSet;
    std::unordered_map<PtrValue, void* > ptrSet;

    explicit I(std::istream& is)
        : _ownedReader(std::make_unique<SpStreamReader>(is))
        , r(_ownedReader.get()) {}
    explicit I(SpReader& reader) : r(&reader) {}

    void clear() {
        sptrSet.clear();
        ptrSet.clear();
    }
}; // struct I

struct JsonVal;            // 前向声明 (DOM 模式)
struct JsonStreamReader;    // 前向声明 (流式模式)

/*
    自定义的类,必须直接或间接继承Base.
    如果只关注基础数据的序列化与反序列化,
    Base没有存在的必要.
    自定义的类当中,有指针的存在,这不可避免.
    本库在版本0.0.3对指针的初步支持,会有这样一个问题:
        C继承于B B继承于A
        A* c = new C();
        o << c;
        首次输出到流,如果是用基类指针,就被当基类对象处理.
    为解决这个问题, 用上多态, 让所有自定义类,继承Base这个基类.
*/
struct Base {
    static constexpr inline char const* _className = "Base";
    static constexpr inline char const* _baseName = "";
    static constexpr inline char const* _membersName[1] = {""};
    Base() = default;
    virtual ~Base() = default;
    virtual Sz typeID() const = 0; // 返回类型ID
    virtual void output(O& o) const {}
    virtual void input(I& i) {}
    virtual void deepCopyFrom(DeepCopier& dc, Base const& v) {} // 被拷贝的对象,其实际类型必须是这个类或者这个类的子类.
    virtual std::span<SpToken const> getDesc() const = 0;
    virtual void toJson(JsonVal& j) const {}     // JSON 序列化 DOM (v0.3.0)
    virtual void fromJson(JsonVal const& j) {}    // JSON 反序列化 DOM (v0.3.0)

    // ============== 流式 JSON 接口 (v0.3.1) ==============
    virtual void toJsonStream(std::ostream& os) const { os << "{}"; }
    virtual void writeJsonStreamMembers(std::ostream& os, bool& _first) const {}
    virtual void fromJsonStream(JsonStreamReader& r);
    virtual bool readJsonStreamMember(JsonStreamReader& r, std::string const& key) { return false; }
    virtual const char* getClassName() const { return _className; }  // 多态类型名 (v0.3.1)

    // ============== Schema 元数据接口 (v0.3.2) ==============
    virtual const char* getBaseName() const { return _baseName; }
    virtual std::span<const char* const> getMemberNames() const { return {}; }
    virtual size_t getMemberCount() const { return 0; }
};
inline O& operator<<(O& o, Base const& v) { v.output(o); return o; }
inline I& operator>>(I& i, Base& v) { v.input(i); return i; }


template<typename T> struct TypeID_t {
    constexpr inline static Sz id = static_cast<Sz>(E_type::e_unknowType);
    constexpr inline static E_type::E kind = E_type::e_unknowType;
};
template<> struct TypeID_t<Base> {
    constexpr inline static Sz id = static_cast<Sz>(E_type::Base);
    constexpr inline static E_type::E kind = E_type::e_customType;
};
#define X_DEF_TypeID_kind(type, newName, kind__) \
template<>\
struct TypeID_t<newName>{\
    constexpr inline static Sz id = static_cast<Sz>(E_type::newName);\
    constexpr inline static E_type::E kind = E_type::kind__;\
};
#define X_DEF_TypeID_basic(type,newName) X_DEF_TypeID_kind(type, newName, Base);
#define X_DEF_TypeID_custom(type,newName) X_DEF_TypeID_kind(type, newName, e_customType);

Xt_BasicType(X_DEF_TypeID_basic);
Xt_CustomType(X_DEF_TypeID_custom);

# undef X_DEF_TypeID_custom
# undef X_DEF_TypeID_basic
# undef X_DEF_TypeID_kind

namespace detail {
    constexpr static size_t customTypeBeginNum = static_cast<size_t>(E_type::Base) + 1;
    constexpr static size_t customTypeNum = static_cast<size_t>(E_type::e_customType) - customTypeBeginNum;
    using PFN_VoidPtrCreator = Base * (*)();

    template <typename T> constexpr bool trivially_copyable =
        std::is_trivially_copyable_v<std::remove_cvref_t<T>>
        &&
        !std::is_pointer_v<std::remove_cvref_t<T>>
    ;
} // namespace detail

// ============================== 跨模块注册表 ==============================
// 将所有全局状态集中到一个结构体中，用户通过 INIT_StreamPunk(&reg) 传入。
// 不传则使用函数内静态变量（向后兼容，单模块场景）。
// 跨 DLL 时，主程序创建 SpRegistry，传给各 DLL 的 INIT_StreamPunk(&reg)。
struct SpRegistry {
    std::array<detail::PFN_VoidPtrCreator, detail::customTypeNum> creators{};
    std::unordered_map<Sz, std::string> typeID2ClassName;
    std::unordered_map<Sz, std::string> ormTableMap;
};

inline SpRegistry*& _sp_registry_ptr() {
    static SpRegistry* ptr = nullptr;
    return ptr;
}

// 修改 all_custom_creator_pfn：优先使用外部注册表
namespace detail {
    inline auto& all_custom_creator_pfn() {
        auto* reg = _sp_registry_ptr();
        if (reg) return reg->creators;
        static std::array<PFN_VoidPtrCreator, customTypeNum> arr{};
        return arr;
    }
}

// typeID → 类名映射（Schema 用），优先使用外部注册表
inline std::unordered_map<Sz, std::string>& typeID2ClassName() {
    auto* reg = _sp_registry_ptr();
    if (reg) return reg->typeID2ClassName;
    static std::unordered_map<Sz, std::string> map;
    return map;
}

// 注册自定义类型的类名（在 INIT_StreamPunk 时调用）
// 用 requires { sizeof(T); } 守卫：T 不完整时跳过，避免 is_base_of_v 编译错误
template<typename T>
inline void registerSchemaClassName() {
    if constexpr (requires { sizeof(T); }) {
        if constexpr (std::is_base_of_v<Base, T>) {
            typeID2ClassName()[TypeID_t<T>::id] = T::_className;
        }
    }
}

template<typename T> struct is_vector_impl : std::false_type {};
template<typename T, typename A> struct is_vector_impl<std::vector<T, A>> : std::true_type {};
template<typename T> inline constexpr bool is_vector_v = is_vector_impl<T>::value;

template<typename T> struct is_deque_impl : std::false_type {};
template<typename T, typename A> struct is_deque_impl<std::deque<T, A>> : std::true_type {};
template<typename T> inline constexpr bool is_deque_v = is_deque_impl<T>::value;

template<typename T> struct is_list_impl : std::false_type {};
template<typename T, typename A> struct is_list_impl<std::list<T, A>> : std::true_type {};
template<typename T> inline constexpr bool is_list_v = is_list_impl<T>::value;

template<typename T> struct is_flist_impl : std::false_type {};
template<typename T, typename A> struct is_flist_impl<std::forward_list<T, A>> : std::true_type {};
template<typename T> inline constexpr bool is_flist_v = is_flist_impl<T>::value;

template<typename T> struct is_set_impl : std::false_type {};
template<typename T, typename C, typename A> struct is_set_impl<std::set<T, C, A>> : std::true_type {};
template<typename T> inline constexpr bool is_set_v = is_set_impl<T>::value;

template<typename T> struct is_uset_impl : std::false_type {};
template<typename T, typename H, typename E, typename A> struct is_uset_impl<std::unordered_set<T, H, E, A>> : std::true_type {};
template<typename T> inline constexpr bool is_uset_v = is_uset_impl<T>::value;

template<typename T> struct is_map_impl : std::false_type {};
template<typename K, typename V, typename C, typename A> struct is_map_impl<std::map<K, V, C, A>> : std::true_type {};
template<typename T> inline constexpr bool is_map_v = is_map_impl<T>::value;

template<typename T> struct is_umap_impl : std::false_type {};
template<typename K, typename V, typename H, typename E, typename A> struct is_umap_impl<std::unordered_map<K, V, H, E, A>> : std::true_type {};
template<typename T> inline constexpr bool is_umap_v = is_umap_impl<T>::value;

// ============================== 可选类型特征 ==============================

template<typename T> struct is_optional_impl : std::false_type {};
template<typename T> struct is_optional_impl<std::optional<T>> : std::true_type {};
template<typename T> inline constexpr bool is_optional_v = is_optional_impl<std::decay_t<T>>::value;

// ============================== 智能指针类型特征 ==============================

template<typename T> struct is_sptr_impl : std::false_type {};
template<typename T> struct is_sptr_impl<std::shared_ptr<T>> : std::true_type {};
template<typename T> inline constexpr bool is_sptr_v = is_sptr_impl<T>::value;

template<typename T> struct is_uptr_impl : std::false_type {};
template<typename T> struct is_uptr_impl<std::unique_ptr<T>> : std::true_type {};
template<typename T> inline constexpr bool is_uptr_v = is_uptr_impl<T>::value;

// ============================== 字符串类型特征 ==============================

template<typename T> struct is_string_impl : std::false_type {};
template<typename T> struct is_string_impl<std::basic_string<T>> : std::true_type {};
template<typename T> inline constexpr bool is_string_v = is_string_impl<T>::value;

// ============================== 组合特征 ==============================

template<typename T> inline constexpr bool is_ordered_container_v = is_vector_v<T> || is_deque_v<T> || is_list_v<T>;
template<typename T> inline constexpr bool is_assoc_container_v = is_map_v<T> || is_umap_v<T>;
template<typename T> inline constexpr bool is_set_container_v = is_set_v<T> || is_uset_v<T>;

/* ======================= 实现各类数据二进制输入输出 ===================== */

    /* ======================= 基本类型 ===================== */
inline O& operator<<(O& s, const u8  & v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline O& operator<<(O& s, const u16 & v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline O& operator<<(O& s, const u32 & v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline O& operator<<(O& s, const u64 & v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline O& operator<<(O& s, const i8  & v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline O& operator<<(O& s, const i16 & v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline O& operator<<(O& s, const i32 & v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline O& operator<<(O& s, const i64 & v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline O& operator<<(O& s, const f32 & v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline O& operator<<(O& s, const f64 & v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline O& operator<<(O& s, const ch  & v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }

inline O& operator<<(O& s, const ch8 & v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline O& operator<<(O& s, const ch16& v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline O& operator<<(O& s, const ch32& v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }
inline O& operator<<(O& s, const bl  & v) { s.w->write(reinterpret_cast<char const*>(&v), sizeof(v)); return s; }

inline I& operator>>(I& s,       u8  & v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s,       u16 & v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s,       u32 & v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s,       u64 & v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s,       i8  & v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s,       i16 & v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s,       i32 & v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s,       i64 & v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s,       f32 & v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s,       f64 & v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s,       ch  & v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }

inline I& operator>>(I& s,       ch8 & v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s,       ch16& v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s,       ch32& v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }
inline I& operator>>(I& s,       bl  & v) { s.r->read(reinterpret_cast<char*>(&v), sizeof(v)); return s; }

template<typename T> std::enable_if_t<std::is_enum_v<T>, O&> operator<<(O& o, T const& v) {
    o << static_cast<std::underlying_type_t<T>>(v);
    return o;
}
template<typename T> std::enable_if_t<std::is_enum_v<T>, I&> operator>>(I& i, T& v) {
    std::underlying_type_t<T> tmp;
    i >> tmp;
    v = static_cast<T>(tmp);
    return i;
}

    /* ======================= chrono的输入输出 ===================== */
struct StreamPunkTime {
    using AttoSec = std::chrono::duration<i64, std::atto>;

    i64 sec;
    i64 attoSec;

    template<typename Rep, typename Period>
    void set(std::chrono::duration<Rep, Period>const& v) {
        using namespace std::chrono;
        // 整数秒部分（向零取整）
        auto sec_part = duration_cast<seconds>(v);
        sec = sec_part.count();
        // 剩余不足1秒的部分
        auto rem = v - sec_part;

        // 统一使用精确的整数转换
        attoSec = duration_cast<AttoSec>(rem).count();
    }

    template<typename Rep, typename Period>
    void get(std::chrono::duration<Rep, Period>& v) const {
        using TargetDuration = std::chrono::duration<Rep, Period>;
        auto secs_as_target = std::chrono::duration_cast<TargetDuration>(std::chrono::seconds(sec));
        v = secs_as_target;
        // 如果存储类型是整数 并且目标单位粒度 粗于秒 则不必计算
        if constexpr (std::is_integral_v<Rep> && std::ratio_greater<Period, std::ratio<1>>::value) {
            return;
        }
        // 如果存储类型是整数 但目标单位粒度小于秒, 则截断计算.
        else if constexpr (std::is_integral_v<Rep>) {
            auto attos_as_tartget = std::chrono::duration_cast<TargetDuration>(AttoSec(attoSec));
            v += attos_as_tartget;
        }
        // 如果存储类型是浮点数, 则四舍五入计算
        else {
            // 计算目标单位与秒的比例
            constexpr double target_period = static_cast<double>(Period::num) / Period::den;
            constexpr double ratio = target_period > 1e-18 ? (1e-18 / target_period) : 0;
            // 整数类型：四舍五入处理
            double value_in_target_units = static_cast<double>(attoSec) * ratio;
            Rep rounded_value = static_cast<Rep>(value_in_target_units);

            // 处理边界情况：当值太小导致计算为0但不应忽略的情况
            if (rounded_value == 0 && attoSec > 0 && value_in_target_units > 0) {
                rounded_value = 1; // 最小有效值
            }
            else if (rounded_value == 0 && attoSec < 0 && value_in_target_units < 0) {
                rounded_value = -1; // 最小有效值
            }
            v += TargetDuration(rounded_value);
        }
    }
};

inline O& operator<<(O& o, StreamPunkTime const& v) { o << v.sec << v.attoSec; return o; }
inline I& operator>>(I& i, StreamPunkTime&       v) { i >> v.sec >> v.attoSec; return i; }

template<typename Rep, typename Period>
inline O& operator<<(O& o, std::chrono::duration<Rep, Period>const& v) {
    StreamPunkTime spTime;
    spTime.set(v);
    o << spTime;
    return o;
}
template<typename Rep, typename Period>
inline I& operator>>(I& i, std::chrono::duration<Rep, Period>& v) {
    StreamPunkTime spTime;
    i >> spTime;
    spTime.get(v);
    return i;
}

/*
    std::chrono::system_clock	​C++20起标准强制一致​：UNIX时间纪元（1970-01-01 00:00 UTC）
    std::chrono::steady_clock	​平台和实现相关​：一般以系统启动时间为起点
*/
template<typename Clock, typename Duration>
inline O& operator<<(O& o, std::chrono::time_point<Clock, Duration>const& tp) {
    o << tp.time_since_epoch(); return o;
}
template<typename Clock, typename Duration>
inline I& operator>>(I& i, std::chrono::time_point<Clock, Duration>& tp) {
    Duration dur;
    i >> dur;
    tp = std::chrono::time_point<Clock, Duration>(dur);
    return i;
}

    /* ======================= 连续空间输入输出 ===================== */
namespace detail {
    template <typename T> struct is_std_array : std::false_type {};
    template <typename T, size_t N> struct is_std_array<std::array<T, N>> : std::true_type {};
    template <typename T> constexpr bool is_std_array_v = is_std_array<T>::value;

    template<typename ValueType, typename T> inline void writeSpan(O& o, T const& v) {
        o << static_cast<Sz>(v.size());
        if constexpr (trivially_copyable<ValueType>) {
            /*
                如果 ValueType 是平凡可拷贝的类型，直接使用二进制写入
                然而会引来一个问题:如果ValueType是std::array,尺寸是固定已知的,所以它被序列化时,会被当作一个整体来处理,就不会先写入长度.
            */
            o.w->write(reinterpret_cast<char const*>(std::data(v)), sizeof(ValueType) * v.size());
        }
        else {
            for (auto&& x : v) {
                o << x;
            }
        }
    }
    template <typename ValueType, typename T> inline void readSpan(I& i, T& v) {
        Sz size;
        i >> size;
        if constexpr (!is_std_array_v<T>) {
            v.resize(size);
        }
        if constexpr (trivially_copyable<ValueType>) {
            i.r->read(reinterpret_cast<char*>(v.data()), sizeof(ValueType) * size);
        }
        else {
            for (auto& x : v) {
                i >> x;
            }
        }
    }
} // namespace detail

template<typename T, typename...Args> inline O& operator<<(O& o, std::vector<T, Args...>const& v) {
    detail::writeSpan<T>(o, v); return o;
}
template<typename T, size_t N       > inline O& operator<<(O& o, std::array <T, N      >const& v) {
    detail::writeSpan<T>(o, v); return o;
}
template<typename T, typename...Args> inline O& operator<<(O& o, std::basic_string <T, Args...>const& v) {
    detail::writeSpan<T>(o, v); return o;
}
template<size_t N> inline O& operator<<(O& o, std::bitset<N>const& v) {
    // 统一使用 (N+7)/8 字节紧凑格式，快路径和慢路径产出完全一致
    constexpr size_t byteCount = (N + 7) / 8;
    if constexpr (std::endian::native == std::endian::little && std::is_trivially_copyable_v<std::bitset<N>>) {
        // 快路径：小端平台 + 平凡可拷贝，直接写内部表示的前 byteCount 字节
        o.w->write(reinterpret_cast<char const*>(&v), byteCount);
    } else {
        // 慢路径：逐位填充到 byteCount 字节，与快路径格式一致
        std::array<u8, byteCount> bytes{};
        for (size_t i = 0; i < N; ++i) {
            if (v[i]) {
                bytes[i / 8] |= static_cast<u8>(1 << (i % 8));
            }
        }
        o.w->write(reinterpret_cast<char const*>(bytes.data()), byteCount);
    }
    return o;
}
template<typename T, typename...Args> inline I& operator>>(I& i, std::vector<T, Args...>& v) {
    detail::readSpan<T>(i, v); return i;
}
template<typename T, size_t N       > inline I& operator>>(I& i, std::array <T, N      >& v) {
    detail::readSpan<T>(i, v); return i;
}
template<typename T, typename...Args> inline I& operator>>(I& i, std::basic_string <T, Args...>& v) {
    detail::readSpan<T>(i, v); return i;
}

template<size_t   N> inline I& operator>>(I& i, std::bitset<N>& v) {
    constexpr size_t byteCount = (N + 7) / 8;
    if constexpr (std::endian::native == std::endian::little && std::is_trivially_copyable_v<std::bitset<N>>) {
        // 快路径：小端平台 + 平凡可拷贝，直接读入内部表示的前 byteCount 字节
        v.reset();
        i.r->read(reinterpret_cast<char*>(&v), byteCount);
    } else {
        // 慢路径：从字节数组逐位还原
        std::array<u8, byteCount> bytes{};
        i.r->read(reinterpret_cast<char*>(bytes.data()), byteCount);
        v.reset();
        for (size_t idx = 0; idx < N; ++idx) {
            v[idx] = (bytes[idx / 8] >> (idx % 8)) & 1;
        }
    }
    return i;
}


    /* ======================= 不连续的容器类型 ===================== */
namespace detail {
    template<typename ValueType, typename T> inline void write(O& o, T const& v) {
        Sz sz = static_cast<Sz>(v.size());
        o << sz;
        for (auto&& x : v) {
            o << x;
        }
    }
    template<typename T> inline void writeMap(O& o, T const& v) {
        Sz sz = static_cast<Sz>(v.size());
        o << sz;
        for (auto&& x : v) {
            o << x.first << x.second;
        }
    }
}   // namespace detail

template<typename T, typename... Args>             inline O& operator<<(O& o, std::deque        <T,     Args...>const& v) {
    detail::write<T>(o, v); return o;
}
template<typename T, typename... Args>             inline O& operator<<(O& o, std::list         <T,     Args...>const& v) {
    detail::write<T>(o, v); return o;
}
template<typename T, typename... Args>             inline O& operator<<(O& o, std::set          <T,     Args...>const& v) {
    detail::write<T>(o, v); return o;
}
template<typename T, typename... Args>             inline O& operator<<(O& o, std::unordered_set<T,     Args...>const& v) {
    detail::write<T>(o, v); return o;
}
template<typename T, typename... Args>             inline O& operator<<(O& o, std::forward_list <T,     Args...>const& v) {
    Sz sz = 0;
    for (auto&& x : v) {
        ++sz;
    }
    o << sz;
    for (auto&& x : v) {
        o << x;
    }
    return o;
}
template<typename K, typename V, typename... Args> inline O& operator<<(O& o, std::map          <K, V,  Args...>const& v) {
    detail::writeMap(o, v); return o;
}
template<typename K, typename V, typename... Args> inline O& operator<<(O& o, std::unordered_map<K, V,  Args...>const& v) {
    detail::writeMap(o, v); return o;
}

namespace detail {
    template<typename T> inline void read(I& i, T& v) {
        Sz sz;
        i >> sz;
        for (size_t j = 0; j < sz; ++j) {
            v.emplace_back();
            i >> v.back();
        }
    }
    template<typename T> inline void readSet(I& i, T& v) {
        Sz sz;
        i >> sz;
        for (size_t j = 0; j < sz; ++j) {
            typename T::value_type temp{};
            i >> temp;
            v.emplace(std::move(temp));
        }
    }
    template<typename T> inline void readMap(I& i, T& v) {
        Sz sz;
        i >> sz;
        for (size_t j = 0; j < sz; ++j) {
            typename T::key_type k{};
            typename T::mapped_type val{};
            i >> k;
            i >> val;
            v.emplace(std::move(k), std::move(val));
        }
    }
}   // namespace detail

template<typename T, typename... Args> inline I& operator>>(I& i, std::deque        <T, Args...>& v) {
    detail::read(i, v); return i;
}
template<typename T, typename... Args> inline I& operator>>(I& i, std::list         <T, Args...>& v) {
    detail::read(i, v); return i;
}
template<typename T, typename... Args> inline I& operator>>(I& i, std::set          <T, Args...>& v) {
    detail::readSet(i, v); return i;
}
template<typename T, typename... Args> inline I& operator>>(I& i, std::unordered_set<T, Args...>& v) {
    detail::readSet(i, v); return i;
}
template<typename T, typename... Args> inline I& operator>>(I& i, std::forward_list <T, Args...>& v) {
    Sz sz;
    i >> sz;
    auto pos = v.before_begin();
    for (Sz j = 0; j < sz; ++j) {
        T temp{};
        i >> temp;
        pos = v.insert_after(pos, std::move(temp));
    }
    return i;
}
template<typename K, typename V, typename... Args> inline I& operator>>(I& i, std::map          <K, V, Args...>& v) {
    detail::readMap(i, v); return i;
}
template<typename K, typename V, typename... Args> inline I& operator>>(I& i, std::unordered_map<K, V, Args...>& v) {
    detail::readMap(i, v); return i;
}

    /* ======================= 指针类型 ===================== */
/*
潜在Bug:
    当对象被释放之后,该空间可能会被复用,成为其他新建的对象的空间.
    这个情况下,将新的对象再放入流,就不会对新建的对象实际输出,从而造成反序列化时读取数据发生错误.
*/
namespace detail {
    inline Base* create_custom_type_from_typeID(Sz typeID) {
        size_t creatorPfnIdx = typeID - (static_cast<size_t>(E_type::Base) + 1);
        auto& pfnArr = all_custom_creator_pfn();
        if (creatorPfnIdx >= pfnArr.size() || pfnArr[creatorPfnIdx] == nullptr) {
            std::stringstream ss;
            ss << "i >> typeID : Invalid typeID " << typeID << ". Valid range: 0-" << (pfnArr.size() - 1);
            throw SpTypeError(ss.str());
        }
        return pfnArr[creatorPfnIdx]();
    }
    template<typename T> inline void createEmptyObj(I& i, T*& v){
        if constexpr (TypeID_t<T>::kind == E_type::e_customType || std::is_same_v<T, Base>) {
            Sz typeID;
            i >> typeID;
            void* voidPtr = create_custom_type_from_typeID(typeID);
            v = static_cast<T*>(static_cast<Base*>(voidPtr));
        }
        else {
            v = new T{};
        }
    }
}   // namespace detail

template<typename T> inline O& operator<<(O& o, T const* const v) {
    auto const p = reinterpret_cast<PtrValue>(v);
    o << p;
    if (v == nullptr) {
        return o;
    }
    if (o.ptrSet.find(p) == o.ptrSet.end()) {
        o.ptrSet.emplace(p);
        if constexpr (TypeID_t<T>::kind == E_type::e_customType || std::is_same_v<T, Base>) {
            o << v->typeID();
        }
        o << (*v);
    }
    return o;
}
template<typename T> inline O& operator<<(O& o, Sptr<T> const& v) { o << v.get(); return o; }
template<typename T> inline O& operator<<(O& o, Wptr<T> const& v) { o << v.lock().get(); return o; }
template<typename T> inline O& operator<<(O& o, Uptr<T> const& v) { o << v.get(); return o; }

template<typename T> inline I& operator>>(I& i, T*& v) {
    PtrValue p = 0;
    i >> p;
    if ( (void*)p == nullptr) {
        v = nullptr;
        return i;
    }
    auto ptrIter = i.ptrSet.find(p);
    auto sptrIter = i.sptrSet.find(p);
    if (ptrIter != i.ptrSet.end()) {
        v = reinterpret_cast<T*>(ptrIter->second);
        return i;
    }
    if (sptrIter != i.sptrSet.end()) {
        v = reinterpret_cast<T*>(sptrIter->second.get());
        return i;
    }
    detail::createEmptyObj(i, v);
    i >> (*v);
    i.ptrSet.emplace(p, v);
    return i;
}
template<typename T> inline I& operator>>(I& i, Sptr<T>& v) {
    PtrValue p = 0;
    i >> p;
    if (p == 0) {
        v.reset();
        return i;
    }
    auto iter = i.sptrSet.find(p);
    if (iter != i.sptrSet.end()) {
        v = std::static_pointer_cast<T>(iter->second);
        return i;
    }
    auto ptrIter = i.ptrSet.find(p);
    if (ptrIter != i.ptrSet.end()) {
        v.reset((T*)(ptrIter->second));
        return i;
    }
    T* ptr = nullptr;
    detail::createEmptyObj(i, ptr);
    //auto ptr = new T{};
    v.reset(ptr);
    i.sptrSet.emplace(p, v);
    i >> *ptr;
    return i;
}
template<typename T> inline I& operator>>(I& i, Wptr<T>& v) {
    PtrValue p;
    i >> p;
    v.reset();
    if (p == 0) {
        return i;
    }
    auto sIter = i.sptrSet.find(p);
    if (sIter != i.sptrSet.end()) {
        v = std::static_pointer_cast<T>(sIter->second);
        return i;
    }
    auto ptrIter = i.ptrSet.find(p);
    if (ptrIter != i.ptrSet.end()) {
        Sptr<T> sptr(reinterpret_cast<T*>(ptrIter->second));
        i.sptrSet.emplace(p, sptr);
        v = sptr;
        return i;
    }
    T* ptr = nullptr;
    detail::createEmptyObj(i, ptr);
    Sptr<T> sptr(ptr);
    i >> *sptr;
    v = sptr;
    i.sptrSet.emplace(p, std::move(sptr));
    return i;
}
template<typename T> inline I& operator>>(I& i, Uptr<T>& v) { T* p{}; i >> p; v.reset(p); return i; }

    /* ======================= 仅限输出 ===================== */
template<typename T> inline O& operator<<(O& o, std::basic_string_view<T> const& v) {
    detail::writeSpan<T>(o, v); return o;
}
template<typename T> inline O& operator<<(O& o, std::initializer_list<T> const& v) {
    detail::writeSpan<T>(o, v); return o;
}
template<typename T> inline O& operator<<(O& o, std::span<T> const& v) {
    detail::writeSpan<T>(o, v); return o;
}

    /* ==================== optional filesystem::path atomic ==================== */
template<typename T> inline O& operator<<(O& o, std::optional<T>const& v) {
    bool has_value = v.has_value();
    o << has_value;
    if (has_value) {
        o << v.value();
    }
    return o;
}
template<typename T> inline I& operator>>(I& i, std::optional<T>& v) {
    bool has_value = false;
    i >> has_value;
    if (has_value) {
        v.emplace();
        i >> v.value();
    }
    else {
        v.reset();
    }
    return i;
}

inline O& operator<<(O& o, std::filesystem::path const& v) {
    std::u8string u8str = v.u8string();
    o << u8str;
    return o;
}
inline I& operator>>(I& i, std::filesystem::path& v) {
    std::u8string u8str;
    i >> u8str;
    v = u8str;
    return i;
}


template<typename T> inline O& operator<<(O& o, std::atomic_ref<T>const& v) {
    T t = v.load(std::memory_order_acquire);
    o << t;
    return o;
}
template<typename T> inline I& operator>>(I& i, std::atomic_ref<T>& v) {
    T t;
    i >> t;
    v.store(t, std::memory_order_release);
    return i;
}
template<typename T> inline O& operator<<(O& o, std::atomic<T>const& v) {
    T t = v.load(std::memory_order_acquire);
    o << t;
    return o;
}
template<typename T> inline I& operator>>(I& i, std::atomic<T>& v) {
    T t;
    i >> t;
    v.store(t, std::memory_order_release);
    return i;
}

/* =================================== variant =================================== */

template<typename... Args> inline O& operator<<(O& o, std::variant<Args...> const& v) {
    o << (Sz)v.index();
    std::visit([&](auto&& arg) { o << arg; }, v);
    return o;
}
namespace detail {
    template<size_t currIdx=0, typename... Args>
    inline void inputVariant(I& i, std::variant<Args...>& v, Sz idx) {
        constexpr auto sz = sizeof...(Args);
        if (idx == currIdx) {
            using T = std::variant_alternative_t<currIdx, std::variant<Args...>>;
            T value;
            i >> value;
            v = std::move(value);
            return;
        }
        if constexpr (currIdx + 1 < sz) {
            inputVariant<currIdx + 1>(i, v, idx);
        }
    }
}
template<typename... Args> inline I& operator>>(I& i, std::variant<Args...>& v) {
    Sz idx = 0;
    i >> idx;
    constexpr std::size_t N = sizeof...(Args);
    if (idx >= N) {
        throw SpDataError("i >> std::variant: index out of range");
    }
    detail::inputVariant(i, v, idx);
    return i;
}

/* =================================== tuple =================================== */
template<typename... Args> inline O& operator<<(O& o, std::tuple<Args...> const& v) {
    std::apply([&](const auto&... elements) {
        ((o << elements), ...);
    }, v);
    return o;
}

template<typename... Args> inline I& operator>>(I& i, std::tuple<Args...>& v) {
    std::apply([&](auto&&... args) {
        ((i >> std::forward<decltype(args)>(args)), ...);
    }, v);
    return i;
}

/* ======================================= 深拷贝 ======================================= */

inline void deepCopy(DeepCopier&, u8  & dstV, u8  const& srcV) { dstV = srcV; }
inline void deepCopy(DeepCopier&, u16 & dstV, u16 const& srcV) { dstV = srcV; }
inline void deepCopy(DeepCopier&, u32 & dstV, u32 const& srcV) { dstV = srcV; }
inline void deepCopy(DeepCopier&, u64 & dstV, u64 const& srcV) { dstV = srcV; }
inline void deepCopy(DeepCopier&, i8  & dstV, i8  const& srcV) { dstV = srcV; }
inline void deepCopy(DeepCopier&, i16 & dstV, i16 const& srcV) { dstV = srcV; }
inline void deepCopy(DeepCopier&, i32 & dstV, i32 const& srcV) { dstV = srcV; }
inline void deepCopy(DeepCopier&, i64 & dstV, i64 const& srcV) { dstV = srcV; }
inline void deepCopy(DeepCopier&, f32 & dstV, f32 const& srcV) { dstV = srcV; }
inline void deepCopy(DeepCopier&, f64 & dstV, f64 const& srcV) { dstV = srcV; }
inline void deepCopy(DeepCopier&, ch  & dstV, ch  const& srcV) { dstV = srcV; }

inline void deepCopy(DeepCopier&, ch8 & dstV, ch  const& srcV) { dstV = srcV; }
inline void deepCopy(DeepCopier&, ch16& dstV, ch  const& srcV) { dstV = srcV; }
inline void deepCopy(DeepCopier&, ch32& dstV, ch  const& srcV) { dstV = srcV; }
inline void deepCopy(DeepCopier&, bl  & dstV, bl  const& srcV) { dstV = srcV; }

template <typename T> inline std::enable_if_t<std::is_enum_v<T>, void> deepCopy(DeepCopier&, T& dstV, const T& srcV) { dstV = srcV; }

template<typename Rep, typename Period>
inline void deepCopy(DeepCopier&, std::chrono::duration<Rep, Period>& dstV, std::chrono::duration<Rep, Period>const& srcV) { dstV = srcV; }

template<typename Clock, typename Duration>
inline void deepCopy(DeepCopier&, std::chrono::time_point<Clock, Duration>& dstV, std::chrono::time_point<Clock, Duration>const& srcV) { dstV = srcV; }

template<typename T, typename...Args> inline void deepCopy(DeepCopier& dc, std::vector<T, Args...>& dstV, std::vector<T, Args...>const& srcV) {
    if (std::addressof(dstV) == std::addressof(srcV)) {
        return;
    }
    dstV.clear();
    dstV.reserve(srcV.size());
    for (const auto& srcElement : srcV) {
        dstV.emplace_back();
        deepCopy(dc, dstV.back(), srcElement);
    }
}

template<typename T, size_t N> inline void deepCopy(DeepCopier& dc, std::array <T, N>& dstV, std::array <T, N>const& srcV) {
    if (std::addressof(dstV) == std::addressof(srcV)) {
        return;
    }
    for (size_t i = 0; i < N; ++i) {
        deepCopy(dc, dstV[i], srcV[i]);
    }
}

template<typename T, typename...Args> inline void deepCopy(DeepCopier&, std::basic_string <T, Args...>& dstV, std::basic_string <T, Args...>const& srcV) { dstV = srcV; }

template<size_t N> inline void deepCopy(DeepCopier&, std::bitset<N>& dstV, std::bitset<N>const& srcV) { dstV = srcV; }

namespace detail {
    template<typename T> inline void deepCopyContainer(DeepCopier& dc, T& dstV, T const& srcV) {
        if (std::addressof(dstV) == std::addressof(srcV)) {
            return;
        }
        dstV.clear();
        for (const auto& srcElement : srcV) {
            dstV.emplace_back();
            deepCopy(dc, dstV.back(), srcElement);
        }
    }
    template<typename T> inline void deepCopySet(DeepCopier& dc, T& dstV, T const& srcV) {
        if (std::addressof(dstV) == std::addressof(srcV)) {
            return;
        }
        dstV.clear();
        for (auto const& srcElement : srcV) {
            typename T::value_type newElement{};
            deepCopy(dc, newElement, srcElement);
            dstV.emplace(std::move(newElement));
        }
    }
    template<typename T> inline void deepCopyMap(DeepCopier& dc, T& dstV, T const& srcV) {
        if (std::addressof(dstV) == std::addressof(srcV)) {
            return;
        }
        dstV.clear();
        auto hint = dstV.cend();
        for (const auto& [srcKey, srcVal] : srcV) {
            typename T::key_type newKey;
            deepCopy(dc, newKey, srcKey);
            typename T::value_type::second_type newVal;
            deepCopy(dc, newVal, srcVal);
            hint = dstV.emplace_hint(hint, std::move(newKey), std::move(newVal));
        }
    }

}   // namespace detail

template<typename T, typename... Args> inline void deepCopy(DeepCopier& dc, std::deque        <T, Args...>& dstV, std::deque        <T, Args...>const& srcV) { detail::deepCopyContainer(dc, dstV, srcV); }
template<typename T, typename... Args> inline void deepCopy(DeepCopier& dc, std::list         <T, Args...>& dstV, std::list         <T, Args...>const& srcV) { detail::deepCopyContainer(dc, dstV, srcV); }
template<typename T, typename... Args> inline void deepCopy(DeepCopier& dc, std::forward_list <T, Args...>& dstV, std::forward_list <T, Args...>const& srcV) {
    if (std::addressof(dstV) == std::addressof(srcV)) {
        return;
    }
    dstV.clear();
    if (srcV.empty()) {
        return;
    }
    auto srcIt = srcV.begin();
    auto current = dstV.before_begin();
    current = dstV.insert_after(current, T{});
    deepCopy(dc, *current, *srcIt);
    ++srcIt;
    while (srcIt != srcV.end()) {
        current = dstV.insert_after(current, T{});
        deepCopy(dc, *current, *srcIt);
        ++srcIt;
    }
}
template<typename T, typename... Args> inline void deepCopy(DeepCopier& dc, std::set          <T, Args...>& dstV, std::set          <T, Args...>const& srcV) {
    detail::deepCopySet(dc, dstV, srcV);
}
template<typename T, typename... Args> inline void deepCopy(DeepCopier& dc, std::unordered_set<T, Args...>& dstV, std::unordered_set<T, Args...>const& srcV) {
    detail::deepCopySet(dc, dstV, srcV);
}
template<typename K, typename V, typename... Args> inline void deepCopy(DeepCopier& dc, std::map          <K, V, Args...>& dstV, std::map          <K, V, Args...>const& srcV) {
    detail::deepCopyMap(dc, dstV, srcV);
}
template<typename K, typename V, typename... Args> inline void deepCopy(DeepCopier& dc, std::unordered_map<K, V, Args...>& dstV, std::unordered_map<K, V, Args...>const& srcV) {
    detail::deepCopyMap(dc, dstV, srcV);
}

template<typename T> inline void deepCopy(DeepCopier& dc, T*& dstV, T*const& srcV) {
    if (std::addressof(dstV) == std::addressof(srcV)) {
        return;
    }
    if (srcV == nullptr) {
        dstV = nullptr;
        return;
    }
    void* key = std::remove_cv_t<T*>(srcV);
    auto itr = dc.find(key);
    if (itr == dc.end()) {
        if constexpr (TypeID_t<T>::kind == E_type::e_customType || std::is_same_v<T, Base>) {
            Base* ptr = detail::create_custom_type_from_typeID(srcV->typeID());
            dstV = static_cast<T*>(ptr);
            dc.emplace(key, PtrRefInfo{ .refCount = 0, .kind = PtrRefInfo::e_raw, .rawPtr = dstV });
            dstV->deepCopyFrom(dc, *srcV);
        }
        else {
            dstV = new T();
            dc.emplace(key, PtrRefInfo{ .refCount = 0, .kind = PtrRefInfo::e_raw, .rawPtr = dstV });
            deepCopy(dc, *dstV, *srcV);
        }
        return;
    }
    auto& info = itr->second;
    switch (info.kind) {
        case PtrRefInfo::e_raw : dstV = static_cast<T*>(info.rawPtr); break;
        case PtrRefInfo::e_uptr: throw SpTypeError("deepCopy raw ptr: target already owned by Uptr"); break;
        case PtrRefInfo::e_sptr: dstV = static_cast<T*>(info.sptr.get()); break;
        default: throw SpTypeError("PtrRefInfo kind error!");
    }
}
template<typename T> inline void deepCopy(DeepCopier& dc, Sptr<T>& dstV, Sptr<T> const& srcV) {
    if (std::addressof(dstV) == std::addressof(srcV)) {
        return;
    }
    if (srcV == nullptr) {
        dstV = nullptr;
        return;
    }
    void* key = static_cast<void*>(srcV.get());
    auto itr = dc.find(key);
    if (itr == dc.end()) {
        if constexpr (TypeID_t<T>::kind == E_type::e_customType || std::is_same_v<T, Base>) {
            Base* ptr = detail::create_custom_type_from_typeID(srcV->typeID());
            dstV.reset(static_cast<T*>(ptr));
            dc.emplace(key, PtrRefInfo{ .refCount = 0, .kind = PtrRefInfo::e_sptr, .sptr = dstV });
            dstV->deepCopyFrom(dc, *srcV);
        }
        else {
            dstV.reset(new T());
            dc.emplace(key, PtrRefInfo{ .refCount = 0, .kind = PtrRefInfo::e_sptr, .sptr = dstV });
            deepCopy(dc, *dstV, *srcV);
        }
        return;
    }
    auto& info = itr->second;
    switch (info.kind) {
        case PtrRefInfo::e_raw : dstV.reset(static_cast<T*>(info.rawPtr)); info.kind = PtrRefInfo::e_sptr; info.sptr = dstV; break;
        case PtrRefInfo::e_sptr: dstV = std::static_pointer_cast<T>(info.sptr); break;
        case PtrRefInfo::e_uptr: throw SpTypeError("Expecting an Sptr, but occupied by Uptr"); break;
        default: throw SpTypeError("PtrRefInfo kind error!");
    }
}
template<typename T> inline void deepCopy(DeepCopier& dc, Wptr<T>& dstV, Wptr<T> const& srcV) {
    if (std::addressof(dstV) == std::addressof(srcV)) {
        return;
    }
    auto srcSptr = srcV.lock();
    if (srcSptr == nullptr) {
        dstV.reset();
        return;
    }
    Sptr<T> dstSptr;
    deepCopy(dc, dstSptr, srcSptr);
    dstV = dstSptr;
}
template<typename T> inline void deepCopy(DeepCopier& dc, Uptr<T>& dstV, Uptr<T> const& srcV) {
    if (std::addressof(dstV) == std::addressof(srcV)) {
        return;
    }
    if (srcV.get() == nullptr) {
        dstV.reset();
        return;
    }
    void* key = static_cast<void*>(srcV.get());
    auto itr = dc.find(key);
    if (itr == dc.end()) {
        if constexpr (TypeID_t<T>::kind == E_type::e_customType || std::is_same_v<T,Base>) {
            Base* ptr = detail::create_custom_type_from_typeID(srcV->typeID());
            dstV.reset(static_cast<T*>(ptr));
            dc.emplace(key, PtrRefInfo{ .refCount = 0, .kind = PtrRefInfo::e_uptr, .rawPtr = dstV.get()});
            dstV->deepCopyFrom(dc, *srcV);
        }
        else {
            dstV.reset(new T());
            dc.emplace(key, PtrRefInfo{ .refCount = 0, .kind = PtrRefInfo::e_uptr, .rawPtr = dstV.get()});
            deepCopy(dc, *dstV, *srcV);
        }
        return;
    }
    auto& info = itr->second;
    switch (info.kind) {
        case PtrRefInfo::e_raw : dstV.reset(static_cast<T*>(info.rawPtr)); info.kind = PtrRefInfo::e_uptr; break;
        case PtrRefInfo::e_sptr: throw SpTypeError("Expecting a Uptr, but occupied by Sptr"); break;
        case PtrRefInfo::e_uptr: throw SpTypeError("Expecting a Uptr, but target already owned by another Uptr"); break;
        default: throw SpTypeError("PtrRefInfo kind error!");
    }
}

template<typename T> inline void deepCopy(DeepCopier& dc, std::optional<T>& dstV, std::optional<T> const& srcV) {
    if (std::addressof(dstV) == std::addressof(srcV)) {
        return;
    }
    if (!srcV.has_value()) {
        dstV.reset();
        return;
    }
    if (!dstV.has_value()) {
        dstV.emplace();
    }
    deepCopy(dc, *dstV, *srcV);
}

namespace detail {
    template <typename... Args, size_t... Is> void deepCopyTupleImpl(DeepCopier& dc, std::tuple<Args...>& dst, const std::tuple<Args...>& src, std::index_sequence<Is...>) {
        static_assert(sizeof...(Args) == sizeof...(Is), "Size mismatch");
        (deepCopy(dc, std::get<Is>(dst), std::get<Is>(src)), ...);
    }
}
template <typename... Args> void deepCopy(DeepCopier& dc, std::tuple<Args...>& dst, const std::tuple<Args...>& src) {
    if (&dst == &src) {
        return;
    }
    detail::deepCopyTupleImpl(dc, dst, src,std::make_index_sequence<sizeof...(Args)>{});
}

// std::filesystem::path
inline void deepCopy(DeepCopier&, std::filesystem::path& dst, std::filesystem::path const& src) {
    if (&dst != &src) dst = src;
}

// std::atomic<T>
template<typename T> void deepCopy(DeepCopier&, std::atomic<T>& dst, std::atomic<T> const& src) {
    dst.store(src.load());
}

// std::variant<Args...>
template<typename... Args> void deepCopy(DeepCopier& dc, std::variant<Args...>& dst, std::variant<Args...> const& src) {
    if (&dst == &src) return;
    std::visit([&](auto const& val) {
        using T = std::decay_t<decltype(val)>;
        if (!std::holds_alternative<T>(dst)) dst = T{};
        deepCopy(dc, std::get<T>(dst), val);
    }, src);
}


template<typename T> inline void deepCopy(DeepCopier& dc, T& dstV, T const& srcV) {
    if constexpr (TypeID_t<T>::kind == E_type::e_customType || std::is_same_v<T, Base>) {
        dstV.deepCopyFrom(dc, srcV);
    }
    else {
        dstV = srcV;
    }
}


// =================================== 类型描述 ===================================
namespace detail {
    template <size_t... Ns> constexpr auto concat_arrays(const SpTokenArr<Ns>&... arrays) {
        constexpr size_t total_size = (Ns + ... + 0);
        SpTokenArr<total_size> result{};
        size_t index = 0;
        ((std::copy_n(arrays.begin(), Ns, result.begin() + index), index += Ns), ...);
        return result;
    }
}


template<typename... Args> struct TypeDesc;
template<> struct TypeDesc<u8  > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::u8  ) }; };
template<> struct TypeDesc<u16 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::u16 ) }; };
template<> struct TypeDesc<u32 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::u32 ) }; };
template<> struct TypeDesc<u64 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::u64 ) }; };
template<> struct TypeDesc<i8  > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::i8  ) }; };
template<> struct TypeDesc<i16 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::i16 ) }; };
template<> struct TypeDesc<i32 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::i32 ) }; };
template<> struct TypeDesc<i64 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::i64 ) }; };
template<> struct TypeDesc<f32 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::f32 ) }; };
template<> struct TypeDesc<f64 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::f64 ) }; };
template<> struct TypeDesc<ch  > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::ch  ) }; };

template<> struct TypeDesc<ch8 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::ch8 ) }; };
template<> struct TypeDesc<ch16> { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::ch16) }; };
template<> struct TypeDesc<ch32> { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::ch32) }; };
template<> struct TypeDesc<bl  > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::bl  ) }; };

template<typename Rep, typename Period>
struct TypeDesc<std::chrono::duration<Rep, Period>> {
    static inline constexpr auto v = detail::concat_arrays(
        SpTokenArr<1>{ static_cast<SpToken>(E_type::dur) },
        TypeDesc<Rep>::v,
        SpTokenArr<2>{ static_cast<SpToken>(Period::num), static_cast<SpToken>(Period::den) }
    );
};

template<typename Clock, typename Duration>
struct TypeDesc<std::chrono::time_point<Clock, Duration>> {
    static inline constexpr auto v = detail::concat_arrays(
        SpTokenArr<1>{ static_cast<SpToken>(E_type::timepoint) },
        TypeDesc<Duration>::v
    );
};

template<typename T, typename... Args> struct TypeDesc<std::vector<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::vector)}, TypeDesc<T>::v);
};

template<typename T, typename... Args> struct TypeDesc<std::basic_string<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::string)}, TypeDesc<T>::v);
};

template<typename T, Sz N> struct TypeDesc<std::array<T, N>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<2>{static_cast<SpToken>(E_type::array), N}, TypeDesc<T>::v);
};

template<Sz N> struct TypeDesc<std::bitset<N>> { static inline constexpr auto v = SpTokenArr<2>{ static_cast<SpToken>(E_type::bitset), N }; };

template<typename T, typename... Args> struct TypeDesc<std::deque<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::deque)}, TypeDesc<T>::v);
};

template<typename T, typename... Args> struct TypeDesc<std::list<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::list)}, TypeDesc<T>::v);
};

template<typename T, typename... Args> struct TypeDesc<std::forward_list<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::flist)}, TypeDesc<T>::v);
};

template<typename T, typename... Args> struct TypeDesc<std::set<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::set)}, TypeDesc<T>::v);
};

template<typename T, typename... Args> struct TypeDesc<std::unordered_set<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::uset)}, TypeDesc<T>::v);
};

template<typename K, typename V, typename... Args> struct TypeDesc<std::map<K, V, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::map)}, TypeDesc<K>::v, TypeDesc<V>::v);
};

template<typename K, typename V, typename... Args> struct TypeDesc<std::unordered_map<K, V, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::umap)}, TypeDesc<K>::v, TypeDesc<V>::v);
};

template<typename T> struct TypeDesc<Sptr<T>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::sptr)}, TypeDesc<T>::v);
};

template<typename T> struct TypeDesc<Wptr<T>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::wptr)}, TypeDesc<T>::v);
};

template<typename T> struct TypeDesc<Uptr<T>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::uptr)}, TypeDesc<T>::v);
};

template<typename T> struct TypeDesc<std::optional<T>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::opt)}, TypeDesc<T>::v);
};

template<> struct TypeDesc<std::filesystem::path> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::path) };
};

template<typename T> struct TypeDesc<std::atomic<T>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::atomic)}, TypeDesc<T>::v);
};

template<typename... Args> struct TypeDesc<std::variant<Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::variant)}, detail::concat_arrays(TypeDesc<Args>::v...), SpTokenArr<1>{static_cast<SpToken>(E_type::ed)});
};

template<typename... Args> struct TypeDesc<std::tuple<Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::tuple)}, detail::concat_arrays(TypeDesc<Args>::v...), SpTokenArr<1>{static_cast<SpToken>(E_type::ed)});
};

template<> struct TypeDesc<Base> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};

template<typename T> struct TypeDesc<T*> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::ptr)}, TypeDesc<T>::v);
};
template<typename T> struct TypeDesc<T const> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::cst)}, TypeDesc<T>::v);
};
template<> struct TypeDesc<void*> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(static_cast<SpToken>(E_type::voidPtr)) };
};

// enum 类型：映射为底层整数类型
template<typename T> struct TypeDesc<T, std::enable_if_t<std::is_enum_v<T>>> {
    static inline constexpr auto v = TypeDesc<std::underlying_type_t<T>>::v;
};

// 自定义类型的 TypeDesc：通过 SFINAE 自动检测类型是否有 _desc
// 有 _desc（UseData/UseDataPod 生成）则使用完整描述符，否则使用类型 ID
template<typename T, typename = void>
struct TypeDescCustom {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(TypeID_t<T>::id) };
};
template<typename T>
struct TypeDescCustom<T, std::void_t<decltype(T::_desc)>> {
    static inline constexpr auto v = T::_desc;
};

#define X_CustomTypeDesc(type, name) \
template<> struct TypeDesc<name> : TypeDescCustom<name> {};
Xt_CustomType(X_CustomTypeDesc)
# undef X_CustomTypeDesc

template<typename... Types> struct TypesDesc { static inline constexpr auto v = detail::concat_arrays(TypeDesc<Types>::v...); };

namespace detail {
    constexpr static inline size_t SpCustomTypeBegin = static_cast<size_t>(E_type::Base) + 1;
    constexpr static inline size_t SpCustomTypeNum = static_cast<size_t>(E_type::e_customType) - SpCustomTypeBegin;
    inline auto& all_custom_type_desc() {
        static std::array<std::vector<SpToken>, SpCustomTypeNum> arr{};
        return arr;
    }
}   // namespace detail


#define X_classMember(type__, name__, default__, ...) type__ name__ = default__;
#define DEC_MemberEnum(name__, Xt__, ...) enum name__{ Xt__(X_enumClassMember) e_maxCount };
#define X_enumClassMember(     type__, name__    , ...) e_##name__,
#define X_tupleMember(     type__, name__    , ...) decltype(name__),
#define X_leftShiftName(type__, name__, ...) << name__
#define X_rightShiftName(type__, name__, ...) >> name__
#define X_deepCopyFrom(type__, name__, ...) deepCopy(dc, name__, v.name__);
#define X_comma_decltypeName(type__, name__, ...) , decltype(name__)
#define X_memberNameStr(type__, name__, ...) #name__ ,
#define X_memberRefForTuple(type__, name__, ...) name__,
// UseDataPod 用：友元函数中引用 v.name__
#define X_leftShiftNamePod(type__, name__, ...) << v.name__
#define X_rightShiftNamePod(type__, name__, ...) >> v.name__



#define UseDataXtBase(TypeName__, Xt__, Base__) \
DEC_MemberEnum(E_idx, Xt__); \
Xt__(X_classMember); \
Sz typeID() const override { return TypeID_t<TypeName__>::id; } \
const char* getClassName() const override { return _className; } \
void output(O& o) const override { Base__::output(o); o Xt__(X_leftShiftName); } \
void input(I& i) override { Base__::input(i); i Xt__(X_rightShiftName); } \
void deepCopyFrom(DeepCopier& dc, Base const& v_) override { Base__::deepCopyFrom(dc, v_); TypeName__ const& v = static_cast<TypeName__ const&>(v_); Xt__(X_deepCopyFrom); }\
static constexpr inline auto _desc = TypesDesc<Base__ Xt__(X_comma_decltypeName)>::v;\
static constexpr inline char const* _className = #TypeName__;\
static constexpr inline char const* _baseName = #Base__;\
static constexpr inline char const* _membersName[] = {Xt__(X_memberNameStr)};\
std::span<SpToken const> getDesc() const override {return _desc;}\
const char* getBaseName() const override { return _baseName; }\
std::span<const char* const> getMemberNames() const override { return {_membersName, E_idx::e_maxCount}; }\
size_t getMemberCount() const override { return E_idx::e_maxCount; }\
struct M{\
enum E{Xt__(X_enumClassMember) e_numMax}; \
using TypeList = std::tuple< Xt__(X_tupleMember) E>; \
};\
auto memberTuple() { return std::tie(Xt__(X_memberRefForTuple) std::ignore); }

#define UseDataXt(TypeName__, Xt__) UseDataXtBase(TypeName__, Xt__, Base)
#define UseDataBase(TypeName__, Base__) UseDataXtBase(TypeName__, Xt_##TypeName__, Base__)
#define UseData(TypeName__) UseDataXtBase(TypeName__, Xt_##TypeName__, Base)

// =============================== UseDataPod — 不继承 Base 的 POD 类型序列化 ===============================
// 用法：
//   struct MyPod {
//       #define Xt_MyPod(X__) X__(i32, x, 0) X__(f64, y, 0.0)
//       UseDataPod(MyPod);
//   };
// 限制：
//   - 不支持多态序列化（不能通过基类指针序列化）
//   - 不支持深拷贝（deepCopyFrom）
//   - 不支持 JSON 序列化
//   - 类型仍需注册到 Xt_CustomType 中以便 TypeDesc 生成
struct _SpPodDummy {};
template<> struct TypeDesc<_SpPodDummy> { static inline constexpr auto v = SpTokenArr<0>{}; };

#define UseDataPodXt(TypeName__, Xt__) \
DEC_MemberEnum(E_idx, Xt__); \
Xt__(X_classMember); \
friend O& operator<<(O& o, TypeName__ const& v) { o Xt__(X_leftShiftNamePod); return o; } \
friend I& operator>>(I& i, TypeName__& v) { i Xt__(X_rightShiftNamePod); return i; }

#define UseDataPod(TypeName__) UseDataPodXt(TypeName__, Xt_##TypeName__)

template<typename T> Base* custom_create() { return new T{}; }
// 注册自定义类型：仅当类型完整且继承 Base 时才注册创建函数
template<typename T>
inline void register_custom_type(Sz id) {
    if constexpr (requires { sizeof(T); }) {
        if constexpr (std::is_base_of_v<Base, T>) {
            detail::all_custom_creator_pfn()[id - detail::customTypeBeginNum] = custom_create<T>;
        }
    }
}
#define X_reg_custom(type, name) \
    register_custom_type<name>(TypeID_t<name>::id); \
    registerSchemaClassName<name>();

inline void _init_stream_punk_impl(SpRegistry* reg = nullptr) {
    _sp_registry_ptr() = reg;
    Xt_CustomType(X_reg_custom);
}

/*
    INIT_StreamPunk() 初始化 StreamPunk 类型系统，注册 Xt_CustomType 中的所有自定义类型。
    自定义类型的反序列化必须在初始化之后进行。

    用法：
        SpRegistry reg;
        INIT_StreamPunk(&reg);    // 跨模块共享，由调用方创建 SpRegistry 并传入
        INIT_StreamPunk();        // 单模块，使用内部静态注册表（向后兼容）

    跨 DLL 场景：
        主程序创建 SpRegistry，各 DLL 的 INIT_StreamPunk(&reg) 传入同一个 reg 指针，
        所有模块的类型注册到同一个注册表中，避免多份副本。
*/
#define INIT_StreamPunk(...) sp::_init_stream_punk_impl(__VA_ARGS__)

// =============================== 语言互通 ===============================
namespace detail {
    
    template <typename T, typename Tuple> struct contains;
    template <typename T, typename... Ts> struct contains<T, std::tuple<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> { };
    template <typename T, typename Tuple> inline constexpr bool contains_v = contains<T, Tuple>::value;

    template <typename Acc, typename In> struct unique_tuple_impl;
    template <typename Acc> struct unique_tuple_impl<Acc, std::tuple<>> { using type = Acc; };
    template <typename Acc, typename T, typename... Rest> struct unique_tuple_impl<Acc, std::tuple<T, Rest...>> {
        static constexpr bool already_present = contains_v<T, Acc>;
        using next_acc = std::conditional_t<already_present,
            Acc,
            decltype(std::tuple_cat(std::declval<Acc>(), std::declval<std::tuple<T>>()))
        >;
        using type = typename unique_tuple_impl<next_acc, std::tuple<Rest...>>::type;
    };
    template <typename Tuple> struct unique_tuple_from {
        using type = typename unique_tuple_impl<std::tuple<>, Tuple>::type;
    };
    template <typename Tuple> using unique_tuple_from_t = typename unique_tuple_from<Tuple>::type;

    template <typename... Tuples> struct tuple_cat_type { using type = decltype(std::tuple_cat(std::declval<Tuples>()...)); };
    template <typename... Tuples> using tuple_cat_t = typename tuple_cat_type<Tuples...>::type;

}   // namespace detail



// =============================== 查询 ===============================
// SPOI（StreamPunk Operation Instruction）协议提供完整的查询/操作能力，
// 定义在 <stream-punk/StreamPunkSPOI.hpp> 等独立头文件中。
// 详见: skills/stream-punk-spoi/SKILL.md

} // namespace sp
