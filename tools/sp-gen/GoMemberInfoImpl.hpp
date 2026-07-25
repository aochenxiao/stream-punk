#pragma once

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../../examples/00-demo-types/Data.hpp"
#include "GoMemberInfo.hpp"
#include <string>
#include <type_traits>
#include <chrono>
#include <filesystem>

using namespace sp;

namespace {
    thread_local int goVarCounter = 0;

    std::string nextGoVar() {
        return "_v" + std::to_string(goVarCounter++);
    }

    void resetGoVarCounter() {
        goVarCounter = 0;
    }
}

template<typename T> inline GoMemberInfo getGoMemberInfo() {
    if constexpr (TypeID_t<T>::kind == E_type::Base) {
        return typeToGo[TypeID_t<T>::id];
    }
    else if constexpr (TypeID_t<T>::id == E_type::Base) {
        GoMemberInfo r;
        r.tname = "SpBase";
        r.ivalue = "nil";
        r.deserializeCode = "ReadObj(i)";
        r.serializeCode = "WriteObj(o, {{_}})";
        return r;
    }
    else if constexpr (TypeID_t<T>::kind == E_type::e_customType && TypeID_t<T>::id != E_type::Base) {
        GoMemberInfo r;
        r.tname = "*" + std::string(T::_className);
        r.ivalue = "&" + std::string(T::_className) + "{}";
        r.deserializeCode = "&" + std::string(T::_className) + "{}; {{_}}.From_(i)";
        r.serializeCode = "WriteObj(o, {{_}})";
        return r;
    }
    else if constexpr (specialization_of<T, std::basic_string>) {
        if constexpr(std::is_same_v<typename T::value_type, wchar_t>) {
            if constexpr (sizeof(wchar_t) == 2) return goU16StrInfo;
            else if constexpr(sizeof(wchar_t) == 4) return goU32StrInfo;
            else return goNullInfo;
        }
        if constexpr (std::is_same_v<typename T::value_type, char>) return goStrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char8_t>) return goU8StrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char16_t>) return goU16StrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char32_t>) return goU32StrInfo;
        return goNullInfo;
    }
    else if constexpr (specialization_of_any<T, std::vector, std::deque, std::list, std::forward_list>) {
        GoMemberInfo r;
        auto vInfo = getGoMemberInfo<typename T::value_type>();
        r.tname = "[]" + vInfo.tname;
        r.ivalue = "nil";
        {
            std::stringstream deser;
            auto d = vInfo.deserializeCode;
            std::string retType = "[]" + vInfo.tname;
            deser << "func() " << retType << " { _len := i.ReadSz(); _tmp := make(" << retType << ", _len); ";
            deser << "for _j := 0; _j < _len; _j++ { ";
            replaceAll(d, VAL, "_tmp[_j]");
            deser << d << " }; return _tmp }()";
            r.deserializeCode = deser.str();
        }
        {
            auto s = vInfo.serializeCode;
            std::string varName = nextGoVar();
            replaceAll(s, VAL, varName);
            std::stringstream ser;
            ser << "o.WriteSz(len({{_}})); for _, " << varName << " := range {{_}} { " << s << " }";
            r.serializeCode = ser.str();
        }
        return r;
    }
    else if constexpr (specialization_of_any<T, std::set, std::unordered_set>) {
        GoMemberInfo r;
        auto vInfo = getGoMemberInfo<typename T::value_type>();
        bool isSliceType = vInfo.tname.starts_with("[");
        if (isSliceType) {
            r.tname = "[]" + vInfo.tname;
            r.ivalue = "nil";
            auto vd = vInfo.deserializeCode;
            std::string varName = nextGoVar();
            replaceAll(vd, VAL, varName);
            std::string retType = r.tname;
            {
                std::stringstream deser;
                deser << "func() " << retType << " { _len := i.ReadSz(); _tmp := make(" << retType << ", _len); ";
                deser << "for _j := 0; _j < _len; _j++ { ";
                deser << varName << " := " << vd << "; ";
                deser << "_tmp[_j] = " << varName << " }; return _tmp }()";
                r.deserializeCode = deser.str();
            }
            {
                auto vs = vInfo.serializeCode;
                std::string varName2 = nextGoVar();
                replaceAll(vs, VAL, varName2);
                std::stringstream ser;
                ser << "o.WriteSz(len({{_}})); for _, " << varName2 << " := range {{_}} { " << vs << " }";
                r.serializeCode = ser.str();
            }
        } else {
            r.tname = "map[" + vInfo.tname + "]struct{}";
            r.ivalue = "nil";
            {
                std::stringstream deser;
                auto d = vInfo.deserializeCode;
                std::string varName = nextGoVar();
                replaceAll(d, VAL, varName);
                std::string retType = "map[" + vInfo.tname + "]struct{}";
                deser << "func() " << retType << " { _len := i.ReadSz(); _tmp := make(" << retType << "); ";
                deser << "for _j := 0; _j < _len; _j++ { ";
                deser << varName << " := " << d << "; ";
                deser << "_tmp[" << varName << "] = struct{}{} }; return _tmp }()";
                r.deserializeCode = deser.str();
            }
            {
                auto s = vInfo.serializeCode;
                std::string varName = nextGoVar();
                replaceAll(s, VAL, varName);
                std::stringstream ser;
                ser << "o.WriteSz(len({{_}})); for " << varName << " := range {{_}} { " << s << " }";
                r.serializeCode = ser.str();
            }
        }
        return r;
    }
    else if constexpr (specialization_of_any<T, std::map, std::unordered_map>) {
        GoMemberInfo r;
        auto kInfo = getGoMemberInfo<typename T::key_type>();
        auto vInfo = getGoMemberInfo<typename T::mapped_type>();
        r.tname = "map[" + kInfo.tname + "]" + vInfo.tname;
        r.ivalue = "nil";
        {
            auto kd = kInfo.deserializeCode;
            auto vd = vInfo.deserializeCode;
            std::string kVarName = nextGoVar();
            std::string vVarName = nextGoVar();
            replaceAll(kd, VAL, kVarName);
            replaceAll(vd, VAL, vVarName);
            std::string retType = "map[" + kInfo.tname + "]" + vInfo.tname;
            std::stringstream deser;
            deser << "func() " << retType << " { _len := i.ReadSz(); _tmp := make(" << retType << "); ";
            deser << "for _j := 0; _j < _len; _j++ { ";
            deser << kVarName << " := " << kd << "; ";
            deser << vVarName << " := " << vd << "; ";
            deser << "_tmp[" << kVarName << "] = " << vVarName << " }; return _tmp }()";
            r.deserializeCode = deser.str();
        }
        {
            auto ks = kInfo.serializeCode;
            auto vs = vInfo.serializeCode;
            std::string kVarName = nextGoVar();
            std::string vVarName = nextGoVar();
            replaceAll(ks, VAL, kVarName);
            replaceAll(vs, VAL, vVarName);
            std::stringstream ser;
            ser << "o.WriteSz(len({{_}})); for " << kVarName << ", " << vVarName << " := range {{_}} { " << ks << "; " << vs << " }";
            r.serializeCode = ser.str();
        }
        return r;
    }
    else if constexpr (std::is_pointer_v<T>) {
        using type = std::remove_pointer_t<T>;
        GoMemberInfo r;
        auto vInfo = getGoMemberInfo<type>();
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.tname = "SpRef";
            r.ivalue = "NewSpRef()";
            r.deserializeCode = "i.ReadPtrWithTypeID()";
            r.serializeCode = "o.WritePtrWithTypeID({{_}}.Value)";
        }
        else {
            r.tname = "SpRef";
            r.ivalue = "NewSpRef()";
            {
                auto d = vInfo.deserializeCode;
                r.deserializeCode = "i.ReadPtr(func() interface{} { return " + d + " })";
            }
            {
                auto s = vInfo.serializeCode;
                std::string varName = nextGoVar();
                replaceAll(s, VAL, varName);
                r.serializeCode = "o.WritePtr({{_}}.Value, {{_}}.Address, func(v interface{}) { " + varName + " := v.(" + vInfo.tname + "); " + s + " })";
            }
        }
        return r;
    }
    else if constexpr (specialization_of_any<T, std::shared_ptr, std::weak_ptr, std::unique_ptr>) {
        using type = typename T::element_type;
        GoMemberInfo r;
        auto vInfo = getGoMemberInfo<type>();
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.tname = "SpRef";
            r.ivalue = "NewSpRef()";
            r.deserializeCode = "i.ReadPtrWithTypeID()";
            r.serializeCode = "o.WritePtrWithTypeID({{_}}.Value)";
        }
        else {
            r.tname = "SpRef";
            r.ivalue = "NewSpRef()";
            {
                auto d = vInfo.deserializeCode;
                r.deserializeCode = "i.ReadPtr(func() interface{} { return " + d + " })";
            }
            {
                auto s = vInfo.serializeCode;
                std::string varName = nextGoVar();
                replaceAll(s, VAL, varName);
                r.serializeCode = "o.WritePtr({{_}}.Value, {{_}}.Address, func(v interface{}) { " + varName + " := v.(" + vInfo.tname + "); " + s + " })";
            }
        }
        return r;
    }
    else if constexpr (is_std_array_v<T>) {
        GoMemberInfo r;
        constexpr auto arrSize = std::tuple_size_v<T>;
        auto vInfo = getGoMemberInfo<typename T::value_type>();
        r.tname = "[" + std::to_string(arrSize) + "]" + vInfo.tname;
        r.ivalue = "[" + std::to_string(arrSize) + "]" + vInfo.tname + "{}";
        r.arraySize = (int)arrSize;
        r.arrayElemType = vInfo.tname;
        {
            std::stringstream deser;
            auto d = vInfo.deserializeCode;
            std::string retType = "[" + std::to_string(arrSize) + "]" + vInfo.tname;
            deser << "func() " << retType << " { var _tmp " << retType << "; ";
            deser << "for _j := 0; _j < " << arrSize << "; _j++ { ";
            replaceAll(d, VAL, "_tmp[_j]");
            deser << d << " }; return _tmp }()";
            r.deserializeCode = deser.str();
        }
        {
            std::stringstream ser;
            auto s = vInfo.serializeCode;
            std::string varName = nextGoVar();
            replaceAll(s, VAL, varName);
            ser << "for _, " << varName << " := range {{_}} { " << s << " }";
            r.serializeCode = ser.str();
        }
        return r;
    }
    else if constexpr (specialization_of<T, std::optional>) {
        GoMemberInfo r;
        auto vInfo = getGoMemberInfo<typename T::value_type>();
        r.tname = "*" + vInfo.tname;
        r.ivalue = "nil";
        {
            auto d = vInfo.deserializeCode;
            replaceAll(d, VAL, "_opt");
            std::stringstream deser;
            deser << "func() *" << vInfo.tname << " { if i.ReadBl() { _opt := " << d << "; return &_opt } else { return nil } }()";
            r.deserializeCode = deser.str();
        }
        {
            auto s = vInfo.serializeCode;
            std::string varName = nextGoVar();
            replaceAll(s, VAL, "(*" + varName + ")");
            std::stringstream ser;
            ser << "if {{_}} != nil { o.WriteBl(true); " << varName << " := {{_}}; " << s << " } else { o.WriteBl(false) }";
            r.serializeCode = ser.str();
        }
        return r;
    }
    else if constexpr (specialization_of<T, std::atomic>) {
        auto vInfo = getGoMemberInfo<typename T::value_type>();
        return vInfo;
    }
    else if constexpr (std::is_same_v<T, std::filesystem::path>) {
        return goStrInfo;
    }
    else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>
        || std::is_same_v<T, std::chrono::steady_clock::time_point>
        || std::is_same_v<T, std::chrono::high_resolution_clock::time_point>) {
        GoMemberInfo r;
        r.tname = "int64";
        r.ivalue = "0";
        r.serializeCode = "o.WriteI64({{_}})";
        r.deserializeCode = "i.ReadI64()";
        return r;
    }
    else if constexpr (specialization_of<T, std::chrono::duration>) {
        GoMemberInfo r;
        r.tname = "int64";
        r.ivalue = "0";
        r.serializeCode = "o.WriteI64({{_}})";
        r.deserializeCode = "i.ReadI64()";
        return r;
    }
    else if constexpr (is_std_bitset_v<T>) {
        GoMemberInfo r;
        r.tname = "[]bool";
        r.ivalue = "nil";
        r.deserializeCode = "func() []bool { _len := i.ReadSz(); _tmp := make([]bool, _len); for _j := 0; _j < _len; _j++ { _tmp[_j] = i.ReadBl() }; return _tmp }()";
        r.serializeCode = "o.WriteSz(len({{_}})); for _, _v := range {{_}} { o.WriteBl(_v) }";
        return r;
    }
    else if constexpr (specialization_of<T, std::tuple>) {
        GoMemberInfo r;
        auto arr = getGoMemberInfoArray<T>();
        r.tname = "struct{";
        r.ivalue = "struct{";
        for (size_t i = 0; i < arr.size(); ++i) {
            r.tname += " F" + std::to_string(i) + " " + arr[i].tname + ";";
            r.ivalue += "F" + std::to_string(i) + ": " + arr[i].ivalue + "; ";
        }
        r.tname += "}";
        r.ivalue += "}";
        {
            std::stringstream deser;
            deser << "func() " << r.tname << " {";
            for (size_t i = 0; i < arr.size(); ++i) {
                auto d = arr[i].deserializeCode;
                std::string varName = nextGoVar();
                replaceAll(d, VAL, varName);
                deser << " " << varName << " := " << d << ";";
            }
            deser << " return " << r.tname << "{";
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) deser << ",";
                deser << "_v" << std::to_string(goVarCounter - arr.size() + i);
            }
            deser << " }}()";
            r.deserializeCode = deser.str();
        }
        {
            std::stringstream ser;
            for (size_t i = 0; i < arr.size(); ++i) {
                auto s = arr[i].serializeCode;
                std::string varName = nextGoVar();
                replaceAll(s, VAL, varName);
                ser << " " << varName << " := {{_}}.F" << i << "; " << s << ";";
            }
            r.serializeCode = ser.str();
        }
        return r;
    }
    else if constexpr (specialization_of<T, std::variant>) {
        GoMemberInfo r;
        r.tname = "SpVariant";
        r.ivalue = "NewSpVariant()";
        r.deserializeCode = "ReadVariant(i)";
        r.serializeCode = "{{_}}.WriteVariant(o)";
        return r;
    }
    else {
        return goNullInfo;
    }
}