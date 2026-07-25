/*
    spoi_kotlin_generator.cpp — Kotlin SPOI 代码生成器

    生成 Kotlin 的 SPOI 查询/更新 builder，包含：
    - 类型常量（成员索引）
    - SpoiQuery 流式 API
    - SpoiUpdate 写操作 API
    - SPOI 二进制序列化
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

std::string generateKotlinSpoi(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "// ============================================================\n";
    ss << "// SPOI — StreamPunk Operation Instruction\n";
    ss << "// Kotlin 查询/更新 Builder（自动生成）\n";
    ss << "// ============================================================\n\n";

    ss << "import java.nio.ByteBuffer\n";
    ss << "import java.nio.ByteOrder\n";
    ss << "import java.nio.charset.StandardCharsets\n\n";

    // ── 操作码常量 ──
    ss << "// 操作码\n";
    ss << "object Op {\n";
    ss << "    const val SET       = 0x04\n";
    ss << "    const val ADD       = 0x05\n";
    ss << "    const val APPEND    = 0x06\n";
    ss << "    const val REMOVE    = 0x07\n";
    ss << "    const val INSERT    = 0x08\n";
    ss << "    const val REPLACE   = 0x09\n";
    ss << "    const val RESET     = 0x0A\n";
    ss << "    const val SETNULL   = 0x0B\n";
    ss << "    const val FILTER    = 0x0C\n";
    ss << "    const val SELECT    = 0x0D\n";
    ss << "    const val SORT      = 0x0E\n";
    ss << "    const val REVERSE   = 0x0F\n";
    ss << "    const val TAKE      = 0x10\n";
    ss << "    const val DROP      = 0x11\n";
    ss << "    const val TAKEWHILE = 0x12\n";
    ss << "    const val DROPWHILE = 0x13\n";
    ss << "    const val DISTINCT  = 0x14\n";
    ss << "    const val COUNT     = 0x15\n";
    ss << "    const val ANY       = 0x16\n";
    ss << "    const val ALL       = 0x17\n";
    ss << "    const val FIND      = 0x18\n";
    ss << "    const val KEYS      = 0x19\n";
    ss << "    const val VALUES    = 0x1A\n";
    ss << "    const val JOIN      = 0x1B\n";
    ss << "    const val ENUMERATE = 0x1C\n";
    ss << "    const val CHUNK     = 0x1D\n";
    ss << "    const val SLIDE     = 0x1E\n";
    ss << "    const val STRIDE    = 0x1F\n";
    ss << "    const val ADJACENT  = 0x20\n";
    ss << "    const val EXEC      = 0x21\n";
    ss << "}\n\n";

    // ── 比较运算符 ──
    ss << "// 比较运算符\n";
    ss << "object Cmp {\n";
    ss << "    const val EQ = 0\n";
    ss << "    const val NE = 1\n";
    ss << "    const val LT = 2\n";
    ss << "    const val GT = 3\n";
    ss << "    const val LE = 4\n";
    ss << "    const val GE = 5\n";
    ss << "}\n\n";

    ss << "const val PATH_DEREF = 0xFFFF\n\n";

    // ── 类型成员常量 ──
    ss << "// 类型成员索引常量\n";
    for (auto& t : types) {
        ss << "// " << t.className << "\n";
        for (auto& f : t.fields) {
            ss << "const val " << t.className << "_" << f.name << " = " << f.index << "\n";
        }
        ss << "\n";
    }

    // ── Varint 编码 ──
    ss << "// Varint 编码\n";
    ss << "fun writeVarint(buf: ArrayList<Byte>, v: Int) {\n";
    ss << "    var value = v\n";
    ss << "    while (value >= 0x80) {\n";
    ss << "        buf.add(((value and 0x7F) or 0x80).toByte())\n";
    ss << "        value = value ushr 7\n";
    ss << "    }\n";
    ss << "    buf.add((value and 0x7F).toByte())\n";
    ss << "}\n\n";
    ss << "fun writeU32(buf: ArrayList<Byte>, v: Int) {\n";
    ss << "    buf.add((v and 0xFF).toByte())\n";
    ss << "    buf.add(((v ushr 8) and 0xFF).toByte())\n";
    ss << "    buf.add(((v ushr 16) and 0xFF).toByte())\n";
    ss << "    buf.add(((v ushr 24) and 0xFF).toByte())\n";
    ss << "}\n\n";

    // ── SpoiInstruction ──
    ss << "// SpoiInstruction\n";
    ss << "class SpoiInstruction(\n";
    ss << "    val op: Int,\n";
    ss << "    val path: IntArray,\n";
    ss << "    val operand: ByteArray = ByteArray(0)\n";
    ss << ") {\n";
    ss << "    fun serialize(): ByteArray {\n";
    ss << "        val buf = ArrayList<Byte>()\n";
    ss << "        buf.add(op.toByte())\n";
    ss << "        writeVarint(buf, path.size)\n";
    ss << "        for (seg in path) writeU32(buf, seg)\n";
    ss << "        writeVarint(buf, operand.size)\n";
    ss << "        for (b in operand) buf.add(b)\n";
    ss << "        return ByteArray(buf.size) { buf[it] }\n";
    ss << "    }\n";
    ss << "}\n\n";

    // ── SpoiStream ──
    ss << "// SpoiStream\n";
    ss << "class SpoiStream {\n";
    ss << "    val instructions = ArrayList<SpoiInstruction>()\n\n";
    ss << "    fun build(): ByteArray {\n";
    ss << "        val buf = ArrayList<Byte>()\n";
    ss << "        writeVarint(buf, instructions.size)\n";
    ss << "        for (inst in instructions) {\n";
    ss << "            for (b in inst.serialize()) buf.add(b)\n";
    ss << "        }\n";
    ss << "        return ByteArray(buf.size) { buf[it] }\n";
    ss << "    }\n\n";
    ss << "    fun buildHex(): String {\n";
    ss << "        return build().joinToString(\"\") { \"%02x\".format(it) }\n";
    ss << "    }\n";
    ss << "}\n\n";

    // ── SpoiUpdate ──
    ss << "// SpoiUpdate — 写操作 Builder\n";
    ss << "class SpoiUpdate {\n";
    ss << "    private val stream = SpoiStream()\n\n";
    ss << "    fun set(path: IntArray, value: ByteArray): SpoiUpdate {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.SET, path, value))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun setI32(path: IntArray, value: Int): SpoiUpdate {\n";
    ss << "        return set(path, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(value).array())\n";
    ss << "    }\n\n";
    ss << "    fun setU32(path: IntArray, value: Int): SpoiUpdate {\n";
    ss << "        return set(path, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(value).array())\n";
    ss << "    }\n\n";
    ss << "    fun setF64(path: IntArray, value: Double): SpoiUpdate {\n";
    ss << "        return set(path, ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putDouble(value).array())\n";
    ss << "    }\n\n";
    ss << "    fun setStr(path: IntArray, value: String): SpoiUpdate {\n";
    ss << "        val data = value.toByteArray(StandardCharsets.UTF_8)\n";
    ss << "        val buf = ArrayList<Byte>()\n";
    ss << "        writeVarint(buf, data.size)\n";
    ss << "        for (b in data) buf.add(b)\n";
    ss << "        return set(path, ByteArray(buf.size) { buf[it] })\n";
    ss << "    }\n\n";
    ss << "    fun setBool(path: IntArray, value: Boolean): SpoiUpdate {\n";
    ss << "        return set(path, byteArrayOf(if (value) 1 else 0))\n";
    ss << "    }\n\n";
    ss << "    fun addI32(path: IntArray, delta: Int): SpoiUpdate {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.ADD, path,\n";
    ss << "            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(delta).array()))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun add(path: IntArray, value: ByteArray): SpoiUpdate {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.ADD, path, value))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun append(path: IntArray, value: ByteArray): SpoiUpdate {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.APPEND, path, value))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun remove(path: IntArray, value: ByteArray): SpoiUpdate {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.REMOVE, path, value))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun insert(path: IntArray, value: ByteArray): SpoiUpdate {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.INSERT, path, value))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun replace(path: IntArray, value: ByteArray): SpoiUpdate {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.REPLACE, path, value))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun reset(path: IntArray): SpoiUpdate {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.RESET, path, ByteArray(0)))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun setnull(path: IntArray): SpoiUpdate {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.SETNULL, path, ByteArray(0)))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun build(): ByteArray = stream.build()\n";
    ss << "    fun buildHex(): String = stream.buildHex()\n";
    ss << "}\n\n";

    // ── SpoiQuery ──
    ss << "// SpoiQuery — 查询 Builder\n";
    ss << "class SpoiQuery {\n";
    ss << "    private val stream = SpoiStream()\n\n";
    ss << "    fun nav(field: Int): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.FILTER, intArrayOf(field), ByteArray(0)))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun filter(field: Int, cmpOp: Int, value: ByteArray): SpoiQuery {\n";
    ss << "        val buf = ArrayList<Byte>()\n";
    ss << "        writeU32(buf, field)\n";
    ss << "        buf.add(cmpOp.toByte())\n";
    ss << "        writeVarint(buf, value.size)\n";
    ss << "        for (b in value) buf.add(b)\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.FILTER, IntArray(0),\n";
    ss << "            ByteArray(buf.size) { buf[it] }))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun filterI32(field: Int, cmpOp: Int, value: Int): SpoiQuery {\n";
    ss << "        return filter(field, cmpOp,\n";
    ss << "            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(value).array())\n";
    ss << "    }\n\n";
    ss << "    fun filterStr(field: Int, cmpOp: Int, value: String): SpoiQuery {\n";
    ss << "        val data = value.toByteArray(StandardCharsets.UTF_8)\n";
    ss << "        val buf = ArrayList<Byte>()\n";
    ss << "        writeVarint(buf, data.size)\n";
    ss << "        for (b in data) buf.add(b)\n";
    ss << "        return filter(field, cmpOp, ByteArray(buf.size) { buf[it] })\n";
    ss << "    }\n\n";
    ss << "    fun select(vararg fields: Int): SpoiQuery {\n";
    ss << "        val buf = ArrayList<Byte>()\n";
    ss << "        writeU32(buf, fields.size)\n";
    ss << "        for (f in fields) writeU32(buf, f)\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.SELECT, IntArray(0),\n";
    ss << "            ByteArray(buf.size) { buf[it] }))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun sort(field: Int, ascending: Boolean = true): SpoiQuery {\n";
    ss << "        val buf = ArrayList<Byte>()\n";
    ss << "        writeU32(buf, field)\n";
    ss << "        buf.add((if (ascending) 1 else 0).toByte())\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.SORT, IntArray(0),\n";
    ss << "            ByteArray(buf.size) { buf[it] }))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun reverse(): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.REVERSE, IntArray(0), ByteArray(0)))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun take(count: Int): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.TAKE, IntArray(0),\n";
    ss << "            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(count).array()))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun drop(count: Int): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.DROP, IntArray(0),\n";
    ss << "            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(count).array()))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun distinct(): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.DISTINCT, IntArray(0), ByteArray(0)))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun count(): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.COUNT, IntArray(0), ByteArray(0)))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun keys(): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.KEYS, IntArray(0), ByteArray(0)))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun values(): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.VALUES, IntArray(0), ByteArray(0)))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun join(field: Int): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.JOIN, IntArray(0),\n";
    ss << "            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(field).array()))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun enumerate(start: Int = 0): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.ENUMERATE, IntArray(0),\n";
    ss << "            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(start).array()))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun chunk(size: Int): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.CHUNK, IntArray(0),\n";
    ss << "            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(size).array()))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun stride(step: Int): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.STRIDE, IntArray(0),\n";
    ss << "            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(step).array()))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun takewhile(field: Int, cmpOp: Int, value: ByteArray): SpoiQuery {\n";
    ss << "        val buf = ArrayList<Byte>()\n";
    ss << "        writeU32(buf, field)\n";
    ss << "        buf.add(cmpOp.toByte())\n";
    ss << "        writeVarint(buf, value.size)\n";
    ss << "        for (b in value) buf.add(b)\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.TAKEWHILE, IntArray(0),\n";
    ss << "            ByteArray(buf.size) { buf[it] }))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun dropwhile(field: Int, cmpOp: Int, value: ByteArray): SpoiQuery {\n";
    ss << "        val buf = ArrayList<Byte>()\n";
    ss << "        writeU32(buf, field)\n";
    ss << "        buf.add(cmpOp.toByte())\n";
    ss << "        writeVarint(buf, value.size)\n";
    ss << "        for (b in value) buf.add(b)\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.DROPWHILE, IntArray(0),\n";
    ss << "            ByteArray(buf.size) { buf[it] }))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun any(field: Int, cmpOp: Int, value: ByteArray): SpoiQuery {\n";
    ss << "        val buf = ArrayList<Byte>()\n";
    ss << "        writeU32(buf, field)\n";
    ss << "        buf.add(cmpOp.toByte())\n";
    ss << "        writeVarint(buf, value.size)\n";
    ss << "        for (b in value) buf.add(b)\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.ANY, IntArray(0),\n";
    ss << "            ByteArray(buf.size) { buf[it] }))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun all(field: Int, cmpOp: Int, value: ByteArray): SpoiQuery {\n";
    ss << "        val buf = ArrayList<Byte>()\n";
    ss << "        writeU32(buf, field)\n";
    ss << "        buf.add(cmpOp.toByte())\n";
    ss << "        writeVarint(buf, value.size)\n";
    ss << "        for (b in value) buf.add(b)\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.ALL, IntArray(0),\n";
    ss << "            ByteArray(buf.size) { buf[it] }))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun find(field: Int, cmpOp: Int, value: ByteArray): SpoiQuery {\n";
    ss << "        val buf = ArrayList<Byte>()\n";
    ss << "        writeU32(buf, field)\n";
    ss << "        buf.add(cmpOp.toByte())\n";
    ss << "        writeVarint(buf, value.size)\n";
    ss << "        for (b in value) buf.add(b)\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.FIND, IntArray(0),\n";
    ss << "            ByteArray(buf.size) { buf[it] }))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun slide(size: Int): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.SLIDE, IntArray(0),\n";
    ss << "            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(size).array()))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun adjacent(n: Int): SpoiQuery {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.ADJACENT, IntArray(0),\n";
    ss << "            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(n).array()))\n";
    ss << "        return this\n";
    ss << "    }\n\n";
    ss << "    fun build(): ByteArray {\n";
    ss << "        stream.instructions.add(SpoiInstruction(Op.EXEC, IntArray(0), ByteArray(0)))\n";
    ss << "        return stream.build()\n";
    ss << "    }\n";
    ss << "}\n";

    return ss.str();
}

int generate_spoi_kotlin(const std::string& output_path, const std::string& meta_path) {
    try {
        auto meta = sp_meta::readMetaFile(meta_path);
        auto types = extractSpoiTypes(meta);
        auto code = generateKotlinSpoi(types);

        std::ofstream out(output_path);
        if (!out.is_open()) throw std::runtime_error("Cannot open: " + output_path);
        out << code;
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "SPOI Kotlin generator error: " << e.what() << "\n";
        return 1;
    }
}