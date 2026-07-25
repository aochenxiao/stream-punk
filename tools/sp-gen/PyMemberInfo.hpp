#pragma once

#include "MemberInfoBase.hpp"

using namespace sp;

using PyMemberInfo = MemberInfo;

inline std::map<Sz, PyMemberInfo> typeToPy = {
    {E_type::u8     ,{"int" , "0"     , "o.write_u8({{_}})" , "i.read_u8()"}},
    {E_type::u16    ,{"int" , "0"     , "o.write_u16({{_}})", "i.read_u16()"}},
    {E_type::u32    ,{"int" , "0"     , "o.write_u32({{_}})", "i.read_u32()"}},
    {E_type::u64    ,{"int" , "0"     , "o.write_u64({{_}})", "i.read_u64()"}},
    {E_type::i8     ,{"int" , "0"     , "o.write_i8({{_}})" , "i.read_i8()"}},
    {E_type::i16    ,{"int" , "0"     , "o.write_i16({{_}})", "i.read_i16()"}},
    {E_type::i32    ,{"int" , "0"     , "o.write_i32({{_}})", "i.read_i32()"}},
    {E_type::i64    ,{"int" , "0"     , "o.write_i64({{_}})", "i.read_i64()"}},
    {E_type::f32    ,{"float", "0.0"  , "o.write_f32({{_}})", "i.read_f32()"}},
    {E_type::f64    ,{"float", "0.0"  , "o.write_f64({{_}})", "i.read_f64()"}},
    {E_type::bl     ,{"bool" ,"False" , "o.write_bl({{_}})" , "i.read_bl()"}},
    {E_type::ch     ,{"str"  ,"\"\""  , "o.write_ch({{_}})" , "i.read_ch()"}},
    {E_type::ch8    ,{"str"  ,"\"\""  , "o.write_ch8({{_}})", "i.read_ch8()"}},
    {E_type::ch16   ,{"str"  ,"\"\""  , "o.write_ch16({{_}})", "i.read_ch16()"}},
    {E_type::ch32   ,{"str"  ,"\"\""  , "o.write_ch32({{_}})", "i.read_ch32()"}},
};

inline PyMemberInfo pyNullInfo  { "", "", "", "" };
inline PyMemberInfo pyStrInfo   { "str", "\"\"", "o.write_string({{_}})", "i.read_string()" };
inline PyMemberInfo pyU8StrInfo { "str", "\"\"", "o.write_u8string({{_}})", "i.read_u8string()" };
inline PyMemberInfo pyU16StrInfo{ "str", "\"\"", "o.write_u16string({{_}})", "i.read_u16string()" };
inline PyMemberInfo pyU32StrInfo{ "str", "\"\"", "o.write_u32string({{_}})", "i.read_u32string()" };

template <typename T>
constexpr auto buildPyMemberInfoArray() {
    using Extractor = type_sequence_extractor<T>;
    constexpr size_t size = Extractor::size;
    return [] <size_t... Is>(std::index_sequence<Is...>) {
        return std::array{
            getPyMemberInfo<typename Extractor::template element<Is>>()...
        };
    }(std::make_index_sequence<size>{});
}

template <typename T>
constexpr auto getPyMemberInfoArray() { return buildPyMemberInfoArray<T>(); }