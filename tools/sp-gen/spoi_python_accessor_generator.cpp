/*
    spoi_python_accessor_generator.cpp — Python SPOI 访问器代码生成器

    从二进制元数据生成 Python 语言的类型特化访问器（Accessor），
    替代运行时的反射机制（getattr/setattr），直接通过字段索引访问/设置值。

    生成内容：
    - TypeId 常量类（与 C++ E_type 枚举值一致）
    - SpoiAccessor 基类（抽象方法：field_count(), get_field(), set_field()）
    - 每个类型的 Accessor 类（用 if/elif 链跳转表）
    - 通用值反序列化函数（基于 type_id 前缀）
    - SpoiAccessorRegistry 字典（静态 dict，替代运行时反射）
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

// =============================== Python 类型映射 ===============================

static std::string pythonTypeHint(const std::string& spTypeName) {
    if (spTypeName == "u8")  return "int";
    if (spTypeName == "u16") return "int";
    if (spTypeName == "u32") return "int";
    if (spTypeName == "u64") return "int";
    if (spTypeName == "i8")  return "int";
    if (spTypeName == "i16") return "int";
    if (spTypeName == "i32") return "int";
    if (spTypeName == "i64") return "int";
    if (spTypeName == "f32") return "float";
    if (spTypeName == "f64") return "float";
    if (spTypeName == "string") return "str";
    if (spTypeName == "bool") return "bool";
    return "Any";
}

static std::string pythonTypeIdConst(const std::string& spTypeName) {
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

std::string generatePythonAccessor(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "# ============================================================\n";
    ss << "# SPOI Accessor — Python 类型特化访问器（自动生成）\n";
    ss << "# 由 sp-gen spoi-python-accessor 从 C++ 元数据生成\n";
    ss << "# 替代反射机制（getattr/setattr），直接通过字段索引访问/设置值\n";
    ss << "# ============================================================\n\n";

    ss << "import struct\n";
    ss << "from typing import Any, Dict\n\n";

    // ── type_id 常量 ──
    ss << "# ============================================================\n";
    ss << "# 基本类型 ID（与 C++ E_type 枚举值一致）\n";
    ss << "# ============================================================\n\n";
    ss << "class TypeId:\n";
    ss << "    U8: int     = " << static_cast<uint32_t>(E_type::u8)     << "\n";
    ss << "    U16: int    = " << static_cast<uint32_t>(E_type::u16)    << "\n";
    ss << "    U32: int    = " << static_cast<uint32_t>(E_type::u32)    << "\n";
    ss << "    U64: int    = " << static_cast<uint32_t>(E_type::u64)    << "\n";
    ss << "    I8: int     = " << static_cast<uint32_t>(E_type::i8)     << "\n";
    ss << "    I16: int    = " << static_cast<uint32_t>(E_type::i16)    << "\n";
    ss << "    I32: int    = " << static_cast<uint32_t>(E_type::i32)    << "\n";
    ss << "    I64: int    = " << static_cast<uint32_t>(E_type::i64)    << "\n";
    ss << "    F32: int    = " << static_cast<uint32_t>(E_type::f32)    << "\n";
    ss << "    F64: int    = " << static_cast<uint32_t>(E_type::f64)    << "\n";
    ss << "    STRING: int = " << static_cast<uint32_t>(E_type::string) << "\n";
    ss << "    BOOL: int   = " << static_cast<uint32_t>(E_type::bl)     << "\n";
    ss << "    CUSTOM: int = 0\n\n";

    // ── SpoiAccessor 基类 ──
    ss << "# ============================================================\n";
    ss << "# SpoiAccessor — 类型特化访问器基类\n";
    ss << "# ============================================================\n\n";
    ss << "class SpoiAccessor:\n";
    ss << "    \"\"\"类型特化访问器基类，子类通过 if/elif 跳转表实现 O(1) 字段访问\"\"\"\n\n";
    ss << "    def field_count(self) -> int:\n";
    ss << "        \"\"\"返回字段数量\"\"\"\n";
    ss << "        raise NotImplementedError\n\n";
    ss << "    def get_field(self, obj: Any, idx: int) -> Any:\n";
    ss << "        \"\"\"通过索引读取字段值\"\"\"\n";
    ss << "        raise NotImplementedError\n\n";
    ss << "    def set_field(self, obj: Any, idx: int, val: Any) -> None:\n";
    ss << "        \"\"\"通过索引设置字段值\"\"\"\n";
    ss << "        raise NotImplementedError\n\n";

    // ── 通用值反序列化（基于 type_id 前缀） ──
    ss << "# ============================================================\n";
    ss << "# DeserializeValue — 通用值反序列化（基于 type_id 前缀）\n";
    ss << "# 格式: [type_id(u32 LE) + value_bytes]\n";
    ss << "# ============================================================\n\n";
    ss << "def deserialize_value(data: bytes) -> Any:\n";
    ss << "    \"\"\"从 type_id 前缀格式的二进制数据反序列化值\"\"\"\n";
    ss << "    if len(data) < 4:\n";
    ss << "        return None\n";
    ss << "    type_id = struct.unpack('<I', data[:4])[0]\n";
    ss << "    value_bytes = data[4:]\n";
    ss << "    if type_id == TypeId.U8:\n";
    ss << "        if len(value_bytes) >= 1:\n";
    ss << "            return value_bytes[0]\n";
    ss << "        return 0\n";
    ss << "    if type_id == TypeId.U16:\n";
    ss << "        if len(value_bytes) >= 2:\n";
    ss << "            return struct.unpack('<H', value_bytes)[0]\n";
    ss << "        return 0\n";
    ss << "    if type_id == TypeId.U32:\n";
    ss << "        if len(value_bytes) >= 4:\n";
    ss << "            return struct.unpack('<I', value_bytes)[0]\n";
    ss << "        return 0\n";
    ss << "    if type_id == TypeId.U64:\n";
    ss << "        if len(value_bytes) >= 8:\n";
    ss << "            return struct.unpack('<Q', value_bytes)[0]\n";
    ss << "        return 0\n";
    ss << "    if type_id == TypeId.I8:\n";
    ss << "        if len(value_bytes) >= 1:\n";
    ss << "            return struct.unpack('<b', value_bytes)[0]\n";
    ss << "        return 0\n";
    ss << "    if type_id == TypeId.I16:\n";
    ss << "        if len(value_bytes) >= 2:\n";
    ss << "            return struct.unpack('<h', value_bytes)[0]\n";
    ss << "        return 0\n";
    ss << "    if type_id == TypeId.I32:\n";
    ss << "        if len(value_bytes) >= 4:\n";
    ss << "            return struct.unpack('<i', value_bytes)[0]\n";
    ss << "        return 0\n";
    ss << "    if type_id == TypeId.I64:\n";
    ss << "        if len(value_bytes) >= 8:\n";
    ss << "            return struct.unpack('<q', value_bytes)[0]\n";
    ss << "        return 0\n";
    ss << "    if type_id == TypeId.F32:\n";
    ss << "        if len(value_bytes) >= 4:\n";
    ss << "            return struct.unpack('<f', value_bytes)[0]\n";
    ss << "        return 0.0\n";
    ss << "    if type_id == TypeId.F64:\n";
    ss << "        if len(value_bytes) >= 8:\n";
    ss << "            return struct.unpack('<d', value_bytes)[0]\n";
    ss << "        return 0.0\n";
    ss << "    if type_id == TypeId.STRING:\n";
    ss << "        return value_bytes.decode('utf-8', errors='replace')\n";
    ss << "    if type_id == TypeId.BOOL:\n";
    ss << "        if len(value_bytes) >= 1:\n";
    ss << "            return value_bytes[0] != 0\n";
    ss << "        return False\n";
    ss << "    return value_bytes\n\n";

    // ── 每个类型的访问器 ──
    for (auto& t : types) {
        ss << "# ============================================================\n";
        ss << "# " << t.className << "Accessor\n";
        ss << "# ============================================================\n\n";
        ss << "class " << t.className << "Accessor(SpoiAccessor):\n";
        ss << "    \"\"\"" << t.className << " 类型特化访问器\"\"\"\n\n";

        ss << "    def field_count(self) -> int:\n";
        ss << "        return " << t.fields.size() << "\n\n";

        // get_field
        ss << "    def get_field(self, obj: Any, idx: int) -> Any:\n";
        ss << "        if idx == " << t.fields[0].index << ":\n";
        ss << "            return obj." << t.fields[0].name << "\n";
        for (size_t i = 1; i < t.fields.size(); ++i) {
            auto& f = t.fields[i];
            ss << "        elif idx == " << f.index << ":\n";
            ss << "            return obj." << f.name << "\n";
        }
        ss << "        else:\n";
        ss << "            raise ValueError(f\"invalid field index for " << t.className << ": {idx}\")\n\n";

        // set_field
        ss << "    def set_field(self, obj: Any, idx: int, val: Any) -> None:\n";
        ss << "        if idx == " << t.fields[0].index << ":\n";
        ss << "            obj." << t.fields[0].name << " = val\n";
        for (size_t i = 1; i < t.fields.size(); ++i) {
            auto& f = t.fields[i];
            ss << "        elif idx == " << f.index << ":\n";
            ss << "            obj." << f.name << " = val\n";
        }
        ss << "        else:\n";
        ss << "            raise ValueError(f\"invalid field index for " << t.className << ": {idx}\")\n\n";
    }

    // ── 静态类型注册表 ──
    ss << "# ============================================================\n";
    ss << "# SpoiAccessorRegistry — 静态类型注册表\n";
    ss << "# 替代运行时 Dict[str, List[str]]（反射版本）\n";
    ss << "# ============================================================\n\n";
    ss << "SpoiAccessorRegistry: Dict[str, SpoiAccessor] = {\n";
    for (auto& t : types) {
        ss << "    \"" << t.className << "\": " << t.className << "Accessor(),\n";
    }
    ss << "}\n";

    return ss.str();
}

// =============================== 入口函数 ===============================

int generate_spoi_python_accessor(const std::string& output_path, const std::string& meta_path) {
    try {
        auto meta = sp_meta::readMetaFile(meta_path);
        auto types = extractSpoiTypes(meta);
        std::cout << "Extracted " << types.size() << " types for Python accessor generation" << std::endl;

        auto code = generatePythonAccessor(types);

        std::ofstream out(output_path);
        if (!out.is_open()) throw std::runtime_error("Cannot open: " + output_path);
        out << code;
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "SPOI Python accessor generator error: " << e.what() << "\n";
        return 1;
    }
}