/**
 * Stream-Punk Java 序列化/反序列化测试套件
 * 编译: javac test_stream_punk.java
 * 运行: java TestStreamPunk
 */

import java.util.*;
import java.nio.charset.StandardCharsets;

class TestStreamPunk {
    static int passed = 0;
    static int failed = 0;

    static void test(String name, Runnable fn) {
        try {
            fn.run();
            passed++;
            System.out.println("  ✓ " + name);
        } catch (Throwable e) {
            failed++;
            System.out.println("  ✗ " + name);
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

    // ======================== 基本类型测试 ========================

    public static void main(String[] args) {
        System.out.println("\n基本类型");
        test("u8 roundtrip", () -> {
            for (int v : new int[]{0, 1, 127, 128, 255}) {
                O o = new O();
                o.write_u8(v);
                I i = new I(o.to_bytes());
                assertEqual(i.read_u8(), v, "u8: " + v);
            }
        });

        test("u16 roundtrip", () -> {
            for (int v : new int[]{0, 1, 256, 1000, 0xFFFF}) {
                O o = new O();
                o.write_u16(v);
                I i = new I(o.to_bytes());
                assertEqual(i.read_u16(), v, "u16: " + v);
            }
        });

        test("u32 roundtrip", () -> {
            for (long v : new long[]{0, 1, 0x10000, 0x7FFFFFFFL, 0xFFFFFFFFL}) {
                O o = new O();
                o.write_u32(v);
                I i = new I(o.to_bytes());
                assertEqual(i.read_u32(), v, "u32: " + v);
            }
        });

        test("u64 roundtrip", () -> {
            for (long v : new long[]{0, 1, 0x100000000L, Long.MAX_VALUE, -1L /* 0xFFFFFFFFFFFFFFFF */}) {
                O o = new O();
                o.write_u64(v);
                I i = new I(o.to_bytes());
                assertEqual(i.read_u64(), v, "u64: " + v);
            }
        });

        test("i8 roundtrip", () -> {
            for (byte v : new byte[]{-128, -1, 0, 1, 127}) {
                O o = new O();
                o.write_i8(v);
                I i = new I(o.to_bytes());
                assertEqual(i.read_i8(), v, "i8: " + v);
            }
        });

        test("i16 roundtrip", () -> {
            for (short v : new short[]{-32768, -1, 0, 1, 32767}) {
                O o = new O();
                o.write_i16(v);
                I i = new I(o.to_bytes());
                assertEqual(i.read_i16(), v, "i16: " + v);
            }
        });

        test("i32 roundtrip", () -> {
            for (int v : new int[]{Integer.MIN_VALUE, -1, 0, 1, Integer.MAX_VALUE}) {
                O o = new O();
                o.write_i32(v);
                I i = new I(o.to_bytes());
                assertEqual(i.read_i32(), v, "i32: " + v);
            }
        });

        test("i64 roundtrip", () -> {
            for (long v : new long[]{Long.MIN_VALUE, -1, 0, 1, Long.MAX_VALUE}) {
                O o = new O();
                o.write_i64(v);
                I i = new I(o.to_bytes());
                assertEqual(i.read_i64(), v, "i64: " + v);
            }
        });

        test("f32 roundtrip", () -> {
            for (float v : new float[]{0.0f, -1.0f, 3.14f}) {
                O o = new O();
                o.write_f32(v);
                I i = new I(o.to_bytes());
                float r = i.read_f32();
                assertTrue(Math.abs(r - v) < 0.0001f, "f32: expected " + v + ", got " + r);
            }
            // Infinity
            O o2 = new O(); o2.write_f32(Float.POSITIVE_INFINITY);
            assertTrue(new I(o2.to_bytes()).read_f32() == Float.POSITIVE_INFINITY);
            // -Infinity
            O o3 = new O(); o3.write_f32(Float.NEGATIVE_INFINITY);
            assertTrue(new I(o3.to_bytes()).read_f32() == Float.NEGATIVE_INFINITY);
        });

        test("f32 NaN", () -> {
            O o = new O(); o.write_f32(Float.NaN);
            I i = new I(o.to_bytes());
            assertTrue(Float.isNaN(i.read_f32()));
        });

        test("f64 roundtrip", () -> {
            for (double v : new double[]{0.0, -1.0, 3.141592653589793}) {
                O o = new O();
                o.write_f64(v);
                I i = new I(o.to_bytes());
                double r = i.read_f64();
                assertTrue(Math.abs(r - v) < 0.0000000001, "f64: expected " + v + ", got " + r);
            }
            O o2 = new O(); o2.write_f64(Double.POSITIVE_INFINITY);
            assertTrue(new I(o2.to_bytes()).read_f64() == Double.POSITIVE_INFINITY);
            O o3 = new O(); o3.write_f64(Double.NEGATIVE_INFINITY);
            assertTrue(new I(o3.to_bytes()).read_f64() == Double.NEGATIVE_INFINITY);
        });

        test("f64 NaN", () -> {
            O o = new O(); o.write_f64(Double.NaN);
            I i = new I(o.to_bytes());
            assertTrue(Double.isNaN(i.read_f64()));
        });

        test("bl true", () -> {
            O o = new O(); o.write_bl(true);
            I i = new I(o.to_bytes());
            assertTrue(i.read_bl());
        });

        test("bl false", () -> {
            O o = new O(); o.write_bl(false);
            I i = new I(o.to_bytes());
            assertFalse(i.read_bl());
        });

        test("ch8 roundtrip", () -> {
            for (char v : new char[]{'A', 'z', '0', '\n'}) {
                O o = new O();
                o.write_ch8(v);
                I i = new I(o.to_bytes());
                assertEqual(i.read_ch8(), v, "ch8: " + v);
            }
        });

        test("ch16 roundtrip", () -> {
            for (char v : new char[]{'A', '\u4e2d' /* 中 */}) {
                O o = new O();
                o.write_ch16(v);
                I i = new I(o.to_bytes());
                assertEqual(i.read_ch16(), v, "ch16: " + v);
            }
        });

        // ======================== 字符串测试 ========================

        System.out.println("\n字符串");
        test("write read empty", () -> {
            O o = new O(); o.write_string("");
            I i = new I(o.to_bytes());
            assertEqual(i.read_string(), "");
        });

        test("write read ascii", () -> {
            O o = new O(); o.write_string("Hello World");
            I i = new I(o.to_bytes());
            assertEqual(i.read_string(), "Hello World");
        });

        test("write read unicode", () -> {
            String s = "你好世界 — 测试";
            O o = new O(); o.write_string(s);
            I i = new I(o.to_bytes());
            assertEqual(i.read_string(), s);
        });

        test("write read long", () -> {
            StringBuilder sb = new StringBuilder();
            for (int j = 0; j < 10000; j++) sb.append('A');
            String s = sb.toString();
            O o = new O(); o.write_string(s);
            I i = new I(o.to_bytes());
            assertEqual(i.read_string(), s);
        });

        test("u8string", () -> {
            O o = new O(); o.write_u8string("Hello UTF-8");
            I i = new I(o.to_bytes());
            assertEqual(i.read_u8string(), "Hello UTF-8");
        });

        test("u16string", () -> {
            String s = "你好世界";
            O o = new O(); o.write_u16string(s);
            I i = new I(o.to_bytes());
            assertEqual(i.read_u16string(), s);
        });

        test("u16string empty", () -> {
            O o = new O(); o.write_u16string("");
            I i = new I(o.to_bytes());
            assertEqual(i.read_u16string(), "");
        });

        test("u32string roundtrip", () -> {
            byte[] data = new byte[]{1, 0, 0, 0, 2, 0, 0, 0};
            O o = new O(); o.write_u32string(data);
            I i = new I(o.to_bytes());
            byte[] result = i.read_u32string();
            assertTrue(Arrays.equals(result, data), "u32string");
        });

        test("std_string", () -> {
            O o = new O(); o.write_string("std::string");
            I i = new I(o.to_bytes());
            assertEqual(i.read_std_string(), "std::string");
        });

        // ======================== 容器测试 ========================

        System.out.println("\n容器");
        test("array empty", () -> {
            O o = new O();
            o.write_Array(new ArrayList<Integer>(), v -> o.write_u32((Integer) v));
            I i = new I(o.to_bytes());
            assertTrue(i.read_Array(() -> i.read_u32()).isEmpty());
        });

        test("array u32", () -> {
            ArrayList<Long> data = new ArrayList<>(Arrays.asList(1L, 2L, 3L, 4L, 5L));
            O o = new O();
            o.write_Array(data, v -> o.write_u32(v));
            I i = new I(o.to_bytes());
            assertEqual(i.read_Array(() -> i.read_u32()), data);
        });

        test("vector", () -> {
            ArrayList<Long> data = new ArrayList<>(Arrays.asList(10L, 20L, 30L));
            O o = new O();
            o.write_Array(data, v -> o.write_u32(v));
            I i = new I(o.to_bytes());
            assertEqual(i.read_vector(() -> i.read_u32()), data);
        });

        test("array strings", () -> {
            ArrayList<String> data = new ArrayList<>(Arrays.asList("Alice", "Bob", "Carol"));
            O o = new O();
            o.write_Array(data, v -> o.write_string(v));
            I i = new I(o.to_bytes());
            assertEqual(i.read_Array(() -> i.read_string()), data);
        });

        test("set", () -> {
            HashSet<Long> data = new HashSet<>(Arrays.asList(1L, 2L, 3L));
            O o = new O();
            o.write_set(data, v -> o.write_u32(v));
            I i = new I(o.to_bytes());
            HashSet<Long> result = i.read_set(() -> i.read_u32());
            assertEqual(result.size(), 3);
            assertTrue(result.contains(1L) && result.contains(2L) && result.contains(3L));
        });

        test("set empty", () -> {
            O o = new O();
            o.write_set(new HashSet<Long>(), v -> o.write_u32(v));
            I i = new I(o.to_bytes());
            assertTrue(i.read_set(() -> i.read_u32()).isEmpty());
        });

        test("map", () -> {
            HashMap<String, Long> data = new HashMap<>();
            data.put("key1", 100L);
            data.put("key2", 200L);
            O o = new O();
            o.write_map(data, k -> o.write_string(k), v -> o.write_u32(v));
            I i = new I(o.to_bytes());
            HashMap<String, Long> result = i.read_map(() -> i.read_string(), () -> i.read_u32());
            assertEqual(result.get("key1"), 100L);
            assertEqual(result.get("key2"), 200L);
        });

        test("map empty", () -> {
            O o = new O();
            o.write_map(new HashMap<String, Long>(), k -> o.write_string(k), v -> o.write_u32(v));
            I i = new I(o.to_bytes());
            assertTrue(i.read_map(() -> i.read_string(), () -> i.read_u32()).isEmpty());
        });

        test("nested array", () -> {
            ArrayList<ArrayList<Long>> data = new ArrayList<>();
            data.add(new ArrayList<>(Arrays.asList(1L, 2L)));
            data.add(new ArrayList<>(Arrays.asList(3L, 4L, 5L)));
            data.add(new ArrayList<>());
            O o = new O();
            o.write_Array(data, inner -> {
                @SuppressWarnings("unchecked")
                ArrayList<Long> list = (ArrayList<Long>) inner;
                o.write_Array(list, v -> o.write_u32(v));
            });
            I i = new I(o.to_bytes());
            ArrayList<ArrayList<Long>> result = i.read_Array(() ->
                i.read_Array(() -> i.read_u32())
            );
            assertEqual(result, data);
        });

        // ======================== 多字段往返测试 ========================

        System.out.println("\n多字段往返");
        test("mixed types", () -> {
            O o = new O();
            o.write_u8(42);
            o.write_i32(-100);
            o.write_f64(3.14);
            o.write_bl(true);
            o.write_string("hello");
            o.write_Array(new ArrayList<>(Arrays.asList(1L, 2L, 3L)), v -> o.write_u32(v));

            I i = new I(o.to_bytes());
            assertEqual(i.read_u8(), 42);
            assertEqual(i.read_i32(), -100);
            assertTrue(Math.abs(i.read_f64() - 3.14) < 0.0000000001);
            assertTrue(i.read_bl());
            assertEqual(i.read_string(), "hello");
            assertEqual(i.read_Array(() -> i.read_u32()), Arrays.asList(1L, 2L, 3L));
        });

        test("offset reader", () -> {
            O o = new O();
            o.write_u32(0); // padding
            o.write_u32(42);
            o.write_u32(0); // padding

            byte[] data = o.to_bytes();
            I i = new I(data, 4);
            assertEqual(i.read_u32(), 42L);
            assertEqual(i.off, 8);
        });

        test("has more data", () -> {
            O o = new O(); o.write_u32(42);
            I i = new I(o.to_bytes());
            assertTrue(i.hasMoreData());
            i.read_u32();
            assertFalse(i.hasMoreData());
        });

        // ======================== 辅助类型测试 ========================

        System.out.println("\n辅助类型");
        test("SpArray create and access", () -> {
            SpArray<String> arr = new SpArray<>(3);
            arr.set(0, "init");
            arr.set(1, "init");
            arr.set(2, "init");
            assertEqual(arr.size(), 3);
            assertEqual(arr.at(0), "init");
            assertEqual(arr.at(1), "init");
            assertEqual(arr.at(2), "init");
        });

        test("SpArray set and get", () -> {
            SpArray<Integer> arr = new SpArray<>(3);
            arr.set(0, 10);
            arr.set(1, 20);
            assertEqual(arr.at(0), 10);
            assertEqual(arr.at(1), 20);
        });

        test("SpRef creation", () -> {
            SpRef<String> ref = new SpRef<>("hello", 0x1000L);
            assertEqual(ref.value, "hello");
            assertEqual(ref.address, 0x1000L);
        });

        test("SpRef null", () -> {
            SpRef<String> ref = new SpRef<>(null, 0L);
            assertNull(ref.value);
            assertEqual(ref.address, 0L);
        });

        // ======================== 边界条件测试 ========================

        System.out.println("\n边界条件");
        test("zero length array", () -> {
            O o = new O();
            o.write_Array(new ArrayList<Integer>(), v -> o.write_u8((Integer) v));
            I i = new I(o.to_bytes());
            assertTrue(i.read_Array(() -> i.read_u8()).isEmpty());
        });

        test("negative in array", () -> {
            ArrayList<Integer> data = new ArrayList<>(Arrays.asList(-1, 0, 1, -100, 100));
            O o = new O();
            o.write_Array(data, v -> o.write_i32(v));
            I i = new I(o.to_bytes());
            assertEqual(i.read_Array(() -> i.read_i32()), data);
        });

        test("max int values", () -> {
            O o = new O();
            o.write_u8(255);
            o.write_u16(0xFFFF);
            o.write_u32(0xFFFFFFFFL);
            o.write_u64(0xFFFFFFFFFFFFFFFFL);
            I i = new I(o.to_bytes());
            assertEqual(i.read_u8(), 255);
            assertEqual(i.read_u16(), 0xFFFF);
            assertEqual(i.read_u32(), 0xFFFFFFFFL);
            assertEqual(i.read_u64(), 0xFFFFFFFFFFFFFFFFL);
        });

        test("null string", () -> {
            O o = new O();
            o.write_string(null);
            I i = new I(o.to_bytes());
            assertEqual(i.read_string(), "");
        });

        // ======================== 总结 ========================

        System.out.println("\n========================================");
        System.out.println("通过: " + passed + ", 失败: " + failed);
        System.out.println("========================================");
        if (failed > 0) {
            System.exit(1);
        }
    }
}