/*
    spoi_ts_accessor_generator.cpp — TypeScript SPOI 访问器代码生成器

    从二进制元数据生成 TypeScript 语言的类型特化访问器（Accessor），
    替代运行时的反射机制（constructor.name + Object.keys），直接通过字段索引访问/设置值。

    生成内容：
    - TypeId 常量（与 C++ E_type 枚举值一致）
    - SpoiAccessor 接口（fieldCount/getField/setField）
    - 每个类型的 Accessor 类（用 switch 跳转表）
    - SpoiAccessorRegistry 静态 Map（替代运行时 Record<string, string[]>）
    - 通用值反序列化函数（基于 type_id 前缀）
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

// =============================== TypeScript 类型映射 ===============================

static std::string tsTypeName(const std::string& spTypeName) {
    if (spTypeName == "u8")  return "number";
    if (spTypeName == "u16") return "number";
    if (spTypeName == "u32") return "number";
    if (spTypeName == "u64") return "bigint";
    if (spTypeName == "i8")  return "number";
    if (spTypeName == "i16") return "number";
    if (spTypeName == "i32") return "number";
    if (spTypeName == "i64") return "bigint";
    if (spTypeName == "f32") return "number";
    if (spTypeName == "f64") return "number";
    if (spTypeName == "ch")  return "number";
    if (spTypeName == "ch8") return "number";
    if (spTypeName == "ch16") return "number";
    if (spTypeName == "ch32") return "number";
    if (spTypeName == "string") return "string";
    if (spTypeName == "bool") return "boolean";
    return "any";
}

// 返回 setField 的运行时 typeof 类型校验代码
// 空字符串表示不需要校验（容器/指针/optional/自定义类型）
static std::string tsSetFieldTypeCheck(const SpoiFieldInfo& f) {
    // 容器、指针、optional、自定义类型：不做运行时类型校验
    if (f.isContainer || f.isPointer || f.isOptional || f.typeName == "custom") {
        return "";
    }
    // i64/u64 在 JS 运行时对应 bigint（与 deserializeValue 返回类型一致）
    if (f.typeName == "i64" || f.typeName == "u64") {
        return "typeof val !== 'bigint'";
    }
    // 整数、浮点、char 类型对应 number
    if (f.typeName == "i8" || f.typeName == "i16" || f.typeName == "i32" ||
        f.typeName == "u8" || f.typeName == "u16" || f.typeName == "u32" ||
        f.typeName == "f32" || f.typeName == "f64" ||
        f.typeName == "ch" || f.typeName == "ch8" || f.typeName == "ch16" || f.typeName == "ch32") {
        return "typeof val !== 'number'";
    }
    if (f.typeName == "string") return "typeof val !== 'string'";
    if (f.typeName == "bool") return "typeof val !== 'boolean'";
    return "";
}

static std::string tsTypeIdConst(const std::string& spTypeName) {
    if (spTypeName == "u8")  return "TypeId.U8";
    if (spTypeName == "u16") return "TypeId.U16";
    if (spTypeName == "u32") return "TypeId.U32";
    if (spTypeName == "u64") return "TypeId.U64";
    if (spTypeName == "i8")  return "TypeId.I8";
    if (spTypeName == "i16") return "TypeId.I16";
    if (spTypeName == "i32") return "TypeId.I32";
    if (spTypeName == "i64") return "TypeId.I64";
    if (spTypeName == "f32") return "TypeId.F32";
    if (spTypeName == "f64") return "TypeId.F64";
    if (spTypeName == "string") return "TypeId.STRING";
    if (spTypeName == "bool") return "TypeId.BOOL";
    return "TypeId.CUSTOM";
}

// =============================== 生成代码 ===============================

std::string generateTsAccessor(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "// ============================================================\n";
    ss << "// SPOI Accessor — TypeScript 类型特化访问器（自动生成）\n";
    ss << "// 由 sp-gen spoi-ts-accessor 从 C++ 元数据生成\n";
    ss << "// 替代反射机制，直接通过字段索引访问/设置值\n";
    ss << "// ============================================================\n\n";

    // ── type_id 常量 ──
    ss << "// ============================================================\n";
    ss << "// 基本类型 ID（与 C++ E_type 枚举值一致）\n";
    ss << "// ============================================================\n\n";
    ss << "export const TypeId = {\n";
    ss << "    U8:     " << static_cast<uint32_t>(E_type::u8)     << ",\n";
    ss << "    U16:    " << static_cast<uint32_t>(E_type::u16)    << ",\n";
    ss << "    U32:    " << static_cast<uint32_t>(E_type::u32)    << ",\n";
    ss << "    U64:    " << static_cast<uint32_t>(E_type::u64)    << ",\n";
    ss << "    I8:     " << static_cast<uint32_t>(E_type::i8)     << ",\n";
    ss << "    I16:    " << static_cast<uint32_t>(E_type::i16)    << ",\n";
    ss << "    I32:    " << static_cast<uint32_t>(E_type::i32)    << ",\n";
    ss << "    I64:    " << static_cast<uint32_t>(E_type::i64)    << ",\n";
    ss << "    F32:    " << static_cast<uint32_t>(E_type::f32)    << ",\n";
    ss << "    F64:    " << static_cast<uint32_t>(E_type::f64)    << ",\n";
    ss << "    CH:     " << static_cast<uint32_t>(E_type::ch)     << ",\n";
    ss << "    CH8:    " << static_cast<uint32_t>(E_type::ch8)    << ",\n";
    ss << "    CH16:   " << static_cast<uint32_t>(E_type::ch16)   << ",\n";
    ss << "    CH32:   " << static_cast<uint32_t>(E_type::ch32)   << ",\n";
    ss << "    STRING: " << static_cast<uint32_t>(E_type::string) << ",\n";
    ss << "    BOOL:   " << static_cast<uint32_t>(E_type::bl)     << ",\n";
    ss << "    CUSTOM: " << static_cast<uint32_t>(E_type::e_unknowType) << ",\n";
    ss << "} as const;\n\n";

    // ── SpoiAccessor 接口 ──
    ss << "// ============================================================\n";
    ss << "// SpoiAccessor — 类型特化访问器接口\n";
    ss << "// ============================================================\n\n";
    ss << "export interface SpoiAccessor {\n";
    ss << "    fieldCount(): number;\n";
    ss << "    getField(obj: any, idx: number): any;\n";
    ss << "    setField(obj: any, idx: number, val: any): void;\n";
    ss << "}\n\n";

    // ── 通用值反序列化（基于 type_id 前缀） ──
    ss << "// ============================================================\n";
    ss << "// deserializeValue — 通用值反序列化（基于 type_id 前缀）\n";
    ss << "// 格式: [type_id(u32 LE) + value_bytes]\n";
    ss << "// ============================================================\n\n";
    ss << "export function deserializeValue(data: Uint8Array): unknown {\n";
    ss << "    if (!data || data.length < 4) return null;\n";
    ss << "    const view = new DataView(data.buffer, data.byteOffset, data.length);\n";
    ss << "    const typeId = view.getUint32(0, true);\n";
    ss << "    const valueBytes = data.slice(4);\n";
    ss << "    if (typeId === TypeId.U8)   return valueBytes.length < 1 ? 0 : valueBytes[0];\n";
    ss << "    if (typeId === TypeId.U16)  return valueBytes.length < 2 ? 0 : view.getUint16(4, true);\n";
    ss << "    if (typeId === TypeId.U32)  return valueBytes.length < 4 ? 0 : view.getUint32(4, true);\n";
    ss << "    if (typeId === TypeId.U64)  return valueBytes.length < 8 ? 0n : view.getBigUint64(4, true);\n";
    ss << "    if (typeId === TypeId.I8)   return valueBytes.length < 1 ? 0 : view.getInt8(4);\n";
    ss << "    if (typeId === TypeId.I16)  return valueBytes.length < 2 ? 0 : view.getInt16(4, true);\n";
    ss << "    if (typeId === TypeId.I32)  return valueBytes.length < 4 ? 0 : view.getInt32(4, true);\n";
    ss << "    if (typeId === TypeId.I64)  return valueBytes.length < 8 ? 0n : view.getBigInt64(4, true);\n";
    ss << "    if (typeId === TypeId.F32)  return valueBytes.length < 4 ? 0.0 : view.getFloat32(4, true);\n";
    ss << "    if (typeId === TypeId.F64)  return valueBytes.length < 8 ? 0.0 : view.getFloat64(4, true);\n";
    ss << "    if (typeId === TypeId.STRING) return new TextDecoder('utf-8').decode(valueBytes);\n";
    ss << "    if (typeId === TypeId.BOOL) return valueBytes.length < 1 ? false : valueBytes[0] !== 0;\n";
    ss << "    if (typeId === TypeId.CH)   return valueBytes.length < 1 ? 0 : valueBytes[0];\n";
    ss << "    if (typeId === TypeId.CH8)  return valueBytes.length < 1 ? 0 : valueBytes[0];\n";
    ss << "    if (typeId === TypeId.CH16) return valueBytes.length < 2 ? 0 : view.getUint16(4, true);\n";
    ss << "    if (typeId === TypeId.CH32) return valueBytes.length < 4 ? 0 : view.getUint32(4, true);\n";
    ss << "    return valueBytes;\n";
    ss << "}\n\n";

    // ── 每个类型的访问器 ──
    for (auto& t : types) {
        ss << "// ============================================================\n";
        ss << "// " << t.className << "Accessor\n";
        ss << "// ============================================================\n\n";
        ss << "class " << t.className << "Accessor implements SpoiAccessor {\n\n";

        ss << "    fieldCount(): number {\n";
        ss << "        return " << t.fields.size() << ";\n";
        ss << "    }\n\n";

        // getField
        ss << "    getField(obj: any, idx: number): any {\n";
        ss << "        const o = obj as any;\n";
        ss << "        switch (idx) {\n";
        for (auto& f : t.fields) {
            ss << "            case " << f.index << ": return o." << f.name << ";\n";
        }
        ss << "            default: throw new Error(`invalid field index for " << t.className << ": ${idx}`);\n";
        ss << "        }\n";
        ss << "    }\n\n";

        // setField
        ss << "    setField(obj: any, idx: number, val: any): void {\n";
        ss << "        const o = obj as any;\n";
        ss << "        switch (idx) {\n";
        for (auto& f : t.fields) {
            std::string check = tsSetFieldTypeCheck(f);
            if (check.empty()) {
                ss << "            case " << f.index << ": o." << f.name << " = val; break;\n";
            } else {
                ss << "            case " << f.index << ":\n";
                ss << "                if (" << check << ") throw new Error(`type mismatch for " << t.className << "." << f.name << ": expected " << tsTypeName(f.typeName) << ", got ${typeof val}`);\n";
                ss << "                o." << f.name << " = val; break;\n";
            }
        }
        ss << "            default: throw new Error(`invalid field index for " << t.className << ": ${idx}`);\n";
        ss << "        }\n";
        ss << "    }\n";
        ss << "}\n\n";
    }

    // ── 静态类型注册表 ──
    ss << "// ============================================================\n";
    ss << "// SpoiAccessorRegistry — 静态类型注册表\n";
    ss << "// 替代运行时 Record<string, string[]>（反射版本）\n";
    ss << "// ============================================================\n\n";
    ss << "export const SpoiAccessorRegistry: Map<string, SpoiAccessor> = new Map([\n";
    for (auto& t : types) {
        ss << "    [\"" << t.className << "\", new " << t.className << "Accessor()],\n";
    }
    ss << "]);\n";

    return ss.str();
}

// =============================== 入口函数 ===============================

int generate_spoi_ts_accessor(const std::string& output_path, const std::string& meta_path) {
    try {
        auto meta = sp_meta::readMetaFile(meta_path);
        auto types = extractSpoiTypes(meta);
        std::cout << "Extracted " << types.size() << " types for TypeScript accessor generation" << std::endl;

        auto code = generateTsAccessor(types);

        std::ofstream out(output_path);
        if (!out.is_open()) throw std::runtime_error("Cannot open: " + output_path);
        out << code;
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "SPOI TypeScript accessor generator error: " << e.what() << "\n";
        return 1;
    }
}