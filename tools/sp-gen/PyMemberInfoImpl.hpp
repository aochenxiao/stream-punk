#pragma once

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../../examples/00-demo-types/Data.hpp"
#include "PyMemberInfo.hpp"
#include <string>
#include <type_traits>
#include <chrono>
#include <filesystem>

using namespace sp;

template<typename T> inline PyMemberInfo getPyMemberInfo() {
    if constexpr (TypeID_t<T>::kind == E_type::Base) {
        return typeToPy[TypeID_t<T>::id];
    }
    if constexpr (TypeID_t<T>::id == E_type::Base) {
        PyMemberInfo r;
        r.tname = "Base";
        r.ivalue = "";
        r.deserializeCode = "read_obj(i)";
        r.serializeCode = "value.to(o)";
        return r;
    }
    if constexpr (TypeID_t<T>::kind == E_type::e_customType && TypeID_t<T>::id != E_type::Base) {
        PyMemberInfo r;
        r.tname = T::_className;
        r.ivalue = r.tname + "()";
        r.deserializeCode = r.tname + "().from_(i)";
        r.serializeCode = "value.to(o)";
        return r;
    }
    if constexpr (specialization_of<T, std::basic_string>) {
        if constexpr(std::is_same_v<typename T::value_type,wchar_t>) {
            if constexpr (sizeof(wchar_t) == 2) {
                return pyU16StrInfo;
            }
            else if constexpr(sizeof(wchar_t) == 4) {
                return pyU32StrInfo;
            }
            else {
                return pyNullInfo;
            }
        }
        if constexpr (std::is_same_v<typename T::value_type, char>) {
            return pyStrInfo;
        }
        if constexpr (std::is_same_v<typename T::value_type, char8_t>) {
            return pyU8StrInfo;
        }
        if constexpr (std::is_same_v<typename T::value_type, char16_t>) {
            return pyU16StrInfo;
        }
        if constexpr (std::is_same_v<typename T::value_type, char32_t>) {
            return pyU32StrInfo;
        }
        return pyNullInfo;
    }
    if constexpr (specialization_of_any<T, std::vector, std::deque, std::list, std::forward_list>) {
        PyMemberInfo r;
        auto vInfo = getPyMemberInfo<typename T::value_type>();
        r.tname = "list";
        r.ivalue = "[]";
        r.deserializeCode = "i.read_Array(lambda: " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        replaceAll(ser, VAL, "v");
        r.serializeCode = "o.write_Array({{_}}, lambda v: " + ser + ")";
        return r;
    }
    if constexpr (specialization_of_any<T, std::set, std::unordered_set>) {
        PyMemberInfo r;
        auto vInfo = getPyMemberInfo<typename T::value_type>();
        r.tname = "set";
        r.ivalue = "set()";
        r.deserializeCode = "i.read_set(lambda: " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        replaceAll(ser, VAL, "v");
        r.serializeCode = "o.write_set({{_}}, lambda v: " + ser + ")";
        return r;
    }
    if constexpr (specialization_of_any<T, std::map, std::unordered_map>) {
        PyMemberInfo r;
        auto kInfo = getPyMemberInfo<typename T::key_type>();
        auto vInfo = getPyMemberInfo<typename T::mapped_type>();
        r.tname = "dict";
        r.ivalue = "{}";
        r.deserializeCode = "i.read_map(lambda: " + kInfo.deserializeCode + ", lambda: " + vInfo.deserializeCode + ")";
        auto kSer = kInfo.serializeCode;
        auto vSer = vInfo.serializeCode;
        replaceAll(kSer, VAL, "k");
        replaceAll(vSer, VAL, "v");
        r.serializeCode = "o.write_map({{_}}, lambda k: " + kSer + ", lambda v: " + vSer + ")";
        return r;
    }
    if constexpr (std::is_pointer_v<T>) {
        using type = std::remove_pointer_t<T>;
        PyMemberInfo r;
        auto vInfo = getPyMemberInfo<type>();
        r.tname = "SpRef";
        r.ivalue = "SpRef(None, 0)";
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.deserializeCode = "i.read_ptr_with_typeID()";
            r.serializeCode = "o.write_ptr_with_typeID({{_}}.value)";
        }
        else {
            r.deserializeCode = "i.read_ptr(lambda: " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.write_ptr({{_}}.value, {{_}}.address, lambda v: " + ser + ")";
        }
        return r;
    }
    if constexpr (specialization_of_any<T, std::shared_ptr, std::weak_ptr, std::unique_ptr>) {
        using type = typename T::element_type;
        PyMemberInfo r;
        auto vInfo = getPyMemberInfo<type>();
        r.tname = "SpRef";
        r.ivalue = "SpRef(None, 0)";
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.deserializeCode = "i.read_ptr_with_typeID()";
            r.serializeCode = "o.write_ptr_with_typeID({{_}}.value)";
        }
        else {
            r.deserializeCode = "i.read_ptr(lambda: " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.write_ptr({{_}}.value, {{_}}.address, lambda v: " + ser + ")";
        }
        return r;
    }
    if constexpr (is_std_bitset_v<T>) {
        PyMemberInfo r;
        r.tname = "list";
        r.ivalue = "[]";
        r.deserializeCode = "i.read_bitset()";
        r.serializeCode = "o.write_bitset({{_}})";
        return r;
    }
    if constexpr (specialization_of<T, std::optional>) {
        PyMemberInfo r;
        auto vInfo = getPyMemberInfo<typename T::value_type>();
        r.tname = vInfo.tname;
        r.ivalue = "None";
        r.deserializeCode = "i.read_optional(lambda: " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        replaceAll(ser, VAL, "v");
        r.serializeCode = "o.write_optional({{_}}, lambda v: " + ser + ")";
        return r;
    }
    if constexpr (specialization_of<T, std::atomic>) {
        auto vInfo = getPyMemberInfo<typename T::value_type>();
        return vInfo;
    }
    if constexpr (std::is_same_v<T, std::filesystem::path>) {
        PyMemberInfo r;
        r.tname = "str";
        r.ivalue = "\"\"";
        r.deserializeCode = "i.read_string()";
        r.serializeCode = "o.write_string({{_}})";
        return r;
    }
    if constexpr (specialization_of<T, std::tuple>) {
        PyMemberInfo r;
        r.tname = "tuple";
        r.ivalue = "()";
        r.deserializeCode = "(";
        r.serializeCode = "(";
        auto arr = getPyMemberInfoArray<T>();
        for (size_t i = 0;i < arr.size(); ++i) {
            if (i != 0) {
                r.deserializeCode += ",";
                r.serializeCode += ",";
            }
            r.deserializeCode += arr[i].deserializeCode;
            auto ser = arr[i].serializeCode;
            replaceAll(ser, VAL, std::string("{{_}}[") + std::to_string(i) + "]");
            r.serializeCode += ser;
        }
        r.deserializeCode += ")";
        r.serializeCode += ")";
        return r;
    }
    if constexpr (specialization_of<T, std::variant>) {
        PyMemberInfo r;
        r.tname = "SpVariant";
        r.ivalue = "SpVariant()";
        r.deserializeCode = "i.read_variant([";
        r.serializeCode = "o.write_variant({{_}}.value, {{_}}.type_index, [";
        auto arr = getPyMemberInfoArray<T>();
        for (size_t i = 0;i < arr.size(); ++i) {
            if (i != 0) {
                r.deserializeCode += ",";
                r.serializeCode += ",";
            }
            r.deserializeCode += "lambda: " + arr[i].deserializeCode;
            auto ser = arr[i].serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode += "lambda v: " + ser;
        }
        r.deserializeCode += "])";
        r.serializeCode += "])";
        return r;
    }
    if constexpr (is_std_array_v<T>) {
        PyMemberInfo r;
        using type = typename T::value_type;
        auto vInfo = getPyMemberInfo<type>();
        auto szStr = std::to_string(std::tuple_size_v<T>);
        r.tname = "SpArray";
        r.ivalue = "SpArray(" + szStr + ")";
        r.deserializeCode = "i.read_SpArray(" + szStr + ", lambda: " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        replaceAll(ser, VAL, "v");
        r.serializeCode = "o.write_SpArray({{_}}, lambda v: " + ser + ")";
        return r;
    }
    if constexpr (
        specialization_of_any<T, std::chrono::duration, std::chrono::time_point>
        || std::is_same_v<T, std::chrono::system_clock::time_point>
        || std::is_same_v<T, std::chrono::steady_clock::time_point>
    ) {
        PyMemberInfo r;
        r.tname = "int";
        r.ivalue = "0";
        r.deserializeCode = "i.read_stream_punk_time()";
        r.serializeCode = "o.write_stream_punk_time({{_}})";
        return r;
    }
    return pyNullInfo;
}