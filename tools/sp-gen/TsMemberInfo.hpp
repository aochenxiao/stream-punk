#pragma once

#include "MemberInfoBase.hpp"

using namespace sp;

// v2 风格：驼峰命名 API
inline std::map<Sz, MemberInfo> typeToTs = {
    {E_type::u8     ,{"number" , "0"     , "o.writeU8({{_}})" , "i.readU8()"}},
    {E_type::u16    ,{"number" , "0"     , "o.writeU16({{_}})", "i.readU16()"}},
    {E_type::u32    ,{"number" , "0"     , "o.writeU32({{_}})", "i.readU32()"}},
    {E_type::u64    ,{"bigint" , "0n"    , "o.writeU64({{_}})", "i.readU64()"}},
    {E_type::i8     ,{"number" , "0"     , "o.writeI8({{_}})" , "i.readI8()"}},
    {E_type::i16    ,{"number" , "0"     , "o.writeI16({{_}})", "i.readI16()"}},
    {E_type::i32    ,{"number" , "0"     , "o.writeI32({{_}})", "i.readI32()"}},
    {E_type::i64    ,{"bigint" , "0n"    , "o.writeI64({{_}})", "i.readI64()"}},
    {E_type::f32    ,{"number" , "0.0"   , "o.writeF32({{_}})", "i.readF32()"}},
    {E_type::f64    ,{"number" , "0.0"   , "o.writeF64({{_}})", "i.readF64()"}},
    {E_type::bl     ,{"boolean","false"  , "o.writeBl({{_}})" , "i.readBl()"}},
    {E_type::ch     ,{"string" ,"\"\""   , "o.writeCh({{_}})" , "i.readCh()"}},
    {E_type::ch8    ,{"string" ,"\"\""   , "o.writeCh8({{_}})", "i.readCh8()"}},
    {E_type::ch16   ,{"string" ,"\"\""   , "o.writeCh16({{_}})", "i.readCh16()"}},
    {E_type::ch32   ,{"string" ,"\"\""   , "o.writeCh32({{_}})", "i.readCh32()"}},
};
inline MemberInfo strInfo   { "string", "\"\"", "o.writeString({{_}})", "i.readString()" };
inline MemberInfo u8StrInfo { "string", "\"\"", "o.writeU8String({{_}})", "i.readU8String()" };
inline MemberInfo u16StrInfo{ "string", "\"\"", "o.writeU16String({{_}})", "i.readU16String()" };
inline MemberInfo u32StrInfo{ "string", "\"\"", "o.writeU32String({{_}})", "i.readU32String()" };

template <typename T>
constexpr auto buildMemberInfoArray() {
    using Extractor = type_sequence_extractor<T>;
    constexpr size_t size = Extractor::size;
    return [] <size_t... Is>(std::index_sequence<Is...>) {
        return std::array{
            getMemberInfo<typename Extractor::template element<Is>>()...
        };
    }(std::make_index_sequence<size>{});
}

template <typename T>
constexpr auto getMemberInfoArray() { return buildMemberInfoArray<T>(); }

template <typename... Ts>
constexpr auto getMemberInfoArray(const std::tuple<Ts...>&) { return buildMemberInfoArray<std::tuple<Ts...>>(); }

template <typename... Ts>
constexpr auto getMemberInfoArray(const std::variant<Ts...>&) { return buildMemberInfoArray<std::variant<Ts...>>(); }