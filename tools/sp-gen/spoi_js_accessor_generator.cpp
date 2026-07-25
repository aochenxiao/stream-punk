/*
    spoi_js_accessor_generator.cpp — JavaScript SPOI 访问器代码生成器

    从二进制元数据生成 JavaScript 语言的类型特化访问器（Accessor），
    替代运行时的反射机制（constructor.name 查找属性名），直接通过字段索引访问/设置值。

    生成内容（CommonJS 格式，同时兼容 ES Module）：
    - TypeId 常量对象（与 C++ E_type 枚举值一致）
    - SpoiAccessor 基类（fieldCount(), getField(), setField()）
    - deserializeValue 函数（基于 type_id 前缀格式：[type_id(u32 LE) + value_bytes]）
    - 每个类型的 Accessor 类（用 switch 跳转表）
    - SpoiAccessorRegistry 静态 Map（替代运行时 Record<string, string[]>）
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

// =============================== JavaScript 类型映射（JSDoc 注释） ===============================

static std::string jsTypeComment(const std::string& spTypeName) {
    if (spTypeName == "u8")  return "{number}";
    if (spTypeName == "u16") return "{number}";
    if (spTypeName == "u32") return "{number}";
    if (spTypeName == "u64") return "{bigint}";
    if (spTypeName == "i8")  return "{number}";
    if (spTypeName == "i16") return "{number}";
    if (spTypeName == "i32") return "{number}";
    if (spTypeName == "i64") return "{bigint}";
    if (spTypeName == "f32") return "{number}";
    if (spTypeName == "f64") return "{number}";
    if (spTypeName == "ch")  return "{number}";
    if (spTypeName == "ch8") return "{number}";
    if (spTypeName == "ch16") return "{number}";
    if (spTypeName == "ch32") return "{number}";
    if (spTypeName == "string") return "{string}";
    if (spTypeName == "bool") return "{boolean}";
    return "{*}";
}

// =============================== JS setField 类型转换表达式 ===============================
// 根据字段的基本类型生成 JS 类型转换表达式，确保 setField 写入的值类型正确
// 基本类型需要强制转换，容器/指针/optional/自定义类型直接赋值
static std::string jsSetFieldConversion(const std::string& typeName) {
    if (typeName == "u8" || typeName == "ch" || typeName == "ch8")  return "val & 0xFF";
    if (typeName == "u16" || typeName == "ch16")                    return "val & 0xFFFF";
    if (typeName == "u32" || typeName == "ch32")                    return "val >>> 0";
    if (typeName == "u64")                                          return "BigInt(val)";
    if (typeName == "i8")                                           return "(val << 24) >> 24";
    if (typeName == "i16")                                          return "(val << 16) >> 16";
    if (typeName == "i32")                                          return "val | 0";
    if (typeName == "i64")                                          return "BigInt(val)";
    if (typeName == "f32")                                          return "Math.fround(val)";
    if (typeName == "f64")                                          return "Number(val)";
    if (typeName == "bool")                                         return "!!val";
    if (typeName == "string")                                       return "String(val)";
    return "val"; // 容器、指针、optional、自定义类型：直接赋值
}

// =============================== 生成代码 ===============================

std::string generateJsAccessor(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "// ============================================================\n";
    ss << "// SPOI Accessor — JavaScript 类型特化访问器（自动生成）\n";
    ss << "// 由 sp-gen spoi-js-accessor 从 C++ 元数据生成\n";
    ss << "// 替代反射机制，直接通过字段索引访问/设置值\n";
    ss << "// ============================================================\n\n";

    ss << "'use strict';\n\n";

    // ── type_id 常量 ──
    ss << "// ============================================================\n";
    ss << "// 基本类型 ID（与 C++ E_type 枚举值一致）\n";
    ss << "// ============================================================\n\n";
    ss << "const TypeId = {\n";
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
    ss << "};\n\n";

    // ── SpoiAccessor 基类 ──
    ss << "// ============================================================\n";
    ss << "// SpoiAccessor — 类型特化访问器基类\n";
    ss << "// ============================================================\n\n";
    ss << "class SpoiAccessor {\n";
    ss << "    /** @returns {number} */\n";
    ss << "    fieldCount() { return 0; }\n\n";
    ss << "    /**\n";
    ss << "     * @param {*} obj\n";
    ss << "     * @param {number} idx\n";
    ss << "     * @returns {*}\n";
    ss << "     */\n";
    ss << "    getField(obj, idx) { return undefined; }\n\n";
    ss << "    /**\n";
    ss << "     * @param {*} obj\n";
    ss << "     * @param {number} idx\n";
    ss << "     * @param {*} val\n";
    ss << "     */\n";
    ss << "    setField(obj, idx, val) {}\n";
    ss << "}\n\n";

    // ── 通用值反序列化（基于 type_id 前缀） ──
    ss << "// ============================================================\n";
    ss << "// DeserializeValue — 通用值反序列化（基于 type_id 前缀）\n";
    ss << "// 格式: [type_id(u32 LE) + value_bytes]\n";
    ss << "// ============================================================\n\n";
    ss << "/**\n";
    ss << " * @param {Uint8Array} data\n";
    ss << " * @returns {*}\n";
    ss << " */\n";
    ss << "function deserializeValue(data) {\n";
    ss << "    if (!data || data.length < 4) return null;\n";
    ss << "    const view = new DataView(data.buffer, data.byteOffset, data.length);\n";
    ss << "    const typeId = view.getUint32(0, true);\n";
    ss << "    const valueBytes = data.length > 4 ? data.slice(4) : new Uint8Array(0);\n";
    ss << "    if (typeId === TypeId.U8)   return valueBytes.length > 0 ? valueBytes[0] : 0;\n";
    ss << "    if (typeId === TypeId.U16)  return valueBytes.length >= 2 ? view.getUint16(4, true) : 0;\n";
    ss << "    if (typeId === TypeId.U32)  return valueBytes.length >= 4 ? view.getUint32(4, true) : 0;\n";
    ss << "    if (typeId === TypeId.U64)  return valueBytes.length >= 8 ? view.getBigUint64(4, true) : 0n;\n";
    ss << "    if (typeId === TypeId.I8)   return valueBytes.length > 0 ? view.getInt8(4) : 0;\n";
    ss << "    if (typeId === TypeId.I16)  return valueBytes.length >= 2 ? view.getInt16(4, true) : 0;\n";
    ss << "    if (typeId === TypeId.I32)  return valueBytes.length >= 4 ? view.getInt32(4, true) : 0;\n";
    ss << "    if (typeId === TypeId.I64)  return valueBytes.length >= 8 ? view.getBigInt64(4, true) : 0n;\n";
    ss << "    if (typeId === TypeId.F32)  return valueBytes.length >= 4 ? view.getFloat32(4, true) : 0.0;\n";
    ss << "    if (typeId === TypeId.F64)  return valueBytes.length >= 8 ? view.getFloat64(4, true) : 0.0;\n";
    ss << "    if (typeId === TypeId.STRING) return new TextDecoder('utf-8').decode(valueBytes);\n";
    ss << "    if (typeId === TypeId.BOOL) return valueBytes.length > 0 ? valueBytes[0] !== 0 : false;\n";
    ss << "    if (typeId === TypeId.CH)   return valueBytes.length > 0 ? valueBytes[0] : 0;\n";
    ss << "    if (typeId === TypeId.CH8)  return valueBytes.length > 0 ? valueBytes[0] : 0;\n";
    ss << "    if (typeId === TypeId.CH16) return valueBytes.length >= 2 ? view.getUint16(4, true) : 0;\n";
    ss << "    if (typeId === TypeId.CH32) return valueBytes.length >= 4 ? view.getUint32(4, true) : 0;\n";
    ss << "    return valueBytes;\n";
    ss << "}\n\n";

    // ── 每个类型的访问器 ──
    for (auto& t : types) {
        ss << "// ============================================================\n";
        ss << "// " << t.className << "Accessor\n";
        ss << "// ============================================================\n\n";
        ss << "class " << t.className << "Accessor extends SpoiAccessor {\n";
        ss << "    fieldCount() { return " << t.fields.size() << "; }\n\n";

        // getField
        ss << "    getField(obj, idx) {\n";
        ss << "        switch (idx) {\n";
        for (auto& f : t.fields) {
            ss << "            case " << f.index << ": return obj." << f.name << ";\n";
        }
        ss << "            default: throw new Error('invalid field index for " << t.className << ": ' + idx);\n";
        ss << "        }\n";
        ss << "    }\n\n";

        // setField（带类型转换）
        ss << "    setField(obj, idx, val) {\n";
        ss << "        switch (idx) {\n";
        for (auto& f : t.fields) {
            // 容器、指针、optional、自定义类型：直接赋值，不做基本类型转换
            auto conv = (f.isContainer || f.isPointer || f.isOptional || f.typeName == "custom")
                ? "val"
                : jsSetFieldConversion(f.typeName);
            ss << "            case " << f.index << ": obj." << f.name << " = " << conv << "; break;\n";
        }
        ss << "            default: throw new Error('invalid field index for " << t.className << ": ' + idx);\n";
        ss << "        }\n";
        ss << "    }\n";
        ss << "}\n\n";
    }

    // ── 静态类型注册表 ──
    ss << "// ============================================================\n";
    ss << "// SpoiAccessorRegistry — 静态类型注册表\n";
    ss << "// 替代运行时 Record<string, string[]>（反射版本）\n";
    ss << "// ============================================================\n\n";
    ss << "const SpoiAccessorRegistry = new Map([\n";
    for (auto& t : types) {
        ss << "    ['" << t.className << "', new " << t.className << "Accessor()],\n";
    }
    ss << "]);\n\n";

    // ── 导出 ──
    ss << "// ============================================================\n";
    ss << "// 导出（同时支持 CommonJS 和 ES Module）\n";
    ss << "// ============================================================\n\n";
    ss << "if (typeof module !== 'undefined' && module.exports) {\n";
    ss << "    module.exports = { TypeId, SpoiAccessor, deserializeValue, SpoiAccessorRegistry };\n";
    for (auto& t : types) {
        ss << "    module.exports." << t.className << "Accessor = " << t.className << "Accessor;\n";
    }
    ss << "}\n";

    return ss.str();
}

// =============================== 入口函数 ===============================

int generate_spoi_js_accessor(const std::string& output_path, const std::string& meta_path) {
    try {
        auto meta = sp_meta::readMetaFile(meta_path);
        auto types = extractSpoiTypes(meta);
        std::cout << "Extracted " << types.size() << " types for JS accessor generation" << std::endl;

        auto code = generateJsAccessor(types);

        std::ofstream out(output_path);
        if (!out.is_open()) throw std::runtime_error("Cannot open: " + output_path);
        out << code;
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "SPOI JS accessor generator error: " << e.what() << "\n";
        return 1;
    }
}