#pragma once

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../../examples/00-demo-types/Data.hpp"
#include "TsMemberInfo.hpp"
#include <string>
#include <type_traits>
#include <chrono>
#include <filesystem>

using namespace sp;

template<typename T> inline MemberInfo getTsMemberInfo() {
    if constexpr (TypeID_t<T>::kind == E_type::Base) {
        return typeToTs[TypeID_t<T>::id];
    }
    if constexpr (TypeID_t<T>::id == E_type::Base) {
        MemberInfo r;
        r.tname = "Base";
        r.ivalue = "";
        r.deserializeCode = "read_obj(i)";
        r.serializeCode = "value.to(o)";
        return r;
    }
    if constexpr (TypeID_t<T>::kind == E_type::e_customType && TypeID_t<T>::id != E_type::Base) {
        MemberInfo r;
        r.tname = T::_className;
        r.ivalue = "new " + r.tname + "()";
        r.deserializeCode = r.tname + ".from_(i)";
        r.serializeCode = "value.to(o)";
        return r;
    }
    if constexpr (specialization_of<T, std::basic_string>) {
        if constexpr(std::is_same_v<typename T::value_type, wchar_t>) {
            if constexpr (sizeof(wchar_t) == 2) {
                return u16StrInfo;
            }
            else if constexpr(sizeof(wchar_t) == 4) {
                return u32StrInfo;
            }
            else {
                return nullInfo;
            }
        }
        if constexpr (std::is_same_v<typename T::value_type, char>) {
            return strInfo;
        }
        if constexpr (std::is_same_v<typename T::value_type, char8_t>) {
            return u8StrInfo;
        }
        if constexpr (std::is_same_v<typename T::value_type, char16_t>) {
            return u16StrInfo;
        }
        if constexpr (std::is_same_v<typename T::value_type, char32_t>) {
            return u32StrInfo;
        }
        return nullInfo;
    }
    if constexpr (specialization_of_any<T, std::vector, std::deque, std::list, std::forward_list>) {
        MemberInfo r;
        auto vInfo = getTsMemberInfo<typename T::value_type>();
        r.tname = "Array";
        r.ivalue = "[]";
        r.deserializeCode = "i.readArray(() => " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        replaceAll(ser, VAL, "v");
        r.serializeCode = "o.writeArray({{_}}, (v) => { " + ser + " })";
        return r;
    }
    if constexpr (specialization_of_any<T, std::set, std::unordered_set>) {
        MemberInfo r;
        auto vInfo = getTsMemberInfo<typename T::value_type>();
        r.tname = "Set";
        r.ivalue = "new Set()";
        r.deserializeCode = "i.readSet(() => " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        replaceAll(ser, VAL, "v");
        r.serializeCode = "o.writeSet({{_}}, (v) => { " + ser + " })";
        return r;
    }
    if constexpr (specialization_of_any<T, std::map, std::unordered_map>) {
        MemberInfo r;
        auto kInfo = getTsMemberInfo<typename T::key_type>();
        auto vInfo = getTsMemberInfo<typename T::mapped_type>();
        r.tname = "Map";
        r.ivalue = "new Map()";
        r.deserializeCode = "i.readMap(() => " + kInfo.deserializeCode + ", () => " + vInfo.deserializeCode + ")";
        auto kSer = kInfo.serializeCode;
        auto vSer = vInfo.serializeCode;
        replaceAll(kSer, VAL, "k");
        replaceAll(vSer, VAL, "v");
        r.serializeCode = "o.writeMap({{_}}, (k) => { " + kSer + " }, (v) => { " + vSer + " })";
        return r;
    }
    if constexpr (std::is_pointer_v<T>) {
        using type = std::remove_pointer_t<T>;
        MemberInfo r;
        auto vInfo = getTsMemberInfo<type>();
        r.tname = "SpRef";
        r.ivalue = "new SpRef(null, 0)";
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.deserializeCode = "i.readPtrWithTypeID()";
            r.serializeCode = "o.writePtrWithTypeID({{_}}.value)";
        }
        else {
            r.deserializeCode = "i.readPtr(() => " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.writePtr({{_}}.value, {{_}}.address, (v) => { " + ser + " })";
        }
        return r;
    }
    if constexpr (specialization_of_any<T, std::shared_ptr, std::weak_ptr, std::unique_ptr>) {
        using type = typename T::element_type;
        MemberInfo r;
        auto vInfo = getTsMemberInfo<type>();
        r.tname = "SpRef";
        r.ivalue = "new SpRef(null, 0)";
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.deserializeCode = "i.readPtrWithTypeID()";
            r.serializeCode = "o.writePtrWithTypeID({{_}}.value)";
        }
        else {
            r.deserializeCode = "i.readPtr(() => " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.writePtr({{_}}.value, {{_}}.address, (v) => { " + ser + " })";
        }
        return r;
    }
    if constexpr (is_std_bitset_v<T>) {
        MemberInfo r;
        r.tname = "Array";
        r.ivalue = "[]";
        r.deserializeCode = "i.readBitset()";
        r.serializeCode = "o.writeBitset({{_}})";
        return r;
    }
    if constexpr (specialization_of<T, std::optional>) {
        MemberInfo r;
        auto vInfo = getTsMemberInfo<typename T::value_type>();
        r.tname = vInfo.tname;
        r.ivalue = "null";
        r.deserializeCode = "i.readOptional(() => " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        replaceAll(ser, VAL, "v");
        r.serializeCode = "o.writeOptional({{_}}, (v) => { " + ser + " })";
        return r;
    }
    if constexpr (specialization_of<T, std::atomic>) {
        auto vInfo = getTsMemberInfo<typename T::value_type>();
        return vInfo;
    }
    if constexpr (std::is_same_v<T, std::filesystem::path>) {
        MemberInfo r;
        r.tname = "string";
        r.ivalue = "\"\"";
        r.deserializeCode = "i.readString()";
        r.serializeCode = "o.writeString({{_}})";
        return r;
    }
    if constexpr (specialization_of<T, std::tuple>) {
        MemberInfo r;
        r.tname = "Array";
        r.ivalue = "[]";
        r.deserializeCode = "[";
        r.serializeCode = "[";
        auto arr = getMemberInfoArray<T>();
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i != 0) {
                r.deserializeCode += ", ";
                r.serializeCode += ", ";
            }
            r.deserializeCode += arr[i].deserializeCode;
            auto ser = arr[i].serializeCode;
            replaceAll(ser, VAL, std::string("{{_}}[") + std::to_string(i) + "]");
            r.serializeCode += ser;
        }
        r.deserializeCode += "]";
        r.serializeCode += "]";
        return r;
    }
    if constexpr (specialization_of<T, std::variant>) {
        MemberInfo r;
        r.tname = "SpVariant";
        r.ivalue = "new SpVariant()";
        r.deserializeCode = "i.readVariant([";
        r.serializeCode = "o.writeVariant({{_}}.value, {{_}}.typeIndex, [";
        auto arr = getMemberInfoArray<T>();
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i != 0) {
                r.deserializeCode += ", ";
                r.serializeCode += ", ";
            }
            r.deserializeCode += "() => " + arr[i].deserializeCode;
            auto ser = arr[i].serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode += "(v) => { " + ser + " }";
        }
        r.deserializeCode += "])";
        r.serializeCode += "])";
        return r;
    }
    if constexpr (is_std_array_v<T>) {
        MemberInfo r;
        using type = typename T::value_type;
        auto vInfo = getTsMemberInfo<type>();
        auto szStr = std::to_string(std::tuple_size_v<T>);
        r.tname = "SpArray";
        r.ivalue = "new SpArray(" + szStr + ")";
        r.deserializeCode = "i.readSpArray(" + szStr + ", () => " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        replaceAll(ser, VAL, "v");
        r.serializeCode = "o.writeSpArray({{_}}, (v) => { " + ser + " })";
        return r;
    }
    if constexpr (
        specialization_of_any<T, std::chrono::duration, std::chrono::time_point>
        || std::is_same_v<T, std::chrono::system_clock::time_point>
        || std::is_same_v<T, std::chrono::steady_clock::time_point>
    ) {
        MemberInfo r;
        r.tname = "bigint";
        r.ivalue = "0n";
        r.deserializeCode = "i.readStreamPunkTime()";
        r.serializeCode = "o.writeStreamPunkTime({{_}})";
        return r;
    }
    return nullInfo;
}

template<typename T> inline MemberInfo getMemberInfo() {
    return getTsMemberInfo<T>();
}