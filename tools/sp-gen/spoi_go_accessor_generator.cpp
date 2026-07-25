/*
    spoi_go_accessor_generator.cpp — Go SPOI 访问器代码生成器

    从二进制元数据生成 Go 语言的类型特化访问器（Accessor），
    替代运行时的反射机制，直接通过字段索引访问/设置值。

    生成内容：
    - 每个类型的 Accessor 结构体（GetField/SetField 用 switch 跳转表）
    - 静态类型注册表（SpoiAccessorRegistry）
    - 通用值反序列化函数（基于 type_id 前缀）
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

// =============================== 辅助函数 ===============================

// Go 导出字段名：首字母大写
static std::string goFieldName(const std::string& name) {
    if (name.empty()) return name;
    std::string result = name;
    if (result[0] >= 'a' && result[0] <= 'z') {
        result[0] = result[0] - 'a' + 'A';
    }
    return result;
}

// =============================== Go 类型映射 ===============================

static std::string goTypeName(const std::string& spTypeName) {
    if (spTypeName == "u8")  return "uint8";
    if (spTypeName == "u16") return "uint16";
    if (spTypeName == "u32") return "uint32";
    if (spTypeName == "u64") return "uint64";
    if (spTypeName == "i8")  return "int8";
    if (spTypeName == "i16") return "int16";
    if (spTypeName == "i32") return "int32";
    if (spTypeName == "i64") return "int64";
    if (spTypeName == "f32") return "float32";
    if (spTypeName == "f64") return "float64";
    if (spTypeName == "ch")   return "uint8";   // char → byte
    if (spTypeName == "ch8")  return "uint8";   // char8_t → byte
    if (spTypeName == "ch16") return "uint16";  // char16_t
    if (spTypeName == "ch32") return "uint32";  // char32_t
    if (spTypeName == "string") return "string";
    if (spTypeName == "bool") return "bool";
    return "interface{}";
}

static std::string goTypeIdConst(const std::string& spTypeName) {
    if (spTypeName == "u8")  return "TypeIdU8";
    if (spTypeName == "u16") return "TypeIdU16";
    if (spTypeName == "u32") return "TypeIdU32";
    if (spTypeName == "u64") return "TypeIdU64";
    if (spTypeName == "i8")  return "TypeIdI8";
    if (spTypeName == "i16") return "TypeIdI16";
    if (spTypeName == "i32") return "TypeIdI32";
    if (spTypeName == "i64") return "TypeIdI64";
    if (spTypeName == "f32") return "TypeIdF32";
    if (spTypeName == "f64") return "TypeIdF64";
    if (spTypeName == "ch")   return "TypeIdCh";
    if (spTypeName == "ch8")  return "TypeIdCh8";
    if (spTypeName == "ch16") return "TypeIdCh16";
    if (spTypeName == "ch32") return "TypeIdCh32";
    if (spTypeName == "string") return "TypeIdString";
    if (spTypeName == "bool") return "TypeIdBool";
    return "TypeIdCustom";
}

// =============================== 生成代码 ===============================

std::string generateGoAccessor(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "// ============================================================\n";
    ss << "// SPOI Accessor — Go 类型特化访问器（自动生成）\n";
    ss << "// 由 sp-gen spoi-go-accessor 从 C++ 元数据生成\n";
    ss << "// 替代反射机制，直接通过字段索引访问/设置值\n";
    ss << "// ============================================================\n\n";
    ss << "package main\n\n";
    ss << "import (\n";
    ss << "\t\"encoding/binary\"\n";
    ss << "\t\"math\"\n";
    ss << ")\n\n";

    // ── type_id 常量 ──
    ss << "// ============================================================\n";
    ss << "// 基本类型 ID（与 C++ E_type 枚举值一致）\n";
    ss << "// ============================================================\n\n";
    ss << "const (\n";
    ss << "\tTypeIdU8     uint32 = " << static_cast<uint32_t>(E_type::u8)     << "\n";
    ss << "\tTypeIdU16    uint32 = " << static_cast<uint32_t>(E_type::u16)    << "\n";
    ss << "\tTypeIdU32    uint32 = " << static_cast<uint32_t>(E_type::u32)    << "\n";
    ss << "\tTypeIdU64    uint32 = " << static_cast<uint32_t>(E_type::u64)    << "\n";
    ss << "\tTypeIdI8     uint32 = " << static_cast<uint32_t>(E_type::i8)     << "\n";
    ss << "\tTypeIdI16    uint32 = " << static_cast<uint32_t>(E_type::i16)    << "\n";
    ss << "\tTypeIdI32    uint32 = " << static_cast<uint32_t>(E_type::i32)    << "\n";
    ss << "\tTypeIdI64    uint32 = " << static_cast<uint32_t>(E_type::i64)    << "\n";
    ss << "\tTypeIdF32    uint32 = " << static_cast<uint32_t>(E_type::f32)    << "\n";
    ss << "\tTypeIdF64    uint32 = " << static_cast<uint32_t>(E_type::f64)    << "\n";
    ss << "\tTypeIdString uint32 = " << static_cast<uint32_t>(E_type::string) << "\n";
    ss << "\tTypeIdBool   uint32 = " << static_cast<uint32_t>(E_type::bl)     << "\n";
    ss << "\tTypeIdCh     uint32 = " << static_cast<uint32_t>(E_type::ch)     << "\n";
    ss << "\tTypeIdCh8    uint32 = " << static_cast<uint32_t>(E_type::ch8)    << "\n";
    ss << "\tTypeIdCh16   uint32 = " << static_cast<uint32_t>(E_type::ch16)   << "\n";
    ss << "\tTypeIdCh32   uint32 = " << static_cast<uint32_t>(E_type::ch32)   << "\n";
    ss << ")\n\n";

    // ── SpoiAccessor 接口 ──
    ss << "// ============================================================\n";
    ss << "// SpoiAccessor — 类型特化访问器接口\n";
    ss << "// ============================================================\n\n";
    ss << "type SpoiAccessor interface {\n";
    ss << "\tFieldCount() int\n";
    ss << "\tGetField(obj any, idx int) any\n";
    ss << "\tSetField(obj any, idx int, val any)\n";
    ss << "}\n\n";

    // ── 通用值反序列化（基于 type_id 前缀） ──
    ss << "// ============================================================\n";
    ss << "// DeserializeValue — 通用值反序列化（基于 type_id 前缀）\n";
    ss << "// 格式: [type_id(u32 LE) + value_bytes]\n";
    ss << "// ============================================================\n\n";
    ss << "func DeserializeValue(data []byte) any {\n";
    ss << "\tif len(data) < 4 {\n";
    ss << "\t\treturn nil\n";
    ss << "\t}\n";
    ss << "\ttypeId := binary.LittleEndian.Uint32(data[:4])\n";
    ss << "\tvalueBytes := data[4:]\n";
    ss << "\tswitch typeId {\n";
    ss << "\tcase TypeIdU8:\n";
    ss << "\t\tif len(valueBytes) >= 1 {\n\t\t\treturn valueBytes[0]\n\t\t}\n\t\treturn uint8(0)\n";
    ss << "\tcase TypeIdU16:\n";
    ss << "\t\tif len(valueBytes) >= 2 {\n\t\t\treturn binary.LittleEndian.Uint16(valueBytes)\n\t\t}\n\t\treturn uint16(0)\n";
    ss << "\tcase TypeIdU32:\n";
    ss << "\t\tif len(valueBytes) >= 4 {\n\t\t\treturn binary.LittleEndian.Uint32(valueBytes)\n\t\t}\n\t\treturn uint32(0)\n";
    ss << "\tcase TypeIdU64:\n";
    ss << "\t\tif len(valueBytes) >= 8 {\n\t\t\treturn binary.LittleEndian.Uint64(valueBytes)\n\t\t}\n\t\treturn uint64(0)\n";
    ss << "\tcase TypeIdI8:\n";
    ss << "\t\tif len(valueBytes) >= 1 {\n\t\t\treturn int8(valueBytes[0])\n\t\t}\n\t\treturn int8(0)\n";
    ss << "\tcase TypeIdI16:\n";
    ss << "\t\tif len(valueBytes) >= 2 {\n\t\t\treturn int16(binary.LittleEndian.Uint16(valueBytes))\n\t\t}\n\t\treturn int16(0)\n";
    ss << "\tcase TypeIdI32:\n";
    ss << "\t\tif len(valueBytes) >= 4 {\n\t\t\treturn int32(binary.LittleEndian.Uint32(valueBytes))\n\t\t}\n\t\treturn int32(0)\n";
    ss << "\tcase TypeIdI64:\n";
    ss << "\t\tif len(valueBytes) >= 8 {\n\t\t\treturn int64(binary.LittleEndian.Uint64(valueBytes))\n\t\t}\n\t\treturn int64(0)\n";
    ss << "\tcase TypeIdF32:\n";
    ss << "\t\tif len(valueBytes) >= 4 {\n\t\t\treturn math.Float32frombits(binary.LittleEndian.Uint32(valueBytes))\n\t\t}\n\t\treturn float32(0)\n";
    ss << "\tcase TypeIdF64:\n";
    ss << "\t\tif len(valueBytes) >= 8 {\n\t\t\treturn math.Float64frombits(binary.LittleEndian.Uint64(valueBytes))\n\t\t}\n\t\treturn float64(0)\n";
    ss << "\tcase TypeIdString:\n";
    ss << "\t\treturn string(valueBytes)\n";
    ss << "\tcase TypeIdBool:\n";
    ss << "\t\tif len(valueBytes) >= 1 {\n\t\t\treturn valueBytes[0] != 0\n\t\t}\n\t\treturn false\n";
    ss << "\tcase TypeIdCh:\n";
    ss << "\t\tif len(valueBytes) >= 1 {\n\t\t\treturn valueBytes[0]\n\t\t}\n\t\treturn uint8(0)\n";
    ss << "\tcase TypeIdCh8:\n";
    ss << "\t\tif len(valueBytes) >= 1 {\n\t\t\treturn valueBytes[0]\n\t\t}\n\t\treturn uint8(0)\n";
    ss << "\tcase TypeIdCh16:\n";
    ss << "\t\tif len(valueBytes) >= 2 {\n\t\t\treturn binary.LittleEndian.Uint16(valueBytes)\n\t\t}\n\t\treturn uint16(0)\n";
    ss << "\tcase TypeIdCh32:\n";
    ss << "\t\tif len(valueBytes) >= 4 {\n\t\t\treturn binary.LittleEndian.Uint32(valueBytes)\n\t\t}\n\t\treturn uint32(0)\n";
    ss << "\tdefault:\n";
    ss << "\t\treturn valueBytes\n";
    ss << "\t}\n";
    ss << "}\n\n";

    // ── 每个类型的访问器 ──
    for (auto& t : types) {
        ss << "// ============================================================\n";
        ss << "// " << t.className << "Accessor\n";
        ss << "// ============================================================\n\n";
        ss << "type " << t.className << "Accessor struct{}\n\n";

        ss << "func (a " << t.className << "Accessor) FieldCount() int {\n";
        ss << "\treturn " << t.fields.size() << "\n";
        ss << "}\n\n";

        // GetField
        ss << "func (a " << t.className << "Accessor) GetField(obj any, idx int) any {\n";
        ss << "\to := obj.(*" << t.className << ")\n";
        ss << "\tswitch idx {\n";
        for (auto& f : t.fields) {
            ss << "\tcase " << f.index << ":\n";
            ss << "\t\treturn o." << goFieldName(f.name) << "\n";
        }
        ss << "\tdefault:\n";
        ss << "\t\tpanic(\"invalid field index for " << t.className << "\")\n";
        ss << "\t}\n";
        ss << "}\n\n";

        // SetField
        ss << "func (a " << t.className << "Accessor) SetField(obj any, idx int, val any) {\n";
        ss << "\to := obj.(*" << t.className << ")\n";
        ss << "\tswitch idx {\n";
        for (auto& f : t.fields) {
            ss << "\tcase " << f.index << ":\n";
            std::string goType = goTypeName(f.typeName);
            // 容器、指针、optional 类型：Go 结构体中存储为 interface{}，直接赋值
            if (f.isContainer || f.isPointer || f.isOptional || f.typeName == "custom") {
                ss << "\t\to." << goFieldName(f.name) << " = val\n";
            }
            // 浮点类型需要处理 float32/float64 互转（跨类型 SET 操作可能传入不匹配的浮点类型）
            else if (goType == "float64") {
                ss << "\t\tswitch v := val.(type) {\n";
                ss << "\t\tcase float64:\n\t\t\to." << goFieldName(f.name) << " = v\n";
                ss << "\t\tcase float32:\n\t\t\to." << goFieldName(f.name) << " = float64(v)\n";
                ss << "\t\tdefault:\n\t\t\tpanic(\"invalid type for " << goFieldName(f.name) << "\")\n";
                ss << "\t\t}\n";
            } else if (goType == "float32") {
                ss << "\t\tswitch v := val.(type) {\n";
                ss << "\t\tcase float32:\n\t\t\to." << goFieldName(f.name) << " = v\n";
                ss << "\t\tcase float64:\n\t\t\to." << goFieldName(f.name) << " = float32(v)\n";
                ss << "\t\tdefault:\n\t\t\tpanic(\"invalid type for " << goFieldName(f.name) << "\")\n";
                ss << "\t\t}\n";
            } else {
                ss << "\t\to." << goFieldName(f.name) << " = val.(" << goType << ")\n";
            }
        }
        ss << "\tdefault:\n";
        ss << "\t\tpanic(\"invalid field index for " << t.className << "\")\n";
        ss << "\t}\n";
        ss << "}\n\n";
    }

    // ── 静态类型注册表 ──
    ss << "// ============================================================\n";
    ss << "// SpoiAccessorRegistry — 静态类型注册表\n";
    ss << "// 替代运行时 map[string][]string\n";
    ss << "// ============================================================\n\n";
    ss << "var SpoiAccessorRegistry = map[string]SpoiAccessor{\n";
    for (auto& t : types) {
        ss << "\t\"" << t.className << "\": " << t.className << "Accessor{},\n";
    }
    ss << "}\n\n";

    return ss.str();
}

// =============================== 入口函数 ===============================

int generate_spoi_go_accessor(const std::string& output_path, const std::string& meta_path) {
    try {
        auto meta = sp_meta::readMetaFile(meta_path);
        auto types = extractSpoiTypes(meta);
        std::cout << "Extracted " << types.size() << " types for Go accessor generation" << std::endl;

        auto code = generateGoAccessor(types);

        std::ofstream out(output_path);
        if (!out.is_open()) throw std::runtime_error("Cannot open: " + output_path);
        out << code;
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "SPOI Go accessor generator error: " << e.what() << "\n";
        return 1;
    }
}