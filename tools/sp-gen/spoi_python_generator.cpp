/*
    spoi_python_generator.cpp — Python SPOI 代码生成器

    生成 Python 的 SPOI 查询/更新 builder，包含：
    - 类型常量（成员索引）
    - SpoiQuery 流式 API
    - SpoiUpdate 写操作 API
    - SPOI 二进制序列化
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>

using namespace spoi_gen;

// =============================== 辅助函数 ===============================

static std::string pyClassName(const std::string& cppName) {
    return cppName; // Python 保持原名
}

static std::string pyMemberConst(const std::string& className, const std::string& fieldName) {
    return className + "_" + fieldName;
}

// =============================== Python 代码生成 ===============================

std::string generatePythonSpoi(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "# ============================================================\n";
    ss << "# SPOI — StreamPunk Operation Instruction\n";
    ss << "# Python 查询/更新 Builder（自动生成）\n";
    ss << "# ============================================================\n\n";
    ss << "import struct\n";
    ss << "from typing import List, Optional, Any, Union\n";
    ss << "from io import BytesIO\n\n";

    // ── 操作码常量 ──
    ss << "# ============================================================\n";
    ss << "# 操作码\n";
    ss << "# ============================================================\n\n";
    ss << "class Op:\n";
    ss << "    SET       = 0x04\n";
    ss << "    ADD       = 0x05\n";
    ss << "    APPEND    = 0x06\n";
    ss << "    REMOVE    = 0x07\n";
    ss << "    INSERT    = 0x08\n";
    ss << "    REPLACE   = 0x09\n";
    ss << "    RESET     = 0x0A\n";
    ss << "    SETNULL   = 0x0B\n";
    ss << "    FILTER    = 0x0C\n";
    ss << "    SELECT    = 0x0D\n";
    ss << "    SORT      = 0x0E\n";
    ss << "    REVERSE   = 0x0F\n";
    ss << "    TAKE      = 0x10\n";
    ss << "    DROP      = 0x11\n";
    ss << "    TAKEWHILE = 0x12\n";
    ss << "    DROPWHILE = 0x13\n";
    ss << "    DISTINCT  = 0x14\n";
    ss << "    COUNT     = 0x15\n";
    ss << "    ANY       = 0x16\n";
    ss << "    ALL       = 0x17\n";
    ss << "    FIND      = 0x18\n";
    ss << "    KEYS      = 0x19\n";
    ss << "    VALUES    = 0x1A\n";
    ss << "    JOIN      = 0x1B\n";
    ss << "    ENUMERATE = 0x1C\n";
    ss << "    CHUNK     = 0x1D\n";
    ss << "    SLIDE     = 0x1E\n";
    ss << "    STRIDE    = 0x1F\n";
    ss << "    ADJACENT  = 0x20\n";
    ss << "    EXEC      = 0x21\n\n";

    // ── 比较运算符 ──
    ss << "# ============================================================\n";
    ss << "# 比较运算符\n";
    ss << "# ============================================================\n\n";
    ss << "class Cmp:\n";
    ss << "    EQ = 0\n";
    ss << "    NE = 1\n";
    ss << "    LT = 2\n";
    ss << "    GT = 3\n";
    ss << "    LE = 4\n";
    ss << "    GE = 5\n\n";

    // ── 类型成员常量 ──
    ss << "# ============================================================\n";
    ss << "# 类型成员索引常量\n";
    ss << "# ============================================================\n\n";

    for (auto& t : types) {
        ss << "# " << t.className << "\n";
        for (auto& f : t.fields) {
            ss << pyMemberConst(t.className, f.name) << " = " << f.index << "\n";
        }
        ss << "\n";
    }

    // ── Varint 编码 ──
    ss << "# ============================================================\n";
    ss << "# Varint 编码\n";
    ss << "# ============================================================\n\n";
    ss << "def _write_varint(buf: bytearray, v: int) -> None:\n";
    ss << "    while v >= 0x80:\n";
    ss << "        buf.append((v & 0x7F) | 0x80)\n";
    ss << "        v >>= 7\n";
    ss << "    buf.append(v & 0x7F)\n\n";
    ss << "def _write_u32(buf: bytearray, v: int) -> None:\n";
    ss << "    buf.extend(struct.pack('<I', v & 0xFFFFFFFF))\n\n";

    // ── SpoiInstruction 序列化 ──
    ss << "# ============================================================\n";
    ss << "# SpoiInstruction 序列化\n";
    ss << "# ============================================================\n\n";
    ss << "class SpoiInstruction:\n";
    ss << "    def __init__(self, op: int, path: List[int], operand: bytes = b''):\n";
    ss << "        self.op = op\n";
    ss << "        self.path = path\n";
    ss << "        self.operand = operand\n\n";
    ss << "    def serialize(self) -> bytes:\n";
    ss << "        buf = bytearray()\n";
    ss << "        buf.append(self.op)\n";
    ss << "        _write_varint(buf, len(self.path))\n";
    ss << "        for seg in self.path:\n";
    ss << "            _write_u32(buf, seg)\n";
    ss << "        _write_varint(buf, len(self.operand))\n";
    ss << "        buf.extend(self.operand)\n";
    ss << "        return bytes(buf)\n\n";

    // ── SpoiStream 序列化 ──
    ss << "# ============================================================\n";
    ss << "# SpoiStream（指令流）\n";
    ss << "# ============================================================\n\n";
    ss << "class SpoiStream:\n";
    ss << "    def __init__(self):\n";
    ss << "        self.instructions: List[SpoiInstruction] = []\n\n";
    ss << "    def build(self) -> bytes:\n";
    ss << "        buf = bytearray()\n";
    ss << "        _write_varint(buf, len(self.instructions))\n";
    ss << "        for inst in self.instructions:\n";
    ss << "            buf.extend(inst.serialize())\n";
    ss << "        return bytes(buf)\n\n";
    ss << "    def build_hex(self) -> str:\n";
    ss << "        return self.build().hex()\n\n";

    // ── SpoiUpdate 写操作 Builder ──
    ss << "# ============================================================\n";
    ss << "# SpoiUpdate — 写操作 Builder\n";
    ss << "# ============================================================\n\n";
    ss << "class SpoiUpdate:\n";
    ss << "    def __init__(self):\n";
    ss << "        self._stream = SpoiStream()\n\n";
    ss << "    def set(self, path: List[int], value_bytes: bytes) -> 'SpoiUpdate':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.SET, path, value_bytes))\n";
    ss << "        return self\n\n";
    ss << "    def set_i32(self, path: List[int], value: int) -> 'SpoiUpdate':\n";
    ss << "        return self.set(path, struct.pack('<i', value))\n\n";
    ss << "    def set_u32(self, path: List[int], value: int) -> 'SpoiUpdate':\n";
    ss << "        return self.set(path, struct.pack('<I', value))\n\n";
    ss << "    def set_f64(self, path: List[int], value: float) -> 'SpoiUpdate':\n";
    ss << "        return self.set(path, struct.pack('<d', value))\n\n";
    ss << "    def set_str(self, path: List[int], value: str) -> 'SpoiUpdate':\n";
    ss << "        data = value.encode('utf-8')\n";
    ss << "        buf = bytearray()\n";
    ss << "        _write_varint(buf, len(data))\n";
    ss << "        buf.extend(data)\n";
    ss << "        return self.set(path, bytes(buf))\n\n";
    ss << "    def set_bool(self, path: List[int], value: bool) -> 'SpoiUpdate':\n";
    ss << "        return self.set(path, b'\\x01' if value else b'\\x00')\n\n";
    ss << "    def add_i32(self, path: List[int], delta: int) -> 'SpoiUpdate':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.ADD, path, struct.pack('<i', delta)))\n";
    ss << "        return self\n\n";
    ss << "    def add_f64(self, path: List[int], delta: float) -> 'SpoiUpdate':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.ADD, path, struct.pack('<d', delta)))\n";
    ss << "        return self\n\n";
    ss << "    def add(self, path: List[int], value_bytes: bytes) -> 'SpoiUpdate':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.ADD, path, value_bytes))\n";
    ss << "        return self\n\n";
    ss << "    def append(self, path: List[int], value_bytes: bytes) -> 'SpoiUpdate':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.APPEND, path, value_bytes))\n";
    ss << "        return self\n\n";
    ss << "    def remove(self, path: List[int], value_bytes: bytes) -> 'SpoiUpdate':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.REMOVE, path, value_bytes))\n";
    ss << "        return self\n\n";
    ss << "    def insert(self, path: List[int], value_bytes: bytes) -> 'SpoiUpdate':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.INSERT, path, value_bytes))\n";
    ss << "        return self\n\n";
    ss << "    def replace(self, path: List[int], value_bytes: bytes) -> 'SpoiUpdate':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.REPLACE, path, value_bytes))\n";
    ss << "        return self\n\n";
    ss << "    def reset(self, path: List[int]) -> 'SpoiUpdate':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.RESET, path))\n";
    ss << "        return self\n\n";
    ss << "    def setnull(self, path: List[int]) -> 'SpoiUpdate':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.SETNULL, path))\n";
    ss << "        return self\n\n";
    ss << "    def build(self) -> SpoiStream:\n";
    ss << "        return self._stream\n\n";
    ss << "    def build_hex(self) -> str:\n";
    ss << "        return self._stream.build_hex()\n\n";

    // ── SpoiQuery 查询 Builder ──
    ss << "# ============================================================\n";
    ss << "# SpoiQuery — 查询 Builder\n";
    ss << "# ============================================================\n\n";
    ss << "class SpoiQuery:\n";
    ss << "    def __init__(self, root_type: str):\n";
    ss << "        self._stream = SpoiStream()\n";
    ss << "        self._root_type = root_type\n\n";
    ss << "    def nav(self, field: int) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.FILTER, [field]))\n";
    ss << "        return self\n\n";
    ss << "    def filter(self, field: int, cmp_op: int, value_bytes: bytes) -> 'SpoiQuery':\n";
    ss << "        buf = bytearray()\n";
    ss << "        _write_u32(buf, field)\n";
    ss << "        buf.append(cmp_op)\n";
    ss << "        _write_varint(buf, len(value_bytes))\n";
    ss << "        buf.extend(value_bytes)\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.FILTER, [], bytes(buf)))\n";
    ss << "        return self\n\n";
    ss << "    def filter_i32(self, field: int, cmp_op: int, value: int) -> 'SpoiQuery':\n";
    ss << "        return self.filter(field, cmp_op, struct.pack('<i', value))\n\n";
    ss << "    def filter_f64(self, field: int, cmp_op: int, value: float) -> 'SpoiQuery':\n";
    ss << "        return self.filter(field, cmp_op, struct.pack('<d', value))\n\n";
    ss << "    def filter_str(self, field: int, cmp_op: int, value: str) -> 'SpoiQuery':\n";
    ss << "        data = value.encode('utf-8')\n";
    ss << "        buf = bytearray()\n";
    ss << "        _write_varint(buf, len(data))\n";
    ss << "        buf.extend(data)\n";
    ss << "        return self.filter(field, cmp_op, bytes(buf))\n\n";
    ss << "    def filter_bool(self, field: int, cmp_op: int, value: bool) -> 'SpoiQuery':\n";
    ss << "        return self.filter(field, cmp_op, b'\\x01' if value else b'\\x00')\n\n";
    ss << "    def select(self, *fields: int) -> 'SpoiQuery':\n";
    ss << "        buf = bytearray()\n";
    ss << "        _write_u32(buf, len(fields))\n";
    ss << "        for f in fields:\n";
    ss << "            _write_u32(buf, f)\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.SELECT, [], bytes(buf)))\n";
    ss << "        return self\n\n";
    ss << "    def sort(self, field: int, ascending: bool = True) -> 'SpoiQuery':\n";
    ss << "        buf = bytearray()\n";
    ss << "        _write_u32(buf, field)\n";
    ss << "        buf.append(1 if ascending else 0)\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.SORT, [], bytes(buf)))\n";
    ss << "        return self\n\n";
    ss << "    def reverse(self) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.REVERSE, []))\n";
    ss << "        return self\n\n";
    ss << "    def take(self, count: int) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.TAKE, [], struct.pack('<I', count)))\n";
    ss << "        return self\n\n";
    ss << "    def drop(self, count: int) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.DROP, [], struct.pack('<I', count)))\n";
    ss << "        return self\n\n";
    ss << "    def distinct(self) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.DISTINCT, []))\n";
    ss << "        return self\n\n";
    ss << "    def count(self) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.COUNT, []))\n";
    ss << "        return self\n\n";
    ss << "    def takewhile(self, field: int, cmp_op: int, value_bytes: bytes) -> 'SpoiQuery':\n";
    ss << "        buf = bytearray()\n";
    ss << "        _write_u32(buf, field)\n";
    ss << "        buf.append(cmp_op)\n";
    ss << "        _write_varint(buf, len(value_bytes))\n";
    ss << "        buf.extend(value_bytes)\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.TAKEWHILE, [], bytes(buf)))\n";
    ss << "        return self\n\n";
    ss << "    def dropwhile(self, field: int, cmp_op: int, value_bytes: bytes) -> 'SpoiQuery':\n";
    ss << "        buf = bytearray()\n";
    ss << "        _write_u32(buf, field)\n";
    ss << "        buf.append(cmp_op)\n";
    ss << "        _write_varint(buf, len(value_bytes))\n";
    ss << "        buf.extend(value_bytes)\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.DROPWHILE, [], bytes(buf)))\n";
    ss << "        return self\n\n";
    ss << "    def any(self, field: int, cmp_op: int, value_bytes: bytes) -> 'SpoiQuery':\n";
    ss << "        buf = bytearray()\n";
    ss << "        _write_u32(buf, field)\n";
    ss << "        buf.append(cmp_op)\n";
    ss << "        _write_varint(buf, len(value_bytes))\n";
    ss << "        buf.extend(value_bytes)\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.ANY, [], bytes(buf)))\n";
    ss << "        return self\n\n";
    ss << "    def all(self, field: int, cmp_op: int, value_bytes: bytes) -> 'SpoiQuery':\n";
    ss << "        buf = bytearray()\n";
    ss << "        _write_u32(buf, field)\n";
    ss << "        buf.append(cmp_op)\n";
    ss << "        _write_varint(buf, len(value_bytes))\n";
    ss << "        buf.extend(value_bytes)\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.ALL, [], bytes(buf)))\n";
    ss << "        return self\n\n";
    ss << "    def find(self, field: int, cmp_op: int, value_bytes: bytes) -> 'SpoiQuery':\n";
    ss << "        buf = bytearray()\n";
    ss << "        _write_u32(buf, field)\n";
    ss << "        buf.append(cmp_op)\n";
    ss << "        _write_varint(buf, len(value_bytes))\n";
    ss << "        buf.extend(value_bytes)\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.FIND, [], bytes(buf)))\n";
    ss << "        return self\n\n";
    ss << "    def keys(self) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.KEYS, []))\n";
    ss << "        return self\n\n";
    ss << "    def values(self) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.VALUES, []))\n";
    ss << "        return self\n\n";
    ss << "    def join(self, field: int) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.JOIN, [], struct.pack('<I', field)))\n";
    ss << "        return self\n\n";
    ss << "    def enumerate(self, start: int = 0) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.ENUMERATE, [], struct.pack('<I', start)))\n";
    ss << "        return self\n\n";
    ss << "    def chunk(self, size: int) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.CHUNK, [], struct.pack('<I', size)))\n";
    ss << "        return self\n\n";
    ss << "    def slide(self, size: int) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.SLIDE, [], struct.pack('<I', size)))\n";
    ss << "        return self\n\n";
    ss << "    def stride(self, step: int) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.STRIDE, [], struct.pack('<I', step)))\n";
    ss << "        return self\n\n";
    ss << "    def adjacent(self, n: int) -> 'SpoiQuery':\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.ADJACENT, [], struct.pack('<I', n)))\n";
    ss << "        return self\n\n";
    ss << "    def build(self) -> SpoiStream:\n";
    ss << "        self._stream.instructions.append(SpoiInstruction(Op.EXEC, []))\n";
    ss << "        return self._stream\n\n";
    ss << "    def build_hex(self) -> str:\n";
    ss << "        self.build()\n";
    ss << "        return self._stream.build_hex()\n\n";

    return ss.str();
}

// =============================== 入口函数 ===============================

int generate_spoi_python(const std::string& output_path, const std::string& meta_path) {
    try {
        auto meta = sp_meta::readMetaFile(meta_path);
        auto types = extractSpoiTypes(meta);

        auto code = generatePythonSpoi(types);

        std::ofstream out(output_path);
        if (!out.is_open()) {
            throw std::runtime_error("Cannot open output file: " + output_path);
        }
        out << code;
        out.close();
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "SPOI Python generator error: " << e.what() << "\n";
        return 1;
    }
}