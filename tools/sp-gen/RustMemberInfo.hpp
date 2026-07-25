#pragma once

#include "MemberInfoBase.hpp"

using namespace sp;

using RustMemberInfo = MemberInfo;

inline std::map<Sz, RustMemberInfo> typeToRust = {
    {E_type::u8   ,{"u8"   , "0"         , "o.write_u8({{_}})"  , "i.read_u8()"  }},
    {E_type::u16  ,{"u16"  , "0"         , "o.write_u16({{_}})" , "i.read_u16()" }},
    {E_type::u32  ,{"u32"  , "0"         , "o.write_u32({{_}})" , "i.read_u32()" }},
    {E_type::u64  ,{"u64"  , "0"         , "o.write_u64({{_}})" , "i.read_u64()" }},
    {E_type::i8   ,{"i8"   , "0"         , "o.write_i8({{_}})"  , "i.read_i8()"  }},
    {E_type::i16  ,{"i16"  , "0"         , "o.write_i16({{_}})" , "i.read_i16()" }},
    {E_type::i32  ,{"i32"  , "0"         , "o.write_i32({{_}})" , "i.read_i32()" }},
    {E_type::i64  ,{"i64"  , "0"         , "o.write_i64({{_}})" , "i.read_i64()" }},
    {E_type::f32  ,{"f32"  , "0.0"       , "o.write_f32({{_}})" , "i.read_f32()" }},
    {E_type::f64  ,{"f64"  , "0.0"       , "o.write_f64({{_}})" , "i.read_f64()" }},
    {E_type::bl   ,{"bool" , "false"     , "o.write_bl({{_}})"  , "i.read_bl()"  }},
    {E_type::ch   ,{"char" , "'\\0'"     , "o.write_ch({{_}})"  , "i.read_ch()"  }},
    {E_type::ch8  ,{"char" , "'\\0'"     , "o.write_ch8({{_}})" , "i.read_ch8()" }},
    {E_type::ch16 ,{"char" , "'\\0'"     , "o.write_ch16({{_}})", "i.read_ch16()"}},
    {E_type::ch32 ,{"u32"  , "0"         , "o.write_ch32({{_}})", "i.read_ch32()"}},
};

inline RustMemberInfo rustNullInfo   { "", "", "", "" };
inline RustMemberInfo rustStrInfo    { "String", "String::new()", "o.write_string(&{{_}})", "i.read_string()" };
inline RustMemberInfo rustU8StrInfo  { "String", "String::new()", "o.write_u8string(&{{_}})", "i.read_u8string()" };
inline RustMemberInfo rustU16StrInfo { "String", "String::new()", "o.write_u16string(&{{_}})", "i.read_u16string()" };
inline RustMemberInfo rustU32StrInfo { "Vec<u32>", "Vec::new()", "o.write_u32string(&{{_}})", "i.read_u32string()" };

template <typename T>
constexpr auto buildRustMemberInfoArray() {
    using Extractor = type_sequence_extractor<T>;
    constexpr size_t size = Extractor::size;
    return[] <size_t... Is> (std::index_sequence<Is...>) {
        return std::array{
            getRustMemberInfo<typename Extractor::template element<Is>>()...
        };
    }(std::make_index_sequence<size>{});
}
template <typename T>
constexpr auto getRustMemberInfoArray() { return buildRustMemberInfoArray<T>(); }