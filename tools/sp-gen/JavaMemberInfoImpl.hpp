#pragma once

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../../examples/00-demo-types/Data.hpp"
#include "JavaMemberInfo.hpp"
#include <string>
#include <type_traits>
#include <chrono>
#include <filesystem>

using namespace sp;

namespace {
    thread_local int javaVarCounter = 0;

    std::string nextJavaVar() {
        return "_v" + std::to_string(javaVarCounter++);
    }

    void resetJavaVarCounter() {
        javaVarCounter = 0;
    }
}

template<typename T> inline JavaMemberInfo getJavaMemberInfo() {
    if constexpr (TypeID_t<T>::kind == E_type::Base) {
        return typeToJava[TypeID_t<T>::id];
    }
    if constexpr (TypeID_t<T>::id == E_type::Base) {
        JavaMemberInfo r;
        r.tname = "Base";
        r.ivalue = "";
        r.deserializeCode = "read_obj(i)";
        r.serializeCode = "write_obj(o, {{_}})";
        return r;
    }
    if constexpr (TypeID_t<T>::kind == E_type::e_customType && TypeID_t<T>::id != E_type::Base) {
        JavaMemberInfo r;
        r.tname = T::_className;
        r.ivalue = "new " + std::string(T::_className) + "()";
        r.deserializeCode = "new " + std::string(T::_className) + "().from_(i)";
        r.serializeCode = "write_obj(o, {{_}})";
        return r;
    }
    if constexpr (specialization_of<T, std::basic_string>) {
        if constexpr(std::is_same_v<typename T::value_type, wchar_t>) {
            if constexpr (sizeof(wchar_t) == 2) return javaU16StrInfo;
            else if constexpr(sizeof(wchar_t) == 4) return javaU32StrInfo;
            else return javaNullInfo;
        }
        if constexpr (std::is_same_v<typename T::value_type, char>) return javaStrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char8_t>) return javaU8StrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char16_t>) return javaU16StrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char32_t>) return javaU32StrInfo;
        return javaNullInfo;
    }
    if constexpr (specialization_of_any<T, std::vector, std::deque, std::list, std::forward_list>) {
        JavaMemberInfo r;
        auto vInfo = getJavaMemberInfo<typename T::value_type>();
        r.tname = "ArrayList<" + javaBoxedType(vInfo.tname) + ">";
        r.ivalue = "new ArrayList<>()";
        if (vInfo.arraySize > 0) {
            std::string block = "() -> { " + vInfo.arrayElemType + "[] _arr = new " + vInfo.arrayElemType + "[" + std::to_string(vInfo.arraySize) + "]; for (int _i=0; _i<" + std::to_string(vInfo.arraySize) + "; _i++) _arr[_i] = " + vInfo.deserializeCode + "; return _arr; }";
            r.deserializeCode = "i.read_Array(" + block + ")";
        } else {
            r.deserializeCode = "i.read_Array(() -> " + vInfo.deserializeCode + ")";
        }
        auto ser = vInfo.serializeCode;
        std::string varName = nextJavaVar();
        replaceAll(ser, VAL, varName);
        r.serializeCode = "o.write_Array({{_}}, " + varName + " -> { " + ser + "; })";
        return r;
    }
    if constexpr (specialization_of_any<T, std::set, std::unordered_set>) {
        JavaMemberInfo r;
        auto vInfo = getJavaMemberInfo<typename T::value_type>();
        r.tname = "HashSet<" + javaBoxedType(vInfo.tname) + ">";
        r.ivalue = "new HashSet<>()";
        r.deserializeCode = "i.read_set(() -> " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        std::string varName = nextJavaVar();
        replaceAll(ser, VAL, varName);
        r.serializeCode = "o.write_set({{_}}, " + varName + " -> { " + ser + "; })";
        return r;
    }
    if constexpr (specialization_of_any<T, std::map, std::unordered_map>) {
        JavaMemberInfo r;
        auto kInfo = getJavaMemberInfo<typename T::key_type>();
        auto vInfo = getJavaMemberInfo<typename T::mapped_type>();
        r.tname = "HashMap<" + javaBoxedType(kInfo.tname) + ", " + javaBoxedType(vInfo.tname) + ">";
        r.ivalue = "new HashMap<>()";
        r.deserializeCode = "i.read_map(() -> " + kInfo.deserializeCode + ", () -> " + vInfo.deserializeCode + ")";
        auto kSer = kInfo.serializeCode;
        auto vSer = vInfo.serializeCode;
        std::string kVarName = nextJavaVar();
        std::string vVarName = nextJavaVar();
        replaceAll(kSer, VAL, kVarName);
        replaceAll(vSer, VAL, vVarName);
        r.serializeCode = "o.write_map({{_}}, " + kVarName + " -> { " + kSer + "; }, " + vVarName + " -> { " + vSer + "; })";
        return r;
    }
    if constexpr (std::is_pointer_v<T>) {
        using type = std::remove_pointer_t<T>;
        JavaMemberInfo r;
        auto vInfo = getJavaMemberInfo<type>();
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.tname = "SpRef<Base>";
            r.ivalue = "new SpRef<>(null, 0)";
            r.deserializeCode = "i.read_ptr_with_typeID()";
            r.serializeCode = "o.write_ptr_with_typeID({{_}}.value)";
        }
        else {
            r.tname = "SpRef<" + javaBoxedType(vInfo.tname) + ">";
            r.ivalue = "new SpRef<>(null, 0)";
            r.deserializeCode = "i.read_ptr(() -> " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            std::string varName = nextJavaVar();
            replaceAll(ser, VAL, varName);
            r.serializeCode = "o.write_ptr({{_}}.value, {{_}}.address, " + varName + " -> { " + ser + "; })";
        }
        return r;
    }
    if constexpr (specialization_of_any<T, std::shared_ptr, std::weak_ptr, std::unique_ptr>) {
        using type = typename T::element_type;
        JavaMemberInfo r;
        auto vInfo = getJavaMemberInfo<type>();
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.tname = "SpRef<Base>";
            r.ivalue = "new SpRef<>(null, 0)";
            r.deserializeCode = "i.read_ptr_with_typeID()";
            r.serializeCode = "o.write_ptr_with_typeID({{_}}.value)";
        }
        else {
            r.tname = "SpRef<" + javaBoxedType(vInfo.tname) + ">";
            r.ivalue = "new SpRef<>(null, 0)";
            r.deserializeCode = "i.read_ptr(() -> " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            std::string varName = nextJavaVar();
            replaceAll(ser, VAL, varName);
            r.serializeCode = "o.write_ptr({{_}}.value, {{_}}.address, " + varName + " -> { " + ser + "; })";
        }
        return r;
    }
    if constexpr (specialization_of<T, std::tuple>) {
        JavaMemberInfo r;
        r.tname = "Object[]";
        r.ivalue = "new Object[0]";
        r.deserializeCode = "null";
        r.serializeCode = "";
        return r;
    }
    if constexpr (specialization_of<T, std::variant>) {
        JavaMemberInfo r;
        r.tname = "Object";
        r.ivalue = "null";
        r.deserializeCode = "null";
        r.serializeCode = "";
        return r;
    }
    if constexpr (specialization_of<T, std::optional>) {
        JavaMemberInfo r;
        auto vInfo = getJavaMemberInfo<typename T::value_type>();
        r.tname = vInfo.tname;
        r.ivalue = vInfo.ivalue;
        r.deserializeCode = vInfo.deserializeCode;
        r.serializeCode = vInfo.serializeCode;
        return r;
    }
    if constexpr (is_std_array_v<T>) {
        using ElemType = typename T::value_type;
        constexpr auto arrSize = std::tuple_size<T>::value;
        JavaMemberInfo r;
        auto vInfo = getJavaMemberInfo<ElemType>();
        r.tname = vInfo.tname + "[]";
        r.ivalue = "new " + vInfo.tname + "[" + std::to_string(arrSize) + "]";
        r.deserializeCode = vInfo.deserializeCode;
        r.arraySize = static_cast<int>(arrSize);
        r.arrayElemType = vInfo.tname;
        r.serializeCode = "";
        return r;
    }
    if constexpr (is_std_bitset_v<T>) {
        JavaMemberInfo r;
        r.tname = "java.util.BitSet";
        r.ivalue = "new java.util.BitSet()";
        r.deserializeCode = "null";
        r.serializeCode = "";
        return r;
    }
    if constexpr (std::is_same_v<T, std::filesystem::path>) {
        return javaStrInfo;
    }
    if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>
        || std::is_same_v<T, std::chrono::steady_clock::time_point>
        || std::is_same_v<T, std::chrono::high_resolution_clock::time_point>) {
        JavaMemberInfo r;
        r.tname = "long";
        r.ivalue = "0L";
        r.serializeCode = "o.write_i64({{_}})";
        r.deserializeCode = "i.read_i64()";
        return r;
    }
    if constexpr (specialization_of<T, std::chrono::duration>) {
        JavaMemberInfo r;
        r.tname = "long";
        r.ivalue = "0L";
        r.serializeCode = "o.write_i64({{_}})";
        r.deserializeCode = "i.read_i64()";
        return r;
    }
    return javaNullInfo;
}