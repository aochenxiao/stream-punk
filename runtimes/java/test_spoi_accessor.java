/**
 * Stream-Punk Java SPOI Accessor 测试套件
 * 编译: javac -encoding UTF-8 spoi_accessor.java spoi_executor.java test_spoi_accessor.java
 * 运行: java TestSpoiAccessor
 */

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.*;

// =============================== 测试数据类（与 accessor 中引用的类型匹配） ===============================

class SpoiTestPlayer {
    public String name;
    public int hp;
    public int level;
    public double posX;
    SpoiTestPlayer(String name, int hp, int level, double posX) {
        this.name = name;
        this.hp = hp;
        this.level = level;
        this.posX = posX;
    }
}

class SpoiTestState {
    public int tick;
    public String currentMap;
    public Object players;
    SpoiTestState(int tick, String currentMap, Object players) {
        this.tick = tick;
        this.currentMap = currentMap;
        this.players = players;
    }
}

class SpoiItem {
    public String name;
    public int value;
    SpoiItem(String name, int value) { this.name = name; this.value = value; }
}

class SpoiInventory {
    public Object items;
    public Object equipped;
    public int gold;
    SpoiInventory(Object items, Object equipped, int gold) {
        this.items = items;
        this.equipped = equipped;
        this.gold = gold;
    }
}

class SpoiCharacter {
    public String name;
    public int hp;
    public Object inventory;
    public Object weapon;
    public int petLevel;
    SpoiCharacter(String name, int hp, Object inventory, Object weapon, int petLevel) {
        this.name = name;
        this.hp = hp;
        this.inventory = inventory;
        this.weapon = weapon;
        this.petLevel = petLevel;
    }
}

class SpoiWorld {
    public String worldName;
    public int tick;
    public Object characters;
    SpoiWorld(String worldName, int tick, Object characters) {
        this.worldName = worldName;
        this.tick = tick;
        this.characters = characters;
    }
}

// =============================== 测试入口 ===============================

class TestSpoiAccessor {
    static int passed = 0;
    static int failed = 0;

    static void test(String name, Runnable fn) {
        try {
            fn.run();
            passed++;
            System.out.println("  \u2713 " + name);
        } catch (Throwable e) {
            failed++;
            System.out.println("  \u2717 " + name);
            System.out.println("    " + e.getMessage());
        }
    }

    static void assertEqual(Object actual, Object expected, String msg) {
        if (!Objects.equals(actual, expected)) {
            throw new AssertionError(msg != null ? msg : "expected " + expected + ", got " + actual);
        }
    }

    static void assertEqual(Object actual, Object expected) {
        assertEqual(actual, expected, null);
    }

    static void assertTrue(boolean v, String msg) {
        if (!v) throw new AssertionError(msg != null ? msg : "expected true, got false");
    }

    static void assertTrue(boolean v) {
        assertTrue(v, null);
    }

    static void assertFalse(boolean v, String msg) {
        if (v) throw new AssertionError(msg != null ? msg : "expected false, got true");
    }

    static void assertFalse(boolean v) {
        assertFalse(v, null);
    }

    static void assertNull(Object v, String msg) {
        if (v != null) throw new AssertionError(msg != null ? msg : "expected null, got " + v);
    }

    static void assertNull(Object v) {
        assertNull(v, null);
    }

    static void assertThrows(Runnable fn, String msg) {
        try {
            fn.run();
            throw new AssertionError(msg != null ? msg : "expected exception, but none thrown");
        } catch (AssertionError e) {
            throw e;
        } catch (Throwable e) {
            // expected
        }
    }

    // =============================== 值编码辅助 ===============================

    /** 构造 [type_id(u32 LE) + value_bytes] 格式的字节数组 */
    static byte[] encodeTypedValue(long typeId, byte[] valueBytes) {
        ByteBuffer buf = ByteBuffer.allocate(4 + valueBytes.length);
        buf.order(ByteOrder.LITTLE_ENDIAN);
        buf.putInt((int) typeId);
        buf.put(valueBytes);
        return buf.array();
    }

    static byte[] encodeUint32(long v) {
        return ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) v).array();
    }

    /** 将 long 值编码为适合 deserializeValue 的字节数组 */
    static byte[] encodeValue(long v) {
        if (v >= 0 && v < 256) {
            return new byte[]{(byte) v};
        } else if (v >= 0 && v < 65536) {
            return ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort((short) v).array();
        } else if (v >= 0 && v < 4294967296L) {
            return ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) v).array();
        } else {
            return ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(v).array();
        }
    }

    // =============================== 指令构建辅助 ===============================

    static byte[] buildSpoiStream(ArrayList<SpoiInstruction> instructions) {
        ByteArrayOutputStream buf = new ByteArrayOutputStream();
        Varint.writeVarint(buf, instructions.size());
        for (SpoiInstruction inst : instructions) {
            buf.write(inst.op);
            Varint.writeVarint(buf, inst.path.size());
            for (int seg : inst.path) {
                Varint.writeVarint(buf, seg);
            }
            Varint.writeVarint(buf, inst.operand.length);
            if (inst.operand.length > 0) {
                try {
                    buf.write(inst.operand);
                } catch (Exception e) {
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

    /** 构造带类型前缀的 SET 指令（v2 deserializer 需要 type_id 前缀） */
    static SpoiInstruction setInt(int[] path, long value) {
        byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) value).array();
        return makeInst(Op.SET, path, encodeTypedValue(TypeId.I32, val));
    }

    static SpoiInstruction setStr(int[] path, String value) {
        byte[] val = value.getBytes(StandardCharsets.UTF_8);
        return makeInst(Op.SET, path, encodeTypedValue(TypeId.STRING, val));
    }

    static SpoiInstruction setVal(int[] path, byte[] valueBytes) {
        return makeInst(Op.SET, path, valueBytes);
    }

    /** 构造带类型前缀的 ADD 指令 */
    static SpoiInstruction addInt(int[] path, long value) {
        byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) value).array();
        return makeInst(Op.ADD, path, encodeTypedValue(TypeId.I32, val));
    }

    static SpoiInstruction pipeInst(int[] path) {
        return makeInst(Op.PIPE, path, new byte[0]);
    }

    static SpoiInstruction execInst() {
        return makeInst(Op.EXEC, null, new byte[0]);
    }

    /** 构建 FILTER 操作数: 4B memberIdx LE + 1B cmpOp + value_len(varint) + [type_id(u32) + value_bytes] */
    static byte[] buildFilterOperand(int memberIdx, int cmpOp, long value) {
        byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) value).array();
        byte[] typedVal = encodeTypedValue(TypeId.I32, val);
        // 计算 value_len（typedVal 的总长度）
        ByteArrayOutputStream vlenBuf = new ByteArrayOutputStream();
        Varint.writeVarint(vlenBuf, typedVal.length);
        byte[] vlenBytes = vlenBuf.toByteArray();

        byte[] operand = new byte[5 + vlenBytes.length + typedVal.length];
        ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(memberIdx);
        operand[4] = (byte) cmpOp;
        System.arraycopy(vlenBytes, 0, operand, 5, vlenBytes.length);
        System.arraycopy(typedVal, 0, operand, 5 + vlenBytes.length, typedVal.length);
        return operand;
    }

    static SpoiInstruction filterGt(int[] path, int memberIdx, long value) {
        return makeInst(Op.FILTER, path, buildFilterOperand(memberIdx, 3, value));
    }

    static SpoiInstruction filterEq(int[] path, int memberIdx, long value) {
        return makeInst(Op.FILTER, path, buildFilterOperand(memberIdx, 0, value));
    }

    static SpoiInstruction selectInst(int[] path) {
        return makeInst(Op.SELECT, path, new byte[0]);
    }

    static SpoiInstruction takeInst(int n) {
        return makeInst(Op.TAKE, null, encodeUint32(n));
    }

    static SpoiInstruction countInst() {
        return makeInst(Op.COUNT, null, new byte[0]);
    }

    // =============================== 测试用例 ===============================

    public static void main(String[] args) {
        // ---- 1. TypeId 常量测试 ----
        System.out.println("\nTypeId 常量");
        test("U8 = 26", () -> assertEqual(TypeId.U8, 26L));
        test("U16 = 27", () -> assertEqual(TypeId.U16, 27L));
        test("U32 = 28", () -> assertEqual(TypeId.U32, 28L));
        test("U64 = 29", () -> assertEqual(TypeId.U64, 29L));
        test("I8 = 30", () -> assertEqual(TypeId.I8, 30L));
        test("I16 = 31", () -> assertEqual(TypeId.I16, 31L));
        test("I32 = 32", () -> assertEqual(TypeId.I32, 32L));
        test("I64 = 33", () -> assertEqual(TypeId.I64, 33L));
        test("F32 = 34", () -> assertEqual(TypeId.F32, 34L));
        test("F64 = 35", () -> assertEqual(TypeId.F64, 35L));
        test("STRING = 9", () -> assertEqual(TypeId.STRING, 9L));
        test("BOOL = 40", () -> assertEqual(TypeId.BOOL, 40L));

        // ---- 2. SpoiDeserializer.deserializeValue 测试 ----
        System.out.println("\nSpoiDeserializer.deserializeValue");

        test("deserialize U8", () -> {
            byte[] data = encodeTypedValue(TypeId.U8, new byte[]{(byte) 200});
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, 200); // 0xFF & 200 = 200
        });

        test("deserialize U8 max", () -> {
            byte[] data = encodeTypedValue(TypeId.U8, new byte[]{(byte) 0xFF});
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, 255);
        });

        test("deserialize U16", () -> {
            byte[] val = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort((short) 1000).array();
            byte[] data = encodeTypedValue(TypeId.U16, val);
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, 1000);
        });

        test("deserialize U16 max", () -> {
            byte[] val = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort((short) 0xFFFF).array();
            byte[] data = encodeTypedValue(TypeId.U16, val);
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, 65535);
        });

        test("deserialize U32", () -> {
            byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(100000).array();
            byte[] data = encodeTypedValue(TypeId.U32, val);
            Object result = SpoiDeserializer.deserializeValue(data);
            // U32 returns Long (via Integer.toUnsignedLong)
            assertEqual(result, 100000L);
        });

        test("deserialize U32 large", () -> {
            byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(-1).array();
            byte[] data = encodeTypedValue(TypeId.U32, val);
            Object result = SpoiDeserializer.deserializeValue(data);
            // -1 as unsigned = 4294967295
            assertEqual(result, 4294967295L);
        });

        test("deserialize U64", () -> {
            byte[] val = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(9999999999L).array();
            byte[] data = encodeTypedValue(TypeId.U64, val);
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, 9999999999L);
        });

        test("deserialize I8 positive", () -> {
            byte[] data = encodeTypedValue(TypeId.I8, new byte[]{(byte) 100});
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, (byte) 100);
        });

        test("deserialize I8 negative", () -> {
            byte[] data = encodeTypedValue(TypeId.I8, new byte[]{(byte) -50});
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, (byte) -50);
        });

        test("deserialize I16", () -> {
            byte[] val = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort((short) -30000).array();
            byte[] data = encodeTypedValue(TypeId.I16, val);
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, (short) -30000);
        });

        test("deserialize I32", () -> {
            byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(-1000000).array();
            byte[] data = encodeTypedValue(TypeId.I32, val);
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, -1000000);
        });

        test("deserialize I64", () -> {
            byte[] val = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(-9999999999L).array();
            byte[] data = encodeTypedValue(TypeId.I64, val);
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, -9999999999L);
        });

        test("deserialize F32", () -> {
            byte[] val = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putFloat(3.14f).array();
            byte[] data = encodeTypedValue(TypeId.F32, val);
            Object result = SpoiDeserializer.deserializeValue(data);
            assertTrue(result instanceof Float);
            assertEqual((float) result, 3.14f);
        });

        test("deserialize F64", () -> {
            byte[] val = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putDouble(2.718281828).array();
            byte[] data = encodeTypedValue(TypeId.F64, val);
            Object result = SpoiDeserializer.deserializeValue(data);
            assertTrue(result instanceof Double);
            assertEqual((double) result, 2.718281828);
        });

        test("deserialize String", () -> {
            byte[] val = "hello world".getBytes(StandardCharsets.UTF_8);
            byte[] data = encodeTypedValue(TypeId.STRING, val);
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, "hello world");
        });

        test("deserialize String empty", () -> {
            byte[] data = encodeTypedValue(TypeId.STRING, new byte[0]);
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, "");
        });

        test("deserialize String unicode", () -> {
            byte[] val = "\u4e2d\u6587\u6d4b\u8bd5".getBytes(StandardCharsets.UTF_8);
            byte[] data = encodeTypedValue(TypeId.STRING, val);
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, "\u4e2d\u6587\u6d4b\u8bd5");
        });

        test("deserialize Bool true", () -> {
            byte[] data = encodeTypedValue(TypeId.BOOL, new byte[]{1});
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, true);
        });

        test("deserialize Bool false", () -> {
            byte[] data = encodeTypedValue(TypeId.BOOL, new byte[]{0});
            Object result = SpoiDeserializer.deserializeValue(data);
            assertEqual(result, false);
        });

        test("deserialize null data", () -> {
            Object result = SpoiDeserializer.deserializeValue(null);
            assertNull(result);
        });

        test("deserialize empty data", () -> {
            Object result = SpoiDeserializer.deserializeValue(new byte[0]);
            assertNull(result);
        });

        test("deserialize too short data (< 4 bytes)", () -> {
            Object result = SpoiDeserializer.deserializeValue(new byte[]{1, 2, 3});
            assertNull(result);
        });

        test("deserialize zero type_id", () -> {
            byte[] data = encodeTypedValue(0, new byte[]{42}); // unknown type_id returns raw bytes
            Object result = SpoiDeserializer.deserializeValue(data);
            assertTrue(result instanceof byte[]);
            assertEqual(((byte[]) result).length, 1);
            assertEqual((int) ((byte[]) result)[0], 42);
        });

        // ---- 3. SpoiTestPlayerAccessor 测试 ----
        System.out.println("\nSpoiTestPlayerAccessor");
        SpoiAccessor playerAcc = new SpoiTestPlayerAccessor();

        test("player fieldCount = 4", () -> {
            assertEqual(playerAcc.fieldCount(), 4);
        });

        test("player getField name(0)", () -> {
            SpoiTestPlayer p = new SpoiTestPlayer("Alice", 100, 10, 1.5);
            assertEqual(playerAcc.getField(p, 0), "Alice");
        });

        test("player getField hp(1)", () -> {
            SpoiTestPlayer p = new SpoiTestPlayer("Alice", 100, 10, 1.5);
            assertEqual(playerAcc.getField(p, 1), 100);
        });

        test("player getField level(2)", () -> {
            SpoiTestPlayer p = new SpoiTestPlayer("Alice", 100, 10, 1.5);
            assertEqual(playerAcc.getField(p, 2), 10);
        });

        test("player getField posX(3)", () -> {
            SpoiTestPlayer p = new SpoiTestPlayer("Alice", 100, 10, 1.5);
            assertEqual(playerAcc.getField(p, 3), 1.5);
        });

        test("player setField name(0)", () -> {
            SpoiTestPlayer p = new SpoiTestPlayer("Old", 50, 1, 0.0);
            playerAcc.setField(p, 0, "NewName");
            assertEqual(p.name, "NewName");
        });

        test("player setField hp(1)", () -> {
            SpoiTestPlayer p = new SpoiTestPlayer("Test", 50, 1, 0.0);
            playerAcc.setField(p, 1, 200);
            assertEqual(p.hp, 200);
        });

        test("player setField level(2)", () -> {
            SpoiTestPlayer p = new SpoiTestPlayer("Test", 50, 1, 0.0);
            playerAcc.setField(p, 2, 99);
            assertEqual(p.level, 99);
        });

        test("player setField posX(3)", () -> {
            SpoiTestPlayer p = new SpoiTestPlayer("Test", 50, 1, 0.0);
            playerAcc.setField(p, 3, 3.14);
            assertEqual(p.posX, 3.14);
        });

        test("player setField name null", () -> {
            SpoiTestPlayer p = new SpoiTestPlayer("Old", 50, 1, 0.0);
            playerAcc.setField(p, 0, null);
            assertEqual(p.name, "");
        });

        test("player setField hp non-number", () -> {
            SpoiTestPlayer p = new SpoiTestPlayer("Test", 50, 1, 0.0);
            playerAcc.setField(p, 1, "not a number");
            assertEqual(p.hp, 0);
        });

        test("player getField invalid index (-1)", () -> {
            SpoiTestPlayer p = new SpoiTestPlayer("A", 1, 1, 0.0);
            assertThrows(() -> playerAcc.getField(p, -1), "expected exception for invalid index");
        });

        test("player getField invalid index (4)", () -> {
            SpoiTestPlayer p = new SpoiTestPlayer("A", 1, 1, 0.0);
            assertThrows(() -> playerAcc.getField(p, 4), "expected exception for invalid index");
        });

        test("player setField invalid index (4)", () -> {
            SpoiTestPlayer p = new SpoiTestPlayer("A", 1, 1, 0.0);
            assertThrows(() -> playerAcc.setField(p, 4, "x"), "expected exception for invalid index");
        });

        // ---- 4. SpoiTestStateAccessor 测试 ----
        System.out.println("\nSpoiTestStateAccessor");
        SpoiAccessor stateAcc = new SpoiTestStateAccessor();

        test("state fieldCount = 3", () -> {
            assertEqual(stateAcc.fieldCount(), 3);
        });

        test("state getField tick(0)", () -> {
            SpoiTestState s = new SpoiTestState(42, "map1", null);
            assertEqual(stateAcc.getField(s, 0), 42);
        });

        test("state getField currentMap(1)", () -> {
            SpoiTestState s = new SpoiTestState(42, "map1", null);
            assertEqual(stateAcc.getField(s, 1), "map1");
        });

        test("state getField players(2)", () -> {
            Object players = new ArrayList<>();
            SpoiTestState s = new SpoiTestState(42, "map1", players);
            assertEqual(stateAcc.getField(s, 2), players);
        });

        test("state setField tick(0)", () -> {
            SpoiTestState s = new SpoiTestState(0, "", null);
            stateAcc.setField(s, 0, 100);
            assertEqual(s.tick, 100);
        });

        test("state setField currentMap(1)", () -> {
            SpoiTestState s = new SpoiTestState(0, "", null);
            stateAcc.setField(s, 1, "overworld");
            assertEqual(s.currentMap, "overworld");
        });

        test("state setField players(2)", () -> {
            SpoiTestState s = new SpoiTestState(0, "", null);
            Object newPlayers = new ArrayList<>();
            stateAcc.setField(s, 2, newPlayers);
            assertEqual(s.players, newPlayers);
        });

        test("state getField invalid index (3)", () -> {
            SpoiTestState s = new SpoiTestState(0, "", null);
            assertThrows(() -> stateAcc.getField(s, 3), "expected exception for invalid index");
        });

        test("state setField invalid index (3)", () -> {
            SpoiTestState s = new SpoiTestState(0, "", null);
            assertThrows(() -> stateAcc.setField(s, 3, "x"), "expected exception for invalid index");
        });

        // ---- 5. SpoiItemAccessor 测试 ----
        System.out.println("\nSpoiItemAccessor");
        SpoiAccessor itemAcc = new SpoiItemAccessor();

        test("item fieldCount = 2", () -> {
            assertEqual(itemAcc.fieldCount(), 2);
        });

        test("item getField name(0)", () -> {
            SpoiItem item = new SpoiItem("Sword", 100);
            assertEqual(itemAcc.getField(item, 0), "Sword");
        });

        test("item getField value(1)", () -> {
            SpoiItem item = new SpoiItem("Sword", 100);
            assertEqual(itemAcc.getField(item, 1), 100);
        });

        test("item setField name(0)", () -> {
            SpoiItem item = new SpoiItem("Old", 0);
            itemAcc.setField(item, 0, "Potion");
            assertEqual(item.name, "Potion");
        });

        test("item setField value(1)", () -> {
            SpoiItem item = new SpoiItem("Old", 0);
            itemAcc.setField(item, 1, 500);
            assertEqual(item.value, 500);
        });

        test("item getField invalid index (2)", () -> {
            SpoiItem item = new SpoiItem("A", 0);
            assertThrows(() -> itemAcc.getField(item, 2), "expected exception for invalid index");
        });

        test("item setField invalid index (2)", () -> {
            SpoiItem item = new SpoiItem("A", 0);
            assertThrows(() -> itemAcc.setField(item, 2, "x"), "expected exception for invalid index");
        });

        // ---- 6. SpoiInventoryAccessor 测试 ----
        System.out.println("\nSpoiInventoryAccessor");
        SpoiAccessor invAcc = new SpoiInventoryAccessor();

        test("inventory fieldCount = 3", () -> {
            assertEqual(invAcc.fieldCount(), 3);
        });

        test("inventory getField items(0)", () -> {
            Object items = new ArrayList<>();
            SpoiInventory inv = new SpoiInventory(items, null, 0);
            assertEqual(invAcc.getField(inv, 0), items);
        });

        test("inventory getField equipped(1)", () -> {
            Object equipped = new SpoiItem("Axe", 50);
            SpoiInventory inv = new SpoiInventory(null, equipped, 0);
            assertEqual(invAcc.getField(inv, 1), equipped);
        });

        test("inventory getField gold(2)", () -> {
            SpoiInventory inv = new SpoiInventory(null, null, 999);
            assertEqual(invAcc.getField(inv, 2), 999);
        });

        test("inventory setField gold(2)", () -> {
            SpoiInventory inv = new SpoiInventory(null, null, 0);
            invAcc.setField(inv, 2, 5000);
            assertEqual(inv.gold, 5000);
        });

        test("inventory setField items(0)", () -> {
            SpoiInventory inv = new SpoiInventory(null, null, 0);
            Object newItems = new ArrayList<>();
            invAcc.setField(inv, 0, newItems);
            assertEqual(inv.items, newItems);
        });

        test("inventory getField invalid index (3)", () -> {
            SpoiInventory inv = new SpoiInventory(null, null, 0);
            assertThrows(() -> invAcc.getField(inv, 3), "expected exception for invalid index");
        });

        test("inventory setField invalid index (3)", () -> {
            SpoiInventory inv = new SpoiInventory(null, null, 0);
            assertThrows(() -> invAcc.setField(inv, 3, "x"), "expected exception for invalid index");
        });

        // ---- 7. SpoiCharacterAccessor 测试 ----
        System.out.println("\nSpoiCharacterAccessor");
        SpoiAccessor charAcc = new SpoiCharacterAccessor();

        test("character fieldCount = 5", () -> {
            assertEqual(charAcc.fieldCount(), 5);
        });

        test("character getField name(0)", () -> {
            SpoiCharacter c = new SpoiCharacter("Hero", 100, null, null, 1);
            assertEqual(charAcc.getField(c, 0), "Hero");
        });

        test("character getField hp(1)", () -> {
            SpoiCharacter c = new SpoiCharacter("Hero", 100, null, null, 1);
            assertEqual(charAcc.getField(c, 1), 100);
        });

        test("character getField inventory(2)", () -> {
            Object inv = new SpoiInventory(null, null, 0);
            SpoiCharacter c = new SpoiCharacter("Hero", 100, inv, null, 1);
            assertEqual(charAcc.getField(c, 2), inv);
        });

        test("character getField weapon(3)", () -> {
            Object weapon = new SpoiItem("Bow", 30);
            SpoiCharacter c = new SpoiCharacter("Hero", 100, null, weapon, 1);
            assertEqual(charAcc.getField(c, 3), weapon);
        });

        test("character getField petLevel(4)", () -> {
            SpoiCharacter c = new SpoiCharacter("Hero", 100, null, null, 5);
            assertEqual(charAcc.getField(c, 4), 5);
        });

        test("character setField name(0)", () -> {
            SpoiCharacter c = new SpoiCharacter("Old", 0, null, null, 0);
            charAcc.setField(c, 0, "NewHero");
            assertEqual(c.name, "NewHero");
        });

        test("character setField hp(1)", () -> {
            SpoiCharacter c = new SpoiCharacter("Old", 0, null, null, 0);
            charAcc.setField(c, 1, 200);
            assertEqual(c.hp, 200);
        });

        test("character setField petLevel(4)", () -> {
            SpoiCharacter c = new SpoiCharacter("Old", 0, null, null, 0);
            charAcc.setField(c, 4, 10);
            assertEqual(c.petLevel, 10);
        });

        test("character setField weapon(3)", () -> {
            SpoiCharacter c = new SpoiCharacter("Old", 0, null, null, 0);
            Object newWeapon = new SpoiItem("Staff", 80);
            charAcc.setField(c, 3, newWeapon);
            assertEqual(c.weapon, newWeapon);
        });

        test("character getField invalid index (5)", () -> {
            SpoiCharacter c = new SpoiCharacter("A", 0, null, null, 0);
            assertThrows(() -> charAcc.getField(c, 5), "expected exception for invalid index");
        });

        test("character setField invalid index (5)", () -> {
            SpoiCharacter c = new SpoiCharacter("A", 0, null, null, 0);
            assertThrows(() -> charAcc.setField(c, 5, "x"), "expected exception for invalid index");
        });

        // ---- 8. SpoiWorldAccessor 测试 ----
        System.out.println("\nSpoiWorldAccessor");
        SpoiAccessor worldAcc = new SpoiWorldAccessor();

        test("world fieldCount = 3", () -> {
            assertEqual(worldAcc.fieldCount(), 3);
        });

        test("world getField worldName(0)", () -> {
            SpoiWorld w = new SpoiWorld("Earth", 0, null);
            assertEqual(worldAcc.getField(w, 0), "Earth");
        });

        test("world getField tick(1)", () -> {
            SpoiWorld w = new SpoiWorld("Earth", 100, null);
            assertEqual(worldAcc.getField(w, 1), 100);
        });

        test("world getField characters(2)", () -> {
            Object chars = new ArrayList<>();
            SpoiWorld w = new SpoiWorld("Earth", 0, chars);
            assertEqual(worldAcc.getField(w, 2), chars);
        });

        test("world setField worldName(0)", () -> {
            SpoiWorld w = new SpoiWorld("Old", 0, null);
            worldAcc.setField(w, 0, "Mars");
            assertEqual(w.worldName, "Mars");
        });

        test("world setField tick(1)", () -> {
            SpoiWorld w = new SpoiWorld("Earth", 0, null);
            worldAcc.setField(w, 1, 500);
            assertEqual(w.tick, 500);
        });

        test("world setField characters(2)", () -> {
            SpoiWorld w = new SpoiWorld("Earth", 0, null);
            Object newChars = new ArrayList<>();
            worldAcc.setField(w, 2, newChars);
            assertEqual(w.characters, newChars);
        });

        test("world getField invalid index (3)", () -> {
            SpoiWorld w = new SpoiWorld("A", 0, null);
            assertThrows(() -> worldAcc.getField(w, 3), "expected exception for invalid index");
        });

        test("world setField invalid index (3)", () -> {
            SpoiWorld w = new SpoiWorld("A", 0, null);
            assertThrows(() -> worldAcc.setField(w, 3, "x"), "expected exception for invalid index");
        });

        // ---- 9. SpoiAccessorRegistry 测试 ----
        System.out.println("\nSpoiAccessorRegistry");

        test("registry contains SpoiTestPlayer", () -> {
            assertTrue(SpoiAccessorRegistry.get("SpoiTestPlayer") != null);
        });

        test("registry contains SpoiTestState", () -> {
            assertTrue(SpoiAccessorRegistry.get("SpoiTestState") != null);
        });

        test("registry contains SpoiItem", () -> {
            assertTrue(SpoiAccessorRegistry.get("SpoiItem") != null);
        });

        test("registry contains SpoiInventory", () -> {
            assertTrue(SpoiAccessorRegistry.get("SpoiInventory") != null);
        });

        test("registry contains SpoiCharacter", () -> {
            assertTrue(SpoiAccessorRegistry.get("SpoiCharacter") != null);
        });

        test("registry contains SpoiWorld", () -> {
            assertTrue(SpoiAccessorRegistry.get("SpoiWorld") != null);
        });

        test("registry returns correct accessor type", () -> {
            SpoiAccessor acc = SpoiAccessorRegistry.get("SpoiTestPlayer");
            assertTrue(acc instanceof SpoiTestPlayerAccessor);
            assertTrue(acc instanceof SpoiAccessor);
        });

        test("registry returns null for unknown type", () -> {
            assertNull(SpoiAccessorRegistry.get("NonExistentType"));
        });

        test("registry size is 6", () -> {
            assertEqual(SpoiAccessorRegistry.registry.size(), 6);
        });

        // ---- 10. Executor 集成测试（使用 SpoiAccessorRegistry） ----
        System.out.println("\nExecutor 集成测试（访问器驱动）");

        test("executor with accessor registry navigates player fields", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiTestPlayer p = new SpoiTestPlayer("Alice", 100, 10, 1.5);
            // navStep uses accessor internally
            assertEqual(exe.navStep(p, 0), "Alice");
            assertEqual(exe.navStep(p, 1), 100);
            assertEqual(exe.navStep(p, 2), 10);
            assertEqual(exe.navStep(p, 3), 1.5);
        });

        test("executor with accessor registry navigates item fields", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiItem item = new SpoiItem("Sword", 100);
            assertEqual(exe.navStep(item, 0), "Sword");
            assertEqual(exe.navStep(item, 1), 100);
        });

        test("executor SET player name via accessor", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiTestPlayer p = new SpoiTestPlayer("OldName", 50, 1, 0.0);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setStr(new int[]{0}, "NewName"));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.name, "NewName");
        });

        test("executor SET player hp via accessor", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiTestPlayer p = new SpoiTestPlayer("Test", 50, 1, 0.0);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            // 用类型化编码设置 hp 为 200
            byte[] val = encodeTypedValue(TypeId.I32, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(200).array());
            insts.add(setVal(new int[]{1}, val));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.hp, 200);
        });

        test("executor SET player level via accessor", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiTestPlayer p = new SpoiTestPlayer("Test", 50, 1, 0.0);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setInt(new int[]{2}, 99));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.level, 99);
        });

        test("executor ADD player hp via accessor", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiTestPlayer p = new SpoiTestPlayer("Test", 100, 1, 0.0);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(addInt(new int[]{1}, 50));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.hp, 150);
        });

        test("executor ADD player level via accessor", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiTestPlayer p = new SpoiTestPlayer("Test", 50, 5, 0.0);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(addInt(new int[]{2}, 3));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.level, 8);
        });

        test("executor SET item name via accessor", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiItem item = new SpoiItem("Old", 0);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setStr(new int[]{0}, "Potion"));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(item, stream);
            assertEqual(item.name, "Potion");
        });

        test("executor SET item value via accessor", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiItem item = new SpoiItem("Sword", 0);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setInt(new int[]{1}, 500));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(item, stream);
            assertEqual(item.value, 500);
        });

        test("executor SET world tick via accessor", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiWorld w = new SpoiWorld("Earth", 0, null);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setInt(new int[]{1}, 999));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(w, stream);
            assertEqual(w.tick, 999);
        });

        test("executor SET world name via accessor", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiWorld w = new SpoiWorld("Old", 0, null);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setStr(new int[]{0}, "Mars"));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(w, stream);
            assertEqual(w.worldName, "Mars");
        });

        test("executor PIPE player list and FILTER by level", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("A", 100, 1, 0.0));
            players.add(new SpoiTestPlayer("B", 80, 5, 0.0));
            players.add(new SpoiTestPlayer("C", 60, 3, 0.0));
            players.add(new SpoiTestPlayer("D", 90, 7, 0.0));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            // memberIdx=2 (level), cmpOp=3 (gt), value=3
            insts.add(filterGt(new int[]{}, 2, 3));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2); // B(5), D(7)
            SpoiTestPlayer first = (SpoiTestPlayer) value.get(0);
            assertEqual(first.name, "B");
        });

        test("executor PIPE item list and FILTER by value", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiItem> items = new ArrayList<>();
            items.add(new SpoiItem("Sword", 100));
            items.add(new SpoiItem("Shield", 50));
            items.add(new SpoiItem("Potion", 20));
            items.add(new SpoiItem("Axe", 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            // memberIdx=1 (value), cmpOp=3 (gt), value=50
            insts.add(filterGt(new int[]{}, 1, 50));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(items, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2); // Sword(100), Axe(80)
        });

        test("executor PIPE -> FILTER -> SELECT -> COUNT chain", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("Alice", 100, 10, 1.0));
            players.add(new SpoiTestPlayer("Bob", 80, 3, 2.0));
            players.add(new SpoiTestPlayer("Carol", 60, 7, 3.0));
            players.add(new SpoiTestPlayer("Dave", 90, 1, 4.0));
            players.add(new SpoiTestPlayer("Eve", 70, 5, 5.0));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));             // 管道所有玩家
            insts.add(filterGt(new int[]{}, 2, 2));       // level > 2
            insts.add(selectInst(new int[]{0}));           // 选择 name
            insts.add(countInst());                        // 计数
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            // level > 2: Alice(10), Bob(3), Carol(7), Eve(5) = 4
            assertEqual(result.get("value"), 4);
        });

        test("executor PIPE -> SELECT -> TAKE chain", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiItem> items = new ArrayList<>();
            items.add(new SpoiItem("Sword", 100));
            items.add(new SpoiItem("Shield", 50));
            items.add(new SpoiItem("Potion", 20));
            items.add(new SpoiItem("Axe", 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(selectInst(new int[]{0}));   // 选择 name
            insts.add(takeInst(2));                  // 取前 2 个
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(items, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
            assertEqual(value.get(0), "Sword");
            assertEqual(value.get(1), "Shield");
        });

        test("executor PIPE -> FILTER -> SELECT name", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            ArrayList<SpoiTestPlayer> players = new ArrayList<>();
            players.add(new SpoiTestPlayer("Alice", 100, 10, 1.0));
            players.add(new SpoiTestPlayer("Bob", 80, 3, 2.0));
            players.add(new SpoiTestPlayer("Carol", 60, 7, 3.0));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            // memberIdx=2 (level), cmpOp=0 (eq), value=7
            insts.add(filterEq(new int[]{}, 2, 7));
            insts.add(selectInst(new int[]{0}));   // 选择 name
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            // 单个结果返回 SINGLE 类型
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), "Carol");
        });

        test("executor navigates nested character -> inventory -> gold", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiInventory inv = new SpoiInventory(null, null, 500);
            SpoiCharacter c = new SpoiCharacter("Hero", 100, inv, null, 1);

            // character.inventory (field 2) -> inventory.gold (field 2)
            Object inventory = exe.navStep(c, 2);
            Object gold = exe.navStep(inventory, 2);
            assertEqual(gold, 500);
        });

        test("executor navigates nested world -> state -> tick", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiTestState state = new SpoiTestState(42, "map1", null);
            ArrayList<SpoiTestState> states = new ArrayList<>();
            states.add(state);
            SpoiWorld w = new SpoiWorld("Earth", 100, states);

            Object characters = exe.navStep(w, 2);
            // characters is a list, get first element
            Object firstState = exe.navStep(characters, 0);
            Object tick = exe.navStep(firstState, 0);
            assertEqual(tick, 42);
        });

        test("executor SET nested character inventory gold", () -> {
            SpoiExecutor exe = new SpoiExecutor(SpoiAccessorRegistry.registry);
            SpoiInventory inv = new SpoiInventory(null, null, 100);
            SpoiCharacter c = new SpoiCharacter("Hero", 100, inv, null, 1);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            // path [2, 2] = inventory(2) -> gold(2)
            insts.add(setInt(new int[]{2, 2}, 9999));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(c, stream);
            assertEqual(inv.gold, 9999);
        });

        // ---- 总结 ----
        System.out.println("\n========================================");
        System.out.println("\u901a\u8fc7: " + passed + ", \u5931\u8d25: " + failed);
        System.out.println("========================================");
        if (failed > 0) {
            System.exit(1);
        }
    }
}