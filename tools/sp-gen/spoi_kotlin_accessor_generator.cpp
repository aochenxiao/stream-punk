/*
    spoi_kotlin_accessor_generator.cpp — Kotlin SPOI 访问器代码生成器

    从二进制元数据生成 Kotlin 语言的类型特化访问器（Accessor），
    替代运行时的反射机制（java.lang.reflect.Field），直接通过字段索引访问/设置值。

    生成内容：
    - SpoiAccessor 接口（GetField/SetField/FieldCount）
    - 每个类型的 Accessor 类（用 when 跳转表）
    - 静态类型注册表（SpoiAccessorRegistry）
    - 通用值反序列化函数（基于 type_id 前缀）
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

// =============================== Kotlin 类型映射 ===============================

static std::string kotlinTypeName(const std::string& spTypeName) {
    if (spTypeName == "u8")  return "Int";
    if (spTypeName == "u16") return "Int";
    if (spTypeName == "u32") return "Long";
    if (spTypeName == "u64") return "Long";
    if (spTypeName == "i8")  return "Byte";
    if (spTypeName == "i16") return "Short";
    if (spTypeName == "i32") return "Int";
    if (spTypeName == "i64") return "Long";
    if (spTypeName == "f32") return "Float";
    if (spTypeName == "f64") return "Double";
    if (spTypeName == "ch")  return "Int";
    if (spTypeName == "ch8") return "Int";
    if (spTypeName == "ch16") return "Int";
    if (spTypeName == "ch32") return "Long";
    if (spTypeName == "string") return "String";
    if (spTypeName == "bool") return "Boolean";
    return "Any";
}

static std::string kotlinTypeIdConst(const std::string& spTypeName) {
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
    if (spTypeName == "string") return "TypeIdString";
    if (spTypeName == "bool") return "TypeIdBool";
    return "TypeIdCustom";
}

// cast 表达式：用于 SetField 中的类型转换（Kotlin 语法）
// 注意：Kotlin 中 val 是保留关键字，必须使用 value 作为参数名
static std::string kotlinCastExpr(const std::string& spTypeName) {
    if (spTypeName == "u8")  return "(value as? Number)?.toInt()?.and(0xFF) ?: 0";
    if (spTypeName == "u16") return "(value as? Number)?.toInt()?.and(0xFFFF) ?: 0";
    if (spTypeName == "u32") return "(value as? Number)?.toLong()?.and(0xFFFFFFFFL) ?: 0L";
    if (spTypeName == "u64") return "(value as? Number)?.toLong() ?: 0L";
    if (spTypeName == "i8")  return "(value as? Number)?.toByte() ?: 0";
    if (spTypeName == "i16") return "(value as? Number)?.toShort() ?: 0";
    if (spTypeName == "i32") return "(value as? Number)?.toInt() ?: 0";
    if (spTypeName == "i64") return "(value as? Number)?.toLong() ?: 0L";
    if (spTypeName == "f32") return "(value as? Number)?.toFloat() ?: 0.0f";
    if (spTypeName == "f64") return "(value as? Number)?.toDouble() ?: 0.0";
    if (spTypeName == "ch")  return "(value as? Number)?.toInt()?.and(0xFF) ?: 0";
    if (spTypeName == "ch8") return "(value as? Number)?.toInt()?.and(0xFF) ?: 0";
    if (spTypeName == "ch16")return "(value as? Number)?.toInt()?.and(0xFFFF) ?: 0";
    if (spTypeName == "ch32")return "(value as? Number)?.toLong()?.and(0xFFFFFFFFL) ?: 0L";
    if (spTypeName == "string") return "value?.toString() ?: \"\"";
    if (spTypeName == "bool") return "value as? Boolean ?: false";
    return "value";
}

// =============================== 生成代码 ===============================

std::string generateKotlinAccessor(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "// ============================================================\n";
    ss << "// SPOI Accessor — Kotlin 类型特化访问器（自动生成）\n";
    ss << "// 由 sp-gen spoi-kotlin-accessor 从 C++ 元数据生成\n";
    ss << "// 替代反射机制，直接通过字段索引访问/设置值\n";
    ss << "// ============================================================\n\n";

    ss << "import java.nio.ByteBuffer\n";
    ss << "import java.nio.ByteOrder\n";
    ss << "import java.nio.charset.StandardCharsets\n\n";

    // ── type_id 常量 ──
    ss << "// ============================================================\n";
    ss << "// 基本类型 ID（与 C++ E_type 枚举值一致）\n";
    ss << "// ============================================================\n\n";
    ss << "object TypeId {\n";
    ss << "    const val U8: Long     = " << static_cast<uint32_t>(E_type::u8)     << "L\n";
    ss << "    const val U16: Long    = " << static_cast<uint32_t>(E_type::u16)    << "L\n";
    ss << "    const val U32: Long    = " << static_cast<uint32_t>(E_type::u32)    << "L\n";
    ss << "    const val U64: Long    = " << static_cast<uint32_t>(E_type::u64)    << "L\n";
    ss << "    const val I8: Long     = " << static_cast<uint32_t>(E_type::i8)     << "L\n";
    ss << "    const val I16: Long    = " << static_cast<uint32_t>(E_type::i16)    << "L\n";
    ss << "    const val I32: Long    = " << static_cast<uint32_t>(E_type::i32)    << "L\n";
    ss << "    const val I64: Long    = " << static_cast<uint32_t>(E_type::i64)    << "L\n";
    ss << "    const val F32: Long    = " << static_cast<uint32_t>(E_type::f32)    << "L\n";
    ss << "    const val F64: Long    = " << static_cast<uint32_t>(E_type::f64)    << "L\n";
    ss << "    const val STRING: Long = " << static_cast<uint32_t>(E_type::string) << "L\n";
    ss << "    const val BOOL: Long   = " << static_cast<uint32_t>(E_type::bl)     << "L\n";
    ss << "}\n\n";

    // ── SpoiAccessor 接口 ──
    ss << "// ============================================================\n";
    ss << "// SpoiAccessor — 类型特化访问器接口\n";
    ss << "// ============================================================\n\n";
    ss << "interface SpoiAccessor {\n";
    ss << "    fun fieldCount(): Int\n";
    ss << "    fun getField(obj: Any, idx: Int): Any?\n";
    ss << "    fun setField(obj: Any, idx: Int, value: Any?)\n";
    ss << "}\n\n";

    // ── 通用值反序列化（基于 type_id 前缀） ──
    ss << "// ============================================================\n";
    ss << "// DeserializeValue — 通用值反序列化（基于 type_id 前缀）\n";
    ss << "// 格式: [type_id(u32 LE) + value_bytes]\n";
    ss << "// ============================================================\n\n";
    ss << "object SpoiDeserializer {\n";
    ss << "    fun deserializeValue(data: ByteArray): Any? {\n";
    ss << "        if (data.size < 4) return null\n";
    ss << "        val typeId = ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).int.toLong() and 0xFFFFFFFFL\n";
    ss << "        val valueBytes = data.copyOfRange(4, data.size)\n";
    ss << "        return when (typeId) {\n";
    ss << "            TypeId.U8     -> if (valueBytes.isNotEmpty()) valueBytes[0].toInt() and 0xFF else 0\n";
    ss << "            TypeId.U16    -> if (valueBytes.size >= 2) (ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).short.toInt() and 0xFFFF) else 0\n";
    ss << "            TypeId.U32    -> if (valueBytes.size >= 4) (ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).int.toLong() and 0xFFFFFFFFL) else 0L\n";
    ss << "            TypeId.U64    -> if (valueBytes.size >= 8) ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).long else 0L\n";
    ss << "            TypeId.I8     -> if (valueBytes.isNotEmpty()) valueBytes[0] else 0.toByte()\n";
    ss << "            TypeId.I16    -> if (valueBytes.size >= 2) ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).short else 0.toShort()\n";
    ss << "            TypeId.I32    -> if (valueBytes.size >= 4) ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).int else 0\n";
    ss << "            TypeId.I64    -> if (valueBytes.size >= 8) ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).long else 0L\n";
    ss << "            TypeId.F32    -> if (valueBytes.size >= 4) ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).float else 0.0f\n";
    ss << "            TypeId.F64    -> if (valueBytes.size >= 8) ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).double else 0.0\n";
    ss << "            TypeId.STRING -> String(valueBytes, StandardCharsets.UTF_8)\n";
    ss << "            TypeId.BOOL   -> if (valueBytes.isNotEmpty()) valueBytes[0].toInt() != 0 else false\n";
    ss << "            else          -> valueBytes\n";
    ss << "        }\n";
    ss << "    }\n";
    ss << "}\n\n";

    // ── 每个类型的访问器 ──
    for (auto& t : types) {
        ss << "// ============================================================\n";
        ss << "// " << t.className << "Accessor\n";
        ss << "// ============================================================\n\n";
        ss << "class " << t.className << "Accessor : SpoiAccessor {\n\n";

        ss << "    override fun fieldCount(): Int = " << t.fields.size() << "\n\n";

        // GetField
        ss << "    override fun getField(obj: Any, idx: Int): Any? {\n";
        ss << "        val o = obj as " << t.className << "\n";
        ss << "        return when (idx) {\n";
        for (auto& f : t.fields) {
            std::string kt = kotlinTypeName(f.typeName);
            ss << "            " << f.index << " -> o." << f.name;
            // 基本类型在 Kotlin 中已经是对象类型，不需要装箱
            ss << "\n";
        }
        ss << "            else -> throw IllegalArgumentException(\"invalid field index for " << t.className << ": \$idx\")\n";
        ss << "        }\n";
        ss << "    }\n\n";

        // SetField
        ss << "    override fun setField(obj: Any, idx: Int, value: Any?) {\n";
        ss << "        val o = obj as " << t.className << "\n";
        ss << "        when (idx) {\n";
        for (auto& f : t.fields) {
            // 容器/指针/optional/自定义类型：直接赋值，不做类型转换
            if (f.isContainer || f.isPointer || f.isOptional || f.typeName == "custom") {
                ss << "            " << f.index << " -> o." << f.name << " = value\n";
            } else {
                ss << "            " << f.index << " -> o." << f.name << " = " << kotlinCastExpr(f.typeName) << "\n";
            }
        }
        ss << "            else -> throw IllegalArgumentException(\"invalid field index for " << t.className << ": \$idx\")\n";
        ss << "        }\n";
        ss << "    }\n";
        ss << "}\n\n";
    }

    // ── 静态类型注册表 ──
    ss << "// ============================================================\n";
    ss << "// SpoiAccessorRegistry — 静态类型注册表\n";
    ss << "// 替代运行时 Map<String, List<String>>（反射版本）\n";
    ss << "// ============================================================\n\n";
    ss << "object SpoiAccessorRegistry {\n";
    ss << "    val registry: Map<String, SpoiAccessor> = mapOf(\n";
    for (size_t i = 0; i < types.size(); ++i) {
        auto& t = types[i];
        ss << "        \"" << t.className << "\" to " << t.className << "Accessor()";
        if (i != types.size() - 1) ss << ",\n";
        else ss << "\n";
    }
    ss << "    )\n";
    ss << "    fun get(typeName: String): SpoiAccessor? = registry[typeName]\n";
    ss << "}\n";

    return ss.str();
}

// =============================== 入口函数 ===============================

int generate_spoi_kotlin_accessor(const std::string& output_path, const std::string& meta_path) {
    try {
        auto meta = sp_meta::readMetaFile(meta_path);
        auto types = extractSpoiTypes(meta);
        std::cout << "Extracted " << types.size() << " types for Kotlin accessor generation" << std::endl;

        auto code = generateKotlinAccessor(types);

        std::ofstream out(output_path);
        if (!out.is_open()) throw std::runtime_error("Cannot open: " + output_path);
        out << code;
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "SPOI Kotlin accessor generator error: " << e.what() << "\n";
        return 1;
    }
}