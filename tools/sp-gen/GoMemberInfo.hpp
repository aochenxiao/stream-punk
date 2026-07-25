#pragma once

#include "MemberInfoBase.hpp"

using namespace sp;

using GoMemberInfo = MemberInfo;

inline std::map<Sz, GoMemberInfo> typeToGo = {
    {E_type::u8   ,{"uint8" , "0"     , "o.WriteU8({{_}})"  , "i.ReadU8()"  }},
    {E_type::u16  ,{"uint16", "0"     , "o.WriteU16({{_}})" , "i.ReadU16()" }},
    {E_type::u32  ,{"uint32", "0"     , "o.WriteU32({{_}})" , "i.ReadU32()" }},
    {E_type::u64  ,{"uint64", "0"     , "o.WriteU64({{_}})" , "i.ReadU64()" }},
    {E_type::i8   ,{"int8"  , "0"     , "o.WriteI8({{_}})"  , "i.ReadI8()"  }},
    {E_type::i16  ,{"int16" , "0"     , "o.WriteI16({{_}})" , "i.ReadI16()" }},
    {E_type::i32  ,{"int32" , "0"     , "o.WriteI32({{_}})" , "i.ReadI32()" }},
    {E_type::i64  ,{"int64" , "0"     , "o.WriteI64({{_}})" , "i.ReadI64()" }},
    {E_type::f32  ,{"float32","0.0"   , "o.WriteF32({{_}})" , "i.ReadF32()" }},
    {E_type::f64  ,{"float64","0.0"   , "o.WriteF64({{_}})" , "i.ReadF64()" }},
    {E_type::bl   ,{"bool"  , "false" , "o.WriteBl({{_}})"  , "i.ReadBl()"  }},
    {E_type::ch   ,{"byte"  , "0"     , "o.WriteCh({{_}})"  , "i.ReadCh()"  }},
    {E_type::ch8  ,{"byte"  , "0"     , "o.WriteCh8({{_}})" , "i.ReadCh8()" }},
    {E_type::ch16 ,{"uint16", "0"     , "o.WriteCh16({{_}})", "i.ReadCh16()"}},
    {E_type::ch32 ,{"uint32", "0"     , "o.WriteCh32({{_}})", "i.ReadCh32()"}},
};

inline GoMemberInfo goNullInfo   { "", "", "", "" };
inline GoMemberInfo goStrInfo    { "string", "\"\"", "o.WriteString({{_}})", "i.ReadString()" };
inline GoMemberInfo goU8StrInfo  { "string", "\"\"", "o.WriteU8String({{_}})", "i.ReadU8String()" };
inline GoMemberInfo goU16StrInfo { "string", "\"\"", "o.WriteU16String({{_}})", "i.ReadU16String()" };
inline GoMemberInfo goU32StrInfo { "[]uint32", "nil", "o.WriteU32String({{_}})", "i.ReadU32String()" };

template <typename T>
constexpr auto buildGoMemberInfoArray() {
    using Extractor = type_sequence_extractor<T>;
    constexpr size_t size = Extractor::size;
    return[] <size_t... Is> (std::index_sequence<Is...>) {
        return std::array{
            getGoMemberInfo<typename Extractor::template element<Is>>()...
        };
    }(std::make_index_sequence<size>{});
}
template <typename T>
constexpr auto getGoMemberInfoArray() { return buildGoMemberInfoArray<T>(); }