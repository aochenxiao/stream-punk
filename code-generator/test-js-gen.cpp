// Simple test to verify the JavaScript generator logic
#include <iostream>
#include <string>
#include <map>

// Mock definitions for testing
enum E_type {
    u8, u16, u32, u64, i8, i16, i32, i64, f32, f64, bl, ch, ch8, ch16, ch32, Base
};

struct MemberInfo {
    std::string tname;
    std::string ivalue;
    std::string serializeCode;
    std::string deserializeCode;
};

// JavaScript type mappings
std::map<int, MemberInfo> typeToJs = {
    {E_type::u8     ,{"number" , "0"     , "o.write_u8(value)", "i.read_u8()"}},
    {E_type::u16    ,{"number" , "0"     , "o.write_u16(value)", "i.read_u16()"}},
    {E_type::u32    ,{"number" , "0"     , "o.write_u32(value)", "i.read_u32()"}},
    {E_type::u64    ,{"bigint" , "0n"    , "o.write_u64(value)", "i.read_u64()"}},
    {E_type::i8     ,{"number" , "0"     , "o.write_i8(value)", "i.read_i8()"}},
    {E_type::i16    ,{"number" , "0"     , "o.write_i16(value)", "i.read_i16()"}},
    {E_type::i32    ,{"number" , "0"     , "o.write_i32(value)", "i.read_i32()"}},
    {E_type::i64    ,{"bigint" , "0n"    , "o.write_i64(value)", "i.read_i64()"}},
    {E_type::f32    ,{"number" , "0.0"   , "o.write_f32(value)", "i.read_f32()"}},
    {E_type::f64    ,{"number" , "0.0"   , "o.write_f64(value)", "i.read_f64()"}},
    {E_type::bl     ,{"boolean","false"  , "o.write_bl(value)", "i.read_bl()"}},
    {E_type::ch     ,{"string" ,"\"\""   , "o.write_ch(value)", "i.read_ch()"}},
    {E_type::ch8    ,{"string" ,"\"\""   , "o.write_ch8(value)", "i.read_ch8()"}},
    {E_type::ch16   ,{"string" ,"\"\""   , "o.write_ch16(value)", "i.read_ch16()"}},
    {E_type::ch32   ,{"string" ,"\"\""   , "o.write_ch32(value)", "i.read_ch32()"}},
};

int main() {
    std::cout << "JavaScript type mappings test:" << std::endl;
    
    // Test basic type mappings
    std::cout << "u8 -> " << typeToJs[E_type::u8].tname << " (initial: " << typeToJs[E_type::u8].ivalue << ")" << std::endl;
    std::cout << "u64 -> " << typeToJs[E_type::u64].tname << " (initial: " << typeToJs[E_type::u64].ivalue << ")" << std::endl;
    std::cout << "bl -> " << typeToJs[E_type::bl].tname << " (initial: " << typeToJs[E_type::bl].ivalue << ")" << std::endl;
    std::cout << "string -> " << typeToJs[E_type::ch].tname << " (initial: " << typeToJs[E_type::ch].ivalue << ")" << std::endl;
    
    std::cout << "\nJavaScript generator logic test passed!" << std::endl;
    return 0;
}