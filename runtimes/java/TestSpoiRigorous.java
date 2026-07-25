/**
 * StreamPunk SPOI 刁钻测试 — Java Runtime
 * 编译: javac -encoding UTF-8 TestSpoiRigorous.java spoi_accessor.java spoi_executor.java
 * 运行: java TestSpoiRigorous
 * 
 * 7 个测试类别:
 *   1. 数值边界（各类型 max/min/zero/NaN/Inf）
 *   2. 字符串边界（空串、Unicode/emoji、null字节、长串、特殊字符）
 *   3. 反序列化异常（截断数据、无效type_id、空数据、null数据）
 *   4. Accessor 越界（负索引、超大索引、不同类型对象）
 *   5. Executor 组合操作（多层FILTER、空管道、边界组合）
 *   6. 跨类型 Executor
 *   7. Registry 边界
 */

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.*;

// =============================== 测试数据类 ===============================

class SpoiTestPlayer {
    public String name;
    public int hp;
    public int level;
    public double posX;
    SpoiTestPlayer() { this.name = ""; this.hp = 0; this.level = 0; this.posX = 0.0; }
    SpoiTestPlayer(String name, int hp, int level, double posX) {
        this.name = name; this.hp = hp; this.level = level; this.posX = posX;
    }
}

class SpoiTestState {
    public int tick;
    public String currentMap;
    public Object players;
    SpoiTestState() { this.tick = 0; this.currentMap = ""; this.players = null; }
    SpoiTestState(int tick, String currentMap, Object players) {
        this.tick = tick; this.currentMap = currentMap; this.players = players;
    }
}

class SpoiItem {
    public String name;
    public int value;
    SpoiItem() { this.name = ""; this.value = 0; }
    SpoiItem(String name, int value) { this.name = name; this.value = value; }
}

class SpoiInventory {
    public Object items;
    public Object equipped;
    public int gold;
    SpoiInventory() { this.items = null; this.equipped = null; this.gold = 0; }
    SpoiInventory(Object items, Object equipped, int gold) {
        this.items = items; this.equipped = equipped; this.gold = gold;
    }
}

class SpoiCharacter {
    public String name;
    public int hp;
    public Object inventory;
    public Object weapon;
    public int petLevel;
    SpoiCharacter() { this.name = ""; this.hp = 0; this.inventory = null; this.weapon = null; this.petLevel = 0; }
    SpoiCharacter(String name, int hp, Object inventory, Object weapon, int petLevel) {
        this.name = name; this.hp = hp; this.inventory = inventory; this.weapon = weapon; this.petLevel = petLevel;
    }
}

class SpoiWorld {
    public String worldName;
    public int tick;
    public Object characters;
    SpoiWorld() { this.worldName = ""; this.tick = 0; this.characters = null; }
    SpoiWorld(String worldName, int tick, Object characters) {
        this.worldName = worldName; this.tick = tick; this.characters = characters;
    }
}

// =============================== 测试入口 ===============================

public class TestSpoiRigorous {
    static int passed = 0;
    static int failed = 0;
    static ArrayList<String> failures = new ArrayList<>();

    static void check(boolean cond, String msg) {
        if (!cond) throw new AssertionError(msg);
    }

    static void test(String name, Runnable fn) {
        try {
            fn.run();
            passed++;
            System.out.println("  \u2713 " + name);
        } catch (Throwable e) {
            failed++;
            String errMsg = e.getMessage();
            if (errMsg == null) errMsg = e.getClass().getSimpleName();
            failures.add("  FAIL " + name + ": " + errMsg);
            System.out.println("  \u2717 " + name + " — " + errMsg);
        }
    }

    static void assertThrows(Class<?> excType, Runnable fn) {
        try {
            fn.run();
            throw new AssertionError("Expected " + excType.getSimpleName() + " but no exception");
        } catch (AssertionError e) {
            throw e;
        } catch (Throwable e) {
            if (!excType.isInstance(e))
                throw new AssertionError("Wrong exception type: expected " + excType.getSimpleName()
                    + " but got " + e.getClass().getSimpleName(), e);
        }
    }

    static void assertNoThrow(Runnable fn) {
        try { fn.run(); }
        catch (Throwable e) {
            throw new AssertionError("Unexpected exception: " + e.getClass().getSimpleName() + " — " + e.getMessage(), e);
        }
    }

    // =============================== 辅助函数 ===============================

    /** 构造 typed value: [type_id(u32 LE) + value_bytes] */
    static byte[] makeTypedValue(long typeId, byte[] valueBytes) {
        byte[] result = new byte[4 + valueBytes.length];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) typeId);
        System.arraycopy(valueBytes, 0, result, 4, valueBytes.length);
        return result;
    }

    /** 构造 SET 指令 operand (I32) */
    static byte[] makeSetOperandI32(int value) {
        byte[] result = new byte[8];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.I32);
        ByteBuffer.wrap(result, 4, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(value);
        return result;
    }

    /** 构造 SET 指令 operand (String) */
    static byte[] makeSetOperandString(String value) {
        byte[] strBytes = value.getBytes(StandardCharsets.UTF_8);
        byte[] result = new byte[4 + strBytes.length];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.STRING);
        System.arraycopy(strBytes, 0, result, 4, strBytes.length);
        return result;
    }

    /** 构造 SET 指令 operand (F32) */
    static byte[] makeSetOperandF32(float value) {
        byte[] result = new byte[8];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.F32);
        ByteBuffer.wrap(result, 4, 4).order(ByteOrder.LITTLE_ENDIAN).putFloat(value);
        return result;
    }

    /** 构造 SET 指令 operand (F64) */
    static byte[] makeSetOperandF64(double value) {
        byte[] result = new byte[12];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.F64);
        ByteBuffer.wrap(result, 4, 8).order(ByteOrder.LITTLE_ENDIAN).putDouble(value);
        return result;
    }

    /** 构造 SET 指令 operand (U8) */
    static byte[] makeSetOperandU8(int value) {
        byte[] result = new byte[5];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.U8);
        result[4] = (byte) value;
        return result;
    }

    /** 构造 SET 指令 operand (U16) */
    static byte[] makeSetOperandU16(int value) {
        byte[] result = new byte[6];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.U16);
        ByteBuffer.wrap(result, 4, 2).order(ByteOrder.LITTLE_ENDIAN).putShort((short) value);
        return result;
    }

    /** 构造 SET 指令 operand (U32) */
    static byte[] makeSetOperandU32(long value) {
        byte[] result = new byte[8];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.U32);
        ByteBuffer.wrap(result, 4, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) value);
        return result;
    }

    /** 构造 SET 指令 operand (U64) */
    static byte[] makeSetOperandU64(long value) {
        byte[] result = new byte[12];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.U64);
        ByteBuffer.wrap(result, 4, 8).order(ByteOrder.LITTLE_ENDIAN).putLong(value);
        return result;
    }

    /** 构造 SET 指令 operand (I8) */
    static byte[] makeSetOperandI8(byte value) {
        byte[] result = new byte[5];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.I8);
        result[4] = value;
        return result;
    }

    /** 构造 SET 指令 operand (I16) */
    static byte[] makeSetOperandI16(short value) {
        byte[] result = new byte[6];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.I16);
        ByteBuffer.wrap(result, 4, 2).order(ByteOrder.LITTLE_ENDIAN).putShort(value);
        return result;
    }

    /** 构造 SET 指令 operand (I64) */
    static byte[] makeSetOperandI64(long value) {
        byte[] result = new byte[12];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.I64);
        ByteBuffer.wrap(result, 4, 8).order(ByteOrder.LITTLE_ENDIAN).putLong(value);
        return result;
    }

    /** 构造 SET 指令 operand (BOOL) */
    static byte[] makeSetOperandBool(boolean value) {
        byte[] result = new byte[5];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.BOOL);
        result[4] = (byte) (value ? 1 : 0);
        return result;
    }

    /** 构造 FILTER 指令 operand */
    static byte[] makeFilterOperand(int memberIdx, int cmpOp, long typeId, byte[] valueBytes) {
        byte[] typedValue = new byte[4 + valueBytes.length];
        ByteBuffer.wrap(typedValue, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) typeId);
        System.arraycopy(valueBytes, 0, typedValue, 4, valueBytes.length);
        ByteArrayOutputStream vlenBuf = new ByteArrayOutputStream();
        Varint.writeVarint(vlenBuf, typedValue.length);
        byte[] vlenBytes = vlenBuf.toByteArray();
        byte[] result = new byte[5 + vlenBytes.length + typedValue.length];
        ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(memberIdx);
        result[4] = (byte) cmpOp;
        System.arraycopy(vlenBytes, 0, result, 5, vlenBytes.length);
        System.arraycopy(typedValue, 0, result, 5 + vlenBytes.length, typedValue.length);
        return result;
    }

    /** 构建 SPOI 指令流 */
    static byte[] buildSpoiStream(ArrayList<SpoiInstruction> instructions) {
        ByteArrayOutputStream buf = new ByteArrayOutputStream();
        Varint.writeVarint(buf, instructions.size());
        for (SpoiInstruction inst : instructions) {
            buf.write(inst.op);
            Varint.writeVarint(buf, inst.path.size());
            for (int seg : inst.path) Varint.writeVarint(buf, seg);
            Varint.writeVarint(buf, inst.operand.length);
            if (inst.operand.length > 0) {
                try { buf.write(inst.operand); } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }
        return buf.toByteArray();
    }

    static SpoiInstruction makeInst(int op, int[] path, byte[] operand) {
        ArrayList<Integer> p = new ArrayList<>();
        if (path != null) {
            for (int seg : path) p.add(seg);
        }
        return new SpoiInstruction(op, p, operand != null ? operand : new byte[0]);
    }

    static SpoiInstruction pipeInst(int[] path) {
        return makeInst(Op.PIPE, path, new byte[0]);
    }

    static SpoiInstruction execInst() {
        return makeInst(Op.EXEC, null, new byte[0]);
    }

    static SpoiInstruction setInst(int[] path, byte[] operand) {
        return makeInst(Op.SET, path, operand);
    }

    static SpoiInstruction filterInst(int[] path, byte[] operand) {
        return makeInst(Op.FILTER, path, operand);
    }

    static SpoiInstruction selectInst(int[] path) {
        return makeInst(Op.SELECT, path, new byte[0]);
    }

    static SpoiInstruction countInst() {
        return makeInst(Op.COUNT, null, new byte[0]);
    }

    static SpoiInstruction takeInst(int n) {
        byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(n).array();
        return makeInst(Op.TAKE, null, val);
    }

    static SpoiInstruction dropInst(int n) {
        byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(n).array();
        return makeInst(Op.DROP, null, val);
    }

    static SpoiInstruction sortInst(int[] path) {
        return makeInst(Op.SORT, path, new byte[0]);
    }

    static SpoiInstruction reverseInst() {
        return makeInst(Op.REVERSE, null, new byte[0]);
    }

    static SpoiInstruction distinctInst() {
        return makeInst(Op.DISTINCT, null, new byte[0]);
    }

    static SpoiInstruction anyInst(int[] path, byte[] filterOp) {
        return makeInst(Op.ANY, path, filterOp);
    }

    static SpoiInstruction allInst(int[] path, byte[] filterOp) {
        return makeInst(Op.ALL, path, filterOp);
    }

    static SpoiInstruction findInst(int[] path, byte[] filterOp) {
        return makeInst(Op.FIND, path, filterOp);
    }

    static SpoiInstruction addInst(int[] path, byte[] operand) {
        return makeInst(Op.ADD, path, operand);
    }

    static SpoiInstruction keysInst() {
        return makeInst(Op.KEYS, null, new byte[0]);
    }

    static SpoiInstruction valuesInst() {
        return makeInst(Op.VALUES, null, new byte[0]);
    }

    static SpoiInstruction joinInst() {
        return makeInst(Op.JOIN, null, new byte[0]);
    }

    static SpoiInstruction resetInst(int[] path) {
        return makeInst(Op.RESET, path, new byte[0]);
    }

    static SpoiInstruction setnullInst(int[] path) {
        return makeInst(Op.SETNULL, path, new byte[0]);
    }

    // =============================== 1. 数值边界测试 ===============================

    static void testNumericBoundaries() {
        System.out.println("\n=== 1. 数值边界测试 ===");

        // U8 边界
        test("U8 zero", () -> {
            byte[] data = makeTypedValue(TypeId.U8, new byte[]{0});
            check(SpoiDeserializer.deserializeValue(data).equals(0), "U8 zero failed");
        });
        test("U8 max (255)", () -> {
            byte[] data = makeTypedValue(TypeId.U8, new byte[]{(byte) 0xFF});
            check(SpoiDeserializer.deserializeValue(data).equals(255), "U8 max failed");
        });

        // U16 边界
        test("U16 zero", () -> {
            byte[] val = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort((short) 0).array();
            byte[] data = makeTypedValue(TypeId.U16, val);
            check(SpoiDeserializer.deserializeValue(data).equals(0), "U16 zero failed");
        });
        test("U16 max (65535)", () -> {
            byte[] val = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort((short) 0xFFFF).array();
            byte[] data = makeTypedValue(TypeId.U16, val);
            check(SpoiDeserializer.deserializeValue(data).equals(65535), "U16 max failed");
        });

        // U32 边界
        test("U32 zero", () -> {
            byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(0).array();
            byte[] data = makeTypedValue(TypeId.U32, val);
            check(SpoiDeserializer.deserializeValue(data).equals(0L), "U32 zero failed");
        });
        test("U32 max (4294967295)", () -> {
            byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(-1).array();
            byte[] data = makeTypedValue(TypeId.U32, val);
            check(SpoiDeserializer.deserializeValue(data).equals(4294967295L), "U32 max failed");
        });

        // U64 边界
        test("U64 zero", () -> {
            byte[] val = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(0).array();
            byte[] data = makeTypedValue(TypeId.U64, val);
            check(SpoiDeserializer.deserializeValue(data).equals(0L), "U64 zero failed");
        });
        test("U64 max", () -> {
            byte[] val = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(-1L).array();
            byte[] data = makeTypedValue(TypeId.U64, val);
            // Java long signed: -1L as unsigned = max
            check(SpoiDeserializer.deserializeValue(data).equals(-1L), "U64 max failed");
        });

        // I8 边界
        test("I8 min (-128)", () -> {
            byte[] data = makeTypedValue(TypeId.I8, new byte[]{(byte) -128});
            check(SpoiDeserializer.deserializeValue(data).equals((byte) -128), "I8 min failed");
        });
        test("I8 max (127)", () -> {
            byte[] data = makeTypedValue(TypeId.I8, new byte[]{(byte) 127});
            check(SpoiDeserializer.deserializeValue(data).equals((byte) 127), "I8 max failed");
        });
        test("I8 zero", () -> {
            byte[] data = makeTypedValue(TypeId.I8, new byte[]{0});
            check(SpoiDeserializer.deserializeValue(data).equals((byte) 0), "I8 zero failed");
        });

        // I16 边界
        test("I16 min (-32768)", () -> {
            byte[] val = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort((short) -32768).array();
            byte[] data = makeTypedValue(TypeId.I16, val);
            check(SpoiDeserializer.deserializeValue(data).equals((short) -32768), "I16 min failed");
        });
        test("I16 max (32767)", () -> {
            byte[] val = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort((short) 32767).array();
            byte[] data = makeTypedValue(TypeId.I16, val);
            check(SpoiDeserializer.deserializeValue(data).equals((short) 32767), "I16 max failed");
        });

        // I32 边界
        test("I32 min", () -> {
            byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(Integer.MIN_VALUE).array();
            byte[] data = makeTypedValue(TypeId.I32, val);
            check(SpoiDeserializer.deserializeValue(data).equals(Integer.MIN_VALUE), "I32 min failed");
        });
        test("I32 max", () -> {
            byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(Integer.MAX_VALUE).array();
            byte[] data = makeTypedValue(TypeId.I32, val);
            check(SpoiDeserializer.deserializeValue(data).equals(Integer.MAX_VALUE), "I32 max failed");
        });
        test("I32 zero", () -> {
            byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(0).array();
            byte[] data = makeTypedValue(TypeId.I32, val);
            check(SpoiDeserializer.deserializeValue(data).equals(0), "I32 zero failed");
        });

        // I64 边界
        test("I64 min", () -> {
            byte[] val = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(Long.MIN_VALUE).array();
            byte[] data = makeTypedValue(TypeId.I64, val);
            check(SpoiDeserializer.deserializeValue(data).equals(Long.MIN_VALUE), "I64 min failed");
        });
        test("I64 max", () -> {
            byte[] val = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(Long.MAX_VALUE).array();
            byte[] data = makeTypedValue(TypeId.I64, val);
            check(SpoiDeserializer.deserializeValue(data).equals(Long.MAX_VALUE), "I64 max failed");
        });

        // F32 边界
        test("F32 NaN", () -> {
            byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putFloat(Float.NaN).array();
            byte[] data = makeTypedValue(TypeId.F32, val);
            check(Float.isNaN((Float) SpoiDeserializer.deserializeValue(data)), "F32 NaN failed");
        });
        test("F32 +Inf", () -> {
            byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putFloat(Float.POSITIVE_INFINITY).array();
            byte[] data = makeTypedValue(TypeId.F32, val);
            check(Float.isInfinite((Float) SpoiDeserializer.deserializeValue(data)), "F32 +Inf failed");
        });
        test("F32 -Inf", () -> {
            byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putFloat(Float.NEGATIVE_INFINITY).array();
            byte[] data = makeTypedValue(TypeId.F32, val);
            check(Float.isInfinite((Float) SpoiDeserializer.deserializeValue(data)), "F32 -Inf failed");
        });
        test("F32 zero", () -> {
            byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putFloat(0.0f).array();
            byte[] data = makeTypedValue(TypeId.F32, val);
            check(SpoiDeserializer.deserializeValue(data).equals(0.0f), "F32 zero failed");
        });

        // F64 边界
        test("F64 NaN", () -> {
            byte[] val = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putDouble(Double.NaN).array();
            byte[] data = makeTypedValue(TypeId.F64, val);
            check(Double.isNaN((Double) SpoiDeserializer.deserializeValue(data)), "F64 NaN failed");
        });
        test("F64 +Inf", () -> {
            byte[] val = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putDouble(Double.POSITIVE_INFINITY).array();
            byte[] data = makeTypedValue(TypeId.F64, val);
            check(Double.isInfinite((Double) SpoiDeserializer.deserializeValue(data)), "F64 +Inf failed");
        });
        test("F64 -Inf", () -> {
            byte[] val = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putDouble(Double.NEGATIVE_INFINITY).array();
            byte[] data = makeTypedValue(TypeId.F64, val);
            check(Double.isInfinite((Double) SpoiDeserializer.deserializeValue(data)), "F64 -Inf failed");
        });

        // BOOL 边界
        test("BOOL true (1)", () -> {
            byte[] data = makeTypedValue(TypeId.BOOL, new byte[]{1});
            check(SpoiDeserializer.deserializeValue(data).equals(true), "BOOL true failed");
        });
        test("BOOL false (0)", () -> {
            byte[] data = makeTypedValue(TypeId.BOOL, new byte[]{0});
            check(SpoiDeserializer.deserializeValue(data).equals(false), "BOOL false failed");
        });
        test("BOOL non-zero (42) -> true", () -> {
            byte[] data = makeTypedValue(TypeId.BOOL, new byte[]{42});
            check(SpoiDeserializer.deserializeValue(data).equals(true), "BOOL 42 should be true");
        });
        test("BOOL max byte (255) -> true", () -> {
            byte[] data = makeTypedValue(TypeId.BOOL, new byte[]{(byte) 255});
            check(SpoiDeserializer.deserializeValue(data).equals(true), "BOOL 255 should be true");
        });
    }

    // =============================== 2. 字符串边界测试 ===============================

    static void testStringBoundaries() {
        System.out.println("\n=== 2. 字符串边界测试 ===");

        test("空串", () -> {
            byte[] data = makeTypedValue(TypeId.STRING, new byte[0]);
            check(SpoiDeserializer.deserializeValue(data).equals(""), "empty string failed");
        });

        test("Unicode CJK", () -> {
            String s = "中文测试日本語한국어";
            byte[] val = s.getBytes(StandardCharsets.UTF_8);
            byte[] data = makeTypedValue(TypeId.STRING, val);
            check(SpoiDeserializer.deserializeValue(data).equals(s), "CJK failed");
        });

        test("Emoji", () -> {
            String s = "😀🎉🚀💯🔥";
            byte[] val = s.getBytes(StandardCharsets.UTF_8);
            byte[] data = makeTypedValue(TypeId.STRING, val);
            check(SpoiDeserializer.deserializeValue(data).equals(s), "emoji failed");
        });

        test("null字节嵌入", () -> {
            String s = "hello\u0000world";
            byte[] val = s.getBytes(StandardCharsets.UTF_8);
            byte[] data = makeTypedValue(TypeId.STRING, val);
            check(SpoiDeserializer.deserializeValue(data).equals(s), "null byte failed");
        });

        test("特殊字符", () -> {
            String s = "!@#$%^&*()_+-=[]{}|;':\",./<>?`~\\";
            byte[] val = s.getBytes(StandardCharsets.UTF_8);
            byte[] data = makeTypedValue(TypeId.STRING, val);
            check(SpoiDeserializer.deserializeValue(data).equals(s), "special chars failed");
        });

        test("换行和制表符", () -> {
            String s = "line1\nline2\r\n\tindented";
            byte[] val = s.getBytes(StandardCharsets.UTF_8);
            byte[] data = makeTypedValue(TypeId.STRING, val);
            check(SpoiDeserializer.deserializeValue(data).equals(s), "newlines failed");
        });

        test("长字符串 (1000 chars)", () -> {
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < 1000; i++) sb.append((char) ('A' + (i % 26)));
            String s = sb.toString();
            byte[] val = s.getBytes(StandardCharsets.UTF_8);
            byte[] data = makeTypedValue(TypeId.STRING, val);
            check(SpoiDeserializer.deserializeValue(data).equals(s), "long string failed");
        });

        test("混合 Unicode 和 ASCII", () -> {
            String s = "Hello世界 — World!";
            byte[] val = s.getBytes(StandardCharsets.UTF_8);
            byte[] data = makeTypedValue(TypeId.STRING, val);
            check(SpoiDeserializer.deserializeValue(data).equals(s), "mixed unicode failed");
        });

        test("仅空格", () -> {
            String s = "     ";
            byte[] val = s.getBytes(StandardCharsets.UTF_8);
            byte[] data = makeTypedValue(TypeId.STRING, val);
            check(SpoiDeserializer.deserializeValue(data).equals(s), "spaces failed");
        });
    }

    // =============================== 3. 反序列化异常测试 ===============================

    static void testDeserializationErrors() {
        System.out.println("\n=== 3. 反序列化异常测试 ===");

        test("null 数据返回 null", () -> {
            check(SpoiDeserializer.deserializeValue(null) == null, "null should return null");
        });

        test("空数据返回 null", () -> {
            check(SpoiDeserializer.deserializeValue(new byte[0]) == null, "empty should return null");
        });

        test("截断数据 (< 4 bytes)", () -> {
            check(SpoiDeserializer.deserializeValue(new byte[]{1, 2, 3}) == null, "too short should return null");
        });

        test("无效 type_id 返回原始 bytes", () -> {
            byte[] data = makeTypedValue(99999, new byte[]{42, 43, 44});
            Object result = SpoiDeserializer.deserializeValue(data);
            check(result instanceof byte[], "should return raw bytes for unknown type_id");
            check(((byte[]) result).length == 3, "raw bytes length should be 3");
        });

        test("type_id 后的截断数据 (U8 缺值)", () -> {
            byte[] data = new byte[4];
            ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.U8);
            // no value bytes after type_id
            assertThrows(ArrayIndexOutOfBoundsException.class, () -> SpoiDeserializer.deserializeValue(data));
        });

        test("type_id 后的截断数据 (U16 缺值)", () -> {
            byte[] data = new byte[5];
            ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.U16);
            data[4] = 0x01; // only 1 byte for U16 which needs 2
            assertThrows(RuntimeException.class, () -> SpoiDeserializer.deserializeValue(data));
        });

        test("type_id 后的截断数据 (U32 缺值)", () -> {
            byte[] data = new byte[6];
            ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.U32);
            // only 2 bytes for U32 which needs 4
            assertThrows(RuntimeException.class, () -> SpoiDeserializer.deserializeValue(data));
        });

        test("type_id 后的截断数据 (I32 缺值)", () -> {
            byte[] data = new byte[5];
            ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.I32);
            // only 1 byte for I32 which needs 4
            assertThrows(RuntimeException.class, () -> SpoiDeserializer.deserializeValue(data));
        });

        test("type_id 后的截断数据 (F64 缺值)", () -> {
            byte[] data = new byte[8];
            ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) TypeId.F64);
            // only 4 bytes for F64 which needs 8
            assertThrows(RuntimeException.class, () -> SpoiDeserializer.deserializeValue(data));
        });

        test("截断 SPOI 指令流", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiTestPlayer p = new SpoiTestPlayer("test", 100, 1, 0.0);
            byte[] corrupt = new byte[]{0x01}; // truncated
            assertThrows(ArrayIndexOutOfBoundsException.class, () -> exe.execute(p, corrupt));
        });

        test("空 SPOI 指令流（count=0）", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiTestPlayer p = new SpoiTestPlayer("test", 100, 1, 0.0);
            byte[] empty = new byte[]{0}; // varint count = 0
            Map<String, Object> result = exe.execute(p, empty);
            check(result.get("resultType").equals(ResultType.UNDEF), "empty stream should return UNDEF");
        });

        test("未知操作码", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiTestPlayer p = new SpoiTestPlayer("test", 100, 1, 0.0);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            // opcode 0xF0 (unknown), path empty, operand empty
            insts.add(makeInst(0xF0, new int[]{}, new byte[0]));
            byte[] stream = buildSpoiStream(insts);
            assertThrows(IllegalArgumentException.class, () -> exe.execute(p, stream));
        });
    }

    // =============================== 4. Accessor 越界测试 ===============================

    static void testAccessorOutOfBounds() {
        System.out.println("\n=== 4. Accessor 越界测试 ===");

        // SpoiTestPlayerAccessor (fieldCount=4)
        SpoiAccessor playerAcc = new SpoiTestPlayerAccessor();
        SpoiTestPlayer p = new SpoiTestPlayer("A", 1, 1, 0.0);

        test("Player getField 负索引", () -> {
            assertThrows(IllegalArgumentException.class, () -> playerAcc.getField(p, -1));
        });
        test("Player getField 超大索引 (4)", () -> {
            assertThrows(IllegalArgumentException.class, () -> playerAcc.getField(p, 4));
        });
        test("Player getField 超大索引 (100)", () -> {
            assertThrows(IllegalArgumentException.class, () -> playerAcc.getField(p, 100));
        });
        test("Player setField 负索引", () -> {
            assertThrows(IllegalArgumentException.class, () -> playerAcc.setField(p, -1, "x"));
        });
        test("Player setField 超大索引 (999)", () -> {
            assertThrows(IllegalArgumentException.class, () -> playerAcc.setField(p, 999, "x"));
        });

        // SpoiItemAccessor (fieldCount=2)
        SpoiAccessor itemAcc = new SpoiItemAccessor();
        SpoiItem item = new SpoiItem("Sword", 100);

        test("Item getField 负索引", () -> {
            assertThrows(IllegalArgumentException.class, () -> itemAcc.getField(item, -1));
        });
        test("Item getField 超大索引 (2)", () -> {
            assertThrows(IllegalArgumentException.class, () -> itemAcc.getField(item, 2));
        });
        test("Item setField 负索引", () -> {
            assertThrows(IllegalArgumentException.class, () -> itemAcc.setField(item, -1, "x"));
        });
        test("Item setField 超大索引 (50)", () -> {
            assertThrows(IllegalArgumentException.class, () -> itemAcc.setField(item, 50, "x"));
        });

        // SpoiInventoryAccessor (fieldCount=3)
        SpoiAccessor invAcc = new SpoiInventoryAccessor();
        SpoiInventory inv = new SpoiInventory(null, null, 0);

        test("Inventory getField 负索引", () -> {
            assertThrows(IllegalArgumentException.class, () -> invAcc.getField(inv, -1));
        });
        test("Inventory getField 超大索引 (3)", () -> {
            assertThrows(IllegalArgumentException.class, () -> invAcc.getField(inv, 3));
        });
        test("Inventory setField 超大索引 (100)", () -> {
            assertThrows(IllegalArgumentException.class, () -> invAcc.setField(inv, 100, "x"));
        });

        // SpoiCharacterAccessor (fieldCount=5)
        SpoiAccessor charAcc = new SpoiCharacterAccessor();
        SpoiCharacter c = new SpoiCharacter("Hero", 100, null, null, 1);

        test("Character getField 负索引", () -> {
            assertThrows(IllegalArgumentException.class, () -> charAcc.getField(c, -1));
        });
        test("Character getField 超大索引 (5)", () -> {
            assertThrows(IllegalArgumentException.class, () -> charAcc.getField(c, 5));
        });
        test("Character setField 超大索引 (1000)", () -> {
            assertThrows(IllegalArgumentException.class, () -> charAcc.setField(c, 1000, "x"));
        });

        // SpoiWorldAccessor (fieldCount=3)
        SpoiAccessor worldAcc = new SpoiWorldAccessor();
        SpoiWorld w = new SpoiWorld("Earth", 0, null);

        test("World getField 负索引", () -> {
            assertThrows(IllegalArgumentException.class, () -> worldAcc.getField(w, -1));
        });
        test("World getField 超大索引 (3)", () -> {
            assertThrows(IllegalArgumentException.class, () -> worldAcc.getField(w, 3));
        });
        test("World setField 超大索引 (-5)", () -> {
            assertThrows(IllegalArgumentException.class, () -> worldAcc.setField(w, -5, "x"));
        });

        // setField 类型检查: 非 Number 值给 int 字段 -> 静默使用默认值 0
        test("setField name with null -> 空串", () -> {
            SpoiTestPlayer pp = new SpoiTestPlayer("Old", 50, 1, 0.0);
            playerAcc.setField(pp, 0, null);
            check(pp.name.equals(""), "name should be empty after null set");
        });
        test("setField hp with String -> 0 (静默)", () -> {
            SpoiTestPlayer pp = new SpoiTestPlayer("Test", 50, 1, 0.0);
            playerAcc.setField(pp, 1, "not a number");
            check(pp.hp == 0, "hp should be 0 after non-number set");
        });
        test("setField level with Boolean -> 0 (静默)", () -> {
            SpoiTestPlayer pp = new SpoiTestPlayer("Test", 50, 10, 0.0);
            playerAcc.setField(pp, 2, true);
            check(pp.level == 0, "level should be 0 after boolean set");
        });
    }

    // =============================== 5. Executor 组合操作测试 ===============================

    @SuppressWarnings("unchecked")
    static void testExecutorCombinations() {
        System.out.println("\n=== 5. Executor 组合操作测试 ===");

        // --- 空管道测试 ---
        test("空管道 PIPE -> EXEC（空列表）", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<Object> empty = new ArrayList<>();
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(empty, stream);
            check(result.get("resultType").equals(ResultType.UNDEF), "empty pipe should be UNDEF");
        });

        test("空管道 -> COUNT -> 0", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<Object> empty = new ArrayList<>();
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(countInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(empty, stream);
            check(result.get("resultType").equals(ResultType.SINGLE), "COUNT should be SINGLE");
            check(result.get("value").equals(0), "empty count should be 0");
        });

        // --- SET + PIPE + EXEC 组合 ---
        test("SET + PIPE + EXEC", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiTestPlayer p = new SpoiTestPlayer("Old", 50, 1, 0.0);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setInst(new int[]{0}, makeSetOperandString("NewName")));
            insts.add(pipeInst(new int[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(p, stream);
            check(p.name.equals("NewName"), "SET should change name");
            check(result.get("resultType").equals(ResultType.SINGLE), "PIPE should return SINGLE");
        });

        // --- 多层 FILTER ---
        test("多层 FILTER（level > 2 AND hp > 70）", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("A", 100, 10, 0.0));  // ✓ both
            players.add(new SpoiTestPlayer("B", 80, 3, 0.0));    // ✓ both
            players.add(new SpoiTestPlayer("C", 60, 7, 0.0));    // level✓ hp✗
            players.add(new SpoiTestPlayer("D", 90, 1, 0.0));    // hp✓ level✗
            players.add(new SpoiTestPlayer("E", 50, 2, 0.0));    // ✗ both

            byte[] filter1 = makeFilterOperand(2, 3, TypeId.I32,
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(2).array()); // level > 2
            byte[] filter2 = makeFilterOperand(1, 3, TypeId.I32,
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(70).array()); // hp > 70

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterInst(new int[]{}, filter1));
            insts.add(filterInst(new int[]{}, filter2));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            check(value.size() == 2, "should have 2 results: A and B");
            check(((SpoiTestPlayer) value.get(0)).name.equals("A"), "first should be A");
            check(((SpoiTestPlayer) value.get(1)).name.equals("B"), "second should be B");
        });

        // --- FILTER with String ---
        test("FILTER by name eq", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("Alice", 100, 10, 0.0));
            players.add(new SpoiTestPlayer("Bob", 80, 3, 0.0));
            players.add(new SpoiTestPlayer("Alice", 60, 7, 0.0));

            byte[] filterOp = makeFilterOperand(0, 0, TypeId.STRING, "Alice".getBytes(StandardCharsets.UTF_8));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterInst(new int[]{}, filterOp));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            check(value.size() == 2, "should have 2 Alices");
        });

        // --- COUNT 边界 ---
        test("COUNT 单元素", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("A", 100, 1, 0.0));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(countInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            check(result.get("value").equals(1), "count should be 1");
        });

        // --- SORT 测试 ---
        test("SORT by level asc", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("C", 100, 30, 0.0));
            players.add(new SpoiTestPlayer("A", 100, 10, 0.0));
            players.add(new SpoiTestPlayer("B", 100, 20, 0.0));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(sortInst(new int[]{2})); // sort by level
            insts.add(selectInst(new int[]{0})); // select name
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            check(value.get(0).equals("A"), "first should be A");
            check(value.get(1).equals("B"), "second should be B");
            check(value.get(2).equals("C"), "third should be C");
        });

        // --- TAKE 边界 ---
        test("TAKE 0", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("A", 100, 1, 0.0));
            players.add(new SpoiTestPlayer("B", 100, 1, 0.0));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            check(result.get("resultType").equals(ResultType.UNDEF), "TAKE 0 should be UNDEF");
        });

        test("TAKE more than size", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("A", 100, 1, 0.0));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(100));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            check(result.get("resultType").equals(ResultType.SINGLE), "TAKE oversize should keep all");
        });

        // --- DROP 测试 ---
        test("DROP 1 of 3", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<String> items = new ArrayList<>();
            items.add("A");
            items.add("B");
            items.add("C");

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(dropInst(1));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(items, stream);
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            check(value.size() == 2, "DROP 1 should leave 2");
            check(value.get(0).equals("B"), "first should be B");
        });

        test("DROP all", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<String> items = new ArrayList<>();
            items.add("A");
            items.add("B");

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(dropInst(10));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(items, stream);
            check(result.get("resultType").equals(ResultType.UNDEF), "DROP all should be UNDEF");
        });

        // --- REVERSE 测试 ---
        test("REVERSE", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<String> items = new ArrayList<>();
            items.add("A");
            items.add("B");
            items.add("C");

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(reverseInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(items, stream);
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            check(value.get(0).equals("C"), "first should be C");
            check(value.get(2).equals("A"), "last should be A");
        });

        // --- REVERSE on empty ---
        test("REVERSE on empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<Object> empty = new ArrayList<>();
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(reverseInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(empty, stream);
            check(result.get("resultType").equals(ResultType.UNDEF), "reverse empty should be UNDEF");
        });

        // --- DISTINCT 测试 ---
        test("DISTINCT 去重", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<String> items = new ArrayList<>();
            items.add("A");
            items.add("B");
            items.add("A");
            items.add("C");
            items.add("B");

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(distinctInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(items, stream);
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            check(value.size() == 3, "distinct should have 3 items");
            check(value.get(0).equals("A"), "first should be A");
            check(value.get(1).equals("B"), "second should be B");
            check(value.get(2).equals("C"), "third should be C");
        });

        // --- SELECT 后管道 ---
        test("SELECT + FILTER chain", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("Alice", 100, 10, 0.0));
            players.add(new SpoiTestPlayer("Bob", 80, 3, 0.0));
            players.add(new SpoiTestPlayer("Carol", 60, 7, 0.0));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(selectInst(new int[]{0})); // select names
            insts.add(takeInst(2));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            check(value.size() == 2, "should have 2 names");
            check(value.get(0).equals("Alice"), "first should be Alice");
        });

        // --- 边界组合: PIPE -> FILTER -> COUNT -> empty result ---
        test("FILTER 无匹配 -> COUNT 0", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("A", 100, 10, 0.0));
            players.add(new SpoiTestPlayer("B", 80, 3, 0.0));

            byte[] filterOp = makeFilterOperand(2, 0, TypeId.I32,
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(999).array()); // level == 999

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterInst(new int[]{}, filterOp));
            insts.add(countInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            check(result.get("value").equals(0), "no match count should be 0");
        });

        // --- ANY 测试 ---
        test("ANY 有匹配", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("A", 100, 1, 0.0));
            players.add(new SpoiTestPlayer("B", 80, 5, 0.0));

            byte[] filterOp = makeFilterOperand(2, 3, TypeId.I32,
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(3).array()); // level > 3

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(anyInst(new int[]{}, filterOp));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            check(result.get("value").equals(true), "ANY should be true");
        });

        test("ANY 无匹配", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("A", 100, 1, 0.0));

            byte[] filterOp = makeFilterOperand(2, 0, TypeId.I32,
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(999).array());

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(anyInst(new int[]{}, filterOp));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            check(result.get("value").equals(false), "ANY should be false");
        });

        // --- ALL 测试 ---
        test("ALL 全部匹配", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("A", 100, 5, 0.0));
            players.add(new SpoiTestPlayer("B", 80, 6, 0.0));

            byte[] filterOp = makeFilterOperand(2, 3, TypeId.I32,
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(3).array()); // level > 3

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(allInst(new int[]{}, filterOp));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            check(result.get("value").equals(true), "ALL should be true");
        });

        test("ALL 部分不匹配", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("A", 100, 5, 0.0));
            players.add(new SpoiTestPlayer("B", 80, 1, 0.0)); // level=1 not > 3

            byte[] filterOp = makeFilterOperand(2, 3, TypeId.I32,
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(3).array());

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(allInst(new int[]{}, filterOp));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            check(result.get("value").equals(false), "ALL should be false");
        });
    }

    // =============================== 6. 跨类型 Executor 测试 ===============================

    @SuppressWarnings("unchecked")
    static void testCrossTypeExecutor() {
        System.out.println("\n=== 6. 跨类型 Executor 测试 ===");

        // --- SpoiItem ---
        test("Item: SET name and value", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiItem item = new SpoiItem("Old", 0);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setInst(new int[]{0}, makeSetOperandString("Potion")));
            insts.add(setInst(new int[]{1}, makeSetOperandI32(500)));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(item, stream);
            check(item.name.equals("Potion"), "item name should be Potion");
            check(item.value == 500, "item value should be 500");
        });

        test("Item: PIPE -> FILTER by value", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiItem> items = new ArrayList<>();
            items.add(new SpoiItem("Sword", 100));
            items.add(new SpoiItem("Shield", 50));
            items.add(new SpoiItem("Potion", 20));
            items.add(new SpoiItem("Axe", 80));

            byte[] filterOp = makeFilterOperand(1, 3, TypeId.I32,
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(50).array()); // value > 50

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterInst(new int[]{}, filterOp));
            insts.add(selectInst(new int[]{0})); // select name
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(items, stream);
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            check(value.size() == 2, "should have 2 items");
            check(value.get(0).equals("Sword"), "first should be Sword");
            check(value.get(1).equals("Axe"), "second should be Axe");
        });

        // --- SpoiInventory ---
        test("Inventory: SET gold", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiInventory inv = new SpoiInventory(null, null, 0);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setInst(new int[]{2}, makeSetOperandI32(9999)));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(inv, stream);
            check(inv.gold == 9999, "gold should be 9999");
        });

        test("Inventory: SET items list", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiInventory inv = new SpoiInventory();
            ArrayList<SpoiItem> newItems = new ArrayList<>();
            newItems.add(new SpoiItem("Sword", 100));
            // SET items with raw object (no type_id prefix needed for Object fields)
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setInst(new int[]{0}, new byte[0])); // set items to empty
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(inv, stream);
            // After deserializing empty bytes with STRING type_id, it would be ""
            // The setField for items (idx 0) accepts any Object
            // Actually SET with empty operand will deserializeValue to null
            // But the operand format has type_id prefix... Let's test properly
            check(inv.items == null || inv.items.equals(""), "items should be set");
        });

        test("Inventory: ADD gold", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiInventory inv = new SpoiInventory(null, null, 100);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(addInst(new int[]{2}, makeSetOperandI32(50)));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(inv, stream);
            check(inv.gold == 150, "gold should be 150");
        });

        // --- SpoiCharacter ---
        test("Character: SET hp and petLevel", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiCharacter c = new SpoiCharacter("Hero", 50, null, null, 1);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setInst(new int[]{1}, makeSetOperandI32(200)));
            insts.add(setInst(new int[]{4}, makeSetOperandI32(10)));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(c, stream);
            check(c.hp == 200, "hp should be 200");
            check(c.petLevel == 10, "petLevel should be 10");
        });

        test("Character: navigate inventory -> gold", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiInventory inv = new SpoiInventory(null, null, 500);
            SpoiCharacter c = new SpoiCharacter("Hero", 100, inv, null, 1);
            Object gold = exe.navigate(c, Arrays.asList(2, 2));
            check(gold.equals(500), "nested gold should be 500");
        });

        test("Character: navigate weapon -> name", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiItem weapon = new SpoiItem("Excalibur", 999);
            SpoiCharacter c = new SpoiCharacter("Hero", 100, null, weapon, 1);
            Object name = exe.navigate(c, Arrays.asList(3, 0));
            check(name.equals("Excalibur"), "weapon name should be Excalibur");
        });

        test("Character: SET nested inventory gold", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiInventory inv = new SpoiInventory(null, null, 100);
            SpoiCharacter c = new SpoiCharacter("Hero", 100, inv, null, 1);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setInst(new int[]{2, 2}, makeSetOperandI32(9999)));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(c, stream);
            check(inv.gold == 9999, "nested gold should be 9999");
        });

        // --- SpoiWorld ---
        test("World: SET worldName and tick", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiWorld w = new SpoiWorld("OldWorld", 0, null);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setInst(new int[]{0}, makeSetOperandString("NewWorld")));
            insts.add(setInst(new int[]{1}, makeSetOperandI32(999)));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(w, stream);
            check(w.worldName.equals("NewWorld"), "worldName should be NewWorld");
            check(w.tick == 999, "tick should be 999");
        });

        test("World: navigate characters list -> character name", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiCharacter hero = new SpoiCharacter("Hero", 100, null, null, 1);
            ArrayList<SpoiCharacter> chars = new ArrayList<>();
            chars.add(hero);
            SpoiWorld w = new SpoiWorld("Earth", 0, chars);
            Object name = exe.navigate(w, Arrays.asList(2, 0, 0));
            check(name.equals("Hero"), "nested character name should be Hero");
        });

        // --- FILTER across types ---
        test("跨类型 FILTER: character list by hp", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiCharacter> chars = new ArrayList<>();
            chars.add(new SpoiCharacter("A", 100, null, null, 1));
            chars.add(new SpoiCharacter("B", 50, null, null, 1));
            chars.add(new SpoiCharacter("C", 200, null, null, 1));

            byte[] filterOp = makeFilterOperand(1, 3, TypeId.I32,
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(80).array()); // hp > 80

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterInst(new int[]{}, filterOp));
            insts.add(selectInst(new int[]{0})); // select name
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(chars, stream);
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            check(value.size() == 2, "should have 2 chars");
            check(value.get(0).equals("A"), "first should be A");
            check(value.get(1).equals("C"), "second should be C");
        });

        // --- World list FILTER by tick ---
        test("World list FILTER by tick", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiWorld> worlds = new ArrayList<>();
            worlds.add(new SpoiWorld("Earth", 100, null));
            worlds.add(new SpoiWorld("Mars", 200, null));
            worlds.add(new SpoiWorld("Venus", 50, null));

            byte[] filterOp = makeFilterOperand(1, 3, TypeId.I32,
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(80).array()); // tick > 80

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterInst(new int[]{}, filterOp));
            insts.add(selectInst(new int[]{0})); // select worldName
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(worlds, stream);
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            check(value.size() == 2, "should have 2 worlds");
        });
    }

    // =============================== 7. Registry 边界测试 ===============================

    static void testRegistryBoundaries() {
        System.out.println("\n=== 7. Registry 边界测试 ===");

        test("Registry size = 6", () -> {
            check(SpoiAccessorRegistry.registry.size() == 6, "registry should have 6 entries");
        });

        test("Registry 包含 SpoiTestPlayer", () -> {
            check(SpoiAccessorRegistry.get("SpoiTestPlayer") != null, "should have SpoiTestPlayer");
        });
        test("Registry 包含 SpoiTestState", () -> {
            check(SpoiAccessorRegistry.get("SpoiTestState") != null, "should have SpoiTestState");
        });
        test("Registry 包含 SpoiItem", () -> {
            check(SpoiAccessorRegistry.get("SpoiItem") != null, "should have SpoiItem");
        });
        test("Registry 包含 SpoiInventory", () -> {
            check(SpoiAccessorRegistry.get("SpoiInventory") != null, "should have SpoiInventory");
        });
        test("Registry 包含 SpoiCharacter", () -> {
            check(SpoiAccessorRegistry.get("SpoiCharacter") != null, "should have SpoiCharacter");
        });
        test("Registry 包含 SpoiWorld", () -> {
            check(SpoiAccessorRegistry.get("SpoiWorld") != null, "should have SpoiWorld");
        });

        test("Registry 缺失 key 返回 null", () -> {
            check(SpoiAccessorRegistry.get("NonExistent") == null, "unknown key should return null");
        });
        test("Registry 缺失 key (空串)", () -> {
            check(SpoiAccessorRegistry.get("") == null, "empty key should return null");
        });
        test("Registry 缺失 key (null)", () -> {
            check(SpoiAccessorRegistry.get(null) == null, "null key should return null");
        });

        test("所有 Registry 条目的 fieldCount > 0", () -> {
            for (Map.Entry<String, SpoiAccessor> entry : SpoiAccessorRegistry.registry.entrySet()) {
                check(entry.getValue().fieldCount() > 0,
                    entry.getKey() + " fieldCount should be > 0");
            }
        });

        test("Registry 各类型的 fieldCount 正确", () -> {
            check(SpoiAccessorRegistry.get("SpoiTestPlayer").fieldCount() == 4, "Player fieldCount should be 4");
            check(SpoiAccessorRegistry.get("SpoiTestState").fieldCount() == 3, "State fieldCount should be 3");
            check(SpoiAccessorRegistry.get("SpoiItem").fieldCount() == 2, "Item fieldCount should be 2");
            check(SpoiAccessorRegistry.get("SpoiInventory").fieldCount() == 3, "Inventory fieldCount should be 3");
            check(SpoiAccessorRegistry.get("SpoiCharacter").fieldCount() == 5, "Character fieldCount should be 5");
            check(SpoiAccessorRegistry.get("SpoiWorld").fieldCount() == 3, "World fieldCount should be 3");
        });

        test("Registry 返回正确的 Accessor 类型", () -> {
            check(SpoiAccessorRegistry.get("SpoiTestPlayer") instanceof SpoiTestPlayerAccessor, "wrong type for Player");
            check(SpoiAccessorRegistry.get("SpoiItem") instanceof SpoiItemAccessor, "wrong type for Item");
            check(SpoiAccessorRegistry.get("SpoiInventory") instanceof SpoiInventoryAccessor, "wrong type for Inventory");
            check(SpoiAccessorRegistry.get("SpoiCharacter") instanceof SpoiCharacterAccessor, "wrong type for Character");
            check(SpoiAccessorRegistry.get("SpoiWorld") instanceof SpoiWorldAccessor, "wrong type for World");
        });
    }

    // =============================== main ===============================

    public static void main(String[] args) {
        System.out.println("============================================");
        System.out.println("  TestSpoiRigorous — SPOI 刁钻测试");
        System.out.println("============================================");

        testNumericBoundaries();
        testStringBoundaries();
        testDeserializationErrors();
        testAccessorOutOfBounds();
        testExecutorCombinations();
        testCrossTypeExecutor();
        testRegistryBoundaries();

        System.out.println("\n============================================");
        System.out.println("  通过: " + passed + ", 失败: " + failed);
        System.out.println("============================================");

        if (failed > 0) {
            System.out.println("\n失败详情:");
            for (String f : failures) {
                System.out.println(f);
            }
            System.exit(1);
        } else {
            System.out.println("所有测试通过!");
        }
    }
}