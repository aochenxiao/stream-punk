/*
    spoi_java_generator.cpp — Java SPOI 代码生成器

    生成 Java 的 SPOI 查询/更新 builder，包含：
    - 类型常量（成员索引）
    - SpoiQuery 流式 API
    - SpoiUpdate 写操作 API
    - SPOI 二进制序列化
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

std::string generateJavaSpoi(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "// ============================================================\n";
    ss << "// SPOI — StreamPunk Operation Instruction\n";
    ss << "// Java 查询/更新 Builder（自动生成）\n";
    ss << "// ============================================================\n\n";

    ss << "import java.nio.ByteBuffer;\n";
    ss << "import java.nio.ByteOrder;\n";
    ss << "import java.nio.charset.StandardCharsets;\n";
    ss << "import java.util.ArrayList;\n";
    ss << "import java.util.Formatter;\n\n";

    // ── 操作码常量 ──
    ss << "// 操作码\n";
    ss << "class Op {\n";
    ss << "    public static final int SET       = 0x04;\n";
    ss << "    public static final int ADD       = 0x05;\n";
    ss << "    public static final int APPEND    = 0x06;\n";
    ss << "    public static final int REMOVE    = 0x07;\n";
    ss << "    public static final int INSERT    = 0x08;\n";
    ss << "    public static final int REPLACE   = 0x09;\n";
    ss << "    public static final int RESET     = 0x0A;\n";
    ss << "    public static final int SETNULL   = 0x0B;\n";
    ss << "    public static final int FILTER    = 0x0C;\n";
    ss << "    public static final int SELECT    = 0x0D;\n";
    ss << "    public static final int SORT      = 0x0E;\n";
    ss << "    public static final int REVERSE   = 0x0F;\n";
    ss << "    public static final int TAKE      = 0x10;\n";
    ss << "    public static final int DROP      = 0x11;\n";
    ss << "    public static final int TAKEWHILE = 0x12;\n";
    ss << "    public static final int DROPWHILE = 0x13;\n";
    ss << "    public static final int DISTINCT  = 0x14;\n";
    ss << "    public static final int COUNT     = 0x15;\n";
    ss << "    public static final int ANY       = 0x16;\n";
    ss << "    public static final int ALL       = 0x17;\n";
    ss << "    public static final int FIND      = 0x18;\n";
    ss << "    public static final int KEYS      = 0x19;\n";
    ss << "    public static final int VALUES    = 0x1A;\n";
    ss << "    public static final int JOIN      = 0x1B;\n";
    ss << "    public static final int ENUMERATE = 0x1C;\n";
    ss << "    public static final int CHUNK     = 0x1D;\n";
    ss << "    public static final int SLIDE     = 0x1E;\n";
    ss << "    public static final int STRIDE    = 0x1F;\n";
    ss << "    public static final int ADJACENT  = 0x20;\n";
    ss << "    public static final int EXEC      = 0x21;\n";
    ss << "}\n\n";

    // ── 比较运算符 ──
    ss << "// 比较运算符\n";
    ss << "class Cmp {\n";
    ss << "    public static final int EQ = 0;\n";
    ss << "    public static final int NE = 1;\n";
    ss << "    public static final int LT = 2;\n";
    ss << "    public static final int GT = 3;\n";
    ss << "    public static final int LE = 4;\n";
    ss << "    public static final int GE = 5;\n";
    ss << "}\n\n";

    ss << "public class SPOI {\n";
    ss << "    public static final int PATH_DEREF = 0xFFFF;\n\n";

    // ── 类型成员常量 ──
    ss << "    // 类型成员索引常量\n";
    for (auto& t : types) {
        ss << "    // " << t.className << "\n";
        for (auto& f : t.fields) {
            ss << "    public static final int " << t.className << "_" << f.name << " = " << f.index << ";\n";
        }
        ss << "\n";
    }

    // ── Varint 编码 ──
    ss << "    // Varint 编码\n";
    ss << "    private static void writeVarint(ArrayList<Byte> buf, int v) {\n";
    ss << "        while (v >= 0x80) {\n";
    ss << "            buf.add((byte)((v & 0x7F) | 0x80));\n";
    ss << "            v >>>= 7;\n";
    ss << "        }\n";
    ss << "        buf.add((byte)(v & 0x7F));\n";
    ss << "    }\n\n";
    ss << "    private static void writeU32(ArrayList<Byte> buf, int v) {\n";
    ss << "        buf.add((byte)(v & 0xFF));\n";
    ss << "        buf.add((byte)((v >>> 8) & 0xFF));\n";
    ss << "        buf.add((byte)((v >>> 16) & 0xFF));\n";
    ss << "        buf.add((byte)((v >>> 24) & 0xFF));\n";
    ss << "    }\n\n";

    auto arrayListToBytes = [](const std::string& varName) -> std::string {
        return
            "        byte[] result = new byte[" + varName + ".size()];\n"
            "        for (int i = 0; i < " + varName + ".size(); i++) result[i] = " + varName + ".get(i);\n";
    };

    // ── SpoiInstruction ──
    ss << "    // SpoiInstruction\n";
    ss << "    public static class SpoiInstruction {\n";
    ss << "        public int op;\n";
    ss << "        public int[] path;\n";
    ss << "        public byte[] operand;\n\n";
    ss << "        public SpoiInstruction(int op, int[] path, byte[] operand) {\n";
    ss << "            this.op = op;\n";
    ss << "            this.path = path;\n";
    ss << "            this.operand = operand != null ? operand : new byte[0];\n";
    ss << "        }\n\n";
    ss << "        public byte[] serialize() {\n";
    ss << "            ArrayList<Byte> buf = new ArrayList<>();\n";
    ss << "            buf.add((byte)op);\n";
    ss << "            writeVarint(buf, path.length);\n";
    ss << "            for (int seg : path) writeU32(buf, seg);\n";
    ss << "            writeVarint(buf, operand.length);\n";
    ss << "            for (byte b : operand) buf.add(b);\n";
    ss << "            " << arrayListToBytes("buf") << "            return result;\n";
    ss << "        }\n";
    ss << "    }\n\n";

    // ── SpoiStream ──
    ss << "    // SpoiStream\n";
    ss << "    public static class SpoiStream {\n";
    ss << "        public ArrayList<SpoiInstruction> instructions = new ArrayList<>();\n\n";
    ss << "        public byte[] build() {\n";
    ss << "            ArrayList<Byte> buf = new ArrayList<>();\n";
    ss << "            writeVarint(buf, instructions.size());\n";
    ss << "            for (SpoiInstruction inst : instructions) {\n";
    ss << "                for (byte b : inst.serialize()) buf.add(b);\n";
    ss << "            }\n";
    ss << "            " << arrayListToBytes("buf") << "            return result;\n";
    ss << "        }\n\n";
    ss << "        public String buildHex() {\n";
    ss << "            StringBuilder sb = new StringBuilder();\n";
    ss << "            Formatter fmt = new Formatter(sb);\n";
    ss << "            for (byte b : build()) fmt.format(\"%02x\", b);\n";
    ss << "            fmt.close();\n";
    ss << "            return sb.toString();\n";
    ss << "        }\n";
    ss << "    }\n\n";

    // ── SpoiUpdate ──
    ss << "    // SpoiUpdate — 写操作 Builder\n";
    ss << "    public static class SpoiUpdate {\n";
    ss << "        private SpoiStream stream = new SpoiStream();\n\n";
    ss << "        public SpoiUpdate set(int[] path, byte[] value) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.SET, path, value));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiUpdate setI32(int[] path, int value) {\n";
    ss << "            return set(path, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(value).array());\n";
    ss << "        }\n\n";
    ss << "        public SpoiUpdate setU32(int[] path, int value) {\n";
    ss << "            return set(path, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(value).array());\n";
    ss << "        }\n\n";
    ss << "        public SpoiUpdate setF64(int[] path, double value) {\n";
    ss << "            return set(path, ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putDouble(value).array());\n";
    ss << "        }\n\n";
    ss << "        public SpoiUpdate setStr(int[] path, String value) {\n";
    ss << "            byte[] data = value.getBytes(StandardCharsets.UTF_8);\n";
    ss << "            ArrayList<Byte> buf = new ArrayList<>();\n";
    ss << "            writeVarint(buf, data.length);\n";
    ss << "            for (byte b : data) buf.add(b);\n";
    ss << "            " << arrayListToBytes("buf") << "            return set(path, result);\n";
    ss << "        }\n\n";
    ss << "        public SpoiUpdate setBool(int[] path, boolean value) {\n";
    ss << "            return set(path, new byte[]{(byte)(value ? 1 : 0)});\n";
    ss << "        }\n\n";
    ss << "        public SpoiUpdate addI32(int[] path, int delta) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.ADD, path,\n";
    ss << "                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(delta).array()));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiUpdate add(int[] path, byte[] value) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.ADD, path, value));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiUpdate append(int[] path, byte[] value) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.APPEND, path, value));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiUpdate remove(int[] path, byte[] value) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.REMOVE, path, value));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiUpdate insert(int[] path, byte[] value) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.INSERT, path, value));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiUpdate replace(int[] path, byte[] value) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.REPLACE, path, value));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiUpdate reset(int[] path) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.RESET, path, new byte[0]));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiUpdate setnull(int[] path) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.SETNULL, path, new byte[0]));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public byte[] build() { return stream.build(); }\n";
    ss << "        public String buildHex() { return stream.buildHex(); }\n";
    ss << "    }\n\n";

    // ── SpoiQuery ──
    ss << "    // SpoiQuery — 查询 Builder\n";
    ss << "    public static class SpoiQuery {\n";
    ss << "        private SpoiStream stream = new SpoiStream();\n\n";
    ss << "        public SpoiQuery nav(int field) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.FILTER, new int[]{field}, new byte[0]));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery filter(int field, int cmpOp, byte[] value) {\n";
    ss << "            ArrayList<Byte> buf = new ArrayList<>();\n";
    ss << "            writeU32(buf, field);\n";
    ss << "            buf.add((byte)cmpOp);\n";
    ss << "            writeVarint(buf, value.length);\n";
    ss << "            for (byte b : value) buf.add(b);\n";
    ss << "            " << arrayListToBytes("buf") << "            stream.instructions.add(new SpoiInstruction(Op.FILTER, new int[0], result));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery filterI32(int field, int cmpOp, int value) {\n";
    ss << "            return filter(field, cmpOp, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(value).array());\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery filterStr(int field, int cmpOp, String value) {\n";
    ss << "            byte[] data = value.getBytes(StandardCharsets.UTF_8);\n";
    ss << "            ArrayList<Byte> buf = new ArrayList<>();\n";
    ss << "            writeVarint(buf, data.length);\n";
    ss << "            for (byte b : data) buf.add(b);\n";
    ss << "            " << arrayListToBytes("buf") << "            return filter(field, cmpOp, result);\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery select(int... fields) {\n";
    ss << "            ArrayList<Byte> buf = new ArrayList<>();\n";
    ss << "            writeU32(buf, fields.length);\n";
    ss << "            for (int f : fields) writeU32(buf, f);\n";
    ss << "            " << arrayListToBytes("buf") << "            stream.instructions.add(new SpoiInstruction(Op.SELECT, new int[0], result));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery sort(int field, boolean ascending) {\n";
    ss << "            ArrayList<Byte> buf = new ArrayList<>();\n";
    ss << "            writeU32(buf, field);\n";
    ss << "            buf.add((byte)(ascending ? 1 : 0));\n";
    ss << "            " << arrayListToBytes("buf") << "            stream.instructions.add(new SpoiInstruction(Op.SORT, new int[0], result));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery reverse() {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.REVERSE, new int[0], new byte[0]));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery take(int count) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.TAKE, new int[0],\n";
    ss << "                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(count).array()));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery drop(int count) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.DROP, new int[0],\n";
    ss << "                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(count).array()));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery distinct() {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.DISTINCT, new int[0], new byte[0]));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery count() {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.COUNT, new int[0], new byte[0]));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery keys() {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.KEYS, new int[0], new byte[0]));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery values() {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.VALUES, new int[0], new byte[0]));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery join(int field) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.JOIN, new int[0],\n";
    ss << "                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(field).array()));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery enumerate(int start) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.ENUMERATE, new int[0],\n";
    ss << "                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(start).array()));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery chunk(int size) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.CHUNK, new int[0],\n";
    ss << "                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(size).array()));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery stride(int step) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.STRIDE, new int[0],\n";
    ss << "                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(step).array()));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery takewhile(int field, int cmpOp, byte[] value) {\n";
    ss << "            ArrayList<Byte> buf = new ArrayList<>();\n";
    ss << "            writeU32(buf, field);\n";
    ss << "            buf.add((byte)cmpOp);\n";
    ss << "            writeVarint(buf, value.length);\n";
    ss << "            for (byte b : value) buf.add(b);\n";
    ss << "            " << arrayListToBytes("buf") << "            stream.instructions.add(new SpoiInstruction(Op.TAKEWHILE, new int[0], result));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery dropwhile(int field, int cmpOp, byte[] value) {\n";
    ss << "            ArrayList<Byte> buf = new ArrayList<>();\n";
    ss << "            writeU32(buf, field);\n";
    ss << "            buf.add((byte)cmpOp);\n";
    ss << "            writeVarint(buf, value.length);\n";
    ss << "            for (byte b : value) buf.add(b);\n";
    ss << "            " << arrayListToBytes("buf") << "            stream.instructions.add(new SpoiInstruction(Op.DROPWHILE, new int[0], result));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery any(int field, int cmpOp, byte[] value) {\n";
    ss << "            ArrayList<Byte> buf = new ArrayList<>();\n";
    ss << "            writeU32(buf, field);\n";
    ss << "            buf.add((byte)cmpOp);\n";
    ss << "            writeVarint(buf, value.length);\n";
    ss << "            for (byte b : value) buf.add(b);\n";
    ss << "            " << arrayListToBytes("buf") << "            stream.instructions.add(new SpoiInstruction(Op.ANY, new int[0], result));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery all(int field, int cmpOp, byte[] value) {\n";
    ss << "            ArrayList<Byte> buf = new ArrayList<>();\n";
    ss << "            writeU32(buf, field);\n";
    ss << "            buf.add((byte)cmpOp);\n";
    ss << "            writeVarint(buf, value.length);\n";
    ss << "            for (byte b : value) buf.add(b);\n";
    ss << "            " << arrayListToBytes("buf") << "            stream.instructions.add(new SpoiInstruction(Op.ALL, new int[0], result));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery find(int field, int cmpOp, byte[] value) {\n";
    ss << "            ArrayList<Byte> buf = new ArrayList<>();\n";
    ss << "            writeU32(buf, field);\n";
    ss << "            buf.add((byte)cmpOp);\n";
    ss << "            writeVarint(buf, value.length);\n";
    ss << "            for (byte b : value) buf.add(b);\n";
    ss << "            " << arrayListToBytes("buf") << "            stream.instructions.add(new SpoiInstruction(Op.FIND, new int[0], result));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery slide(int size) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.SLIDE, new int[0],\n";
    ss << "                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(size).array()));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public SpoiQuery adjacent(int n) {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.ADJACENT, new int[0],\n";
    ss << "                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(n).array()));\n";
    ss << "            return this;\n";
    ss << "        }\n\n";
    ss << "        public byte[] build() {\n";
    ss << "            stream.instructions.add(new SpoiInstruction(Op.EXEC, new int[0], new byte[0]));\n";
    ss << "            return stream.build();\n";
    ss << "        }\n";
    ss << "    }\n";
    ss << "}\n";

    return ss.str();
}

int generate_spoi_java(const std::string& output_path, const std::string& meta_path) {
    try {
        auto meta = sp_meta::readMetaFile(meta_path);
        auto types = extractSpoiTypes(meta);
        auto code = generateJavaSpoi(types);

        std::ofstream out(output_path);
        if (!out.is_open()) throw std::runtime_error("Cannot open: " + output_path);
        out << code;
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "SPOI Java generator error: " << e.what() << "\n";
        return 1;
    }
}