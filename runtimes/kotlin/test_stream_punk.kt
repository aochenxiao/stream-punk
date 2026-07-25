/**
 * Stream-Punk Kotlin 序列化/反序列化测试套件
 * 编译: kotlinc stream-punk.kt test_stream_punk.kt -include-runtime -d test_stream_punk.jar
 * 运行: java -jar test_stream_punk.jar TestStreamPunkKt
 */

var passed = 0
var failed = 0

fun test(name: String, fn: () -> Unit) {
    try {
        fn()
        passed++
        println("  ✓ $name")
    } catch (e: Throwable) {
        failed++
        println("  ✗ $name")
        println("    ${e.message}")
    }
}

fun assertEqual(actual: Any?, expected: Any?, msg: String? = null) {
    if (actual != expected) {
        throw AssertionError(msg ?: "expected $expected, got $actual")
    }
}

fun assertTrue(v: Boolean, msg: String? = null) {
    if (!v) throw AssertionError(msg ?: "expected true, got false")
}

fun assertFalse(v: Boolean, msg: String? = null) {
    if (v) throw AssertionError(msg ?: "expected false, got true")
}

fun assertNull(v: Any?, msg: String? = null) {
    if (v != null) throw AssertionError(msg ?: "expected null, got $v")
}

// ======================== 基本类型测试 ========================

fun main() {
    println("\n基本类型")
    test("u8 roundtrip") {
        for (v in listOf(0, 1, 127, 128, 255)) {
            val o = O()
            o.write_u8(v)
            val i = I(o.toBytes())
            assertEqual(i.read_u8(), v, "u8: $v")
        }
    }

    test("u16 roundtrip") {
        for (v in listOf(0, 1, 256, 1000, 0xFFFF)) {
            val o = O()
            o.write_u16(v)
            val i = I(o.toBytes())
            assertEqual(i.read_u16(), v, "u16: $v")
        }
    }

    test("u32 roundtrip") {
        for (v in listOf(0L, 1L, 0x10000L, 0x7FFFFFFFL, 0xFFFFFFFFL)) {
            val o = O()
            o.write_u32(v)
            val i = I(o.toBytes())
            assertEqual(i.read_u32(), v, "u32: $v")
        }
    }

    test("u64 roundtrip") {
        for (v in listOf(0L, 1L, 0x100000000L, Long.MAX_VALUE, -1L /* 0xFFFFFFFFFFFFFFFF */)) {
            val o = O()
            o.write_u64(v)
            val i = I(o.toBytes())
            assertEqual(i.read_u64(), v, "u64: $v")
        }
    }

    test("i8 roundtrip") {
        for (v in byteArrayOf(-128, -1, 0, 1, 127)) {
            val o = O()
            o.write_i8(v)
            val i = I(o.toBytes())
            assertEqual(i.read_i8(), v, "i8: $v")
        }
    }

    test("i16 roundtrip") {
        for (v in shortArrayOf(-32768, -1, 0, 1, 32767)) {
            val o = O()
            o.write_i16(v)
            val i = I(o.toBytes())
            assertEqual(i.read_i16(), v, "i16: $v")
        }
    }

    test("i32 roundtrip") {
        for (v in listOf(Int.MIN_VALUE, -1, 0, 1, Int.MAX_VALUE)) {
            val o = O()
            o.write_i32(v)
            val i = I(o.toBytes())
            assertEqual(i.read_i32(), v, "i32: $v")
        }
    }

    test("i64 roundtrip") {
        for (v in listOf(Long.MIN_VALUE, -1L, 0L, 1L, Long.MAX_VALUE)) {
            val o = O()
            o.write_i64(v)
            val i = I(o.toBytes())
            assertEqual(i.read_i64(), v, "i64: $v")
        }
    }

    test("f32 roundtrip") {
        for (v in listOf(0.0f, -1.0f, 3.14f)) {
            val o = O()
            o.write_f32(v)
            val i = I(o.toBytes())
            val r = i.read_f32()
            assertTrue(Math.abs(r - v) < 0.0001f, "f32: expected $v, got $r")
        }
        // Infinity
        val o2 = O(); o2.write_f32(Float.POSITIVE_INFINITY)
        assertTrue(I(o2.toBytes()).read_f32() == Float.POSITIVE_INFINITY)
        // -Infinity
        val o3 = O(); o3.write_f32(Float.NEGATIVE_INFINITY)
        assertTrue(I(o3.toBytes()).read_f32() == Float.NEGATIVE_INFINITY)
    }

    test("f32 NaN") {
        val o = O(); o.write_f32(Float.NaN)
        val i = I(o.toBytes())
        assertTrue(i.read_f32().isNaN())
    }

    test("f64 roundtrip") {
        for (v in listOf(0.0, -1.0, 3.141592653589793)) {
            val o = O()
            o.write_f64(v)
            val i = I(o.toBytes())
            val r = i.read_f64()
            assertTrue(Math.abs(r - v) < 0.0000000001, "f64: expected $v, got $r")
        }
        val o2 = O(); o2.write_f64(Double.POSITIVE_INFINITY)
        assertTrue(I(o2.toBytes()).read_f64() == Double.POSITIVE_INFINITY)
        val o3 = O(); o3.write_f64(Double.NEGATIVE_INFINITY)
        assertTrue(I(o3.toBytes()).read_f64() == Double.NEGATIVE_INFINITY)
    }

    test("f64 NaN") {
        val o = O(); o.write_f64(Double.NaN)
        val i = I(o.toBytes())
        assertTrue(i.read_f64().isNaN())
    }

    test("bl true") {
        val o = O(); o.write_bl(true)
        val i = I(o.toBytes())
        assertTrue(i.read_bl())
    }

    test("bl false") {
        val o = O(); o.write_bl(false)
        val i = I(o.toBytes())
        assertFalse(i.read_bl())
    }

    test("ch8 roundtrip") {
        for (v in listOf('A', 'z', '0', '\n')) {
            val o = O()
            o.write_ch8(v)
            val i = I(o.toBytes())
            assertEqual(i.read_ch8(), v, "ch8: $v")
        }
    }

    test("ch16 roundtrip") {
        for (v in listOf('A', '\u4e2d' /* 中 */)) {
            val o = O()
            o.write_ch16(v)
            val i = I(o.toBytes())
            assertEqual(i.read_ch16(), v, "ch16: $v")
        }
    }

    // ======================== 字符串测试 ========================

    println("\n字符串")
    test("write read empty") {
        val o = O(); o.writeString("")
        val i = I(o.toBytes())
        assertEqual(i.readString(), "")
    }

    test("write read ascii") {
        val o = O(); o.writeString("Hello World")
        val i = I(o.toBytes())
        assertEqual(i.readString(), "Hello World")
    }

    test("write read unicode") {
        val s = "你好世界 — 测试"
        val o = O(); o.writeString(s)
        val i = I(o.toBytes())
        assertEqual(i.readString(), s)
    }

    test("write read long") {
        val s = "A".repeat(10000)
        val o = O(); o.writeString(s)
        val i = I(o.toBytes())
        assertEqual(i.readString(), s)
    }

    test("u8string") {
        val o = O(); o.write_u8string("Hello UTF-8")
        val i = I(o.toBytes())
        assertEqual(i.read_u8string(), "Hello UTF-8")
    }

    test("u16string") {
        val s = "你好世界"
        val o = O(); o.write_u16string(s)
        val i = I(o.toBytes())
        assertEqual(i.read_u16string(), s)
    }

    test("u16string empty") {
        val o = O(); o.write_u16string("")
        val i = I(o.toBytes())
        assertEqual(i.read_u16string(), "")
    }

    test("u32string roundtrip") {
        val data = byteArrayOf(1, 0, 0, 0, 2, 0, 0, 0)
        val o = O(); o.write_u32string(data)
        val i = I(o.toBytes())
        val result = i.read_u32string()
        assertTrue(data.contentEquals(result), "u32string")
    }

    // ======================== 容器测试 ========================

    println("\n容器")
    test("array empty") {
        val o = O()
        o.write_Array(ArrayList<Long>()) { o.write_u32(it) }
        val i = I(o.toBytes())
        assertTrue(i.read_Array { i.read_u32() }.isEmpty())
    }

    test("array u32") {
        val data = arrayListOf(1L, 2L, 3L, 4L, 5L)
        val o = O()
        o.write_Array(data) { o.write_u32(it) }
        val i = I(o.toBytes())
        assertEqual(i.read_Array { i.read_u32() }, data)
    }

    test("array strings") {
        val data = arrayListOf("Alice", "Bob", "Carol")
        val o = O()
        o.write_Array(data) { o.writeString(it) }
        val i = I(o.toBytes())
        assertEqual(i.read_Array { i.readString() }, data)
    }

    test("set") {
        val data = hashSetOf(1L, 2L, 3L)
        val o = O()
        o.write_set(data) { o.write_u32(it) }
        val i = I(o.toBytes())
        val result = i.read_set { i.read_u32() }
        assertEqual(result.size, 3)
        assertTrue(result.contains(1L) && result.contains(2L) && result.contains(3L))
    }

    test("set empty") {
        val o = O()
        o.write_set(HashSet<Long>()) { o.write_u32(it) }
        val i = I(o.toBytes())
        assertTrue(i.read_set { i.read_u32() }.isEmpty())
    }

    test("map") {
        val data = hashMapOf("key1" to 100L, "key2" to 200L)
        val o = O()
        o.write_map(data, { o.writeString(it) }, { o.write_u32(it) })
        val i = I(o.toBytes())
        val result = i.read_map({ i.readString() }, { i.read_u32() })
        assertEqual(result["key1"], 100L)
        assertEqual(result["key2"], 200L)
    }

    test("map empty") {
        val o = O()
        o.write_map(HashMap<String, Long>(), { o.writeString(it) }, { o.write_u32(it) })
        val i = I(o.toBytes())
        assertTrue(i.read_map({ i.readString() }, { i.read_u32() }).isEmpty())
    }

    // ======================== 多字段往返测试 ========================

    println("\n多字段往返")
    test("mixed types") {
        val o = O()
        o.write_u8(42)
        o.write_i32(-100)
        o.write_f64(3.14)
        o.write_bl(true)
        o.writeString("hello")
        o.write_Array(arrayListOf(1L, 2L, 3L)) { o.write_u32(it) }

        val i = I(o.toBytes())
        assertEqual(i.read_u8(), 42)
        assertEqual(i.read_i32(), -100)
        assertTrue(Math.abs(i.read_f64() - 3.14) < 0.0000000001)
        assertTrue(i.read_bl())
        assertEqual(i.readString(), "hello")
        assertEqual(i.read_Array { i.read_u32() }, listOf(1L, 2L, 3L))
    }

    test("offset reader") {
        val o = O()
        o.write_u32(0) // padding
        o.write_u32(42)
        o.write_u32(0) // padding

        val data = o.toBytes()
        val i = I(data, 4)
        assertEqual(i.read_u32(), 42L)
        assertEqual(i.off, 8)
    }

    test("has more data") {
        val o = O(); o.write_u32(42)
        val i = I(o.toBytes())
        assertTrue(i.hasMoreData())
        i.read_u32()
        assertFalse(i.hasMoreData())
    }

    // ======================== 辅助类型测试 ========================

    println("\n辅助类型")
    test("SpArray create and access") {
        val arr = SpArray<String>(3)
        arr[0] = "init"
        arr[1] = "init"
        arr[2] = "init"
        assertEqual(arr.size(), 3)
        assertEqual(arr.at(0), "init")
        assertEqual(arr.at(1), "init")
        assertEqual(arr.at(2), "init")
    }

    test("SpArray set and get") {
        val arr = SpArray<Int>(3)
        arr[0] = 10
        arr[1] = 20
        assertEqual(arr.at(0), 10)
        assertEqual(arr.at(1), 20)
    }

    test("SpRef creation") {
        val ref = SpRef("hello", 0x1000L)
        assertEqual(ref.value, "hello")
        assertEqual(ref.address, 0x1000L)
    }

    test("SpRef null") {
        val ref = SpRef<String?>(null, 0L)
        assertNull(ref.value)
        assertEqual(ref.address, 0L)
    }

    // ======================== 边界条件测试 ========================

    println("\n边界条件")
    test("zero length array") {
        val o = O()
        o.write_Array(ArrayList<Int>()) { o.write_u8(it) }
        val i = I(o.toBytes())
        assertTrue(i.read_Array { i.read_u8() }.isEmpty())
    }

    test("negative in array") {
        val data = arrayListOf(-1, 0, 1, -100, 100)
        val o = O()
        o.write_Array(data) { o.write_i32(it) }
        val i = I(o.toBytes())
        assertEqual(i.read_Array { i.read_i32() }, data)
    }

    test("max int values") {
        val o = O()
        o.write_u8(255)
        o.write_u16(0xFFFF)
        o.write_u32(0xFFFFFFFFL)
        o.write_u64(-1L)
        val i = I(o.toBytes())
        assertEqual(i.read_u8(), 255)
        assertEqual(i.read_u16(), 0xFFFF)
        assertEqual(i.read_u32(), 0xFFFFFFFFL)
        assertEqual(i.read_u64(), -1L)
    }

    test("null string") {
        val o = O()
        o.writeString(null)
        val i = I(o.toBytes())
        assertEqual(i.readString(), "")
    }

    // ======================== 总结 ========================

    println("\n========================================")
    println("通过: $passed, 失败: $failed")
    println("========================================")
    if (failed > 0) {
        kotlin.system.exitProcess(1)
    }
}