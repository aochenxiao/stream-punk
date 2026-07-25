#pragma once

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../../examples/00-demo-types/Data.hpp"
#include "KotlinMemberInfo.hpp"
#include <string>
#include <type_traits>
#include <chrono>
#include <filesystem>

using namespace sp;

namespace {
    thread_local int kotlinVarCounter = 0;

    std::string nextKotlinVar() {
        return "_v" + std::to_string(kotlinVarCounter++);
    }

    void resetKotlinVarCounter() {
        kotlinVarCounter = 0;
    }
}

template<typename T> inline KotlinMemberInfo getKotlinMemberInfo() {
    if constexpr (TypeID_t<T>::kind == E_type::Base) {
        return typeToKotlin[TypeID_t<T>::id];
    }
    if constexpr (TypeID_t<T>::id == E_type::Base) {
        KotlinMemberInfo r;
        r.tname = "Base";
        r.ivalue = "";
        r.deserializeCode = "read_obj(i)";
        r.serializeCode = "write_obj(o, {{_}})";
        return r;
    }
    if constexpr (TypeID_t<T>::kind == E_type::e_customType && TypeID_t<T>::id != E_type::Base) {
        KotlinMemberInfo r;
        r.tname = T::_className;
        r.ivalue = std::string(T::_className) + "()";
        r.deserializeCode = std::string(T::_className) + "().from_(i)";
        r.serializeCode = "write_obj(o, {{_}})";
        return r;
    }
    if constexpr (specialization_of<T, std::basic_string>) {
        if constexpr(std::is_same_v<typename T::value_type, wchar_t>) {
            if constexpr (sizeof(wchar_t) == 2) return kotlinU16StrInfo;
            else if constexpr(sizeof(wchar_t) == 4) return kotlinU32StrInfo;
            else return kotlinNullInfo;
        }
        if constexpr (std::is_same_v<typename T::value_type, char>) return kotlinStrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char8_t>) return kotlinU8StrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char16_t>) return kotlinU16StrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char32_t>) return kotlinU32StrInfo;
        return kotlinNullInfo;
    }
    if constexpr (specialization_of_any<T, std::vector, std::deque, std::list, std::forward_list>) {
        KotlinMemberInfo r;
        auto vInfo = getKotlinMemberInfo<typename T::value_type>();
        r.tname = "ArrayList<" + kotlinBoxedType(vInfo.tname) + ">";
        r.ivalue = "ArrayList()";
        if (vInfo.arraySize > 0) {
            std::string block = "Array<" + vInfo.arrayElemType + ">(" + std::to_string(vInfo.arraySize) + ") { " + vInfo.deserializeCode + " }";
            r.deserializeCode = "i.read_Array { " + block + " }";
        } else {
            r.deserializeCode = "i.read_Array { " + vInfo.deserializeCode + " }";
        }
        auto ser = vInfo.serializeCode;
        std::string varName = nextKotlinVar();
        replaceAll(ser, VAL, varName);
        r.serializeCode = "o.write_Array({{_}}) { " + varName + " -> " + ser + " }";
        return r;
    }
    if constexpr (specialization_of_any<T, std::set, std::unordered_set>) {
        KotlinMemberInfo r;
        auto vInfo = getKotlinMemberInfo<typename T::value_type>();
        r.tname = "HashSet<" + kotlinBoxedType(vInfo.tname) + ">";
        r.ivalue = "HashSet()";
        r.deserializeCode = "i.read_set { " + vInfo.deserializeCode + " }";
        auto ser = vInfo.serializeCode;
        std::string varName = nextKotlinVar();
        replaceAll(ser, VAL, varName);
        r.serializeCode = "o.write_set({{_}}) { " + varName + " -> " + ser + " }";
        return r;
    }
    if constexpr (specialization_of_any<T, std::map, std::unordered_map>) {
        KotlinMemberInfo r;
        auto kInfo = getKotlinMemberInfo<typename T::key_type>();
        auto vInfo = getKotlinMemberInfo<typename T::mapped_type>();
        r.tname = "HashMap<" + kotlinBoxedType(kInfo.tname) + ", " + kotlinBoxedType(vInfo.tname) + ">";
        r.ivalue = "HashMap()";
        r.deserializeCode = "i.read_map({ " + kInfo.deserializeCode + " }, { " + vInfo.deserializeCode + " })";
        auto kSer = kInfo.serializeCode;
        auto vSer = vInfo.serializeCode;
        std::string kVarName = nextKotlinVar();
        std::string vVarName = nextKotlinVar();
        replaceAll(kSer, VAL, kVarName);
        replaceAll(vSer, VAL, vVarName);
        r.serializeCode = "o.write_map({{_}}, { " + kVarName + " -> " + kSer + " }, { " + vVarName + " -> " + vSer + " })";
        return r;
    }
    if constexpr (std::is_pointer_v<T>) {
        using type = std::remove_pointer_t<T>;
        KotlinMemberInfo r;
        auto vInfo = getKotlinMemberInfo<type>();
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.tname = "SpRef<Base?>";
            r.ivalue = "SpRef<Base?>(null, 0)";
            r.deserializeCode = "i.read_ptr_with_typeID()";
            r.serializeCode = "o.write_ptr_with_typeID({{_}}.value)";
        }
        else {
            r.tname = "SpRef<" + kotlinBoxedType(vInfo.tname) + "?>";
            r.ivalue = "SpRef<" + kotlinBoxedType(vInfo.tname) + "?>(null, 0)";
            r.deserializeCode = "i.read_ptr { " + vInfo.deserializeCode + " }";
            auto ser = vInfo.serializeCode;
            std::string varName = nextKotlinVar();
            replaceAll(ser, VAL, varName);
            r.serializeCode = "o.write_ptr({{_}}.value, {{_}}.address) { " + varName + " -> " + ser + " }";
        }
        return r;
    }
    if constexpr (specialization_of_any<T, std::shared_ptr, std::weak_ptr, std::unique_ptr>) {
        using type = typename T::element_type;
        KotlinMemberInfo r;
        auto vInfo = getKotlinMemberInfo<type>();
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.tname = "SpRef<Base?>";
            r.ivalue = "SpRef<Base?>(null, 0)";
            r.deserializeCode = "i.read_ptr_with_typeID()";
            r.serializeCode = "o.write_ptr_with_typeID({{_}}.value)";
        }
        else {
            r.tname = "SpRef<" + kotlinBoxedType(vInfo.tname) + "?>";
            r.ivalue = "SpRef<" + kotlinBoxedType(vInfo.tname) + "?>(null, 0)";
            r.deserializeCode = "i.read_ptr { " + vInfo.deserializeCode + " }";
            auto ser = vInfo.serializeCode;
            std::string varName = nextKotlinVar();
            replaceAll(ser, VAL, varName);
            r.serializeCode = "o.write_ptr({{_}}.value, {{_}}.address) { " + varName + " -> " + ser + " }";
        }
        return r;
    }
    if constexpr (specialization_of<T, std::tuple>) {
        KotlinMemberInfo r;
        r.tname = "Array<Any?>?";
        r.ivalue = "null";
        r.deserializeCode = "null";
        r.serializeCode = "";
        return r;
    }
    if constexpr (specialization_of<T, std::variant>) {
        KotlinMemberInfo r;
        r.tname = "Any?";
        r.ivalue = "null";
        r.deserializeCode = "null";
        r.serializeCode = "";
        return r;
    }
    if constexpr (specialization_of<T, std::optional>) {
        KotlinMemberInfo r;
        auto vInfo = getKotlinMemberInfo<typename T::value_type>();
        r.tname = vInfo.tname;
        r.ivalue = vInfo.ivalue;
        r.deserializeCode = vInfo.deserializeCode;
        r.serializeCode = vInfo.serializeCode;
        return r;
    }
    if constexpr (is_std_array_v<T>) {
        using ElemType = typename T::value_type;
        constexpr auto arrSize = std::tuple_size<T>::value;
        KotlinMemberInfo r;
        auto vInfo = getKotlinMemberInfo<ElemType>();
        r.tname = "Array<" + vInfo.tname + ">";
        r.ivalue = "Array<" + vInfo.tname + ">(" + std::to_string(arrSize) + ") { " + vInfo.ivalue + " }";
        r.deserializeCode = vInfo.deserializeCode;
        r.arraySize = static_cast<int>(arrSize);
        r.arrayElemType = vInfo.tname;
        r.serializeCode = "";
        return r;
    }
    if constexpr (is_std_bitset_v<T>) {
        KotlinMemberInfo r;
        r.tname = "java.util.BitSet?";
        r.ivalue = "null";
        r.deserializeCode = "null";
        r.serializeCode = "";
        return r;
    }
    if constexpr (std::is_same_v<T, std::filesystem::path>) {
        return kotlinStrInfo;
    }
    if constexpr (specialization_of<T, std::chrono::duration>) {
        KotlinMemberInfo r;
        r.tname = "Long";
        r.ivalue = "0L";
        r.serializeCode = "o.write_i64({{_}})";
        r.deserializeCode = "i.read_i64()";
        return r;
    }
    if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>
        || std::is_same_v<T, std::chrono::steady_clock::time_point>
        || std::is_same_v<T, std::chrono::high_resolution_clock::time_point>) {
        KotlinMemberInfo r;
        r.tname = "Long";
        r.ivalue = "0L";
        r.serializeCode = "o.write_i64({{_}})";
        r.deserializeCode = "i.read_i64()";
        return r;
    }
    return kotlinNullInfo;
}