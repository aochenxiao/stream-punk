// ============================================================
// SPOI — StreamPunk Operation Instruction
// Java 查询/更新 Builder（自动生成）
// ============================================================

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Formatter;

// 操作码
class Op {
    public static final int SET       = 0x04;
    public static final int ADD       = 0x05;
    public static final int APPEND    = 0x06;
    public static final int REMOVE    = 0x07;
    public static final int INSERT    = 0x08;
    public static final int REPLACE   = 0x09;
    public static final int RESET     = 0x0A;
    public static final int SETNULL   = 0x0B;
    public static final int FILTER    = 0x0C;
    public static final int SELECT    = 0x0D;
    public static final int SORT      = 0x0E;
    public static final int REVERSE   = 0x0F;
    public static final int TAKE      = 0x10;
    public static final int DROP      = 0x11;
    public static final int TAKEWHILE = 0x12;
    public static final int DROPWHILE = 0x13;
    public static final int DISTINCT  = 0x14;
    public static final int COUNT     = 0x15;
    public static final int ANY       = 0x16;
    public static final int ALL       = 0x17;
    public static final int FIND      = 0x18;
    public static final int KEYS      = 0x19;
    public static final int VALUES    = 0x1A;
    public static final int JOIN      = 0x1B;
    public static final int ENUMERATE = 0x1C;
    public static final int CHUNK     = 0x1D;
    public static final int SLIDE     = 0x1E;
    public static final int STRIDE    = 0x1F;
    public static final int ADJACENT  = 0x20;
    public static final int EXEC      = 0x21;
}

// 比较运算符
class Cmp {
    public static final int EQ = 0;
    public static final int NE = 1;
    public static final int LT = 2;
    public static final int GT = 3;
    public static final int LE = 4;
    public static final int GE = 5;
}

public class SPOI {
    public static final int PATH_DEREF = 0xFFFF;

    // 类型成员索引常量
    // SpoiTestPlayer
    public static final int SpoiTestPlayer_name = 0;
    public static final int SpoiTestPlayer_hp = 1;
    public static final int SpoiTestPlayer_level = 2;
    public static final int SpoiTestPlayer_posX = 3;

    // SpoiTestState
    public static final int SpoiTestState_tick = 0;
    public static final int SpoiTestState_currentMap = 1;
    public static final int SpoiTestState_players = 2;

    // SpoiItem
    public static final int SpoiItem_name = 0;
    public static final int SpoiItem_value = 1;

    // SpoiInventory
    public static final int SpoiInventory_items = 0;
    public static final int SpoiInventory_equipped = 1;
    public static final int SpoiInventory_gold = 2;

    // SpoiCharacter
    public static final int SpoiCharacter_name = 0;
    public static final int SpoiCharacter_hp = 1;
    public static final int SpoiCharacter_inventory = 2;
    public static final int SpoiCharacter_weapon = 3;
    public static final int SpoiCharacter_petLevel = 4;

    // SpoiWorld
    public static final int SpoiWorld_worldName = 0;
    public static final int SpoiWorld_tick = 1;
    public static final int SpoiWorld_characters = 2;

    // Varint 编码
    private static void writeVarint(ArrayList<Byte> buf, int v) {
        while (v >= 0x80) {
            buf.add((byte)((v & 0x7F) | 0x80));
            v >>>= 7;
        }
        buf.add((byte)(v & 0x7F));
    }

    private static void writeU32(ArrayList<Byte> buf, int v) {
        buf.add((byte)(v & 0xFF));
        buf.add((byte)((v >>> 8) & 0xFF));
        buf.add((byte)((v >>> 16) & 0xFF));
        buf.add((byte)((v >>> 24) & 0xFF));
    }

    // SpoiInstruction
    public static class SpoiInstruction {
        public int op;
        public int[] path;
        public byte[] operand;

        public SpoiInstruction(int op, int[] path, byte[] operand) {
            this.op = op;
            this.path = path;
            this.operand = operand != null ? operand : new byte[0];
        }

        public byte[] serialize() {
            ArrayList<Byte> buf = new ArrayList<>();
            buf.add((byte)op);
            writeVarint(buf, path.length);
            for (int seg : path) writeU32(buf, seg);
            writeVarint(buf, operand.length);
            for (byte b : operand) buf.add(b);
                    byte[] result = new byte[buf.size()];
        for (int i = 0; i < buf.size(); i++) result[i] = buf.get(i);
            return result;
        }
    }

    // SpoiStream
    public static class SpoiStream {
        public ArrayList<SpoiInstruction> instructions = new ArrayList<>();

        public byte[] build() {
            ArrayList<Byte> buf = new ArrayList<>();
            writeVarint(buf, instructions.size());
            for (SpoiInstruction inst : instructions) {
                for (byte b : inst.serialize()) buf.add(b);
            }
                    byte[] result = new byte[buf.size()];
        for (int i = 0; i < buf.size(); i++) result[i] = buf.get(i);
            return result;
        }

        public String buildHex() {
            StringBuilder sb = new StringBuilder();
            Formatter fmt = new Formatter(sb);
            for (byte b : build()) fmt.format("%02x", b);
            fmt.close();
            return sb.toString();
        }
    }

    // SpoiUpdate — 写操作 Builder
    public static class SpoiUpdate {
        private SpoiStream stream = new SpoiStream();

        public SpoiUpdate set(int[] path, byte[] value) {
            stream.instructions.add(new SpoiInstruction(Op.SET, path, value));
            return this;
        }

        public SpoiUpdate setI32(int[] path, int value) {
            return set(path, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(value).array());
        }

        public SpoiUpdate setU32(int[] path, int value) {
            return set(path, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(value).array());
        }

        public SpoiUpdate setF64(int[] path, double value) {
            return set(path, ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putDouble(value).array());
        }

        public SpoiUpdate setStr(int[] path, String value) {
            byte[] data = value.getBytes(StandardCharsets.UTF_8);
            ArrayList<Byte> buf = new ArrayList<>();
            writeVarint(buf, data.length);
            for (byte b : data) buf.add(b);
                    byte[] result = new byte[buf.size()];
        for (int i = 0; i < buf.size(); i++) result[i] = buf.get(i);
            return set(path, result);
        }

        public SpoiUpdate setBool(int[] path, boolean value) {
            return set(path, new byte[]{(byte)(value ? 1 : 0)});
        }

        public SpoiUpdate addI32(int[] path, int delta) {
            stream.instructions.add(new SpoiInstruction(Op.ADD, path,
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(delta).array()));
            return this;
        }

        public SpoiUpdate add(int[] path, byte[] value) {
            stream.instructions.add(new SpoiInstruction(Op.ADD, path, value));
            return this;
        }

        public SpoiUpdate append(int[] path, byte[] value) {
            stream.instructions.add(new SpoiInstruction(Op.APPEND, path, value));
            return this;
        }

        public SpoiUpdate remove(int[] path, byte[] value) {
            stream.instructions.add(new SpoiInstruction(Op.REMOVE, path, value));
            return this;
        }

        public SpoiUpdate insert(int[] path, byte[] value) {
            stream.instructions.add(new SpoiInstruction(Op.INSERT, path, value));
            return this;
        }

        public SpoiUpdate replace(int[] path, byte[] value) {
            stream.instructions.add(new SpoiInstruction(Op.REPLACE, path, value));
            return this;
        }

        public SpoiUpdate reset(int[] path) {
            stream.instructions.add(new SpoiInstruction(Op.RESET, path, new byte[0]));
            return this;
        }

        public SpoiUpdate setnull(int[] path) {
            stream.instructions.add(new SpoiInstruction(Op.SETNULL, path, new byte[0]));
            return this;
        }

        public byte[] build() { return stream.build(); }
        public String buildHex() { return stream.buildHex(); }
    }

    // SpoiQuery — 查询 Builder
    public static class SpoiQuery {
        private SpoiStream stream = new SpoiStream();

        public SpoiQuery nav(int field) {
            stream.instructions.add(new SpoiInstruction(Op.FILTER, new int[]{field}, new byte[0]));
            return this;
        }

        public SpoiQuery filter(int field, int cmpOp, byte[] value) {
            ArrayList<Byte> buf = new ArrayList<>();
            writeU32(buf, field);
            buf.add((byte)cmpOp);
            writeVarint(buf, value.length);
            for (byte b : value) buf.add(b);
                    byte[] result = new byte[buf.size()];
        for (int i = 0; i < buf.size(); i++) result[i] = buf.get(i);
            stream.instructions.add(new SpoiInstruction(Op.FILTER, new int[0], result));
            return this;
        }

        public SpoiQuery filterI32(int field, int cmpOp, int value) {
            return filter(field, cmpOp, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(value).array());
        }

        public SpoiQuery filterStr(int field, int cmpOp, String value) {
            byte[] data = value.getBytes(StandardCharsets.UTF_8);
            ArrayList<Byte> buf = new ArrayList<>();
            writeVarint(buf, data.length);
            for (byte b : data) buf.add(b);
                    byte[] result = new byte[buf.size()];
        for (int i = 0; i < buf.size(); i++) result[i] = buf.get(i);
            return filter(field, cmpOp, result);
        }

        public SpoiQuery select(int... fields) {
            ArrayList<Byte> buf = new ArrayList<>();
            writeU32(buf, fields.length);
            for (int f : fields) writeU32(buf, f);
                    byte[] result = new byte[buf.size()];
        for (int i = 0; i < buf.size(); i++) result[i] = buf.get(i);
            stream.instructions.add(new SpoiInstruction(Op.SELECT, new int[0], result));
            return this;
        }

        public SpoiQuery sort(int field, boolean ascending) {
            ArrayList<Byte> buf = new ArrayList<>();
            writeU32(buf, field);
            buf.add((byte)(ascending ? 1 : 0));
                    byte[] result = new byte[buf.size()];
        for (int i = 0; i < buf.size(); i++) result[i] = buf.get(i);
            stream.instructions.add(new SpoiInstruction(Op.SORT, new int[0], result));
            return this;
        }

        public SpoiQuery reverse() {
            stream.instructions.add(new SpoiInstruction(Op.REVERSE, new int[0], new byte[0]));
            return this;
        }

        public SpoiQuery take(int count) {
            stream.instructions.add(new SpoiInstruction(Op.TAKE, new int[0],
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(count).array()));
            return this;
        }

        public SpoiQuery drop(int count) {
            stream.instructions.add(new SpoiInstruction(Op.DROP, new int[0],
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(count).array()));
            return this;
        }

        public SpoiQuery distinct() {
            stream.instructions.add(new SpoiInstruction(Op.DISTINCT, new int[0], new byte[0]));
            return this;
        }

        public SpoiQuery count() {
            stream.instructions.add(new SpoiInstruction(Op.COUNT, new int[0], new byte[0]));
            return this;
        }

        public SpoiQuery keys() {
            stream.instructions.add(new SpoiInstruction(Op.KEYS, new int[0], new byte[0]));
            return this;
        }

        public SpoiQuery values() {
            stream.instructions.add(new SpoiInstruction(Op.VALUES, new int[0], new byte[0]));
            return this;
        }

        public SpoiQuery join(int field) {
            stream.instructions.add(new SpoiInstruction(Op.JOIN, new int[0],
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(field).array()));
            return this;
        }

        public SpoiQuery enumerate(int start) {
            stream.instructions.add(new SpoiInstruction(Op.ENUMERATE, new int[0],
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(start).array()));
            return this;
        }

        public SpoiQuery chunk(int size) {
            stream.instructions.add(new SpoiInstruction(Op.CHUNK, new int[0],
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(size).array()));
            return this;
        }

        public SpoiQuery stride(int step) {
            stream.instructions.add(new SpoiInstruction(Op.STRIDE, new int[0],
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(step).array()));
            return this;
        }

        public SpoiQuery takewhile(int field, int cmpOp, byte[] value) {
            ArrayList<Byte> buf = new ArrayList<>();
            writeU32(buf, field);
            buf.add((byte)cmpOp);
            writeVarint(buf, value.length);
            for (byte b : value) buf.add(b);
                    byte[] result = new byte[buf.size()];
        for (int i = 0; i < buf.size(); i++) result[i] = buf.get(i);
            stream.instructions.add(new SpoiInstruction(Op.TAKEWHILE, new int[0], result));
            return this;
        }

        public SpoiQuery dropwhile(int field, int cmpOp, byte[] value) {
            ArrayList<Byte> buf = new ArrayList<>();
            writeU32(buf, field);
            buf.add((byte)cmpOp);
            writeVarint(buf, value.length);
            for (byte b : value) buf.add(b);
                    byte[] result = new byte[buf.size()];
        for (int i = 0; i < buf.size(); i++) result[i] = buf.get(i);
            stream.instructions.add(new SpoiInstruction(Op.DROPWHILE, new int[0], result));
            return this;
        }

        public SpoiQuery any(int field, int cmpOp, byte[] value) {
            ArrayList<Byte> buf = new ArrayList<>();
            writeU32(buf, field);
            buf.add((byte)cmpOp);
            writeVarint(buf, value.length);
            for (byte b : value) buf.add(b);
                    byte[] result = new byte[buf.size()];
        for (int i = 0; i < buf.size(); i++) result[i] = buf.get(i);
            stream.instructions.add(new SpoiInstruction(Op.ANY, new int[0], result));
            return this;
        }

        public SpoiQuery all(int field, int cmpOp, byte[] value) {
            ArrayList<Byte> buf = new ArrayList<>();
            writeU32(buf, field);
            buf.add((byte)cmpOp);
            writeVarint(buf, value.length);
            for (byte b : value) buf.add(b);
                    byte[] result = new byte[buf.size()];
        for (int i = 0; i < buf.size(); i++) result[i] = buf.get(i);
            stream.instructions.add(new SpoiInstruction(Op.ALL, new int[0], result));
            return this;
        }

        public SpoiQuery find(int field, int cmpOp, byte[] value) {
            ArrayList<Byte> buf = new ArrayList<>();
            writeU32(buf, field);
            buf.add((byte)cmpOp);
            writeVarint(buf, value.length);
            for (byte b : value) buf.add(b);
                    byte[] result = new byte[buf.size()];
        for (int i = 0; i < buf.size(); i++) result[i] = buf.get(i);
            stream.instructions.add(new SpoiInstruction(Op.FIND, new int[0], result));
            return this;
        }

        public SpoiQuery slide(int size) {
            stream.instructions.add(new SpoiInstruction(Op.SLIDE, new int[0],
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(size).array()));
            return this;
        }

        public SpoiQuery adjacent(int n) {
            stream.instructions.add(new SpoiInstruction(Op.ADJACENT, new int[0],
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(n).array()));
            return this;
        }

        public byte[] build() {
            stream.instructions.add(new SpoiInstruction(Op.EXEC, new int[0], new byte[0]));
            return stream.build();
        }
    }
}
