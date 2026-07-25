#pragma once

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../../examples/00-demo-types/Data.hpp"
#include "RustMemberInfo.hpp"
#include <string>
#include <type_traits>
#include <chrono>
#include <filesystem>

using namespace sp;

namespace {
    thread_local int rustVarCounter = 0;

    std::string nextRustVar() {
        return "_v" + std::to_string(rustVarCounter++);
    }

    void resetRustVarCounter() {
        rustVarCounter = 0;
    }
}

template<typename T> inline RustMemberInfo getRustMemberInfo() {
    if constexpr (TypeID_t<T>::kind == E_type::Base) {
        return typeToRust[TypeID_t<T>::id];
    }
    else if constexpr (TypeID_t<T>::id == E_type::Base) {
        RustMemberInfo r;
        r.tname = "Option<Arc<dyn SpBase>>";
        r.ivalue = "None";
        r.deserializeCode = "read_obj(i).map(|b| Arc::from(b))";
        r.serializeCode = "if let Some(ref v) = {{_}} { write_obj(o, v.as_ref()) }";
        return r;
    }
    else if constexpr (TypeID_t<T>::kind == E_type::e_customType && TypeID_t<T>::id != E_type::Base) {
        RustMemberInfo r;
        r.tname = T::_className;
        r.ivalue = std::string(T::_className) + "::default()";
        r.deserializeCode = std::string(T::_className) + "::from_i(i)";
        r.serializeCode = "write_obj(o, &{{_}})";
        return r;
    }
    else if constexpr (specialization_of<T, std::basic_string>) {
        if constexpr(std::is_same_v<typename T::value_type, wchar_t>) {
            if constexpr (sizeof(wchar_t) == 2) return rustU16StrInfo;
            else if constexpr(sizeof(wchar_t) == 4) return rustU32StrInfo;
            else return rustNullInfo;
        }
        if constexpr (std::is_same_v<typename T::value_type, char>) return rustStrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char8_t>) return rustU8StrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char16_t>) return rustU16StrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char32_t>) return rustU32StrInfo;
        return rustNullInfo;
    }
    else if constexpr (specialization_of_any<T, std::vector, std::deque, std::list, std::forward_list>) {
        RustMemberInfo r;
        auto vInfo = getRustMemberInfo<typename T::value_type>();
        r.tname = "Vec<" + vInfo.tname + ">";
        r.ivalue = "Vec::new()";
        r.deserializeCode = "i.read_Array(|i| " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        std::string varName = nextRustVar();
        replaceAll(ser, VAL, std::string("*") + varName);
        r.serializeCode = "o.write_Array(&{{_}}, |o, " + varName + "| { " + ser + " })";
        return r;
    }
    else if constexpr (specialization_of_any<T, std::set, std::unordered_set>) {
        RustMemberInfo r;
        auto vInfo = getRustMemberInfo<typename T::value_type>();
        r.tname = "HashSet<" + vInfo.tname + ">";
        r.ivalue = "HashSet::new()";
        r.deserializeCode = "i.read_set(|i| " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        std::string varName = nextRustVar();
        replaceAll(ser, VAL, std::string("*") + varName);
        r.serializeCode = "o.write_set(&{{_}}, |o, " + varName + "| { " + ser + " })";
        return r;
    }
    else if constexpr (specialization_of_any<T, std::map, std::unordered_map>) {
        RustMemberInfo r;
        auto kInfo = getRustMemberInfo<typename T::key_type>();
        auto vInfo = getRustMemberInfo<typename T::mapped_type>();
        r.tname = "HashMap<" + kInfo.tname + ", " + vInfo.tname + ">";
        r.ivalue = "HashMap::new()";
        r.deserializeCode = "i.read_map(|i| " + kInfo.deserializeCode + ", |i| " + vInfo.deserializeCode + ")";
        auto kSer = kInfo.serializeCode;
        auto vSer = vInfo.serializeCode;
        std::string kVarName = nextRustVar();
        std::string vVarName = nextRustVar();
        replaceAll(kSer, VAL, std::string("*") + kVarName);
        replaceAll(vSer, VAL, std::string("*") + vVarName);
        r.serializeCode = "o.write_map(&{{_}}, |o, " + kVarName + "| { " + kSer + " }, |o, " + vVarName + "| { " + vSer + " })";
        return r;
    }
    else if constexpr (std::is_pointer_v<T>) {
        using type = std::remove_pointer_t<T>;
        RustMemberInfo r;
        auto vInfo = getRustMemberInfo<type>();
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.tname = "SpRef<Arc<dyn SpBase>>";
            r.ivalue = "SpRef::none()";
            r.deserializeCode = "i.read_ptr_with_typeID()";
            r.serializeCode = "o.write_ptr_with_typeID(({{_}}).value.as_deref())";
        }
        else {
            r.tname = "SpRef<" + vInfo.tname + ">";
            r.ivalue = "SpRef::none()";
            r.deserializeCode = "i.read_ptr(|i| " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            std::string varName = nextRustVar();
            replaceAll(ser, VAL, std::string("*") + varName);
            r.serializeCode = "o.write_ptr(({{_}}).value.as_ref(), ({{_}}).address, |o, " + varName + "| { " + ser + " })";
        }
        return r;
    }
    else if constexpr (specialization_of_any<T, std::shared_ptr, std::weak_ptr, std::unique_ptr>) {
        using type = typename T::element_type;
        RustMemberInfo r;
        auto vInfo = getRustMemberInfo<type>();
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.tname = "SpRef<Arc<dyn SpBase>>";
            r.ivalue = "SpRef::none()";
            r.deserializeCode = "i.read_ptr_with_typeID()";
            r.serializeCode = "o.write_ptr_with_typeID(({{_}}).value.as_deref())";
        }
        else {
            r.tname = "SpRef<" + vInfo.tname + ">";
            r.ivalue = "SpRef::none()";
            r.deserializeCode = "i.read_ptr(|i| " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            std::string varName = nextRustVar();
            replaceAll(ser, VAL, std::string("*") + varName);
            r.serializeCode = "o.write_ptr(({{_}}).value.as_ref(), ({{_}}).address, |o, " + varName + "| { " + ser + " })";
        }
        return r;
    }
    else if constexpr (is_std_array_v<T>) {
        RustMemberInfo r;
        constexpr auto arrSize = std::tuple_size_v<T>;
        auto vInfo = getRustMemberInfo<typename T::value_type>();
        r.tname = "[" + vInfo.tname + "; " + std::to_string(arrSize) + "]";
        r.ivalue = "Default::default()";
        r.arraySize = (int)arrSize;
        r.arrayElemType = vInfo.tname;
        std::stringstream ser;
        ser << "for _idx in 0.." << arrSize << " { ";
        auto s = vInfo.serializeCode;
        replaceAll(s, VAL, std::string("{{_}}[_idx]"));
        ser << s << "; ";
        ser << "}";
        r.serializeCode = ser.str();
        std::stringstream deser;
        deser << "{ let mut arr: " << r.tname << " = Default::default(); ";
        deser << "for _idx in 0.." << arrSize << " { ";
        auto d = vInfo.deserializeCode;
        deser << "arr[_idx] = " << d << "; ";
        deser << "} ";
        deser << "arr }";
        r.deserializeCode = deser.str();
        return r;
    }
    else if constexpr (specialization_of<T, std::optional>) {
        RustMemberInfo r;
        auto vInfo = getRustMemberInfo<typename T::value_type>();
        r.tname = "Option<" + vInfo.tname + ">";
        r.ivalue = "None";
        r.deserializeCode = "if i.read_bl() { Some(" + vInfo.deserializeCode + ") } else { None }";
        auto ser = vInfo.serializeCode;
        std::string varName = nextRustVar();
        replaceAll(ser, VAL, std::string("*") + varName);
        r.serializeCode = "match {{_}} { Some(ref " + varName + ") => { o.write_bl(true); " + ser + " } None => o.write_bl(false) }";
        return r;
    }
    else if constexpr (specialization_of<T, std::atomic>) {
        auto vInfo = getRustMemberInfo<typename T::value_type>();
        return vInfo;
    }
    else if constexpr (std::is_same_v<T, std::filesystem::path>) {
        return rustStrInfo;
    }
    else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>
        || std::is_same_v<T, std::chrono::steady_clock::time_point>
        || std::is_same_v<T, std::chrono::high_resolution_clock::time_point>) {
        RustMemberInfo r;
        r.tname = "i64";
        r.ivalue = "0";
        r.serializeCode = "o.write_i64({{_}})";
        r.deserializeCode = "i.read_i64()";
        return r;
    }
    else if constexpr (specialization_of<T, std::chrono::duration>) {
        RustMemberInfo r;
        r.tname = "i64";
        r.ivalue = "0";
        r.serializeCode = "o.write_i64({{_}})";
        r.deserializeCode = "i.read_i64()";
        return r;
    }
    else if constexpr (is_std_bitset_v<T>) {
        RustMemberInfo r;
        r.tname = "Vec<bool>";
        r.ivalue = "Vec::new()";
        r.deserializeCode = "i.read_Array(|i| i.read_bl())";
        std::string varName = nextRustVar();
        r.serializeCode = "o.write_Array(&{{_}}, |o, " + varName + "| o.write_bl(*" + varName + "))";
        return r;
    }
    else if constexpr (specialization_of<T, std::tuple>) {
        RustMemberInfo r;
        auto arr = getRustMemberInfoArray<T>();
        r.tname = "(";
        r.ivalue = "(";
        r.deserializeCode = "(";
        r.serializeCode = "";
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i != 0) {
                r.tname += ", ";
                r.ivalue += ", ";
                r.deserializeCode += ", ";
            }
            r.tname += arr[i].tname;
            r.ivalue += arr[i].ivalue;
            r.deserializeCode += arr[i].deserializeCode;
        }
        r.tname += ")";
        r.ivalue += ")";
        r.deserializeCode += ")";
        return r;
    }
    else if constexpr (specialization_of<T, std::variant>) {
        RustMemberInfo r;
        r.tname = "SpVariant";
        r.ivalue = "SpVariant::new()";
        r.deserializeCode = "SpVariant::read_variant(i)";
        r.serializeCode = "SpVariant::write_variant(&{{_}}, o)";
        return r;
    }
    else {
        return rustNullInfo;
    }
}