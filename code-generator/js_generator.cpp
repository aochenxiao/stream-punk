#include "../stream-punk/StreamPunk.hpp"
#include "../stream-punk/Data.hpp"
#include "TsMemberInfo.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <format>
#include <regex>
#include <sstream>
#include <ranges>
#include <type_traits>

static std::regex reg_value(R"(\$\(value\))");
template<typename T> inline MemberInfo getMemberInfo() {
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
        r.deserializeCode = "new " + r.tname + "().from(i)";
        r.serializeCode = "value.to(o)";
        return r;
    }
    if constexpr (specialization_of<T, std::basic_string>) {
        if constexpr (std::is_same_v<typename T::value_type, wchar_t>) {
            if constexpr (sizeof(wchar_t) == 2) {
                return u16StrInfo;
            }
            else if constexpr (sizeof(wchar_t) == 4) {
                return u32StrInfo;
            }
            else {
                return nullInfo; // Unsupported wchar_t size
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
        auto vInfo = getMemberInfo<typename T::value_type>();
        r.tname = "Array<" + vInfo.tname + ">";
        r.ivalue = "new Array()";
        r.deserializeCode = "i.read_Array(()=>" + vInfo.deserializeCode + ")";
        r.serializeCode = "o.write_Array($(value), (v)=>" + std::regex_replace(vInfo.serializeCode, reg_value, "v") + ")";
        return r;
    }
    if constexpr (specialization_of_any<T, std::set, std::unordered_set>) {
        MemberInfo r;
        auto vInfo = getMemberInfo<typename T::value_type>();
        r.tname = "Set<" + vInfo.tname + ">";
        r.ivalue = "new Set()";
        r.deserializeCode = "i.read_set(()=>" + vInfo.deserializeCode + ")";
        r.serializeCode = "o.write_set($(value), (v)=>" + std::regex_replace(vInfo.serializeCode, reg_value, "v") + ")";
        return r;
    }
    if constexpr (specialization_of_any<T, std::map, std::unordered_map>) {
        MemberInfo r;
        auto kInfo = getMemberInfo<typename T::key_type>();
        auto vInfo = getMemberInfo<typename T::value_type::second_type>();
        r.tname = "Map<" + kInfo.tname + "," + vInfo.tname + ">";
        r.ivalue = "new Map()";
        r.deserializeCode = "i.read_map(()=>" + kInfo.deserializeCode + ",()=>" + vInfo.deserializeCode + ")";
        r.serializeCode = "o.write_map($(value), (k)=>" + std::regex_replace(kInfo.serializeCode, reg_value, "k") + ", (v)=>" + std::regex_replace(vInfo.serializeCode, reg_value, "v") + ")";
        return r;
    }
    if constexpr (std::is_pointer_v<T>) {
        using type = std::remove_pointer_t<T>;
        MemberInfo r;
        auto vInfo = getMemberInfo<type>();
        r.tname = "SpRef<" + vInfo.tname + "| null>";
        r.ivalue = "new SpRef(null, 0n)";
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.deserializeCode = std::string("i.read_ptr_with_typeID<") + type::_className + ">()";
            r.serializeCode = std::string("o.write_ptr_with_typeID<") + type::_className + ">($(value).value)";
        }
        else {
            r.deserializeCode = "i.read_ptr(()=>" + vInfo.deserializeCode + ")";
            r.serializeCode = "o.write_ptr($(value).value, $(value).address, (v)=>" + std::regex_replace(vInfo.serializeCode, reg_value, "v") + ")";
        }
        return r;
    }
    if constexpr (specialization_of_any<T, std::shared_ptr, std::weak_ptr, std::unique_ptr>) {
        using type = T::element_type;
        MemberInfo r;
        auto vInfo = getMemberInfo<type>();
        r.tname = "SpRef<" + vInfo.tname + "| null>";
        r.ivalue = "new SpRef(null, 0n)";
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.deserializeCode = std::string("i.read_ptr_with_typeID<") + type::_className + ">()";
            r.serializeCode = std::string("o.write_ptr_with_typeID<") + type::_className + ">($(value).value)";
        }
        else {
            r.deserializeCode = "i.read_ptr(()=>" + vInfo.deserializeCode + ")";
            r.serializeCode = "o.write_ptr($(value).value, $(value).address, (v)=>" + std::regex_replace(vInfo.serializeCode, reg_value, "v") + ")";
        }
        return r;
    }
    if constexpr (is_std_bitset_v<T>) {
        MemberInfo r;
        r.tname = "Array<boolean>";
        r.ivalue = "new Array()";
        r.deserializeCode = "i.read_bitset()";
        r.serializeCode = "o.write_bitset($(value))";
        return r;
    }
    if constexpr (specialization_of<T, std::optional>) {
        MemberInfo r;
        auto vInfo = getMemberInfo<typename T::value_type>();
        r.tname = vInfo.tname + " | null";
        r.ivalue = "null";
        r.deserializeCode = "i.read_optional(()=>" + vInfo.deserializeCode + ")";
        r.serializeCode = "o.write_optional($(value), (v)=>{" + std::regex_replace(vInfo.serializeCode, reg_value, "v") + "})";
        return r;
    }
    if constexpr (specialization_of<T, std::atomic>) {
        auto vInfo = getMemberInfo<typename T::value_type>();
        return vInfo;
    }
    if constexpr (std::is_same_v<T, std::filesystem::path>) {
        MemberInfo r;
        r.tname = "string";
        r.ivalue = "''";
        r.deserializeCode = "i.read_string()";
        r.serializeCode = "o.write_string($(value))";
        return r;
    }
    if constexpr (specialization_of<T, std::tuple>) {
        MemberInfo r;
        r.tname = "[";
        r.ivalue = "[";
        r.deserializeCode = "[";
        r.serializeCode = "";
        auto arr = getMemberInfoArray<T>();
        for (size_t i = 0;i < arr.size(); ++i) {
            if (i != 0) {
                r.tname += ","; r.ivalue += ","; r.deserializeCode += ",";
            }
            r.tname += arr[i].tname;
            r.ivalue += arr[i].ivalue;
            r.deserializeCode += arr[i].deserializeCode;
            r.serializeCode += std::regex_replace(arr[i].serializeCode, reg_value, std::string("$(value)[") + std::to_string(i) + "]") + ";";
        }
        r.tname += "]";
        r.ivalue += "]";
        r.deserializeCode += "]";
        return r;
    }
    if constexpr (specialization_of<T, std::variant>) {
        MemberInfo r;
        r.tname = "SpVariant<[";
        r.ivalue = "";
        r.deserializeCode = "i.read_variant([";
        r.serializeCode = "o.write_variant($(value).value, $(value).typeIndex, [";
        auto arr = getMemberInfoArray<T>();
        for (size_t i = 0;i < arr.size(); ++i) {
            if (i != 0) {
                r.tname += "|"; r.deserializeCode += ","; r.serializeCode += ",";
            }
            else {
                r.ivalue = arr[0].ivalue;
            }
            r.tname += arr[i].tname;
            r.deserializeCode += "()=> " + arr[i].deserializeCode;
            r.serializeCode += "(v)=> " + std::regex_replace(arr[i].serializeCode, reg_value, "v");
        }
        r.deserializeCode += "])";
        r.tname += "]>";
        r.deserializeCode = "new " + r.tname + "(" + r.deserializeCode + ")";
        r.ivalue = "new " + r.tname + "(" + r.ivalue + ")";
        r.serializeCode += "])";
        return r;
    }
    if constexpr (is_std_array_v<T>) {
        MemberInfo r;
        using type = T::value_type;
        auto vInfo = getMemberInfo<type>();
        auto szStr = std::to_string(std::tuple_size_v<T>);
        r.tname = "SpArray<" + vInfo.tname + ">";
        r.ivalue += "new SpArray<" + szStr + "," + vInfo.ivalue + ">";
        r.deserializeCode += "i.read_SpArray(" + szStr + ", ()=>" + vInfo.deserializeCode + ")";
        r.serializeCode = "o.write_SpArray($(value), (v)=>" + std::regex_replace(vInfo.serializeCode, reg_value, "v") + ")";
        return r;
    }
    if constexpr (
        specialization_of_any<T, std::chrono::duration, std::chrono::time_point>
        || std::is_same_v<T, std::chrono::system_clock::time_point>
        || std::is_same_v<T, std::chrono::steady_clock::time_point>
        ) {
        MemberInfo r;
        r.tname = "Date";
        r.ivalue = "new Date()";
        r.deserializeCode = "i.read_stream_punk_time()";
        r.serializeCode = "o.write_stream_punk_time($(value))";
        return r;
    }
    return nullInfo;
}


template<typename CT> std::string genClassCode() {
    std::stringstream ss;
    ss << "export class " << CT::_className << " extends " << CT::_baseName << " {\n"
        "  static typeID = E_StreamPunkType." << CT::_className << ";\n";

    using TupleType = typename CT::M::TypeList;
    constexpr size_t tupleSize = std::tuple_size<TupleType>::value;
    auto getTypeStrs = []<size_t... Is>(std::index_sequence<Is...>) {
        return std::vector<MemberInfo>{getMemberInfo<std::tuple_element_t<Is, TupleType>>()...};
    };
    auto typeStrs = getTypeStrs(std::make_index_sequence<tupleSize>{});
    auto sz = std::size(CT::_membersName);
    for (size_t i = 0; i < sz; ++i) {
        auto& info = typeStrs[i];
        ss << "    " << CT::_membersName[i] << ": " << info.tname << " = " << info.ivalue << ";\n";
    }
    ss << 
        "  from(i:I) {\n"
        "    super.from(i);\n";
    for (size_t i = 0; i < sz; ++i) {
        auto& info = typeStrs[i];
        ss << "    this." << CT::_membersName[i] << " = " << info.deserializeCode << ";\n";
    }
    ss <<
        "    return this;\n"
        "  }\n"
        "  to(o:O) {\n"
        "    super.to(o);\n"
        ;
    for (size_t i = 0; i < sz; ++i) {
        auto& info = typeStrs[i];
        ss << "    " << std::regex_replace(info.serializeCode, reg_value, std::string("this.") + CT::_membersName[i]) << ";\n";
    }
    ss <<
        "    return this;\n"
        "  }\n"
        "}\n"
        ;
    return ss.str();
}



int generate_js(const std::string& output_path) {
    std::ofstream outfile(output_path);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_path << std::endl;
        return -1;
    }

    std::ifstream input("stream-punk.js");
    if (input.is_open()) {
        outfile << input.rdbuf();
        input.close();
    }
    else {
        std::cerr << "Warning: Could not open stream-punk.js" << std::endl;
        return -1;
    }

    outfile << "export const E_StreamPunkType = {\n";

# define X_outPutEnumMember(type, name, ...) outfile << "  " << #name << ": " << static_cast<int>(E_type::name) << ",\n";

    Xt_Type(X_outPutEnumMember);

    outfile << "};\n";
    outfile <<
        R"(
export class Base {
  static typeID = E_StreamPunkType.Base;
  from(i) { void i; return this; }
  to  (o) { void o; return this; }
}
)";
    outfile << R"(
export function read_obj(i) {
  const id = i.read_u32();
  switch(id){
)";
# define X_case_type(typeName__, ...) outfile<<"    case E_StreamPunkType." #typeName__ ":{ const obj = new " #typeName__ "(); obj.from(i); return obj; }\n";

    Xt_CustomType(X_case_type);
    outfile << "  }\n  return null;\n}\n";

    outfile << R"(
export function write_obj(o, obj) {
  o.write_u32(obj.constructor.typeID);
  obj.to(o);
}

)";

# define X_outputClassCode(typeName__, ...) outfile << genClassCode<typeName__>();
    Xt_CustomType(X_outputClassCode);

    outfile.close();
    return 0;
}