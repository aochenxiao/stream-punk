/*
    spoi_rust_accessor_generator.cpp — Rust SPOI 访问器代码生成器

    从二进制元数据生成 Rust 语言的类型特化访问器（Accessor），
    替代运行时的反射机制，直接通过字段索引访问/设置值。

    生成内容：
    - 每个类型的 Accessor 结构体（get_field/set_field 用 match 跳转表）
    - 静态类型注册表（SpoiAccessorRegistry）
    - 通用值反序列化函数（基于 type_id 前缀）
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

// =============================== Rust 类型映射 ===============================

static std::string rustTypeName(const std::string& spTypeName) {
    if (spTypeName == "u8")  return "u8";
    if (spTypeName == "u16") return "u16";
    if (spTypeName == "u32") return "u32";
    if (spTypeName == "u64") return "u64";
    if (spTypeName == "i8")  return "i8";
    if (spTypeName == "i16") return "i16";
    if (spTypeName == "i32") return "i32";
    if (spTypeName == "i64") return "i64";
    if (spTypeName == "f32") return "f32";
    if (spTypeName == "f64") return "f64";
    if (spTypeName == "ch")  return "u8";
    if (spTypeName == "ch8") return "u8";
    if (spTypeName == "ch16") return "u16";
    if (spTypeName == "ch32") return "u32";
    if (spTypeName == "string") return "String";
    if (spTypeName == "bool") return "bool";
    return "Box<dyn Any>";
}

static std::string rustTypeIdConst(const std::string& spTypeName) {
    if (spTypeName == "u8")  return "TYPE_ID_U8";
    if (spTypeName == "u16") return "TYPE_ID_U16";
    if (spTypeName == "u32") return "TYPE_ID_U32";
    if (spTypeName == "u64") return "TYPE_ID_U64";
    if (spTypeName == "i8")  return "TYPE_ID_I8";
    if (spTypeName == "i16") return "TYPE_ID_I16";
    if (spTypeName == "i32") return "TYPE_ID_I32";
    if (spTypeName == "i64") return "TYPE_ID_I64";
    if (spTypeName == "f32") return "TYPE_ID_F32";
    if (spTypeName == "f64") return "TYPE_ID_F64";
    if (spTypeName == "ch")  return "TYPE_ID_CH";
    if (spTypeName == "ch8") return "TYPE_ID_CH8";
    if (spTypeName == "ch16") return "TYPE_ID_CH16";
    if (spTypeName == "ch32") return "TYPE_ID_CH32";
    if (spTypeName == "string") return "TYPE_ID_STRING";
    if (spTypeName == "bool") return "TYPE_ID_BOOL";
    return "TYPE_ID_CUSTOM";
}

// 根据字段的完整类型信息（含容器/optional/指针标记）计算 Rust 类型名
static std::string rustTypeFull(const SpoiFieldInfo& f) {
    if (f.isPointer) {
        return "Box<dyn Any>";
    }
    if (f.isContainer) {
        // 容器类型：Vec<T> 或 HashMap<K, V>
        if (f.containerKind == "map" || f.containerKind == "umap") {
            return "HashMap<" + rustTypeName(f.containerKeyTypeName) + ", " + rustTypeName(f.containerValueTypeName) + ">";
        }
        return "Vec<" + rustTypeName(f.containerValueTypeName) + ">";
    }
    if (f.isOptional) {
        return "Option<" + rustTypeName(f.containerValueTypeName) + ">";
    }
    return rustTypeName(f.typeName);
}

// =============================== 生成代码 ===============================

std::string generateRustAccessor(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "// ============================================================\n";
    ss << "// SPOI Accessor — Rust 类型特化访问器（自动生成）\n";
    ss << "// 由 sp-gen spoi-rust-accessor 从 C++ 元数据生成\n";
    ss << "// 替代反射机制，直接通过字段索引访问/设置值\n";
    ss << "// ============================================================\n\n";

    ss << "use std::any::Any;\n";
    ss << "use std::collections::HashMap;\n";
    ss << "use super::*;\n\n";

    // ── type_id 常量 ──
    ss << "// ============================================================\n";
    ss << "// 基本类型 ID（与 C++ E_type 枚举值一致）\n";
    ss << "// ============================================================\n\n";
    ss << "pub const TYPE_ID_U8: u32 = " << static_cast<uint32_t>(E_type::u8) << ";\n";
    ss << "pub const TYPE_ID_U16: u32 = " << static_cast<uint32_t>(E_type::u16) << ";\n";
    ss << "pub const TYPE_ID_U32: u32 = " << static_cast<uint32_t>(E_type::u32) << ";\n";
    ss << "pub const TYPE_ID_U64: u32 = " << static_cast<uint32_t>(E_type::u64) << ";\n";
    ss << "pub const TYPE_ID_I8: u32 = " << static_cast<uint32_t>(E_type::i8) << ";\n";
    ss << "pub const TYPE_ID_I16: u32 = " << static_cast<uint32_t>(E_type::i16) << ";\n";
    ss << "pub const TYPE_ID_I32: u32 = " << static_cast<uint32_t>(E_type::i32) << ";\n";
    ss << "pub const TYPE_ID_I64: u32 = " << static_cast<uint32_t>(E_type::i64) << ";\n";
    ss << "pub const TYPE_ID_F32: u32 = " << static_cast<uint32_t>(E_type::f32) << ";\n";
    ss << "pub const TYPE_ID_F64: u32 = " << static_cast<uint32_t>(E_type::f64) << ";\n";
    ss << "pub const TYPE_ID_CH: u32 = " << static_cast<uint32_t>(E_type::ch) << ";\n";
    ss << "pub const TYPE_ID_CH8: u32 = " << static_cast<uint32_t>(E_type::ch8) << ";\n";
    ss << "pub const TYPE_ID_CH16: u32 = " << static_cast<uint32_t>(E_type::ch16) << ";\n";
    ss << "pub const TYPE_ID_CH32: u32 = " << static_cast<uint32_t>(E_type::ch32) << ";\n";
    ss << "pub const TYPE_ID_STRING: u32 = " << static_cast<uint32_t>(E_type::string) << ";\n";
    ss << "pub const TYPE_ID_BOOL: u32 = " << static_cast<uint32_t>(E_type::bl) << ";\n\n";

    // ── SpoiAccessor trait ──
    ss << "// ============================================================\n";
    ss << "// SpoiAccessor — 类型特化访问器 trait\n";
    ss << "// ============================================================\n\n";
    ss << "pub trait SpoiAccessor: Send + Sync {\n";
    ss << "    fn field_count(&self) -> usize;\n";
    ss << "    fn get_field<'a>(&self, obj: &'a dyn Any, idx: usize) -> Option<&'a dyn Any>;\n";
    ss << "    fn set_field(&self, obj: &mut dyn Any, idx: usize, val: Box<dyn Any>);\n";
    ss << "}\n\n";

    // ── 通用值反序列化（基于 type_id 前缀） ──
    ss << "// ============================================================\n";
    ss << "// deserialize_value — 通用值反序列化（基于 type_id 前缀）\n";
    ss << "// 格式: [type_id(u32 LE) + value_bytes]\n";
    ss << "// ============================================================\n\n";
    ss << "pub fn deserialize_value(data: &[u8]) -> Box<dyn Any> {\n";
    ss << "    if data.len() < 4 {\n";
    ss << "        return Box::new(());\n";
    ss << "    }\n";
    ss << "    let type_id = u32::from_le_bytes([data[0], data[1], data[2], data[3]]);\n";
    ss << "    let value_bytes = &data[4..];\n";
    ss << "    match type_id {\n";
    ss << "        TYPE_ID_U8 => {\n";
    ss << "            Box::new(value_bytes.first().copied().unwrap_or(0))\n";
    ss << "        }\n";
    ss << "        TYPE_ID_U16 => {\n";
    ss << "            Box::new(if value_bytes.len() >= 2 { u16::from_le_bytes([value_bytes[0], value_bytes[1]]) } else { 0u16 })\n";
    ss << "        }\n";
    ss << "        TYPE_ID_U32 => {\n";
    ss << "            Box::new(if value_bytes.len() >= 4 { u32::from_le_bytes([value_bytes[0], value_bytes[1], value_bytes[2], value_bytes[3]]) } else { 0u32 })\n";
    ss << "        }\n";
    ss << "        TYPE_ID_U64 => {\n";
    ss << "            Box::new(if value_bytes.len() >= 8 {\n";
    ss << "                u64::from_le_bytes([\n";
    ss << "                    value_bytes[0], value_bytes[1], value_bytes[2], value_bytes[3],\n";
    ss << "                    value_bytes[4], value_bytes[5], value_bytes[6], value_bytes[7],\n";
    ss << "                ])\n";
    ss << "            } else { 0u64 })\n";
    ss << "        }\n";
    ss << "        TYPE_ID_I8 => {\n";
    ss << "            Box::new(value_bytes.first().copied().unwrap_or(0) as i8)\n";
    ss << "        }\n";
    ss << "        TYPE_ID_I16 => {\n";
    ss << "            Box::new(if value_bytes.len() >= 2 { i16::from_le_bytes([value_bytes[0], value_bytes[1]]) } else { 0i16 })\n";
    ss << "        }\n";
    ss << "        TYPE_ID_I32 => {\n";
    ss << "            Box::new(if value_bytes.len() >= 4 { i32::from_le_bytes([value_bytes[0], value_bytes[1], value_bytes[2], value_bytes[3]]) } else { 0i32 })\n";
    ss << "        }\n";
    ss << "        TYPE_ID_I64 => {\n";
    ss << "            Box::new(if value_bytes.len() >= 8 {\n";
    ss << "                i64::from_le_bytes([\n";
    ss << "                    value_bytes[0], value_bytes[1], value_bytes[2], value_bytes[3],\n";
    ss << "                    value_bytes[4], value_bytes[5], value_bytes[6], value_bytes[7],\n";
    ss << "                ])\n";
    ss << "            } else { 0i64 })\n";
    ss << "        }\n";
    ss << "        TYPE_ID_F32 => {\n";
    ss << "            Box::new(if value_bytes.len() >= 4 { f32::from_le_bytes([value_bytes[0], value_bytes[1], value_bytes[2], value_bytes[3]]) } else { 0.0f32 })\n";
    ss << "        }\n";
    ss << "        TYPE_ID_F64 => {\n";
    ss << "            Box::new(if value_bytes.len() >= 8 {\n";
    ss << "                f64::from_le_bytes([\n";
    ss << "                    value_bytes[0], value_bytes[1], value_bytes[2], value_bytes[3],\n";
    ss << "                    value_bytes[4], value_bytes[5], value_bytes[6], value_bytes[7],\n";
    ss << "                ])\n";
    ss << "            } else { 0.0f64 })\n";
    ss << "        }\n";
    ss << "        TYPE_ID_STRING => {\n";
    ss << "            match String::from_utf8(value_bytes.to_vec()) {\n";
    ss << "                Ok(s) => Box::new(s),\n";
    ss << "                Err(_) => Box::new(value_bytes.to_vec()),\n";
    ss << "            }\n";
    ss << "        }\n";
    ss << "        TYPE_ID_BOOL => {\n";
    ss << "            Box::new(value_bytes.first().copied().unwrap_or(0) != 0)\n";
    ss << "        }\n";
    ss << "        TYPE_ID_CH => {\n";
    ss << "            Box::new(value_bytes.first().copied().unwrap_or(0))\n";
    ss << "        }\n";
    ss << "        TYPE_ID_CH8 => {\n";
    ss << "            Box::new(value_bytes.first().copied().unwrap_or(0))\n";
    ss << "        }\n";
    ss << "        TYPE_ID_CH16 => {\n";
    ss << "            Box::new(if value_bytes.len() >= 2 { u16::from_le_bytes([value_bytes[0], value_bytes[1]]) } else { 0u16 })\n";
    ss << "        }\n";
    ss << "        TYPE_ID_CH32 => {\n";
    ss << "            Box::new(if value_bytes.len() >= 4 { u32::from_le_bytes([value_bytes[0], value_bytes[1], value_bytes[2], value_bytes[3]]) } else { 0u32 })\n";
    ss << "        }\n";
    ss << "        _ => Box::new(value_bytes.to_vec()),\n";
    ss << "    }\n";
    ss << "}\n\n";

    // ── 每个类型的访问器 ──
    for (auto& t : types) {
        ss << "// ============================================================\n";
        ss << "// " << t.className << "Accessor\n";
        ss << "// ============================================================\n\n";
        ss << "pub struct " << t.className << "Accessor;\n\n";

        ss << "impl SpoiAccessor for " << t.className << "Accessor {\n";

        ss << "    fn field_count(&self) -> usize {\n";
        ss << "        " << t.fields.size() << "\n";
        ss << "    }\n\n";

        // get_field
        ss << "    fn get_field<'a>(&self, obj: &'a dyn Any, idx: usize) -> Option<&'a dyn Any> {\n";
        ss << "        let o = obj.downcast_ref::<" << t.className << ">()?;\n";
        ss << "        match idx {\n";
        for (auto& f : t.fields) {
            ss << "            " << f.index << " => Some(&o." << f.name << "),\n";
        }
        ss << "            _ => None,\n";
        ss << "        }\n";
        ss << "    }\n\n";

        // set_field
        ss << "    fn set_field(&self, obj: &mut dyn Any, idx: usize, val: Box<dyn Any>) {\n";
        ss << "        let o = match obj.downcast_mut::<" << t.className << ">() {\n";
        ss << "            Some(o) => o,\n";
        ss << "            None => return,\n";
        ss << "        };\n";
        ss << "        match idx {\n";
        for (auto& f : t.fields) {
            std::string rustType = rustTypeFull(f);
            // 容器/optional/指针类型使用完整类型名；基本类型使用简化分支
            if (f.isContainer || f.isOptional || f.isPointer) {
                ss << "            " << f.index << " => {\n";
                ss << "                if let Ok(v) = val.downcast::<" << rustType << ">() {\n";
                ss << "                    o." << f.name << " = *v;\n";
                ss << "                }\n";
                ss << "            }\n";
            } else {
                ss << "            " << f.index << " => {\n";
                ss << "                if let Ok(v) = val.downcast::<" << rustType << ">() {\n";
                ss << "                    o." << f.name << " = *v;\n";
                ss << "                }\n";
                ss << "            }\n";
            }
        }
        ss << "            _ => {}\n";
        ss << "        }\n";
        ss << "    }\n";

        ss << "}\n\n";
    }

    // ── 静态类型注册表 ──
    ss << "// ============================================================\n";
    ss << "// SpoiAccessorRegistry — 静态类型注册表\n";
    ss << "// 替代运行时 HashMap<String, Vec<String>>\n";
    ss << "// ============================================================\n\n";
    ss << "pub fn create_spoi_accessor_registry() -> HashMap<String, Box<dyn SpoiAccessor>> {\n";
    ss << "    let mut registry: HashMap<String, Box<dyn SpoiAccessor>> = HashMap::new();\n";
    for (auto& t : types) {
        ss << "    registry.insert(\"" << t.className << "\".to_string(), Box::new(" << t.className << "Accessor));\n";
    }
    ss << "    registry\n";
    ss << "}\n\n";

    return ss.str();
}

// =============================== 入口函数 ===============================

int generate_spoi_rust_accessor(const std::string& output_path, const std::string& meta_path) {
    try {
        auto meta = sp_meta::readMetaFile(meta_path);
        auto types = extractSpoiTypes(meta);
        std::cout << "Extracted " << types.size() << " types for Rust accessor generation" << std::endl;

        auto code = generateRustAccessor(types);

        std::ofstream out(output_path);
        if (!out.is_open()) throw std::runtime_error("Cannot open: " + output_path);
        out << code;
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "SPOI Rust accessor generator error: " << e.what() << "\n";
        return 1;
    }
}