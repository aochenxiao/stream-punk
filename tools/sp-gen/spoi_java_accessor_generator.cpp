/*
    spoi_java_accessor_generator.cpp — Java SPOI 访问器代码生成器

    从二进制元数据生成 Java 语言的类型特化访问器（Accessor），
    替代运行时的反射机制（java.lang.reflect.Field），直接通过字段索引访问/设置值。

    生成内容：
    - SpoiAccessor 接口（GetField/SetField/FieldCount）
    - 每个类型的 Accessor 类（用 switch 跳转表）
    - 静态类型注册表（SpoiAccessorRegistry）
    - 通用值反序列化函数（基于 type_id 前缀）
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

// =============================== Java 类型映射 ===============================

static std::string javaTypeName(const std::string& spTypeName) {
    if (spTypeName == "u8")  return "int";
    if (spTypeName == "u16") return "int";
    if (spTypeName == "u32") return "long";
    if (spTypeName == "u64") return "long";
    if (spTypeName == "i8")  return "byte";
    if (spTypeName == "i16") return "short";
    if (spTypeName == "i32") return "int";
    if (spTypeName == "i64") return "long";
    if (spTypeName == "f32") return "float";
    if (spTypeName == "f64") return "double";
    if (spTypeName == "string") return "String";
    if (spTypeName == "bool") return "boolean";
    return "Object";
}

static std::string javaTypeIdConst(const std::string& spTypeName) {
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

// cast 表达式：用于 SetField 中的类型转换
static std::string javaCastExpr(const std::string& spTypeName) {
    if (spTypeName == "u8")  return "(int)(val instanceof Number ? ((Number)val).intValue() & 0xFF : 0)";
    if (spTypeName == "u16") return "(int)(val instanceof Number ? ((Number)val).intValue() & 0xFFFF : 0)";
    if (spTypeName == "u32") return "val instanceof Number ? ((Number)val).longValue() & 0xFFFFFFFFL : 0L";
    if (spTypeName == "u64") return "val instanceof Number ? ((Number)val).longValue() : 0L";
    if (spTypeName == "i8")  return "val instanceof Number ? ((Number)val).byteValue() : 0";
    if (spTypeName == "i16") return "val instanceof Number ? ((Number)val).shortValue() : 0";
    if (spTypeName == "i32") return "val instanceof Number ? ((Number)val).intValue() : 0";
    if (spTypeName == "i64") return "val instanceof Number ? ((Number)val).longValue() : 0L";
    if (spTypeName == "f32") return "val instanceof Number ? ((Number)val).floatValue() : 0.0f";
    if (spTypeName == "f64") return "val instanceof Number ? ((Number)val).doubleValue() : 0.0";
    if (spTypeName == "string") return "val != null ? val.toString() : \"\"";
    if (spTypeName == "bool") return "val instanceof Boolean ? ((Boolean)val).booleanValue() : false";
    return "val";
}

// =============================== 生成代码 ===============================

std::string generateJavaAccessor(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "// ============================================================\n";
    ss << "// SPOI Accessor — Java 类型特化访问器（自动生成）\n";
    ss << "// 由 sp-gen spoi-java-accessor 从 C++ 元数据生成\n";
    ss << "// 替代反射机制，直接通过字段索引访问/设置值\n";
    ss << "// ============================================================\n\n";

    ss << "import java.nio.ByteBuffer;\n";
    ss << "import java.nio.ByteOrder;\n";
    ss << "import java.nio.charset.StandardCharsets;\n";
    ss << "import java.util.*;\n\n";

    // ── type_id 常量 ──
    ss << "// ============================================================\n";
    ss << "// 基本类型 ID（与 C++ E_type 枚举值一致）\n";
    ss << "// ============================================================\n\n";
    ss << "class TypeId {\n";
    ss << "    public static final long U8     = " << static_cast<uint32_t>(E_type::u8)     << "L;\n";
    ss << "    public static final long U16    = " << static_cast<uint32_t>(E_type::u16)    << "L;\n";
    ss << "    public static final long U32    = " << static_cast<uint32_t>(E_type::u32)    << "L;\n";
    ss << "    public static final long U64    = " << static_cast<uint32_t>(E_type::u64)    << "L;\n";
    ss << "    public static final long I8     = " << static_cast<uint32_t>(E_type::i8)     << "L;\n";
    ss << "    public static final long I16    = " << static_cast<uint32_t>(E_type::i16)    << "L;\n";
    ss << "    public static final long I32    = " << static_cast<uint32_t>(E_type::i32)    << "L;\n";
    ss << "    public static final long I64    = " << static_cast<uint32_t>(E_type::i64)    << "L;\n";
    ss << "    public static final long F32    = " << static_cast<uint32_t>(E_type::f32)    << "L;\n";
    ss << "    public static final long F64    = " << static_cast<uint32_t>(E_type::f64)    << "L;\n";
    ss << "    public static final long STRING = " << static_cast<uint32_t>(E_type::string) << "L;\n";
    ss << "    public static final long BOOL   = " << static_cast<uint32_t>(E_type::bl)     << "L;\n";
    ss << "}\n\n";

    // ── SpoiAccessor 接口 ──
    ss << "// ============================================================\n";
    ss << "// SpoiAccessor — 类型特化访问器接口\n";
    ss << "// ============================================================\n\n";
    ss << "interface SpoiAccessor {\n";
    ss << "    int fieldCount();\n";
    ss << "    Object getField(Object obj, int idx);\n";
    ss << "    void setField(Object obj, int idx, Object val);\n";
    ss << "}\n\n";

    // ── 通用值反序列化（基于 type_id 前缀） ──
    ss << "// ============================================================\n";
    ss << "// DeserializeValue — 通用值反序列化（基于 type_id 前缀）\n";
    ss << "// 格式: [type_id(u32 LE) + value_bytes]\n";
    ss << "// ============================================================\n\n";
    ss << "class SpoiDeserializer {\n";
    ss << "    static Object deserializeValue(byte[] data) {\n";
    ss << "        if (data == null || data.length < 4) return null;\n";
    ss << "        long typeId = Integer.toUnsignedLong(\n";
    ss << "            ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).getInt());\n";
    ss << "        byte[] valueBytes = Arrays.copyOfRange(data, 4, data.length);\n";
    ss << "        if (typeId == TypeId.U8)   return valueBytes.length > 0 ? (valueBytes[0] & 0xFF) : 0;\n";
    ss << "        if (typeId == TypeId.U16)  return valueBytes.length >= 2 ? (int)(ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getShort() & 0xFFFF) : 0;\n";
    ss << "        if (typeId == TypeId.U32)  return valueBytes.length >= 4 ? Integer.toUnsignedLong(ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getInt()) : 0L;\n";
    ss << "        if (typeId == TypeId.U64)  return valueBytes.length >= 8 ? ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getLong() : 0L;\n";
    ss << "        if (typeId == TypeId.I8)   return valueBytes.length > 0 ? valueBytes[0] : (byte)0;\n";
    ss << "        if (typeId == TypeId.I16)  return valueBytes.length >= 2 ? ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getShort() : (short)0;\n";
    ss << "        if (typeId == TypeId.I32)  return valueBytes.length >= 4 ? ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getInt() : 0;\n";
    ss << "        if (typeId == TypeId.I64)  return valueBytes.length >= 8 ? ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getLong() : 0L;\n";
    ss << "        if (typeId == TypeId.F32)  return valueBytes.length >= 4 ? ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getFloat() : 0.0f;\n";
    ss << "        if (typeId == TypeId.F64)  return valueBytes.length >= 8 ? ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getDouble() : 0.0;\n";
    ss << "        if (typeId == TypeId.STRING) return new String(valueBytes, StandardCharsets.UTF_8);\n";
    ss << "        if (typeId == TypeId.BOOL) return valueBytes.length > 0 ? (valueBytes[0] != 0) : false;\n";
    ss << "        return valueBytes;\n";
    ss << "    }\n";
    ss << "}\n\n";

    // ── 每个类型的访问器 ──
    for (auto& t : types) {
        ss << "// ============================================================\n";
        ss << "// " << t.className << "Accessor\n";
        ss << "// ============================================================\n\n";
        ss << "class " << t.className << "Accessor implements SpoiAccessor {\n\n";

        ss << "    public int fieldCount() { return " << t.fields.size() << "; }\n\n";

        // GetField
        ss << "    public Object getField(Object obj, int idx) {\n";
        ss << "        " << t.className << " o = (" << t.className << ") obj;\n";
        ss << "        switch (idx) {\n";
        for (auto& f : t.fields) {
            ss << "            case " << f.index << ": return ";
            // 容器/指针/optional/自定义类型：直接返回引用，不装箱
            if (f.isContainer || f.isPointer || f.isOptional) {
                ss << "o." << f.name;
            } else {
                std::string jt = javaTypeName(f.typeName);
                if (jt == "boolean") {
                    ss << "Boolean.valueOf(o." << f.name << ")";
                } else if (jt == "int" || jt == "long" || jt == "byte" || jt == "short" || jt == "float" || jt == "double") {
                    // 基本类型装箱
                    if (jt == "int") ss << "Integer.valueOf(o." << f.name << ")";
                    else if (jt == "long") ss << "Long.valueOf(o." << f.name << ")";
                    else if (jt == "byte") ss << "Byte.valueOf(o." << f.name << ")";
                    else if (jt == "short") ss << "Short.valueOf(o." << f.name << ")";
                    else if (jt == "float") ss << "Float.valueOf(o." << f.name << ")";
                    else if (jt == "double") ss << "Double.valueOf(o." << f.name << ")";
                } else {
                    ss << "o." << f.name;
                }
            }
            ss << ";\n";
        }
        ss << "            default: throw new IllegalArgumentException(\"invalid field index for " << t.className << ": \" + idx);\n";
        ss << "        }\n";
        ss << "    }\n\n";

        // SetField
        ss << "    public void setField(Object obj, int idx, Object val) {\n";
        ss << "        " << t.className << " o = (" << t.className << ") obj;\n";
        ss << "        switch (idx) {\n";
        for (auto& f : t.fields) {
            ss << "            case " << f.index << ": o." << f.name << " = ";
            // 容器/指针/optional/自定义类型：直接赋值
            if (f.isContainer || f.isPointer || f.isOptional) {
                ss << "val";
            } else {
                ss << javaCastExpr(f.typeName);
            }
            ss << "; break;\n";
        }
        ss << "            default: throw new IllegalArgumentException(\"invalid field index for " << t.className << ": \" + idx);\n";
        ss << "        }\n";
        ss << "    }\n";
        ss << "}\n\n";
    }

    // ── 静态类型注册表 ──
    ss << "// ============================================================\n";
    ss << "// SpoiAccessorRegistry — 静态类型注册表\n";
    ss << "// 替代运行时 Map<String, List<String>>（反射版本）\n";
    ss << "// ============================================================\n\n";
    ss << "class SpoiAccessorRegistry {\n";
    ss << "    static final Map<String, SpoiAccessor> registry = new HashMap<>();\n";
    ss << "    static {\n";
    for (auto& t : types) {
        ss << "        registry.put(\"" << t.className << "\", new " << t.className << "Accessor());\n";
    }
    ss << "    }\n";
    ss << "    static SpoiAccessor get(String typeName) { return registry.get(typeName); }\n";
    ss << "}\n";

    return ss.str();
}

// =============================== 入口函数 ===============================

int generate_spoi_java_accessor(const std::string& output_path, const std::string& meta_path) {
    try {
        auto meta = sp_meta::readMetaFile(meta_path);
        auto types = extractSpoiTypes(meta);
        std::cout << "Extracted " << types.size() << " types for Java accessor generation" << std::endl;

        auto code = generateJavaAccessor(types);

        std::ofstream out(output_path);
        if (!out.is_open()) throw std::runtime_error("Cannot open: " + output_path);
        out << code;
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "SPOI Java accessor generator error: " << e.what() << "\n";
        return 1;
    }
}