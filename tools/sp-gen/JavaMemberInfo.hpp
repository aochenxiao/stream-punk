#pragma once

#include "MemberInfoBase.hpp"

using namespace sp;

using JavaMemberInfo = MemberInfo;

inline std::map<Sz, JavaMemberInfo> typeToJava = {
    {E_type::u8    ,{"int"    , "0"    , "o.write_u8({{_}})"    , "i.read_u8()"   }},
    {E_type::u16   ,{"int"    , "0"    , "o.write_u16({{_}})"   , "i.read_u16()"  }},
    {E_type::u32   ,{"long"   , "0L"   , "o.write_u32({{_}})"   , "i.read_u32()"  }},
    {E_type::u64   ,{"long"   , "0L"   , "o.write_u64({{_}})"   , "i.read_u64()"  }},
    {E_type::i8    ,{"byte"   , "0"    , "o.write_i8({{_}})"    , "i.read_i8()"   }},
    {E_type::i16   ,{"short"  , "0"    , "o.write_i16({{_}})"   , "i.read_i16()"  }},
    {E_type::i32   ,{"int"    , "0"    , "o.write_i32({{_}})"   , "i.read_i32()"  }},
    {E_type::i64   ,{"long"   , "0L"   , "o.write_i64({{_}})"   , "i.read_i64()"  }},
    {E_type::f32   ,{"float"  , "0.0f" , "o.write_f32({{_}})"   , "i.read_f32()"  }},
    {E_type::f64   ,{"double" , "0.0"  , "o.write_f64({{_}})"   , "i.read_f64()"  }},
    {E_type::bl    ,{"boolean","false" , "o.write_bl({{_}})"    , "i.read_bl()"   }},
    {E_type::ch    ,{"char"   ,"'\\0'" , "o.write_ch({{_}})"    , "i.read_ch()"   }},
    {E_type::ch8   ,{"char"   ,"'\\0'" , "o.write_ch8({{_}})"   , "i.read_ch8()"  }},
    {E_type::ch16  ,{"char"   ,"'\\0'" , "o.write_ch16({{_}})"  , "i.read_ch16()" }},
    {E_type::ch32  ,{"int"    , "0"    , "o.write_ch32({{_}})"  , "i.read_ch32()" }},
};

inline JavaMemberInfo javaNullInfo   { "", "", "", "" };
inline JavaMemberInfo javaStrInfo    { "String", "\"\"", "o.write_string({{_}})", "i.read_string()" };
inline JavaMemberInfo javaU8StrInfo  { "String", "\"\"", "o.write_u8string({{_}})", "i.read_u8string()" };
inline JavaMemberInfo javaU16StrInfo { "String", "\"\"", "o.write_u16string({{_}})", "i.read_u16string()" };
inline JavaMemberInfo javaU32StrInfo { "byte[]", "new byte[0]", "o.write_u32string({{_}})", "i.read_u32string()" };

inline std::string javaBoxedType(const std::string& t) {
    if (t == "int") return "Integer";
    if (t == "long") return "Long";
    if (t == "float") return "Float";
    if (t == "double") return "Double";
    if (t == "boolean") return "Boolean";
    if (t == "char") return "Character";
    if (t == "byte") return "Byte";
    if (t == "short") return "Short";
    return t;
}

template <typename T>
constexpr auto buildJavaMemberInfoArray() {
    using Extractor = type_sequence_extractor<T>;
    constexpr size_t size = Extractor::size;
    return[] <size_t... Is> (std::index_sequence<Is...>) {
        return std::array{
            getJavaMemberInfo<typename Extractor::template element<Is>>()...
        };
    }(std::make_index_sequence<size>{});
}
template <typename T>
constexpr auto getJavaMemberInfoArray() { return buildJavaMemberInfoArray<T>(); }