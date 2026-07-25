/**
 * Stream-Punk Java SPOI 执行器测试套件
 * 编译: javac -encoding UTF-8 test_spoi_executor.java
 * 运行: java TestSpoiExecutor
 */

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.*;

// =============================== 测试数据类 ===============================

class Item {
    public String name;
    public int price;
    Item(String name, int price) { this.name = name; this.price = price; }
}

class Player {
    public String name;
    public int level;
    public int health;
    public ArrayList<Item> items;
    public HashMap<String, Object> metadata;
    Player(String name, int level, int health) {
        this.name = name;
        this.level = level;
        this.health = health;
        this.items = new ArrayList<>();
        this.metadata = new HashMap<>();
    }
}

// =============================== 测试入口 ===============================

class TestSpoiExecutor {
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

    // =============================== 类型注册表 ===============================

    static Map<String, List<String>> makeTypes() {
        Map<String, List<String>> types = new LinkedHashMap<>();
        types.put("Player", Arrays.asList("name", "level", "health", "items", "metadata"));
        types.put("Item", Arrays.asList("name", "price"));
        return types;
    }

    // =============================== 值编码辅助 ===============================

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

    /** 将 int 值编码为 uint32 LE 字节 */
    static byte[] encodeUint32(long v) {
        return ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) v).array();
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

    static SpoiInstruction setInt(int[] path, long value) {
        return makeInst(Op.SET, path, encodeUint32(value));
    }

    static SpoiInstruction setStr(int[] path, String value) {
        return makeInst(Op.SET, path, value.getBytes(StandardCharsets.UTF_8));
    }

    static SpoiInstruction setVal(int[] path, byte[] valueBytes) {
        return makeInst(Op.SET, path, valueBytes);
    }

    static SpoiInstruction addInt(int[] path, long value) {
        return makeInst(Op.ADD, path, encodeValue(value));
    }

    static SpoiInstruction pipeInst(int[] path) {
        return makeInst(Op.PIPE, path, new byte[0]);
    }

    /** 构建 FILTER 操作数: 4B memberIdx LE + 1B cmpOp + 值字节 */
    static byte[] buildFilterOperand(int memberIdx, int cmpOp, long value) {
        byte[] valBytes = encodeValue(value);
        byte[] operand = new byte[5 + valBytes.length];
        ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(memberIdx);
        operand[4] = (byte) cmpOp;
        System.arraycopy(valBytes, 0, operand, 5, valBytes.length);
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

    static SpoiInstruction sortInst(int[] path) {
        return makeInst(Op.SORT, path, new byte[0]);
    }

    static SpoiInstruction takeInst(int n) {
        return makeInst(Op.TAKE, null, encodeUint32(n));
    }

    static SpoiInstruction dropInst(int n) {
        return makeInst(Op.DROP, null, encodeUint32(n));
    }

    static SpoiInstruction reverseInst() {
        return makeInst(Op.REVERSE, null, new byte[0]);
    }

    static SpoiInstruction distinctInst() {
        return makeInst(Op.DISTINCT, null, new byte[0]);
    }

    static SpoiInstruction countInst() {
        return makeInst(Op.COUNT, null, new byte[0]);
    }

    static SpoiInstruction anyInst(int memberIdx, int cmpOp, long value) {
        return makeInst(Op.ANY, null, buildFilterOperand(memberIdx, cmpOp, value));
    }

    static SpoiInstruction allInst(int memberIdx, int cmpOp, long value) {
        return makeInst(Op.ALL, null, buildFilterOperand(memberIdx, cmpOp, value));
    }

    static SpoiInstruction findInst(int memberIdx, int cmpOp, long value) {
        return makeInst(Op.FIND, null, buildFilterOperand(memberIdx, cmpOp, value));
    }

    static SpoiInstruction execInst() {
        return makeInst(Op.EXEC, null, new byte[0]);
    }

    // =============================== 测试用例 ===============================

    public static void main(String[] args) {
        // ---- 1. varint 往返 ----
        System.out.println("\nVarint");
        test("varint zero", () -> {
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            Varint.writeVarint(buf, 0);
            byte[] data = buf.toByteArray();
            int[] offset = new int[]{0};
            assertEqual(Varint.readVarint(data, offset), 0);
            assertEqual(offset[0], data.length);
        });

        test("varint small values", () -> {
            for (int v : new int[]{1, 42, 127, 128, 255, 300, 1000, 16383, 16384, 0xFFFFF}) {
                ByteArrayOutputStream buf = new ByteArrayOutputStream();
                Varint.writeVarint(buf, v);
                byte[] data = buf.toByteArray();
                int[] offset = new int[]{0};
                assertEqual(Varint.readVarint(data, offset), v, "varint: " + v);
            }
        });

        test("varint multi-byte roundtrip", () -> {
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            Varint.writeVarint(buf, 128);
            Varint.writeVarint(buf, 16384);
            Varint.writeVarint(buf, 2097152);
            byte[] data = buf.toByteArray();
            int[] offset = new int[]{0};
            assertEqual(Varint.readVarint(data, offset), 128);
            assertEqual(Varint.readVarint(data, offset), 16384);
            assertEqual(Varint.readVarint(data, offset), 2097152);
        });

        // ---- 2. 指令流解析 ----
        System.out.println("\n指令流解析");
        test("parse single instruction", () -> {
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{0}));
            byte[] stream = buildSpoiStream(insts);
            ArrayList<SpoiInstruction> parsed = SpoiParser.parseSpoiStream(stream);
            assertEqual(parsed.size(), 1);
            assertEqual(parsed.get(0).op, Op.PIPE);
            assertEqual(parsed.get(0).path.size(), 1);
            assertEqual(parsed.get(0).path.get(0), 0);
        });

        test("parse multiple instructions", () -> {
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{0}));
            insts.add(takeInst(3));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            ArrayList<SpoiInstruction> parsed = SpoiParser.parseSpoiStream(stream);
            assertEqual(parsed.size(), 3);
            assertEqual(parsed.get(0).op, Op.PIPE);
            assertEqual(parsed.get(1).op, Op.TAKE);
            assertEqual(parsed.get(2).op, Op.EXEC);
        });

        test("parse with operand", () -> {
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setStr(new int[]{0}, "hello"));
            byte[] stream = buildSpoiStream(insts);
            ArrayList<SpoiInstruction> parsed = SpoiParser.parseSpoiStream(stream);
            assertEqual(parsed.size(), 1);
            assertEqual(parsed.get(0).op, Op.SET);
            String operandStr = new String(parsed.get(0).operand, StandardCharsets.UTF_8);
            assertEqual(operandStr, "hello");
        });

        test("parse empty stream", () -> {
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            Varint.writeVarint(buf, 0);
            ArrayList<SpoiInstruction> parsed = SpoiParser.parseSpoiStream(buf.toByteArray());
            assertEqual(parsed.size(), 0);
        });

        // ---- 3. 基本导航 ----
        System.out.println("\n基本导航");
        test("navStep by field index", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            // field index 0 = name
            assertEqual(exe.navStep(p, 0), "Alice");
            // field index 1 = level
            assertEqual(exe.navStep(p, 1), 10);
            // field index 2 = health
            assertEqual(exe.navStep(p, 2), 100);
        });

        test("navStep into list", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<String> list = new ArrayList<>(Arrays.asList("a", "b", "c"));
            assertEqual(exe.navStep(list, 0), "a");
            assertEqual(exe.navStep(list, 2), "c");
        });

        test("navStep into nested Player items", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Bob", 5, 80);
            p.items.add(new Item("Sword", 100));
            p.items.add(new Item("Shield", 50));
            // items(3) -> list -> index 0 -> Item -> name(0)
            Object items = exe.navStep(p, 3);
            Object item0 = exe.navStep(items, 0);
            assertEqual(exe.navStep(item0, 0), "Sword");
            assertEqual(exe.navStep(item0, 1), 100);
        });

        // ---- 4. SET 操作 ----
        System.out.println("\nSET 操作");
        test("set string field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("OldName", 1, 50);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setStr(new int[]{0}, "NewName"));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.name, "NewName");
        });

        test("set int field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Test", 1, 50);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            // 用 1 字节编码小整数，deserializeValue 返回 Integer
            insts.add(setVal(new int[]{2}, new byte[]{80}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.health, 80);
        });

        // ---- 5. ADD 操作 ----
        System.out.println("\nADD 操作");
        test("add to int field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Test", 1, 50);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            // 先用 1 字节设置初始值，然后用 1 字节 ADD
            insts.add(setVal(new int[]{2}, new byte[]{50}));
            insts.add(addInt(new int[]{2}, 25));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.health, 75);
        });

        test("add to level", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Test", 5, 100);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(addInt(new int[]{1}, 3));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.level, 8);
        });

        // ---- 6. PIPE 操作 ----
        System.out.println("\nPIPE 操作");
        test("pipe list from root", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3, 4, 5));
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.VECTOR);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 5);
        });

        test("pipe player items", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));
            p.items.add(new Item("Shield", 50));
            p.items.add(new Item("Potion", 20));
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            // path [3] = items 字段
            insts.add(pipeInst(new int[]{3}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(p, stream);
            assertEqual(result.get("resultType"), ResultType.VECTOR);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
        });

        // ---- 7. FILTER 操作 ----
        System.out.println("\nFILTER 操作");
        test("filter gt level", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Root", 0, 0);
            p.items = new ArrayList<>();
            // 将玩家列表作为根数据
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("A", 1, 100));
            players.add(new Player("B", 3, 80));
            players.add(new Player("C", 5, 60));
            players.add(new Player("D", 2, 90));
            players.add(new Player("E", 4, 70));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            // memberIdx=1 (level), cmpOp=3 (gt), value=2
            insts.add(filterGt(new int[]{}, 1, 2));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3); // B(3), C(5), E(4)
        });

        test("filter eq level", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("A", 1, 100));
            players.add(new Player("B", 3, 80));
            players.add(new Player("C", 1, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterEq(new int[]{}, 1, 1));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2); // A(1), C(1)
        });

        test("filter no match", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("A", 1, 100));
            players.add(new Player("B", 2, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 10));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            // 空管道返回 UNDEF，无 "value" 键
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        // ---- 8. SELECT 操作 ----
        System.out.println("\nSELECT 操作");
        test("select name field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 5, 80));
            players.add(new Player("Carol", 3, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(selectInst(new int[]{0})); // name
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
            assertEqual(value.get(0), "Alice");
            assertEqual(value.get(1), "Bob");
            assertEqual(value.get(2), "Carol");
        });

        test("select level field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("A", 10, 100));
            players.add(new Player("B", 5, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(selectInst(new int[]{1})); // level
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
            assertEqual(value.get(0), 10);
            assertEqual(value.get(1), 5);
        });

        // ---- 9. SORT 操作 ----
        System.out.println("\nSORT 操作");
        test("sort by level ascending", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("C", 5, 60));
            players.add(new Player("A", 1, 100));
            players.add(new Player("B", 3, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(sortInst(new int[]{1})); // sort by level
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
            Player first = (Player) value.get(0);
            assertEqual(first.name, "A");
            assertEqual(first.level, 1);
        });

        test("sort by name", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Zoe", 1, 100));
            players.add(new Player("Alice", 2, 80));
            players.add(new Player("Bob", 3, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(sortInst(new int[]{0})); // sort by name
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            Player first = (Player) value.get(0);
            assertEqual(first.name, "Alice");
        });

        // ---- 10. TAKE 操作 ----
        System.out.println("\nTAKE 操作");
        test("take first 2", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3, 4, 5));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(2));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });

        test("take more than size", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(10));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
        });

        test("take zero", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            // take(0) 清空管道，返回 UNDEF
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        // ---- 11. DROP 操作 ----
        System.out.println("\nDROP 操作");
        test("drop first 2", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3, 4, 5));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(dropInst(2));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
            assertEqual(value.get(0), 3);
        });

        test("drop all", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(dropInst(5));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            // drop 全部后管道为空，返回 UNDEF
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("drop zero", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(dropInst(0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
        });

        // ---- 12. REVERSE 操作 ----
        System.out.println("\nREVERSE 操作");
        test("reverse list", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3, 4, 5));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(reverseInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 5);
            assertEqual(value.get(0), 5);
            assertEqual(value.get(4), 1);
        });

        test("reverse single element", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(42));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(reverseInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), 42);
        });

        // ---- 13. DISTINCT 操作 ----
        System.out.println("\nDISTINCT 操作");
        test("distinct integers", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 2, 3, 1, 4, 3));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(distinctInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 4);
            assertEqual(value.get(0), 1);
            assertEqual(value.get(1), 2);
            assertEqual(value.get(2), 3);
            assertEqual(value.get(3), 4);
        });

        test("distinct strings", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<String> data = new ArrayList<>(Arrays.asList("a", "b", "a", "c", "b"));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(distinctInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
        });

        // ---- 14. COUNT 操作 ----
        System.out.println("\nCOUNT 操作");
        test("count elements", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3, 4, 5));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(countInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), 5);
        });

        test("count empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>();

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(countInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), 0);
        });

        // ---- 15. ANY 操作 ----
        System.out.println("\nANY 操作");
        test("any level gt 3", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("A", 1, 100));
            players.add(new Player("B", 3, 80));
            players.add(new Player("C", 5, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(anyInst(1, 3, 3)); // memberIdx=1(level), cmpOp=3(gt), value=3
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), true);
        });

        test("any level gt 10 (none)", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("A", 1, 100));
            players.add(new Player("B", 3, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(anyInst(1, 3, 10)); // memberIdx=1(level), cmpOp=3(gt), value=10
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), false);
        });

        test("any on empty pipe", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(anyInst(1, 3, 3));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), false);
        });

        // ---- 16. ALL 操作 ----
        System.out.println("\nALL 操作");
        test("all level gt 0", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("A", 1, 100));
            players.add(new Player("B", 3, 80));
            players.add(new Player("C", 5, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(allInst(1, 3, 0)); // memberIdx=1(level), cmpOp=3(gt), value=0
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("value"), true);
        });

        test("all level gt 3 (not all)", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("A", 1, 100));
            players.add(new Player("B", 3, 80));
            players.add(new Player("C", 5, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(allInst(1, 3, 3)); // memberIdx=1(level), cmpOp=3(gt), value=3
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("value"), false);
        });

        test("all on empty pipe", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(allInst(1, 3, 0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("value"), true);
        });

        // ---- 17. FIND 操作 ----
        System.out.println("\nFIND 操作");
        test("find first level eq 3", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("A", 1, 100));
            players.add(new Player("B", 3, 80));
            players.add(new Player("C", 5, 60));
            players.add(new Player("D", 3, 90));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(findInst(1, 0, 3)); // memberIdx=1(level), cmpOp=0(eq), value=3
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            Player found = (Player) result.get("value");
            assertEqual(found.name, "B");
        });

        test("find no match", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("A", 1, 100));
            players.add(new Player("B", 3, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(findInst(1, 0, 99));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        // ---- 18. 完整管道: PIPE -> FILTER -> SELECT -> TAKE -> EXEC ----
        System.out.println("\n完整管道");
        test("PIPE -> FILTER -> SELECT -> TAKE -> EXEC", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 3, 80));
            players.add(new Player("Carol", 7, 60));
            players.add(new Player("Dave", 1, 90));
            players.add(new Player("Eve", 5, 70));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));             // 管道所有玩家
            insts.add(filterGt(new int[]{}, 1, 2));       // 过滤 level > 2 → Alice(10), Bob(3), Carol(7), Eve(5)
            insts.add(selectInst(new int[]{0}));           // 选择 name 字段
            insts.add(takeInst(2));                        // 取前 2 个 → Alice, Bob
            insts.add(execInst());                         // 执行
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
            assertEqual(value.get(0), "Alice");
            assertEqual(value.get(1), "Bob");
        });

        test("PIPE -> FILTER -> SORT -> SELECT -> DROP -> TAKE -> EXEC", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Charlie", 5, 60));
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 3, 80));
            players.add(new Player("Eve", 7, 70));
            players.add(new Player("Dave", 1, 90));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));             // 管道所有玩家
            insts.add(filterGt(new int[]{}, 1, 2));       // 过滤 level > 2 → Alice(10), Charlie(5), Bob(3), Eve(7)
            insts.add(sortInst(new int[]{1}));             // 按 level 排序 → Bob(3), Charlie(5), Eve(7), Alice(10)
            insts.add(selectInst(new int[]{0}));           // 选择 name → Bob, Charlie, Eve, Alice
            insts.add(dropInst(1));                        // 丢弃第 1 个 → Charlie, Eve, Alice
            insts.add(takeInst(2));                        // 取前 2 个 → Charlie, Eve
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
            assertEqual(value.get(0), "Charlie");
            assertEqual(value.get(1), "Eve");
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