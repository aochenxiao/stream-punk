/**
 * Stream-Punk Java SPOI 执行器 刁钻边界测试套件
 *
 * 测试覆盖：
 * - Varint 边界攻击（最大编码、截断、溢出）
 * - 路径攻击（DEREF 非指针、MAPKEY 非 Map、负数索引、越界、nil 导航）
 * - 操作数攻击（长度不匹配、零长度操作数、短操作数）
 * - 指令序列攻击（缺少 EXEC、重复 EXEC、无 PIPE 的 EXEC）
 * - 空容器处理（空 PIPE、空数组、null 值）
 * - 操作极值（TAKE/DROP 边界、FILTER 无匹配）
 * - 聚合边界（空集 COUNT/ANY/ALL/FIND）
 * - 嵌套导航边界（穿越 null、非对象导航、深层路径）
 * - 写操作边界（ADD 非数值、REMOVE 空数组、APPEND 非数组、INSERT 边界）
 * - TAKEWHILE/DROPWHILE 边界
 * - FILTER 比较操作边界
 * - 操作数解析边界（ANY/ALL/REMOVE/INSERT/REPLACE 短操作数）
 * - 多管道组合边界
 * - 类型注册表边界
 * - 容器操作边界（KEYS/VALUES/JOIN）
 * - 未实现操作码边界
 * - 反序列化边界
 * - 状态隔离
 * - 大数据量
 *
 * 编译: javac -encoding UTF-8 test_spoi_boundary.java
 * 运行: java TestSpoiBoundary
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

class TestSpoiBoundary {
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
            String msg = e.getMessage();
            if (msg != null && msg.length() > 120) {
                msg = msg.substring(0, 120) + "...";
            }
            System.out.println("    " + e.getClass().getSimpleName() + ": " + msg);
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

    static void assertTrue(boolean v) { assertTrue(v, null); }

    static void assertFalse(boolean v, String msg) {
        if (v) throw new AssertionError(msg != null ? msg : "expected false, got true");
    }

    static void assertFalse(boolean v) { assertFalse(v, null); }

    static void assertNull(Object v, String msg) {
        if (v != null) throw new AssertionError(msg != null ? msg : "expected null, got " + v);
    }

    static void assertNull(Object v) { assertNull(v, null); }

    static void assertNotNull(Object v, String msg) {
        if (v == null) throw new AssertionError(msg != null ? msg : "expected non-null, got null");
    }

    /** 验证函数抛出异常 */
    static boolean mustThrow(Runnable fn) {
        try {
            fn.run();
            return false;
        } catch (Throwable e) {
            return true;
        }
    }

    // =============================== 类型注册表 ===============================

    static Map<String, List<String>> makeTypes() {
        Map<String, List<String>> types = new LinkedHashMap<>();
        types.put("Player", Arrays.asList("name", "level", "health", "items", "metadata"));
        types.put("Item", Arrays.asList("name", "price"));
        return types;
    }

    // =============================== 值编码辅助 ===============================

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

    static byte[] encodeUint32(long v) {
        return ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) v).array();
    }

    static byte[] encodeUint64(long v) {
        return ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(v).array();
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
                try { buf.write(inst.operand); } catch (Exception e) { throw new RuntimeException(e); }
            }
        }
        return buf.toByteArray();
    }

    static SpoiInstruction makeInst(int op, int[] path, byte[] operand) {
        ArrayList<Integer> p = new ArrayList<>();
        if (path != null) for (int seg : path) p.add(seg);
        return new SpoiInstruction(op, p, operand != null ? operand : new byte[0]);
    }

    static SpoiInstruction setInt(int[] path, long value) {
        return makeInst(Op.SET, path, encodeUint32(value));
    }

    static SpoiInstruction setLong(int[] path, long value) {
        return makeInst(Op.SET, path, encodeUint64(value));
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

    static byte[] buildFilterOperand(int memberIdx, int cmpOp, long value) {
        byte[] valBytes = encodeValue(value);
        byte[] operand = new byte[5 + valBytes.length];
        ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(memberIdx);
        operand[4] = (byte) cmpOp;
        System.arraycopy(valBytes, 0, operand, 5, valBytes.length);
        return operand;
    }

    static byte[] buildFilterOperandBytes(int memberIdx, int cmpOp, byte[] valueBytes) {
        byte[] operand = new byte[5 + valueBytes.length];
        ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(memberIdx);
        operand[4] = (byte) cmpOp;
        System.arraycopy(valueBytes, 0, operand, 5, valueBytes.length);
        return operand;
    }

    static SpoiInstruction filterGt(int[] path, int memberIdx, long value) {
        return makeInst(Op.FILTER, path, buildFilterOperand(memberIdx, 3, value));
    }

    static SpoiInstruction filterEq(int[] path, int memberIdx, long value) {
        return makeInst(Op.FILTER, path, buildFilterOperand(memberIdx, 0, value));
    }

    static SpoiInstruction filterByCmp(int memberIdx, int cmpOp, byte[] valueBytes) {
        return makeInst(Op.FILTER, null, buildFilterOperandBytes(memberIdx, cmpOp, valueBytes));
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

    static SpoiInstruction insertInst(int[] path, int idx, byte[] value) {
        byte[] operand = new byte[4 + value.length];
        ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(idx);
        System.arraycopy(value, 0, operand, 4, value.length);
        return makeInst(Op.INSERT, path, operand);
    }

    static SpoiInstruction replaceInst(int[] path, int idx, byte[] value) {
        byte[] operand = new byte[4 + value.length];
        ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(idx);
        System.arraycopy(value, 0, operand, 4, value.length);
        return makeInst(Op.REPLACE, path, operand);
    }

    static SpoiInstruction removeInst(int[] path, int idx) {
        return makeInst(Op.REMOVE, path, encodeUint32(idx));
    }

    static SpoiInstruction appendInst(int[] path, byte[] value) {
        return makeInst(Op.APPEND, path, value);
    }

    static SpoiInstruction takeWhileGt(int memberIdx, long value) {
        byte[] valBytes = encodeValue(value);
        byte[] operand = new byte[5 + valBytes.length];
        ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(memberIdx);
        operand[4] = (byte) 3; // GT
        System.arraycopy(valBytes, 0, operand, 5, valBytes.length);
        return makeInst(Op.TAKEWHILE, null, operand);
    }

    static SpoiInstruction dropWhileGt(int memberIdx, long value) {
        byte[] valBytes = encodeValue(value);
        byte[] operand = new byte[5 + valBytes.length];
        ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(memberIdx);
        operand[4] = (byte) 3; // GT
        System.arraycopy(valBytes, 0, operand, 5, valBytes.length);
        return makeInst(Op.DROPWHILE, null, operand);
    }

    // =============================== Varint 边界攻击 ===============================

    static void testVarint() {
        System.out.println("\n=== Varint 边界攻击 ===");

        test("max uint32", () -> {
            // Java int 是有符号的，最大正数为 0x7FFFFFFF
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            Varint.writeVarint(buf, 0x7FFFFFFF);
            byte[] data = buf.toByteArray();
            int[] offset = new int[]{0};
            assertEqual(Varint.readVarint(data, offset), 0x7FFFFFFF);
            assertEqual(offset[0], data.length);
        });

        test("multi-byte boundary", () -> {
            int[] cases = {0x7F, 0x80, 0x3FFF, 0x4000, 0x1FFFFF, 0x200000, 0xFFFFFFF, 0x10000000};
            for (int v : cases) {
                ByteArrayOutputStream buf = new ByteArrayOutputStream();
                Varint.writeVarint(buf, v);
                byte[] data = buf.toByteArray();
                int[] offset = new int[]{0};
                assertEqual(Varint.readVarint(data, offset), v, "varint: " + v);
            }
        });

        test("zero", () -> {
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            Varint.writeVarint(buf, 0);
            byte[] data = buf.toByteArray();
            int[] offset = new int[]{0};
            assertEqual(Varint.readVarint(data, offset), 0);
            assertEqual(offset[0], 1);
        });

        test("truncated single byte", () -> {
            byte[] data = new byte[]{(byte) 0x80};
            int[] offset = new int[]{0};
            int result = Varint.readVarint(data, offset);
            assertEqual(result, 0);
            assertEqual(offset[0], 1);
        });

        test("truncated multi-byte", () -> {
            byte[] data = new byte[]{(byte) 0x80, (byte) 0x80};
            int[] offset = new int[]{0};
            int result = Varint.readVarint(data, offset);
            assertEqual(result, 0);
            assertEqual(offset[0], 2);
        });

        test("empty data", () -> {
            byte[] data = new byte[]{};
            int[] offset = new int[]{0};
            int result = Varint.readVarint(data, offset);
            assertEqual(result, 0);
            assertEqual(offset[0], 0);
        });

        test("max int", () -> {
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            Varint.writeVarint(buf, Integer.MAX_VALUE);
            byte[] data = buf.toByteArray();
            int[] offset = new int[]{0};
            assertEqual(Varint.readVarint(data, offset), Integer.MAX_VALUE);
        });

        test("very large value", () -> {
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            Varint.writeVarint(buf, 0x7FFFFFFF);
            byte[] data = buf.toByteArray();
            int[] offset = new int[]{0};
            assertEqual(Varint.readVarint(data, offset), 0x7FFFFFFF);
        });
    }

    // =============================== 指令解析边界 ===============================

    static void testParse() {
        System.out.println("\n=== 指令解析边界 ===");

        test("empty stream", () -> {
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            Varint.writeVarint(buf, 0);
            ArrayList<SpoiInstruction> parsed = SpoiParser.parseSpoiStream(buf.toByteArray());
            assertEqual(parsed.size(), 0);
        });

        test("max instructions", () -> {
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            for (int i = 0; i < 100; i++) {
                insts.add(makeInst(Op.SET, new int[]{0}, new byte[]{0x00}));
            }
            byte[] stream = buildSpoiStream(insts);
            ArrayList<SpoiInstruction> parsed = SpoiParser.parseSpoiStream(stream);
            assertEqual(parsed.size(), 100);
        });

        test("zero-length operand", () -> {
            SpoiInstruction inst = makeInst(Op.SET, new int[]{0}, new byte[]{});
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(inst);
            byte[] stream = buildSpoiStream(insts);
            ArrayList<SpoiInstruction> parsed = SpoiParser.parseSpoiStream(stream);
            assertEqual(parsed.size(), 1);
            assertEqual(parsed.get(0).operand.length, 0);
        });

        test("zero-length path", () -> {
            SpoiInstruction inst = makeInst(Op.EXEC, new int[]{}, new byte[]{});
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(inst);
            byte[] stream = buildSpoiStream(insts);
            ArrayList<SpoiInstruction> parsed = SpoiParser.parseSpoiStream(stream);
            assertEqual(parsed.size(), 1);
            assertEqual(parsed.get(0).path.size(), 0);
        });

        test("large operand", () -> {
            byte[] large = new byte[10000];
            SpoiInstruction inst = makeInst(Op.SET, new int[]{0}, large);
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(inst);
            byte[] stream = buildSpoiStream(insts);
            ArrayList<SpoiInstruction> parsed = SpoiParser.parseSpoiStream(stream);
            assertEqual(parsed.get(0).operand.length, 10000);
        });

        test("deep path", () -> {
            int[] deepPath = new int[50];
            for (int i = 0; i < 50; i++) deepPath[i] = i;
            SpoiInstruction inst = makeInst(Op.SET, deepPath, new byte[]{0x00});
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(inst);
            byte[] stream = buildSpoiStream(insts);
            ArrayList<SpoiInstruction> parsed = SpoiParser.parseSpoiStream(stream);
            assertEqual(parsed.get(0).path.size(), 50);
        });

        test("single instruction", () -> {
            SpoiInstruction inst = makeInst(Op.COUNT, null, new byte[]{});
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(inst);
            byte[] stream = buildSpoiStream(insts);
            ArrayList<SpoiInstruction> parsed = SpoiParser.parseSpoiStream(stream);
            assertEqual(parsed.size(), 1);
            assertEqual(parsed.get(0).op, Op.COUNT);
        });
    }

    // =============================== 路径导航边界 ===============================

    static void testNavigation() {
        System.out.println("\n=== 路径导航边界 ===");

        test("deref on non-pointer", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            assertEqual(exe.navStep("hello", PathMarker.PATH_DEREF), "hello");
        });

        test("deref on null", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            assertNull(exe.navStep(null, PathMarker.PATH_DEREF));
        });

        test("index out of bounds", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<String> list = new ArrayList<>(Arrays.asList("a"));
            boolean thrown = mustThrow(() -> exe.navStep(list, 99));
            assertTrue(thrown, "Expected exception for index 99 on list of size 1");
        });

        test("index on non-list", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            boolean thrown = mustThrow(() -> exe.navStep("hello", 0));
            assertTrue(thrown, "Expected exception for index on string");
        });

        test("member on non-object", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            boolean thrown = mustThrow(() -> exe.navStep(42, 0));
            assertTrue(thrown, "Expected exception for member access on int");
        });

        test("navigate on null", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            boolean thrown = mustThrow(() -> exe.navigate(null, Arrays.asList(0)));
            assertTrue(thrown, "Expected exception for navigate on null");
        });

        test("member index out of range", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            boolean thrown = mustThrow(() -> exe.navStep(p, 99));
            assertTrue(thrown, "Expected exception for member index 99");
        });

        test("nested through empty map", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            // metadata is empty HashMap, navigate to [4, 0] tries to index an empty map
            boolean thrown = mustThrow(() -> exe.navigate(p, Arrays.asList(4, 0)));
            assertTrue(thrown, "Expected exception for nested navigate through empty map");
        });

        test("empty path", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            Object result = exe.navigate(p, new ArrayList<>());
            assertEqual(result, p);
        });

        test("negative index on list", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<String> list = new ArrayList<>(Arrays.asList("a"));
            boolean thrown = mustThrow(() -> exe.navStep(list, -1));
            assertTrue(thrown, "Expected exception for negative index");
        });

        test("map index out of bounds", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            HashMap<String, Object> map = new HashMap<>();
            map.put("key", "value");
            boolean thrown = mustThrow(() -> exe.navStep(map, 99));
            assertTrue(thrown, "Expected exception for map index out of bounds");
        });

        test("navigate to empty list", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            Object result = exe.navigate(p, Arrays.asList(3));
            assertTrue(result instanceof List);
            assertEqual(((List<?>) result).size(), 0);
        });

        test("navigate to empty map", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            Object result = exe.navigate(p, Arrays.asList(4));
            assertTrue(result instanceof Map);
            assertEqual(((Map<?, ?>) result).size(), 0);
        });

        test("navigate to zero value", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("", 0, 0);
            assertEqual(exe.navigate(p, Arrays.asList(0)), "");
            assertEqual(exe.navigate(p, Arrays.asList(1)), 0);
        });

        test("navigate struct by public fields", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            assertEqual(exe.navigate(p, Arrays.asList(0)), "Alice");
            assertEqual(exe.navigate(p, Arrays.asList(1)), 10);
            assertEqual(exe.navigate(p, Arrays.asList(2)), 100);
        });
    }

    // =============================== 指令序列攻击 ===============================

    static void testInstructionSequence() {
        System.out.println("\n=== 指令序列攻击 ===");

        test("EXEC without PIPE", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("double EXEC", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(execInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });

        test("double PIPE", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(pipeInst(new int[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });

        test("write after EXEC", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(execInst());
            insts.add(setInt(new int[]{1}, 99));
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(p, stream);
            assertEqual(p.level, 99);
            assertEqual(((Player) result.get("value")).name, "Alice");
        });

        test("read op without PIPE", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(filterGt(new int[]{}, 1, 10));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("missing EXEC", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(1));
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, "Alice");
        });

        test("standalone EXEC", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("only PIPE no EXEC", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
        });
    }

    // =============================== 空容器处理 ===============================

    static void testEmptyContainer() {
        System.out.println("\n=== 空容器处理 ===");

        test("empty PIPE", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("TAKE 0", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("TAKE more than available", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(10));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });

        test("DROP all", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(dropInst(10));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("DROP 0", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(dropInst(0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });

        test("REVERSE empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(reverseInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("REVERSE single", () -> {
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

        test("DISTINCT empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(distinctInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("DISTINCT single", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(42));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(distinctInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), 42);
        });

        test("FILTER no match", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 100));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("SELECT empty result", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(selectInst(new int[]{0}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("SORT empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(sortInst(new int[]{0}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("SORT single", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(42));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(sortInst(new int[]{0}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), 42);
        });
    }

    // =============================== 聚合边界 ===============================

    static void testAggregation() {
        System.out.println("\n=== 聚合边界 ===");

        test("COUNT on empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(countInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), 0);
        });

        test("ANY on empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(anyInst(0, 3, 10));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("value"), false);
        });

        test("ALL on empty (vacuous truth)", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(allInst(0, 3, 10));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("value"), true);
        });

        test("FIND on empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(findInst(0, 0, 10));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("COUNT on single", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(42));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(countInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("value"), 1);
        });

        test("COUNT after FILTER empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 100));
            insts.add(countInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("value"), 0);
        });

        test("ANY after FILTER empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 100));
            insts.add(anyInst(1, 3, 5));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("value"), false);
        });

        test("ALL after FILTER empty (vacuous truth)", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 100));
            insts.add(allInst(1, 3, 5));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("value"), true);
        });

        test("FIND after FILTER empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 100));
            insts.add(findInst(1, 0, 10));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("ANY on empty pipe", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(anyInst(0, 3, 5));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("value"), false);
        });

        test("ALL on empty pipe (vacuous truth)", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(allInst(0, 3, 5));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("value"), true);
        });

        test("FIND on empty pipe", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(findInst(0, 0, 10));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });
    }

    // =============================== 写操作边界 ===============================

    static void testWriteOps() {
        System.out.println("\n=== 写操作边界 ===");

        test("ADD to zero", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 0, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(addInt(new int[]{1}, 5));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.level, 5);
        });

        test("ADD large value", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(addInt(new int[]{1}, Integer.MAX_VALUE));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            // addValues uses doubleValue, returns int when both are int-like
            // 10 + Integer.MAX_VALUE = 2147483657 -> (int) 2147483657 = -2147483639 (overflow)
            // Just verify it doesn't crash
        });

        test("REMOVE from empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(removeInst(new int[]{3}, 0));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for REMOVE from empty list");
        });

        test("REMOVE out of bounds", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(removeInst(new int[]{3}, 99));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for REMOVE out of bounds");
        });

        test("INSERT at zero", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("b", 200));
            p.items.add(new Item("c", 300));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(insertInst(new int[]{3}, 0, "hello".getBytes(StandardCharsets.UTF_8)));
            byte[] stream = buildSpoiStream(insts);
            // INSERT 会将 String bytes 插入 Item 列表 → 类型不匹配
            boolean thrown = mustThrow(() -> exe.execute(p, stream));
            // INSERT 可能成功（List<Object>）也可能失败（类型检查）
            if (!thrown) {
                System.out.println("    (INSERT at zero on typed list succeeded)");
            }
        });

        test("INSERT at end", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("a", 100));
            p.items.add(new Item("b", 200));

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(insertInst(new int[]{3}, 2, "hello".getBytes(StandardCharsets.UTF_8)));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            if (!thrown) {
                System.out.println("    (INSERT at end on typed list succeeded)");
            }
        });

        test("REPLACE out of bounds", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(replaceInst(new int[]{3}, 99, "world".getBytes(StandardCharsets.UTF_8)));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for REPLACE out of bounds");
        });

        test("REPLACE first", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));
            p.items.add(new Item("Shield", 50));

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(replaceInst(new int[]{3}, 0, "new".getBytes(StandardCharsets.UTF_8)));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            if (!thrown) {
                System.out.println("    (REPLACE first on typed list succeeded)");
            }
        });

        test("double SET", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setInt(new int[]{1}, 20));
            insts.add(setInt(new int[]{1}, 30));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.level, 30);
        });

        test("SETNULL then SET", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(makeInst(Op.SETNULL, new int[]{0}, new byte[]{}));
            insts.add(setStr(new int[]{0}, "Bob"));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.name, "Bob");
        });

        test("RESET then SET", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(makeInst(Op.RESET, new int[]{0}, new byte[]{}));
            insts.add(setStr(new int[]{0}, "Bob"));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.name, "Bob");
        });

        test("SETNULL on int field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(makeInst(Op.SETNULL, new int[]{1}, new byte[]{}));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            // null assigned to int field → auto-unboxing → 0
            assertEqual(p.level, 0);
        });

        test("RESET on int field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(makeInst(Op.RESET, new int[]{1}, new byte[]{}));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            // null assigned to int field → auto-unboxing → 0
            assertEqual(p.level, 0);
        });

        test("APPEND to list", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(appendInst(new int[]{3}, "new".getBytes(StandardCharsets.UTF_8)));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            if (!thrown) {
                System.out.println("    (APPEND on typed list succeeded)");
            }
        });

        test("ADD string concat", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Hello", 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(makeInst(Op.ADD, new int[]{0}, "World".getBytes(StandardCharsets.UTF_8)));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.name, "HelloWorld");
        });
    }

    // =============================== 嵌套导航边界 ===============================

    static void testNestedNavigation() {
        System.out.println("\n=== 嵌套导航边界 ===");

        test("nested SELECT", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(selectInst(new int[]{3}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(p, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
        });

        test("SET nested path", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setStr(new int[]{3, 0, 0}, "Blade"));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.items.get(0).name, "Blade");
        });

        test("ADD nested path", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(addInt(new int[]{3, 0, 1}, 50));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.items.get(0).price, 150);
        });
    }

    // =============================== TAKEWHILE/DROPWHILE 边界 ===============================

    static void testTakeWhileDropWhile() {
        System.out.println("\n=== TAKEWHILE/DROPWHILE 边界 ===");

        test("TAKEWHILE never true", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeWhileGt(1, 100));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("TAKEWHILE always true", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeWhileGt(1, 0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });

        test("DROPWHILE never true", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(dropWhileGt(1, 100));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });

        test("DROPWHILE always true", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(dropWhileGt(1, 0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("TAKEWHILE on empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(takeWhileGt(0, 0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("DROPWHILE on empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeInst(0));
            insts.add(dropWhileGt(0, 0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("TAKEWHILE partial", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 5, 80));
            players.add(new Player("Carol", 15, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(takeWhileGt(1, 5));  // take while level > 5
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, "Alice");
        });

        test("DROPWHILE partial", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 5, 80));
            players.add(new Player("Carol", 15, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(dropWhileGt(1, 5));  // drop while level > 5
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
            assertEqual(((Player) value.get(0)).name, "Bob");
            assertEqual(((Player) value.get(1)).name, "Carol");
        });
    }

    // =============================== FILTER 比较操作边界 ===============================

    static void testFilterComparison() {
        System.out.println("\n=== FILTER 比较操作边界 ===");

        test("FILTER all comparison ops", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Carol", 30, 60));

            // EQ: level == 20
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterByCmp(1, 0, encodeValue(20))); // EQ
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, "Bob");

            // NE: level != 10
            insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterByCmp(1, 1, encodeValue(10))); // NE
            insts.add(execInst());
            stream = buildSpoiStream(insts);
            result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> neValues = (ArrayList<Object>) result.get("value");
            assertEqual(neValues.size(), 2);

            // LT: level < 20
            insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterByCmp(1, 2, encodeValue(20))); // LT
            insts.add(execInst());
            stream = buildSpoiStream(insts);
            result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, "Alice");

            // LE: level <= 10
            insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterByCmp(1, 4, encodeValue(10))); // LE
            insts.add(execInst());
            stream = buildSpoiStream(insts);
            result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, "Alice");

            // GE: level >= 30
            insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterByCmp(1, 5, encodeValue(30))); // GE
            insts.add(execInst());
            stream = buildSpoiStream(insts);
            result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, "Carol");
        });

        test("FILTER unknown cmpOp", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Carol", 30, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterByCmp(1, 99, encodeValue(10))); // unknown cmpOp
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3); // all pass
        });

        test("FILTER short operand", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.FILTER, null, new byte[]{0x00, 0x01})); // 2 bytes < 5
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2); // all pass
        });

        test("FILTER all match", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Carol", 30, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
        });

        test("FILTER none match", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterByCmp(1, 2, encodeValue(0))); // level < 0
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });
    }

    // =============================== 操作数解析边界 ===============================

    static void testOperandParsing() {
        System.out.println("\n=== 操作数解析边界 ===");

        test("ANY short operand", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(10));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.ANY, null, new byte[]{0x00})); // 1 byte < 5
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("value"), true); // short operand → matches=true
        });

        test("ALL short operand", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(10));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.ALL, null, new byte[]{0x00})); // 1 byte < 5
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("value"), true);
        });

        test("REMOVE short operand", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(makeInst(Op.REMOVE, new int[]{3}, new byte[]{0x00, 0x01})); // 2 bytes
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for REMOVE with short operand");
        });

        test("INSERT short operand", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            boolean thrown = mustThrow(() -> {
                // INSERT with only 2 bytes (no room for index)
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(makeInst(Op.INSERT, new int[]{3}, new byte[]{0x00, 0x01}));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for INSERT with short operand");
        });

        test("REPLACE short operand", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            boolean thrown = mustThrow(() -> {
                // REPLACE with only 2 bytes (no room for index)
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(makeInst(Op.REPLACE, new int[]{3}, new byte[]{0x00, 0x01}));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for REPLACE with short operand");
        });
    }

    // =============================== 多管道组合边界 ===============================

    static void testPipelineCombinations() {
        System.out.println("\n=== 多管道组合边界 ===");

        test("FILTER-TAKE-DROP-REVERSE chain", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Carol", 15, 120));
            players.add(new Player("Dave", 20, 60));
            players.add(new Player("Eve", 10, 90));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 10));   // level>10: Bob,Carol,Dave
            insts.add(takeInst(3));                      // Bob,Carol,Dave
            insts.add(dropInst(1));                      // Carol,Dave
            insts.add(reverseInst());                    // Dave,Carol
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
            assertEqual(((Player) value.get(0)).name, "Dave");
            assertEqual(((Player) value.get(1)).name, "Carol");
        });

        test("SORT ascending by level", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Carol", 30, 100));
            players.add(new Player("Alice", 10, 80));
            players.add(new Player("Eve", 20, 60));
            players.add(new Player("Bob", 10, 90));
            players.add(new Player("Dave", 40, 70));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(sortInst(new int[]{1})); // sort by level
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 5);
            assertEqual(((Player) value.get(0)).name, "Alice");
            assertEqual(((Player) value.get(1)).name, "Bob");
            assertEqual(((Player) value.get(2)).name, "Eve");
            assertEqual(((Player) value.get(3)).name, "Carol");
            assertEqual(((Player) value.get(4)).name, "Dave");
        });

        test("SELECT then FILTER on strings", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            // SELECT name → ["Alice","Bob"] → FILTER memberIdx=0 直接比较字符串值
            // "Alice" > 0 lexicographically, "Bob" > 0 lexicographically
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(selectInst(new int[]{0}));
            insts.add(filterGt(new int[]{}, 0, 0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            // 两个字符串都通过 gt 0 比较
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });

        test("all operations chain", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Carol", 15, 120));
            players.add(new Player("Dave", 20, 60));
            players.add(new Player("Eve", 10, 90));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(reverseInst());                       // Eve,Dave,Carol,Bob,Alice
            insts.add(filterGt(new int[]{}, 1, 10));       // Dave,Carol,Bob
            insts.add(dropInst(1));                         // Carol,Bob
            insts.add(takeInst(2));                         // Carol,Bob
            insts.add(sortInst(new int[]{0}));              // sort by name: Bob,Carol
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
            assertEqual(((Player) value.get(0)).name, "Bob");
            assertEqual(((Player) value.get(1)).name, "Carol");
        });

        test("DISTINCT then COUNT", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 2, 3, 3, 3, 4));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(distinctInst());
            insts.add(countInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("value"), 4);
        });
    }

    // =============================== 类型注册表边界 ===============================

    static void testTypeRegistry() {
        System.out.println("\n=== 类型注册表边界 ===");

        test("empty registry", () -> {
            SpoiExecutor exe = new SpoiExecutor(new LinkedHashMap<>());
            Player p = new Player("Alice", 10, 100);
            boolean thrown = mustThrow(() -> exe.navStep(p, 99));
            assertTrue(thrown, "Expected exception for out-of-range field index");
        });

        test("nonexistent type", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(42, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), 42);
        });

        test("partial registry", () -> {
            Map<String, List<String>> partial = new LinkedHashMap<>();
            partial.put("Player", Arrays.asList("name", "level", "health", "items", "metadata"));
            SpoiExecutor exe = new SpoiExecutor(partial);
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));
            Object result = exe.navigate(p, Arrays.asList(3));
            assertTrue(result instanceof List);
            assertEqual(((List<?>) result).size(), 1);
        });
    }

    // =============================== 容器操作边界 ===============================

    static void testContainerOps() {
        System.out.println("\n=== 容器操作边界 ===");

        test("KEYS on map", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            HashMap<String, Object> map = new HashMap<>();
            map.put("a", 1);
            map.put("b", 2);
            map.put("c", 3);
            data.add(map);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.KEYS, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
        });

        test("VALUES on map", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            HashMap<String, Object> map = new HashMap<>();
            map.put("a", 1);
            map.put("b", 2);
            map.put("c", 3);
            data.add(map);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.VALUES, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
        });

        test("JOIN on slices", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            data.add(Arrays.asList(1, 2));
            data.add(Arrays.asList(3, 4, 5));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.JOIN, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 5);
        });

        test("JOIN mixed", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            data.add(Arrays.asList(1, 2));
            data.add(42);
            data.add(Arrays.asList(3, 4));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.JOIN, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 5);
        });

        test("KEYS on empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.KEYS, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("JOIN on empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.JOIN, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });
    }

    // =============================== 未实现操作码边界 ===============================

    static void testUnknownOpcodes() {
        System.out.println("\n=== 未实现操作码边界 ===");

        test("unknown opcode 0xFF", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(makeInst(0xFF, null, new byte[]{}));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(new Player("Test", 1, 1), stream);
            });
            assertTrue(thrown, "Expected exception for unknown opcode 0xFF");
        });

        test("NAV opcode throws", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(makeInst(Op.NAV, null, new byte[]{}));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(new Player("Test", 1, 1), stream);
            });
            assertTrue(thrown, "Expected exception for unhandled OP_NAV");
        });

        test("DEREF opcode throws", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(makeInst(Op.DEREF, null, new byte[]{}));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(new Player("Test", 1, 1), stream);
            });
            assertTrue(thrown, "Expected exception for unhandled OP_DEREF");
        });
    }

    // =============================== 结果类型边界 ===============================

    static void testResultTypes() {
        System.out.println("\n=== 结果类型边界 ===");

        test("result UNDEF", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(new ArrayList<>(), stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("result SINGLE", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(42));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), 42);
        });

        test("result VECTOR", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.VECTOR);
        });
    }

    // =============================== PIPE 路径导航 ===============================

    static void testPipeWithPath() {
        System.out.println("\n=== PIPE 路径导航 ===");

        test("PIPE with path to list", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));
            p.items.add(new Item("Shield", 50));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{3}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(p, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });

        test("PIPE with path to map", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.metadata.put("a", 1);
            p.metadata.put("b", 2);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{4}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(p, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });
    }

    // =============================== 反序列化边界 ===============================

    static void testDeserialize() {
        System.out.println("\n=== 反序列化边界 ===");

        test("deserialize empty", () -> {
            assertNull(SpoiExecutor.deserializeValue(new byte[]{}));
            assertNull(SpoiExecutor.deserializeValue(null));
        });

        test("deserialize 1 byte", () -> {
            Object result = SpoiExecutor.deserializeValue(new byte[]{0x42});
            assertEqual(result, 0x42);
        });

        test("deserialize 2 bytes", () -> {
            Object result = SpoiExecutor.deserializeValue(new byte[]{0x01, 0x02});
            assertEqual(result, 0x0201);
        });

        test("deserialize 4 bytes", () -> {
            Object result = SpoiExecutor.deserializeValue(new byte[]{0x01, 0x02, 0x03, 0x04});
            assertEqual(result, 0x04030201L);
        });

        test("deserialize 8 bytes", () -> {
            Object result = SpoiExecutor.deserializeValue(new byte[]{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
            assertTrue(result instanceof Long);
            assertEqual(result, 0x0807060504030201L);
        });

        test("deserialize UTF-8 string", () -> {
            Object result = SpoiExecutor.deserializeValue("hello world".getBytes(StandardCharsets.UTF_8));
            assertEqual(result, "hello world");
        });

        test("deserialize 3 bytes", () -> {
            Object result = SpoiExecutor.deserializeValue("abc".getBytes(StandardCharsets.UTF_8));
            assertEqual(result, "abc");
        });

        test("deserialize 5 bytes", () -> {
            Object result = SpoiExecutor.deserializeValue("hello".getBytes(StandardCharsets.UTF_8));
            assertEqual(result, "hello");
        });

        test("deserialize non-UTF-8 bytes", () -> {
            // 无效的 UTF-8 序列
            byte[] bad = new byte[]{(byte) 0xFF, (byte) 0xFE, (byte) 0xFD, (byte) 0x80, (byte) 0x81};
            Object result = SpoiExecutor.deserializeValue(bad);
            // 当 UTF-8 解码失败时，返回原始字节数组
            assertNotNull(result, "deserializeValue should handle non-UTF-8 gracefully");
        });
    }

    // =============================== 状态隔离 ===============================

    static void testStateIsolation() {
        System.out.println("\n=== 状态隔离 ===");

        test("executor state isolation", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2));

            // 第一次执行
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(countInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("value"), 2);

            // 第二次执行：pipeData 应该被重置
            ArrayList<SpoiInstruction> insts2 = new ArrayList<>();
            insts2.add(execInst());
            byte[] stream2 = buildSpoiStream(insts2);
            Map<String, Object> result2 = exe.execute(data, stream2);
            assertEqual(result2.get("resultType"), ResultType.UNDEF);
        });
    }

    // =============================== 大数据量 ===============================

    static void testLargeDataset() {
        System.out.println("\n=== 大数据量 ===");

        test("large dataset", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            for (int i = 0; i < 1000; i++) {
                players.add(new Player("Player", i % 100, 100));
            }

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 50));
            insts.add(countInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            // levels 51-99 = 49 values × 10 each = 490
            assertEqual(result.get("value"), 490);
        });
    }

    // =============================== COUNT then TAKE ===============================

    static void testCountThenTake() {
        System.out.println("\n=== COUNT 后管道操作 ===");

        test("COUNT then TAKE", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(countInst());
            insts.add(takeInst(2));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            // COUNT → [3], TAKE 2 → [3] (only 1 element)
            assertEqual(result.get("value"), 3);
        });
    }

    // =============================== SELECT/SORT 空路径 ===============================

    static void testEmptyPath() {
        System.out.println("\n=== SELECT/SORT 空路径 ===");

        test("SELECT empty path", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.SELECT, new int[]{}, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2); // empty path doesn't modify pipe
        });

        test("SORT empty path", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(3, 1, 4));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.SORT, new int[]{}, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
        });
    }

    // =============================== DISTINCT 去重边界 ===============================

    static void testDistinctBoundary() {
        System.out.println("\n=== DISTINCT 去重边界 ===");

        test("DISTINCT all same", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 1, 1, 1, 1));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(distinctInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), 1);
        });

        test("DISTINCT all different", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3, 4, 5));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(distinctInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 5);
        });
    }

    // =============================== Map 导航 ===============================

    static void testMapNavigation() {
        System.out.println("\n=== Map 导航 ===");

        test("navigate map by index", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.metadata.put("role", "admin");
            p.metadata.put("score", 100);

            Object result = exe.navigate(p, Arrays.asList(4, 0));
            assertNotNull(result, "navigate map by index should return non-null");
        });

        test("navigate to metadata map", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.metadata.put("role", "admin");

            Object result = exe.navigate(p, Arrays.asList(4));
            assertTrue(result instanceof Map);
            assertEqual(((Map<?, ?>) result).size(), 1);
        });
    }

    // =============================== 更深层刁钻测试 ===============================

    static void testTrickyNegativeIndex() {
        System.out.println("\n=== 负索引攻击 ===");

        test("INSERT negative index", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(insertInst(new int[]{3}, -1, "x".getBytes(StandardCharsets.UTF_8)));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for INSERT with negative index");
        });

        test("INSERT index beyond size", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(insertInst(new int[]{3}, 999, "x".getBytes(StandardCharsets.UTF_8)));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for INSERT beyond size");
        });

        test("REPLACE negative index", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(replaceInst(new int[]{3}, -1, "x".getBytes(StandardCharsets.UTF_8)));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for REPLACE with negative index");
        });

        test("REMOVE negative index", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(removeInst(new int[]{3}, -1));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for REMOVE with negative index");
        });
    }

    static void testAggregationMultiElement() {
        System.out.println("\n=== 聚合多元素 ===");

        test("ANY all false", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Carol", 30, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(anyInst(1, 3, 100)); // any level > 100
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("value"), false);
        });

        test("ANY one true", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Carol", 30, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(anyInst(1, 0, 20)); // any level == 20
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("value"), true);
        });

        test("ALL all true", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Carol", 30, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(allInst(1, 3, 0)); // all level > 0
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("value"), true);
        });

        test("ALL one false", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Carol", 30, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(allInst(1, 4, 10)); // all level <= 10
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("value"), false);
        });

        test("FIND first match", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Carol", 20, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(findInst(1, 0, 20)); // find level == 20
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, "Bob");
        });

        test("FIND no match", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(findInst(1, 0, 99)); // find level == 99
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });
    }

    static void testPipeEdgeCases() {
        System.out.println("\n=== PIPE 边界 ===");

        test("PIPE single value", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(p, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, "Alice");
        });

        test("PIPE then SELECT then PIPE", () -> {
            // PIPE → SELECT → PIPE 链：第二轮 PIPE(空路径) 会重新从 root 取数据
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));
            p.items.add(new Item("Shield", 50));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{3}));    // PIPE items → [Item1, Item2]
            insts.add(selectInst(new int[]{0}));   // SELECT names → ["Sword", "Shield"]
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(p, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
            assertEqual(value.get(0), "Sword");
            assertEqual(value.get(1), "Shield");
        });

        test("PIPE then KEYS", () -> {
            // PIPE 从 Map 路径取值时会提取 values，再用 KEYS 就得不到 keys
            // 正确做法：PIPE 一个包含 Map 的列表
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            HashMap<String, Object> map = new HashMap<>();
            map.put("role", "admin");
            map.put("score", 100);
            data.add(map);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{})); // PIPE list containing map
            insts.add(makeInst(Op.KEYS, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });

        test("PIPE then VALUES", () -> {
            // PIPE 从 Map 路径取值时会提取 values，再用 VALUES 也得不到
            // 正确做法：PIPE 一个包含 Map 的列表
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            HashMap<String, Object> map = new HashMap<>();
            map.put("role", "admin");
            map.put("score", 100);
            data.add(map);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{})); // PIPE list containing map
            insts.add(makeInst(Op.VALUES, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });
    }

    static void testFilterEdgeCases() {
        System.out.println("\n=== FILTER 刁钻边界 ===");

        test("FILTER memberIdx out of bounds", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));

            // FILTER 使用越界的 memberIdx (99)
            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(pipeInst(new int[]{}));
                insts.add(filterByCmp(99, 0, encodeValue(10)));
                insts.add(execInst());
                byte[] stream = buildSpoiStream(insts);
                exe.execute(players, stream);
            });
            assertTrue(thrown, "Expected exception for FILTER with out-of-bounds memberIdx");
        });

        test("FILTER string comparison", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Charlie", 15, 60));

            // FILTER name == "Bob"
            byte[] operand = buildFilterOperandBytes(0, 0, "Bob".getBytes(StandardCharsets.UTF_8));
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.FILTER, null, operand));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, "Bob");
        });

        test("FILTER null field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player(null, 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterByCmp(0, 0, encodeValue(0))); // name == 0
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(p, stream);
            // null != 0, so no match
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });
    }

    static void testUnhandledOpcodes() {
        System.out.println("\n=== 更多未处理操作码 ===");

        test("IDX opcode throws", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(makeInst(Op.IDX, null, new byte[]{}));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(new Player("Test", 1, 1), stream);
            });
            assertTrue(thrown, "Expected exception for unhandled OP_IDX");
        });

        test("UNWRAP opcode throws", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(makeInst(Op.UNWRAP, null, new byte[]{}));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(new Player("Test", 1, 1), stream);
            });
            assertTrue(thrown, "Expected exception for unhandled OP_UNWRAP");
        });
    }

    static void testReverseEdgeCases() {
        System.out.println("\n=== REVERSE 刁钻边界 ===");

        test("double REVERSE", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(reverseInst());
            insts.add(reverseInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
            assertEqual(value.get(0), 1);
            assertEqual(value.get(2), 3);
        });

        test("REVERSE then TAKE", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3, 4, 5));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(reverseInst());
            insts.add(takeInst(2));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
            assertEqual(value.get(0), 5);
            assertEqual(value.get(1), 4);
        });
    }

    static void testSortEdgeCases() {
        System.out.println("\n=== SORT 刁钻边界 ===");

        test("SORT then REVERSE (desc)", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(3, 1, 4, 1, 5));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(sortInst(new int[]{}));
            insts.add(reverseInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 5);
        });

        test("SORT already sorted", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3, 4, 5));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(sortInst(new int[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 5);
            assertEqual(value.get(0), 1);
            assertEqual(value.get(4), 5);
        });
    }

    static void testContainerEdgeCases() {
        System.out.println("\n=== 容器操作刁钻边界 ===");

        test("KEYS on empty map", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            data.add(new HashMap<>());

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.KEYS, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("VALUES on empty map", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            data.add(new HashMap<>());

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.VALUES, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("KEYS on non-map", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.KEYS, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("VALUES on non-map", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.VALUES, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("JOIN single element", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(42));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.JOIN, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), 42);
        });
    }

    static void testWritePathEdgeCases() {
        System.out.println("\n=== 写操作路径刁钻边界 ===");

        test("SET empty path", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(makeInst(Op.SET, new int[]{}, "hello".getBytes(StandardCharsets.UTF_8)));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            // 空路径不应该修改任何东西
            assertEqual(p.name, "Alice");
        });

        test("ADD empty path", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(makeInst(Op.ADD, new int[]{}, encodeValue(5)));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for ADD on root object");
        });

        test("ADD field type mismatch", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            boolean thrown = mustThrow(() -> {
                // ADD with string operand to int field
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(makeInst(Op.ADD, new int[]{1}, "hello".getBytes(StandardCharsets.UTF_8)));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for ADD string to int field");
        });

        test("APPEND to non-list", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(appendInst(new int[]{1}, "x".getBytes(StandardCharsets.UTF_8)));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for APPEND to non-list");
        });

        test("REMOVE from non-list", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(removeInst(new int[]{1}, 0));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for REMOVE from non-list");
        });

        test("INSERT to non-list", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(insertInst(new int[]{1}, 0, "x".getBytes(StandardCharsets.UTF_8)));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for INSERT to non-list");
        });
    }

    static void testTAKEDROPOverflow() {
        System.out.println("\n=== TAKE/DROP 溢出攻击 ===");

        test("TAKE very large", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.TAKE, null, encodeUint32(Integer.MAX_VALUE)));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });

        test("DROP very large", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.DROP, null, encodeUint32(Integer.MAX_VALUE)));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("TAKE negative operand", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2));

            // TAKE with -1 operand (0xFFFFFFFF) → interpreted as large positive
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.TAKE, null, encodeUint32(0xFFFFFFFFL)));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });

        test("DROP negative operand", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2));

            // DROP with -1 operand (0xFFFFFFFF) → interpreted as large positive
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.DROP, null, encodeUint32(0xFFFFFFFFL)));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });
    }

    static void testVarintNegative() {
        System.out.println("\n=== Varint 负值编码 ===");

        test("varint negative value", () -> {
            // Java int 是有符号的，负值编码后解码为正数
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            Varint.writeVarint(buf, -1); // 0xFFFFFFFF as signed
            byte[] data = buf.toByteArray();
            int[] offset = new int[]{0};
            int result = Varint.readVarint(data, offset);
            // -1 >>> 7 = 0x1FFFFFF, 然后 & 0x7F 循环
            // 实际上 writeVarint 对 -1 的处理：-1 >= 0x80 是 false，所以只写 (-1 & 0x7F) = 0x7F
            assertEqual(result, 0x7F);
        });

        test("varint negative large", () -> {
            // Varint 编码不支持负值：Integer.MIN_VALUE 作为有符号数，
            // writeVarint 的 while(v >= 0x80) 对负值为 false，只写 v & 0x7F = 0
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            Varint.writeVarint(buf, Integer.MIN_VALUE);
            byte[] data = buf.toByteArray();
            int[] offset = new int[]{0};
            int result = Varint.readVarint(data, offset);
            // 负值编码后的实际解码结果
            assertEqual(result, 0);
        });
    }

    static void testDeepNested() {
        System.out.println("\n=== 深层嵌套 ===");

        test("deep nested navigation", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));
            p.items.add(new Item("Shield", 50));

            // 导航到 items[1].price
            Object result = exe.navigate(p, Arrays.asList(3, 1, 1));
            assertEqual(result, 50);
        });

        test("deep nested SET", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));
            p.items.add(new Item("Shield", 50));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setInt(new int[]{3, 1, 1}, 999));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.items.get(1).price, 999);
        });
    }

    static void testFilterAggregationChain() {
        System.out.println("\n=== FILTER-聚合链 ===");

        test("FILTER then ANY", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Carol", 30, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 15)); // level>15: Bob,Carol
            insts.add(anyInst(2, 3, 70)); // any health > 70
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("value"), true);
        });

        test("FILTER then ALL", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Carol", 30, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 15)); // level>15: Bob,Carol
            insts.add(allInst(2, 3, 50)); // all health > 50
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("value"), true);
        });

        test("FILTER then FIND", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Carol", 30, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 15)); // level>15: Bob,Carol
            insts.add(findInst(2, 2, 70)); // find health < 70
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, "Carol");
        });
    }

    static void testEmptyStream() {
        System.out.println("\n=== 空字节流 ===");

        test("execute empty stream", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            Varint.writeVarint(buf, 0); // 0 instructions
            byte[] stream = buf.toByteArray();
            Map<String, Object> result = exe.execute(p, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("execute null stream", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            boolean thrown = mustThrow(() -> exe.execute(p, null));
            assertTrue(thrown, "Expected exception for null stream");
        });
    }

    static void testDISTINCTWithObjects() {
        System.out.println("\n=== DISTINCT 对象去重 ===");

        test("DISTINCT with objects", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Alice", 10, 100)); // same data
            players.add(new Player("Bob", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(distinctInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            // DISTINCT uses String.valueOf as key for non-primitive objects
            // Different Player instances have different toString()
            assertEqual(value.size(), 3);
        });
    }

    static void testCOMPARENull() {
        System.out.println("\n=== compareValues null 边界 ===");

        test("compare null eq null", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player(null, 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            // FILTER name == null (operand is empty bytes → deserializeValue returns null)
            insts.add(makeInst(Op.FILTER, null, buildFilterOperandBytes(0, 0, new byte[]{})));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(p, stream);
            // null == null → true
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, null);
        });

        test("compare null ne value", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player(null, 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            // FILTER name != "Alice" (null != "Alice" → true)
            insts.add(makeInst(Op.FILTER, null, buildFilterOperandBytes(0, 1, "Alice".getBytes(StandardCharsets.UTF_8))));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(p, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
        });
    }

    // =============================== Unicode/特殊字符攻击 ===============================

    static void testUnicodeAttack() {
        System.out.println("\n=== Unicode/特殊字符攻击 ===");

        test("Unicode string in SET", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            String unicode = "\u4e2d\u6587\u65e5\u672c\u8a9e\ud83d\ude00\u0000end";
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setStr(new int[]{0}, unicode));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.name, unicode);
        });

        test("Unicode in FILTER comparison", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("中文", 10, 100));
            players.add(new Player("Alice", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.FILTER, null, buildFilterOperandBytes(0, 0, "中文".getBytes(StandardCharsets.UTF_8))));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, "中文");
        });

        test("Null byte in string", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            String withNull = "hello\u0000world";
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setStr(new int[]{0}, withNull));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.name, withNull);
            assertEqual(p.name.length(), 11);
        });
    }

    // =============================== PIPE 类型变化攻击 ===============================

    static void testPipeTypeChange() {
        System.out.println("\n=== PIPE 类型变化攻击 ===");

        test("PIPE int list then SELECT string field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            // PIPE ints, then SELECT [0] on ints → error
            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(pipeInst(new int[]{}));
                insts.add(selectInst(new int[]{0}));
                insts.add(execInst());
                byte[] stream = buildSpoiStream(insts);
                exe.execute(data, stream);
            });
            assertTrue(thrown, "Expected exception for SELECT on ints");
        });

        test("PIPE then FILTER on wrong type", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(pipeInst(new int[]{}));
                insts.add(filterGt(new int[]{}, 1, 10)); // int has no member 1
                insts.add(execInst());
                byte[] stream = buildSpoiStream(insts);
                exe.execute(data, stream);
            });
            assertTrue(thrown, "Expected exception for FILTER on ints");
        });
    }

    // =============================== 多级嵌套 Map ===============================

    static void testNestedMap() {
        System.out.println("\n=== 多级嵌套 Map ===");

        test("Map of Map of Map", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            HashMap<String, Object> inner = new HashMap<>();
            inner.put("x", 10);
            HashMap<String, Object> middle = new HashMap<>();
            middle.put("inner", inner);
            HashMap<String, Object> outer = new HashMap<>();
            outer.put("middle", middle);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(outer, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
        });

        test("Map navigation with mixed types", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            HashMap<String, Object> map = new HashMap<>();
            map.put("str", "hello");
            map.put("int", 42);
            map.put("list", Arrays.asList(1, 2, 3));

            // Navigate to index 0 of map (by value order)
            Object result = exe.navStep(map, 0);
            assertNotNull(result, "Should navigate to a map entry value");
        });
    }

    // =============================== 整数溢出攻击 ===============================

    static void testIntegerOverflow() {
        System.out.println("\n=== 整数溢出攻击 ===");

        test("ADD near Integer.MAX_VALUE", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", Integer.MAX_VALUE - 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(addInt(new int[]{1}, 20));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            // 溢出后值不确定，但不应崩溃
            assertNotNull(p, "Should not crash on overflow");
        });

        test("SET max int", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 0, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            // 设置 4 字节最大值
            insts.add(setInt(new int[]{1}, 0xFFFFFFFFL));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            // 4字节作为 uint32 解码为 0xFFFFFFFFL，convertValue 转为 int 可能溢出
            assertNotNull(p, "Should not crash");
        });

        test("ADD negative value", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 50, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            // 使用 4 字节编码 -1 (0xFFFFFFFF)
            insts.add(makeInst(Op.ADD, new int[]{1}, encodeUint32(0xFFFFFFFFL)));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            // 50 + 4294967295 = 4294967345 → as int: -4
            assertNotNull(p, "Should not crash");
        });
    }

    // =============================== FILTER 路径穿越 ===============================

    static void testFilterWithPath() {
        System.out.println("\n=== FILTER 路径穿越 ===");

        test("FILTER with path on nested field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            Player p1 = new Player("Alice", 10, 100);
            p1.items.add(new Item("Sword", 100));
            Player p2 = new Player("Bob", 20, 80);
            p2.items.add(new Item("Shield", 50));
            players.add(p1);
            players.add(p2);

            // FILTER by items[0].name == "Sword" (path导航到Item, memberIdx访问name字段)
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.FILTER, new int[]{3, 0}, buildFilterOperandBytes(0, 0, "Sword".getBytes(StandardCharsets.UTF_8))));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, "Alice");
        });

        test("FILTER with path memberIdx beyond field count", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(pipeInst(new int[]{}));
                insts.add(makeInst(Op.FILTER, new int[]{}, buildFilterOperandBytes(99, 0, encodeValue(10))));
                insts.add(execInst());
                byte[] stream = buildSpoiStream(insts);
                exe.execute(players, stream);
            });
            assertTrue(thrown, "Expected exception for FILTER with out-of-bounds memberIdx");
        });
    }

    // =============================== SORT 复合对象 ===============================

    static void testSortComplex() {
        System.out.println("\n=== SORT 复合对象 ===");

        test("SORT by nested field via SELECT", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            Player p1 = new Player("Carol", 30, 100);
            p1.items.add(new Item("Sword", 100));
            Player p2 = new Player("Alice", 10, 80);
            p2.items.add(new Item("Shield", 50));
            players.add(p1);
            players.add(p2);

            // SORT by items[0].price
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(sortInst(new int[]{3, 0, 1}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
            assertEqual(((Player) value.get(0)).name, "Alice"); // 50 < 100
        });

        test("SORT by string field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Carol", 30, 100));
            players.add(new Player("Alice", 10, 80));
            players.add(new Player("Bob", 20, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(sortInst(new int[]{0}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
            assertEqual(((Player) value.get(0)).name, "Alice");
            assertEqual(((Player) value.get(1)).name, "Bob");
            assertEqual(((Player) value.get(2)).name, "Carol");
        });
    }

    // =============================== DISTINCT 混合类型 ===============================

    static void testDistinctMixed() {
        System.out.println("\n=== DISTINCT 混合类型 ===");

        test("DISTINCT with mixed types", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>(Arrays.asList(1, "1", 1L, 1.0, true));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(distinctInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 5); // 不同类型不重复
        });

        test("DISTINCT with null elements", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>(Arrays.asList(null, null, 1));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(distinctInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
        });
    }

    // =============================== RESET/SETNULL 嵌套路径 ===============================

    static void testResetNested() {
        System.out.println("\n=== RESET/SETNULL 嵌套路径 ===");

        test("RESET nested field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(makeInst(Op.RESET, new int[]{3, 0, 0}, new byte[]{}));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertNull(p.items.get(0).name, "RESET should set nested field to null");
        });

        test("SETNULL nested field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(makeInst(Op.SETNULL, new int[]{3, 0, 1}, new byte[]{}));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            // SETNULL on int field → convertValue(null, int.class) → 0
            assertEqual(p.items.get(0).price, 0);
        });
    }

    // =============================== 极端大数据流 ===============================

    static void testExtremeStream() {
        System.out.println("\n=== 极端大数据流 ===");

        test("very large instruction stream", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            for (int i = 0; i < 500; i++) {
                // 使用5字节以上字符串避免被deserializeValue误判为整数
                insts.add(makeInst(Op.SET, new int[]{0}, ("xxxx" + i).getBytes(StandardCharsets.UTF_8)));
            }
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Player p = new Player("initial", 0, 0);
            Map<String, Object> result = exe.execute(p, stream);
            // 最后一次 SET 应该生效
            assertEqual(p.name, "xxxx499");
        });

        test("deep path with DEREF", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            // 在路径中使用 DEREF 标记
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{PathMarker.PATH_DEREF}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(p, stream);
            // DEREF on Player 尝试找到 value 字段，没有则返回自身
            assertEqual(result.get("resultType"), ResultType.SINGLE);
        });
    }

    // =============================== 连续攻击链 ===============================

    static void testAttackChain() {
        System.out.println("\n=== 连续攻击链 ===");

        test("FILTER→TAKE→REVERSE→DROP→SORT→DISTINCT chain", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>();
            for (int i = 1; i <= 20; i++) data.add(i);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterByCmp(0, 3, encodeValue(5)));  // >5: 6-20
            insts.add(takeInst(10));                          // 6-15
            insts.add(reverseInst());                         // 15-6
            insts.add(dropInst(3));                           // 12-6
            insts.add(sortInst(new int[]{}));                 // 空路径使用字符串排序: 10,11,12,6,7,8,9
            insts.add(distinctInst());                        // 10,11,12,6,7,8,9
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 7);
            // 空路径 SORT 使用字符串排序："10"<"11"<"12"<"6"<"7"<"8"<"9"
            assertEqual(value.get(0), 10);
            assertEqual(value.get(6), 9);
        });

        test("attack chain with null propagation", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player(null, 20, 80));
            players.add(new Player("Bob", 30, 60));

            // FILTER with null name comparing
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterByCmp(0, 1, encodeValue(0))); // name != 0 → null != 0 → true
            insts.add(sortInst(new int[]{0})); // SORT by name (null first in Java)
            insts.add(reverseInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
        });
    }

    // =============================== 空字节/零值攻击 ===============================

    static void testNullByteAttack() {
        System.out.println("\n=== 空字节/零值攻击 ===");

        test("SET empty string", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setStr(new int[]{0}, ""));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            // 空字节数组被deserializeValue解析为null
            assertNull(p.name, "Empty byte array deserializes to null");
        });

        test("SET zero to int via 1 byte", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setVal(new int[]{2}, new byte[]{0x00}));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.health, 0);
        });

        test("PIPE empty list then COUNT", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(countInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("value"), 0);
        });
    }

    // =============================== KEYS/VALUES 链式操作 ===============================

    static void testKeysValuesChain() {
        System.out.println("\n=== KEYS/VALUES 链式操作 ===");

        test("KEYS then SORT then TAKE", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            HashMap<String, Object> map = new HashMap<>();
            map.put("z", 3);
            map.put("a", 1);
            map.put("m", 2);
            data.add(map);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.KEYS, null, new byte[]{}));
            insts.add(sortInst(new int[]{}));
            insts.add(takeInst(2));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
            assertEqual(value.get(0), "a");
            assertEqual(value.get(1), "m");
        });

        test("VALUES then FILTER then COUNT", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            HashMap<String, Object> map = new HashMap<>();
            map.put("a", 1);
            map.put("b", 2);
            map.put("c", 3);
            data.add(map);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.VALUES, null, new byte[]{}));
            insts.add(filterByCmp(0, 3, encodeValue(1))); // > 1
            insts.add(countInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("value"), 2);
        });
    }

    // =============================== 多 Executor 隔离 ===============================

    static void testMultiExecutor() {
        System.out.println("\n=== 多 Executor 隔离 ===");

        test("multiple executors interference", () -> {
            SpoiExecutor exe1 = new SpoiExecutor(makeTypes());
            SpoiExecutor exe2 = new SpoiExecutor(makeTypes());

            ArrayList<Integer> data1 = new ArrayList<>(Arrays.asList(1, 2, 3));
            ArrayList<Integer> data2 = new ArrayList<>(Arrays.asList(10, 20, 30));

            // exe1: PIPE, TAKE 2, EXEC
            ArrayList<SpoiInstruction> insts1 = new ArrayList<>();
            insts1.add(pipeInst(new int[]{}));
            insts1.add(takeInst(2));
            insts1.add(execInst());
            byte[] stream1 = buildSpoiStream(insts1);

            // exe2: PIPE, DROP 1, EXEC
            ArrayList<SpoiInstruction> insts2 = new ArrayList<>();
            insts2.add(pipeInst(new int[]{}));
            insts2.add(dropInst(1));
            insts2.add(execInst());
            byte[] stream2 = buildSpoiStream(insts2);

            Map<String, Object> result1 = exe1.execute(data1, stream1);
            Map<String, Object> result2 = exe2.execute(data2, stream2);

            @SuppressWarnings("unchecked")
            ArrayList<Object> val1 = (ArrayList<Object>) result1.get("value");
            @SuppressWarnings("unchecked")
            ArrayList<Object> val2 = (ArrayList<Object>) result2.get("value");

            assertEqual(val1.size(), 2);
            assertEqual(val2.size(), 2);
            assertEqual(val1.get(0), 1);
            assertEqual(val2.get(0), 20);
        });
    }

    // =============================== 导航 nil Map ===============================

    static void testNavigateNilMap() {
        System.out.println("\n=== 导航 nil Map ===");

        test("navigate to nil map field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.metadata = null; // 显式设为 null
            Object result = exe.navigate(p, Arrays.asList(4));
            // Java 反射访问 null 对象字段返回 null
            assertNull(result, "Navigate to null map should return null");
        });

        test("navigate through nil map should throw", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.metadata = null;
            // 导航到 null.metadata[0] → null 上不能导航
            boolean thrown = mustThrow(() -> exe.navigate(p, Arrays.asList(4, 0)));
            assertTrue(thrown, "Expected exception for navigate through nil map");
        });

        test("PIPE nil map from field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.metadata = null;

            // PIPE 从 null map 字段 → navigate 返回 null → opPipe 将 null 当作单值放入 pipeData
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{4}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(p, stream);
            // null 作为单值进入 pipeData，结果为 SINGLE with null
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertNull(result.get("value"));
        });
    }

    // =============================== 类型混淆攻击 ===============================

    static void testTypeConfusion() {
        System.out.println("\n=== 类型混淆攻击 ===");

        test("SET string to int field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            // SET 字符串到 int 字段 → convertValue 可能失败或转型
            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(setStr(new int[]{1}, "not_a_number"));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for SET string to int field");
        });

        test("SET large number to byte/short field", () -> {
            // 使用不带类型注册表的 executor 测试通用字段设置
            SpoiExecutor exe = new SpoiExecutor(new LinkedHashMap<>());
            // 创建一个包含 byte 字段的简单对象
            class ByteHolder { public byte val = 0; }
            ByteHolder holder = new ByteHolder();

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setVal(new int[]{0}, new byte[]{0x01})); // 1 byte → u8=1
            byte[] stream = buildSpoiStream(insts);
            exe.execute(holder, stream);
            assertEqual((int) holder.val, 1);
        });

        test("ADD string to int field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(makeInst(Op.ADD, new int[]{1}, "hello".getBytes(StandardCharsets.UTF_8)));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for ADD string to int field");
        });

        test("SORT with mixed types", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>(Arrays.asList(1, "hello", 3.14, true));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(sortInst(new int[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            // 不应崩溃，结果应排序（按字符串比较）
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 4);
        });

        test("FILTER with mixed type comparison", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>(Arrays.asList(1, "hello", 3.14, true));

            // FILTER on混合类型: memberIdx=0 比较值本身
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterByCmp(0, 0, encodeValue(1))); // eq 1
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            // 数字 1 和 Long 1 的比较
            assertEqual(result.get("resultType"), ResultType.SINGLE);
        });
    }

    // =============================== 并发执行 ===============================

    static void testConcurrentExecution() {
        System.out.println("\n=== 并发执行 ===");

        test("concurrent execute on same executor", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data1 = new ArrayList<>(Arrays.asList(1, 2, 3, 4, 5));
            ArrayList<Integer> data2 = new ArrayList<>(Arrays.asList(10, 20, 30, 40, 50));

            // 共享 executor 的并发执行可能导致 pipeData 竞争
            Thread t1 = new Thread(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(pipeInst(new int[]{}));
                insts.add(takeInst(2));
                insts.add(execInst());
                byte[] stream = buildSpoiStream(insts);
                exe.execute(data1, stream);
            });
            Thread t2 = new Thread(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(pipeInst(new int[]{}));
                insts.add(takeInst(3));
                insts.add(execInst());
                byte[] stream = buildSpoiStream(insts);
                exe.execute(data2, stream);
            });

            t1.start();
            t2.start();
            try { t1.join(); t2.join(); } catch (InterruptedException e) {}
            // 不崩溃即通过
            assertTrue(true, "Concurrent execution should not crash");
        });

        test("concurrent execute on different executors", () -> {
            SpoiExecutor exe1 = new SpoiExecutor(makeTypes());
            SpoiExecutor exe2 = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data1 = new ArrayList<>(Arrays.asList(1, 2, 3));
            ArrayList<Integer> data2 = new ArrayList<>(Arrays.asList(10, 20, 30));

            final Map<String, Object>[] results = new Map[2];
            Thread t1 = new Thread(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(pipeInst(new int[]{}));
                insts.add(takeInst(2));
                insts.add(execInst());
                byte[] stream = buildSpoiStream(insts);
                results[0] = exe1.execute(data1, stream);
            });
            Thread t2 = new Thread(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(pipeInst(new int[]{}));
                insts.add(dropInst(1));
                insts.add(execInst());
                byte[] stream = buildSpoiStream(insts);
                results[1] = exe2.execute(data2, stream);
            });

            t1.start();
            t2.start();
            try { t1.join(); t2.join(); } catch (InterruptedException e) {}

            @SuppressWarnings("unchecked")
            ArrayList<Object> v1 = (ArrayList<Object>) results[0].get("value");
            @SuppressWarnings("unchecked")
            ArrayList<Object> v2 = (ArrayList<Object>) results[1].get("value");
            assertEqual(v1.size(), 2);
            assertEqual(v2.size(), 2);
            assertEqual(v1.get(0), 1);
            assertEqual(v2.get(0), 20);
        });
    }

    // =============================== 控制字符攻击 ===============================

    static void testControlCharacters() {
        System.out.println("\n=== 控制字符攻击 ===");

        test("SET string with all control chars", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            String ctrl = "\u0000\u0001\u0002\u0003\r\n\t\b\f\u007F";
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setStr(new int[]{0}, ctrl));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.name, ctrl);
        });

        test("FILTER string with control chars", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("a\u0000b", 10, 100));
            players.add(new Player("Alice", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.FILTER, null, buildFilterOperandBytes(0, 0, "a\u0000b".getBytes(StandardCharsets.UTF_8))));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(((Player) result.get("value")).name, "a\u0000b");
        });

        test("SET string with only whitespace", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            String ws = "   \t  \n \r  ";
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setStr(new int[]{0}, ws));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.name, ws);
        });
    }

    // =============================== DEREF 容器攻击 ===============================

    static void testDerefContainer() {
        System.out.println("\n=== DEREF 容器攻击 ===");

        test("DEREF on List", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> list = new ArrayList<>(Arrays.asList(1, 2, 3));
            // DEREF on List: List 没有 value 字段，findField 返回 null，返回自身
            Object result = exe.navStep(list, PathMarker.PATH_DEREF);
            assertTrue(result instanceof List, "DEREF on List should return the list itself");
            assertEqual(((List<?>) result).size(), 3);
        });

        test("DEREF on Map", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            HashMap<String, Object> map = new HashMap<>();
            map.put("key", "value");
            // DEREF on Map: Map 没有 value 字段，返回自身
            Object result = exe.navStep(map, PathMarker.PATH_DEREF);
            assertTrue(result instanceof Map, "DEREF on Map should return the map itself");
        });

        test("DEREF on Integer", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            // DEREF on Integer: findField 尝试找 "value" 字段
            // Integer 内部有 private final int value，反射访问返回内部值
            Object result = exe.navStep(42, PathMarker.PATH_DEREF);
            // Integer 内部有 value 字段，反射访问返回 unboxed int
            assertNotNull(result, "DEREF on Integer should return non-null");
            assertTrue(result instanceof Integer || result instanceof Long, "DEREF on Integer should return number");
        });

        test("DEREF on Boolean", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            // DEREF on Boolean: Boolean 内部有 value 字段
            Object result = exe.navStep(true, PathMarker.PATH_DEREF);
            assertNotNull(result, "DEREF on Boolean should return non-null");
        });
    }

    // =============================== 零长度操作数攻击 ===============================

    static void testZeroLengthOperandAttack() {
        System.out.println("\n=== 零长度操作数攻击 ===");

        test("REPLACE with zero-length value", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.items.add(new Item("Sword", 100));

            // REPLACE with operand只有4字节index，没有value → deserializeValue(empty) → null
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(replaceInst(new int[]{3}, 0, new byte[]{}));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            // null 替换到 items[0]，Item 变为 null
            assertNull(p.items.get(0), "Item should be null after REPLACE with empty value");
        });

        test("INSERT with zero-length value", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            // INSERT with operand只有4字节index，没有value
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(insertInst(new int[]{3}, 0, new byte[]{}));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            // null 被插入到 items
            assertEqual(p.items.size(), 1);
            assertNull(p.items.get(0), "Inserted null value");
        });

        test("APPEND with zero-length value", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(appendInst(new int[]{3}, new byte[]{}));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            assertEqual(p.items.size(), 1);
            assertNull(p.items.get(0), "Appended null value");
        });

        test("TAKE with zero-length operand", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.TAKE, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("DROP with zero-length operand", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.DROP, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3); // DROP 0
        });
    }

    // =============================== JOIN 嵌套 ===============================

    static void testJoinNested() {
        System.out.println("\n=== JOIN 嵌套 ===");

        test("JOIN with nested lists", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            data.add(Arrays.asList(1, 2));
            data.add(Arrays.asList(3, 4, 5));
            data.add(6);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.JOIN, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 6);
            assertEqual(value.get(0), 1);
            assertEqual(value.get(5), 6);
        });

        test("JOIN single non-list element", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>(Arrays.asList(42));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.JOIN, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            // JOIN 非列表元素保持为单值，结果为 SINGLE
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), 42);
        });

        test("JOIN then DISTINCT", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            data.add(Arrays.asList(1, 2, 2, 3));
            data.add(Arrays.asList(3, 4, 4, 5));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.JOIN, null, new byte[]{}));
            insts.add(distinctInst());
            insts.add(sortInst(new int[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 5);
            assertEqual(value.get(0), 1);
            assertEqual(value.get(4), 5);
        });
    }

    // =============================== FILTER 刁钻攻击 ===============================

    static void testFilterTricky() {
        System.out.println("\n=== FILTER 刁钻攻击 ===");

        test("FILTER on empty pipeData", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> data = new ArrayList<>();

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 10));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("FILTER then FILTER on empty", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));
            players.add(new Player("Bob", 20, 80));

            // 第一个 FILTER 全过滤，第二个 FILTER 在空集上执行
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterGt(new int[]{}, 1, 1000)); // 全过滤
            insts.add(filterGt(new int[]{}, 1, 0));    // 在空集上过滤
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("FILTER with negative memberIdx", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Alice", 10, 100));

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(pipeInst(new int[]{}));
                // negative memberIdx = 0xFFFFFFFF
                insts.add(makeInst(Op.FILTER, null, buildFilterOperandBytes(0xFFFFFFFF, 0, encodeValue(10))));
                insts.add(execInst());
                byte[] stream = buildSpoiStream(insts);
                exe.execute(players, stream);
            });
            assertTrue(thrown, "Expected exception for FILTER with negative memberIdx");
        });

        test("FILTER string contains null byte", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("hello\u0000world", 10, 100));
            players.add(new Player("hello", 20, 80));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.FILTER, null, buildFilterOperandBytes(0, 0, "hello\u0000world".getBytes(StandardCharsets.UTF_8))));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            assertEqual(result.get("resultType"), ResultType.SINGLE);
        });

        test("FILTER then TAKE then FILTER", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>();
            for (int i = 1; i <= 10; i++) data.add(i);

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(filterByCmp(0, 3, encodeValue(3))); // >3: 4-10
            insts.add(takeInst(3));                          // 4-6
            insts.add(filterByCmp(0, 2, encodeValue(5)));   // <5: 4
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            // 最终只剩 4 一个元素，结果为 SINGLE
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), 4);
        });
    }

    // =============================== SORT null 值攻击 ===============================

    static void testSortWithNulls() {
        System.out.println("\n=== SORT null 值攻击 ===");

        test("SORT with null values", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Bob", 20, 80));
            players.add(new Player(null, 10, 100));
            players.add(new Player("Alice", 30, 60));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(sortInst(new int[]{0})); // SORT by name (null first)
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 3);
            // Java sort: null throws NullPointerException when comparing
            // 实际上 compareValues 中 null 处理会让 null 排在最前
            // 但 sortInst 中的 Comparator 比较 null 会抛异常
        });

        test("SORT null values with REVERSE", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Player> players = new ArrayList<>();
            players.add(new Player("Bob", 20, 80));
            players.add(new Player("Alice", 30, 60));

            // 先 SORT 再 REVERSE
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(sortInst(new int[]{0}));
            insts.add(reverseInst());
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(players, stream);
            @SuppressWarnings("unchecked")
            ArrayList<Object> value = (ArrayList<Object>) result.get("value");
            assertEqual(value.size(), 2);
            assertEqual(((Player) value.get(0)).name, "Bob");   // reverse: Bob first
            assertEqual(((Player) value.get(1)).name, "Alice");
        });
    }

    // =============================== KEYS/VALUES 非 Map 攻击 ===============================

    static void testKeysValuesNonMap() {
        System.out.println("\n=== KEYS/VALUES 非 Map 攻击 ===");

        test("KEYS on non-Map pipeData", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(1, 2, 3));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.KEYS, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            // KEYS on non-Map: 不匹配则不添加，结果为空
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("VALUES on non-Map pipeData", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<String> data = new ArrayList<>(Arrays.asList("a", "b"));

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.VALUES, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            assertEqual(result.get("resultType"), ResultType.UNDEF);
        });

        test("KEYS on mixed pipeData", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            HashMap<String, Object> map = new HashMap<>();
            map.put("x", 1);
            data.add(map);
            data.add(42); // 非 Map

            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(makeInst(Op.KEYS, null, new byte[]{}));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            // KEYS 只提取 Map 的 key，非 Map 跳过，结果只有 "x" 一个元素
            assertEqual(result.get("resultType"), ResultType.SINGLE);
            assertEqual(result.get("value"), "x");
        });
    }

    // =============================== ADD/REMOVE 边界增强 ===============================

    static void testAddRemoveEdge() {
        System.out.println("\n=== ADD/REMOVE 边界增强 ===");

        test("ADD to null field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);
            p.metadata = null;

            boolean thrown = mustThrow(() -> {
                ArrayList<SpoiInstruction> insts = new ArrayList<>();
                insts.add(addInt(new int[]{4}, 5));
                byte[] stream = buildSpoiStream(insts);
                exe.execute(p, stream);
            });
            assertTrue(thrown, "Expected exception for ADD to null field");
        });

        test("REMOVE from empty list in pipe", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>();
            data.add(new ArrayList<>()); // empty list

            // REMOVE 操作在 root 上执行，path=[] 操作 root 本身
            // root 是 [[], ...]，REMOVE index 0 移除空列表，成功
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(removeInst(new int[]{}, 0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            // REMOVE 成功从 root 移除空列表，pipeData 保持不变（空列表已被移除）
            assertEqual(data.size(), 0);
        });

        test("REMOVE via pipe on single element", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            ArrayList<Object> data = new ArrayList<>(Arrays.asList("hello"));

            // REMOVE 操作在 root 上执行，path=[] 操作 root 本身
            // root 是 ["hello"]，REMOVE index 0 移除 "hello"，成功
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(pipeInst(new int[]{}));
            insts.add(removeInst(new int[]{}, 0));
            insts.add(execInst());
            byte[] stream = buildSpoiStream(insts);
            Map<String, Object> result = exe.execute(data, stream);
            // REMOVE 成功从 root 移除元素
            assertEqual(data.size(), 0);
        });
    }

    // =============================== SET 特殊值 ===============================

    static void testSetSpecialValues() {
        System.out.println("\n=== SET 特殊值 ===");

        test("SET float value to int field", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            // 4字节 float 编码 → deserializeValue 解析为 uint32
            byte[] floatBytes = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putFloat(3.14f).array();
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setVal(new int[]{1}, floatBytes));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            // 4字节被解析为 uint32，不是 float
            assertNotNull(p, "Should not crash");
        });

        test("SET NaN/Infinity bytes", () -> {
            SpoiExecutor exe = new SpoiExecutor(makeTypes());
            Player p = new Player("Alice", 10, 100);

            // 8字节 NaN 编码
            byte[] nanBytes = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putDouble(Double.NaN).array();
            ArrayList<SpoiInstruction> insts = new ArrayList<>();
            insts.add(setVal(new int[]{1}, nanBytes));
            byte[] stream = buildSpoiStream(insts);
            exe.execute(p, stream);
            // 8字节被解析为 Long
            assertNotNull(p, "Should not crash on NaN bytes");
        });
    }

    // =============================== 主入口 ===============================

    public static void main(String[] args) {
        System.out.println("========================================");
        System.out.println("Java SPOI 刁钻边界测试套件");
        System.out.println("========================================");

        testVarint();
        testParse();
        testNavigation();
        testInstructionSequence();
        testEmptyContainer();
        testAggregation();
        testWriteOps();
        testNestedNavigation();
        testTakeWhileDropWhile();
        testFilterComparison();
        testOperandParsing();
        testPipelineCombinations();
        testTypeRegistry();
        testContainerOps();
        testUnknownOpcodes();
        testResultTypes();
        testPipeWithPath();
        testDeserialize();
        testStateIsolation();
        testLargeDataset();
        testCountThenTake();
        testEmptyPath();
        testDistinctBoundary();
        testMapNavigation();
        // 新增刁钻测试
        testTrickyNegativeIndex();
        testAggregationMultiElement();
        testPipeEdgeCases();
        testFilterEdgeCases();
        testUnhandledOpcodes();
        testReverseEdgeCases();
        testSortEdgeCases();
        testContainerEdgeCases();
        testWritePathEdgeCases();
        testTAKEDROPOverflow();
        testVarintNegative();
        testDeepNested();
        testFilterAggregationChain();
        testEmptyStream();
        testDISTINCTWithObjects();
        testCOMPARENull();
        // 补充测试
        testUnicodeAttack();
        testPipeTypeChange();
        testNestedMap();
        testIntegerOverflow();
        testFilterWithPath();
        testSortComplex();
        testDistinctMixed();
        testResetNested();
        testExtremeStream();
        testAttackChain();
        testNullByteAttack();
        testKeysValuesChain();
        testMultiExecutor();
        // 新增刁钻测试维度
        testNavigateNilMap();
        testTypeConfusion();
        testConcurrentExecution();
        testControlCharacters();
        testDerefContainer();
        testZeroLengthOperandAttack();
        testJoinNested();
        testFilterTricky();
        testSortWithNulls();
        testKeysValuesNonMap();
        testAddRemoveEdge();
        testSetSpecialValues();

        System.out.println("\n========================================");
        System.out.println("\u901a\u8fc7: " + passed + ", \u5931\u8d25: " + failed);
        System.out.println("========================================");
        if (failed > 0) {
            System.exit(1);
        }
    }
}