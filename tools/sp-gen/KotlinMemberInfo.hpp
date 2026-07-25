#pragma once

#include "MemberInfoBase.hpp"

using namespace sp;

using KotlinMemberInfo = MemberInfo;

inline std::map<Sz, KotlinMemberInfo> typeToKotlin = {
    {E_type::u8    ,{"Int"    , "0"    , "o.write_u8({{_}})"    , "i.read_u8()"   }},
    {E_type::u16   ,{"Int"    , "0"    , "o.write_u16({{_}})"   , "i.read_u16()"  }},
    {E_type::u32   ,{"Long"   , "0L"   , "o.write_u32({{_}})"   , "i.read_u32()"  }},
    {E_type::u64   ,{"Long"   , "0L"   , "o.write_u64({{_}})"   , "i.read_u64()"  }},
    {E_type::i8    ,{"Byte"   , "0"    , "o.write_i8({{_}})"    , "i.read_i8()"   }},
    {E_type::i16   ,{"Short"  , "0"    , "o.write_i16({{_}})"   , "i.read_i16()"  }},
    {E_type::i32   ,{"Int"    , "0"    , "o.write_i32({{_}})"   , "i.read_i32()"  }},
    {E_type::i64   ,{"Long"   , "0L"   , "o.write_i64({{_}})"   , "i.read_i64()"  }},
    {E_type::f32   ,{"Float"  , "0.0f" , "o.write_f32({{_}})"   , "i.read_f32()"  }},
    {E_type::f64   ,{"Double" , "0.0"  , "o.write_f64({{_}})"   , "i.read_f64()"  }},
    {E_type::bl    ,{"Boolean","false" , "o.write_bl({{_}})"    , "i.read_bl()"   }},
    {E_type::ch    ,{"Char"   ,"0.toChar()", "o.write_ch({{_}})"  , "i.read_ch()"   }},
    {E_type::ch8   ,{"Char"   ,"0.toChar()", "o.write_ch8({{_}})" , "i.read_ch8()"  }},
    {E_type::ch16  ,{"Char"   ,"0.toChar()", "o.write_ch16({{_}})", "i.read_ch16()" }},
    {E_type::ch32  ,{"Int"    , "0"    , "o.write_ch32({{_}})"  , "i.read_ch32()" }},
};

inline KotlinMemberInfo kotlinNullInfo   { "", "", "", "" };
inline KotlinMemberInfo kotlinStrInfo    { "String", "\"\"", "o.write_string({{_}})", "i.read_string()" };
inline KotlinMemberInfo kotlinU8StrInfo  { "String", "\"\"", "o.write_u8string({{_}})", "i.read_u8string()" };
inline KotlinMemberInfo kotlinU16StrInfo { "String", "\"\"", "o.write_u16string({{_}})", "i.read_u16string()" };
inline KotlinMemberInfo kotlinU32StrInfo { "ByteArray", "ByteArray(0)", "o.write_u32string({{_}})", "i.read_u32string()" };

inline std::string kotlinBoxedType(const std::string& t) {
    if (t == "Int") return "Int";
    if (t == "Long") return "Long";
    if (t == "Float") return "Float";
    if (t == "Double") return "Double";
    if (t == "Boolean") return "Boolean";
    if (t == "Char") return "Char";
    if (t == "Byte") return "Byte";
    if (t == "Short") return "Short";
    return t;
}

template <typename T>
constexpr auto buildKotlinMemberInfoArray() {
    using Extractor = type_sequence_extractor<T>;
    constexpr size_t size = Extractor::size;
    return [] <size_t... Is>(std::index_sequence<Is...>) {
        return std::array{
            getKotlinMemberInfo<typename Extractor::template element<Is>>()...
        };
    }(std::make_index_sequence<size>{});
}
template <typename T>
constexpr auto getKotlinMemberInfoArray() { return buildKotlinMemberInfoArray<T>(); }