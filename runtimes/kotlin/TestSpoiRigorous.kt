/**
 * Stream-Punk SPOI 刁钻测试套件 (Kotlin)
 *
 * 测试覆盖 7 大类别：数值边界、字符串边界、反序列化异常、Accessor 越界、
 * Executor 组合操作、跨类型 Executor、Registry 边界。
 *
 * 编译: kotlinc TestSpoiRigorous.kt spoi_accessor.kt spoi_executor.kt -include-runtime -d TestSpoiRigorous.jar
 * 运行: kotlin -classpath TestSpoiRigorous.jar TestSpoiRigorousKt
 */

import java.io.ByteArrayOutputStream
import java.nio.BufferUnderflowException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.StandardCharsets

// =============================== 测试数据类 ===============================

data class SpoiTestPlayer(
    var name: String = "",
    var hp: Int = 0,
    var level: Int = 0,
    var posX: Double = 0.0
)

class SpoiTestState {
    var tick: Int = 0
    var currentMap: String = ""
    var players: Any? = null
}

class SpoiItem {
    var name: String = ""
    var value: Int = 0
}

class SpoiInventory {
    var items: Any? = null
    var equipped: Any? = null
    var gold: Int = 0
}

class SpoiCharacter {
    var name: String = ""
    var hp: Int = 0
    var inventory: Any? = null
    var weapon: Any? = null
    var petLevel: Int = 0
}

class SpoiWorld {
    var worldName: String = ""
    var tick: Int = 0
    var characters: Any? = null
}

// =============================== 测试框架 ===============================

var passed = 0
var failed = 0
val failures = mutableListOf<String>()

fun check(cond: Boolean, msg: String = "") {
    if (!cond) throw AssertionError(msg)
}

fun test(name: String, fn: () -> Unit) {
    try {
        fn()
        passed++
        println("   \u2713 $name")
    } catch (e: Exception) {
        failed++
        failures.add("  FAIL $name: ${e.message}")
        println("   \u2717 $name")
        println("    ${e.message}")
    }
}

fun assertThrows(excType: Class<*>, fn: () -> Unit) {
    try {
        fn()
        throw AssertionError("Expected ${excType.simpleName} but no exception")
    } catch (e: Exception) {
        if (!excType.isInstance(e))
            throw RuntimeException("Wrong exception type: ${e.javaClass.simpleName}", e)
    }
}

fun assertNoThrow(fn: () -> Unit) {
    try {
        fn()
    } catch (e: Exception) {
        throw AssertionError("Unexpected exception: ${e.message}", e)
    }
}

fun assertEqual(actual: Any?, expected: Any?, msg: String = "") {
    if (actual != expected) {
        throw AssertionError(if (msg.isNotEmpty()) msg else "expected $expected, got $actual")
    }
}

fun assertTrue(v: Boolean, msg: String = "") {
    if (!v) throw AssertionError(if (msg.isNotEmpty()) msg else "expected true")
}

fun assertFalse(v: Boolean, msg: String = "") {
    if (v) throw AssertionError(if (msg.isNotEmpty()) msg else "expected false")
}

fun assertNull(v: Any?, msg: String = "") {
    if (v != null) throw AssertionError(if (msg.isNotEmpty()) msg else "expected null, got $v")
}

fun assertNotNull(v: Any?, msg: String = "") {
    if (v == null) throw AssertionError(if (msg.isNotEmpty()) msg else "expected non-null")
}

// =============================== 辅助函数 ===============================

/** 构造 typed value: [type_id(u32 LE) + value_bytes] */
fun makeTypedValue(typeId: Long, valueBytes: ByteArray): ByteArray {
    val result = ByteArray(4 + valueBytes.size)
    ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(typeId.toInt())
    System.arraycopy(valueBytes, 0, result, 4, valueBytes.size)
    return result
}

/** 构造 SET I32 operand */
fun makeSetOperandI32(typeId: Long, value: Int): ByteArray {
    val result = ByteArray(4 + 4)
    ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(typeId.toInt())
    ByteBuffer.wrap(result, 4, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(value)
    return result
}

/** 构造 SET String operand */
fun makeSetOperandString(typeId: Long, value: String): ByteArray {
    val strBytes = value.toByteArray(StandardCharsets.UTF_8)
    val result = ByteArray(4 + strBytes.size)
    ByteBuffer.wrap(result, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(typeId.toInt())
    System.arraycopy(strBytes, 0, result, 4, strBytes.size)
    return result
}

/** 构造 FILTER operand: memberIdx(u32) + cmpOp(u8) + value_len(varint) + typed_value */
fun makeFilterOperand(memberIdx: Int, cmpOp: Int, typeId: Long, valueBytes: ByteArray): ByteArray {
    val typedValue = makeTypedValue(typeId, valueBytes)
    val buf = ByteArrayOutputStream()
    // memberIdx (u32 LE)
    val idxBuf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
    idxBuf.putInt(memberIdx)
    buf.write(idxBuf.array())
    // cmpOp (u8)
    buf.write(cmpOp)
    // value_len (varint)
    writeVarint(buf, typedValue.size)
    // typed_value
    buf.write(typedValue)
    return buf.toByteArray()
}

/** 构造 I32 值字节 */
fun i32Bytes(v: Int): ByteArray {
    val buf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
    buf.putInt(v)
    return buf.array()
}

/** 构造 F64 值字节 */
fun f64Bytes(v: Double): ByteArray {
    val buf = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN)
    buf.putDouble(v)
    return buf.array()
}

/** 构造 F32 值字节 */
fun f32Bytes(v: Float): ByteArray {
    val buf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
    buf.putFloat(v)
    return buf.array()
}

/** 构造 SET 指令 */
fun makeSetInst(path: List<Int>, operand: ByteArray): SpoiInstruction {
    return SpoiInstruction(Op.SET, path.toMutableList(), operand)
}

/** 构造 PIPE 指令 */
fun makePipeInst(path: List<Int>): SpoiInstruction {
    return SpoiInstruction(Op.PIPE, path.toMutableList(), ByteArray(0))
}

/** 构造 EXEC 指令 */
fun makeExecInst(): SpoiInstruction {
    return SpoiInstruction(Op.EXEC, mutableListOf(), ByteArray(0))
}

/** 构造 FILTER 指令 */
fun makeFilterInst(path: List<Int>, operand: ByteArray): SpoiInstruction {
    return SpoiInstruction(Op.FILTER, path.toMutableList(), operand)
}

/** 构造 SELECT 指令 */
fun makeSelectInst(path: List<Int>): SpoiInstruction {
    return SpoiInstruction(Op.SELECT, path.toMutableList(), ByteArray(0))
}

/** 构造 SORT 指令 */
fun makeSortInst(path: List<Int>): SpoiInstruction {
    return SpoiInstruction(Op.SORT, path.toMutableList(), ByteArray(0))
}

/** 构造 TAKE 指令 */
fun makeTakeInst(n: Int): SpoiInstruction {
    val buf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
    buf.putInt(n)
    return SpoiInstruction(Op.TAKE, mutableListOf(), buf.array())
}

/** 构造 DROP 指令 */
fun makeDropInst(n: Int): SpoiInstruction {
    val buf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
    buf.putInt(n)
    return SpoiInstruction(Op.DROP, mutableListOf(), buf.array())
}

/** 构造 REVERSE 指令 */
fun makeReverseInst(): SpoiInstruction {
    return SpoiInstruction(Op.REVERSE, mutableListOf(), ByteArray(0))
}

/** 构造 DISTINCT 指令 */
fun makeDistinctInst(): SpoiInstruction {
    return SpoiInstruction(Op.DISTINCT, mutableListOf(), ByteArray(0))
}

/** 构造 COUNT 指令 */
fun makeCountInst(): SpoiInstruction {
    return SpoiInstruction(Op.COUNT, mutableListOf(), ByteArray(0))
}

/** 构建 SPOI 指令流 */
fun buildSpoiStream(instructions: List<SpoiInstruction>): ByteArray {
    val buf = ByteArrayOutputStream()
    writeVarint(buf, instructions.size)
    for (inst in instructions) {
        buf.write(inst.op)
        writeVarint(buf, inst.path.size)
        for (seg in inst.path) writeVarint(buf, seg)
        writeVarint(buf, inst.operand.size)
        buf.write(inst.operand)
    }
    return buf.toByteArray()
}

// =============================== 1. 数值边界测试 ===============================

fun testNumericalBoundaries() {
    println("\n========================================")
    println("1. 数值边界测试")
    println("========================================")

    // --- U8 ---
    println("\n--- U8 ---")
    test("U8 0") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U8, byteArrayOf(0))) == 0)
    }
    test("U8 255") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U8, byteArrayOf(255.toByte()))) == 255)
    }
    test("U8 128") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U8, byteArrayOf(128.toByte()))) == 128)
    }

    // --- U16 ---
    println("\n--- U16 ---")
    test("U16 0") {
        val vb = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort(0).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U16, vb)) == 0)
    }
    test("U16 65535") {
        val vb = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort(65535.toShort()).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U16, vb)) == 65535)
    }
    test("U16 32768") {
        val vb = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort(32768.toShort()).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U16, vb)) == 32768)
    }

    // --- U32 ---
    println("\n--- U32 ---")
    test("U32 0") {
        val vb = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(0).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U32, vb)) == 0L)
    }
    test("U32 4294967295") {
        val vb = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(0xFFFFFFFF.toInt()).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U32, vb)) == 0xFFFFFFFFL)
    }
    test("U32 2147483648") {
        val vb = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(0x80000000.toInt()).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U32, vb)) == 0x80000000L)
    }

    // --- U64 ---
    println("\n--- U64 ---")
    test("U64 0") {
        val vb = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(0).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U64, vb)) == 0L)
    }
    test("U64 max") {
        val vb = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(-1L).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U64, vb)) == -1L)
    }
    test("U64 mid") {
        val vb = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(Long.MIN_VALUE).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U64, vb)) == Long.MIN_VALUE)
    }

    // --- I8 ---
    println("\n--- I8 ---")
    test("I8 -128") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I8, byteArrayOf((-128).toByte()))) == (-128).toByte())
    }
    test("I8 127") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I8, byteArrayOf(127))) == 127.toByte())
    }
    test("I8 0") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I8, byteArrayOf(0))) == 0.toByte())
    }
    test("I8 -1") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I8, byteArrayOf((-1).toByte()))) == (-1).toByte())
    }

    // --- I16 ---
    println("\n--- I16 ---")
    test("I16 -32768") {
        val vb = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort(-32768).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I16, vb)) == (-32768).toShort())
    }
    test("I16 32767") {
        val vb = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort(32767).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I16, vb)) == 32767.toShort())
    }
    test("I16 -1") {
        val vb = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort(-1).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I16, vb)) == (-1).toShort())
    }

    // --- I32 ---
    println("\n--- I32 ---")
    test("I32 -2147483648") {
        val vb = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(Int.MIN_VALUE).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I32, vb)) == Int.MIN_VALUE)
    }
    test("I32 2147483647") {
        val vb = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(Int.MAX_VALUE).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I32, vb)) == Int.MAX_VALUE)
    }
    test("I32 -1") {
        val vb = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(-1).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I32, vb)) == -1)
    }

    // --- I64 ---
    println("\n--- I64 ---")
    test("I64 -9223372036854775808") {
        val vb = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(Long.MIN_VALUE).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I64, vb)) == Long.MIN_VALUE)
    }
    test("I64 9223372036854775807") {
        val vb = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(Long.MAX_VALUE).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I64, vb)) == Long.MAX_VALUE)
    }
    test("I64 -1") {
        val vb = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(-1L).array()
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I64, vb)) == -1L)
    }

    // --- F32 ---
    println("\n--- F32 ---")
    test("F32 0.0f") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.F32, f32Bytes(0.0f))) == 0.0f)
    }
    test("F32 -0.0f") {
        val v = SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.F32, f32Bytes(-0.0f))) as Float
        check(java.lang.Float.floatToRawIntBits(v) == java.lang.Float.floatToRawIntBits(-0.0f))
    }
    test("F32 NaN") {
        val v = SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.F32, f32Bytes(Float.NaN))) as Float
        check(v.isNaN(), "F32 NaN")
    }
    test("F32 +Infinity") {
        val v = SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.F32, f32Bytes(Float.POSITIVE_INFINITY))) as Float
        check(v.isInfinite() && v > 0, "F32 +Infinity")
    }
    test("F32 -Infinity") {
        val v = SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.F32, f32Bytes(Float.NEGATIVE_INFINITY))) as Float
        check(v.isInfinite() && v < 0, "F32 -Infinity")
    }

    // --- F64 ---
    println("\n--- F64 ---")
    test("F64 0.0") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.F64, f64Bytes(0.0))) == 0.0)
    }
    test("F64 -0.0") {
        val v = SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.F64, f64Bytes(-0.0))) as Double
        check(java.lang.Double.doubleToRawLongBits(v) == java.lang.Double.doubleToRawLongBits(-0.0))
    }
    test("F64 NaN") {
        val v = SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.F64, f64Bytes(Double.NaN))) as Double
        check(v.isNaN(), "F64 NaN")
    }
    test("F64 +Infinity") {
        val v = SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.F64, f64Bytes(Double.POSITIVE_INFINITY))) as Double
        check(v.isInfinite() && v > 0, "F64 +Infinity")
    }
    test("F64 -Infinity") {
        val v = SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.F64, f64Bytes(Double.NEGATIVE_INFINITY))) as Double
        check(v.isInfinite() && v < 0, "F64 -Infinity")
    }
    test("F64 max safe") {
        val v = SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.F64, f64Bytes(9007199254740991.0))) as Double
        check(v == 9007199254740991.0, "F64 max safe")
    }

    // --- Bool ---
    println("\n--- Bool ---")
    test("Bool 1 => true") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.BOOL, byteArrayOf(1))) == true)
    }
    test("Bool 0 => false") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.BOOL, byteArrayOf(0))) == false)
    }
    test("Bool 42 => true (非零即真)") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.BOOL, byteArrayOf(42))) == true)
    }
    test("Bool 255 => true (非零即真)") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.BOOL, byteArrayOf(255.toByte()))) == true)
    }
}

// =============================== 2. 字符串边界测试 ===============================

fun testStringBoundaries() {
    println("\n========================================")
    println("2. 字符串边界测试")
    println("========================================")

    println("\n--- 空串 ---")
    test("空字符串") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, ByteArray(0))) == "")
    }

    println("\n--- Unicode/Emoji ---")
    test("你好世界") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, "你好世界".toByteArray(StandardCharsets.UTF_8))) == "你好世界")
    }
    test("你好世界🌍🎉") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, "你好世界🌍🎉".toByteArray(StandardCharsets.UTF_8))) == "你好世界🌍🎉")
    }
    test("CJK 字符") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, "日本語テスト".toByteArray(StandardCharsets.UTF_8))) == "日本語テスト")
    }
    test("混合 Emoji") {
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, "😀😃😄😁😆".toByteArray(StandardCharsets.UTF_8))) == "😀😃😄😁😆")
    }

    println("\n--- null 字节 ---")
    test("字符串含 null 字节") {
        val str = "hello\u0000world"
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, str.toByteArray(StandardCharsets.UTF_8))) == str)
    }
    test("仅 null 字节") {
        val str = "\u0000"
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, str.toByteArray(StandardCharsets.UTF_8))) == str)
    }

    println("\n--- 特殊字符 ---")
    test("换行符") {
        val str = "line1\nline2\nline3"
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, str.toByteArray(StandardCharsets.UTF_8))) == str)
    }
    test("制表符") {
        val str = "col1\tcol2\tcol3"
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, str.toByteArray(StandardCharsets.UTF_8))) == str)
    }
    test("引号") {
        val str = "he said \"hello\""
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, str.toByteArray(StandardCharsets.UTF_8))) == str)
    }
    test("反斜杠") {
        val str = "C:\\path\\to\\file"
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, str.toByteArray(StandardCharsets.UTF_8))) == str)
    }

    println("\n--- 长串 ---")
    test("1000 字符") {
        val str = "A".repeat(1000)
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, str.toByteArray(StandardCharsets.UTF_8))) == str)
    }

    println("\n--- 空格 ---")
    test("仅空格") {
        val str = "   "
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, str.toByteArray(StandardCharsets.UTF_8))) == str)
    }
    test("前后空格") {
        val str = "  hello  "
        check(SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, str.toByteArray(StandardCharsets.UTF_8))) == str)
    }
}

// =============================== 3. 反序列化异常测试 ===============================

fun testDeserializationErrors() {
    println("\n========================================")
    println("3. 反序列化异常测试")
    println("========================================")

    println("\n--- 截断数据 ---")
    test("空数据返回 null") {
        check(SpoiDeserializer.deserializeValue(ByteArray(0)) == null)
    }
    test("2 字节返回 null") {
        check(SpoiDeserializer.deserializeValue(byteArrayOf(1, 2)) == null)
    }
    test("3 字节返回 null") {
        check(SpoiDeserializer.deserializeValue(byteArrayOf(1, 2, 3)) == null)
    }
    test("U32 type_id 但只有 5 字节（值截断）") {
        val data = ByteArray(5)
        ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(TypeId.U32.toInt())
        data[4] = 0x42
        // U32 需要 4 字节，但只有 1 字节；ByteBuffer.wrap 按 4 字节读会抛异常
        try {
            SpoiDeserializer.deserializeValue(data)
            throw AssertionError("Expected exception for truncated U32")
        } catch (e: Exception) {
            check(e is java.nio.BufferUnderflowException || e is IndexOutOfBoundsException,
                "Should throw BufferUnderflowException or IndexOutOfBoundsException")
        }
    }
    test("U64 type_id 但只有 6 字节（值截断）") {
        val data = ByteArray(6)
        ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(TypeId.U64.toInt())
        try {
            SpoiDeserializer.deserializeValue(data)
            throw AssertionError("Expected exception for truncated U64")
        } catch (e: Exception) {
            check(e is java.nio.BufferUnderflowException || e is IndexOutOfBoundsException,
                "Should throw BufferUnderflowException or IndexOutOfBoundsException")
        }
    }

    println("\n--- 无效 type_id ---")
    test("type_id = 999（未知类型）返回原始字节") {
        val data = ByteArray(8)
        ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(999)
        val result = SpoiDeserializer.deserializeValue(data)
        check(result is ByteArray, "unknown type should return raw bytes")
    }
    test("type_id = 0 返回原始字节") {
        val data = ByteArray(8)
        ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(0)
        val result = SpoiDeserializer.deserializeValue(data)
        check(result is ByteArray, "type_id 0 should return raw bytes")
    }
    test("type_id = 255 返回原始字节") {
        val data = ByteArray(8)
        ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(255)
        val result = SpoiDeserializer.deserializeValue(data)
        check(result is ByteArray, "type_id 255 should return raw bytes")
    }

    println("\n--- type_id 无值 ---")
    test("U8 type_id 无值 => 异常") {
        try {
            SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U8, ByteArray(0)))
            check(false, "应该抛出异常")
        } catch (e: IndexOutOfBoundsException) {
            // 预期异常
        }
    }
    test("U16 type_id 无值 => 异常") {
        try {
            SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U16, ByteArray(0)))
            check(false, "应该抛出异常")
        } catch (e: BufferUnderflowException) {
            // 预期异常
        }
    }
    test("U32 type_id 无值 => 异常") {
        try {
            SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U32, ByteArray(0)))
            check(false, "应该抛出异常")
        } catch (e: BufferUnderflowException) {
            // 预期异常
        }
    }
    test("U64 type_id 无值 => 异常") {
        try {
            SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.U64, ByteArray(0)))
            check(false, "应该抛出异常")
        } catch (e: BufferUnderflowException) {
            // 预期异常
        }
    }
    test("I8 type_id 无值 => 异常") {
        try {
            SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.I8, ByteArray(0)))
            check(false, "应该抛出异常")
        } catch (e: IndexOutOfBoundsException) {
            // 预期异常
        }
    }
    test("STRING type_id 无值 => \"\"") {
        val result = SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.STRING, ByteArray(0)))
        check(result == "")
    }
    test("BOOL type_id 无值 => 异常") {
        try {
            SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.BOOL, ByteArray(0)))
            check(false, "应该抛出异常")
        } catch (e: IndexOutOfBoundsException) {
            // 预期异常
        }
    }
    test("F32 type_id 无值 => 异常") {
        try {
            SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.F32, ByteArray(0)))
            check(false, "应该抛出异常")
        } catch (e: BufferUnderflowException) {
            // 预期异常
        }
    }
    test("F64 type_id 无值 => 异常") {
        try {
            SpoiDeserializer.deserializeValue(makeTypedValue(TypeId.F64, ByteArray(0)))
            check(false, "应该抛出异常")
        } catch (e: BufferUnderflowException) {
            // 预期异常
        }
    }
}

// =============================== 4. Accessor 越界测试 ===============================

fun testAccessorOutOfBounds() {
    println("\n========================================")
    println("4. Accessor 越界测试")
    println("========================================")

    println("\n--- 负索引 ---")
    test("getField 负索引抛出") {
        val accessor = SpoiTestPlayerAccessor()
        val obj = SpoiTestPlayer()
        assertThrows(IllegalArgumentException::class.java) {
            accessor.getField(obj, -1)
        }
    }
    test("setField 负索引抛出") {
        val accessor = SpoiTestPlayerAccessor()
        val obj = SpoiTestPlayer()
        assertThrows(IllegalArgumentException::class.java) {
            accessor.setField(obj, -1, 1)
        }
    }

    println("\n--- fieldCount 越界 ---")
    test("getField fieldCount 越界") {
        val accessor = SpoiTestPlayerAccessor()
        val obj = SpoiTestPlayer()
        assertThrows(IllegalArgumentException::class.java) {
            accessor.getField(obj, accessor.fieldCount())
        }
    }
    test("setField fieldCount 越界") {
        val accessor = SpoiTestPlayerAccessor()
        val obj = SpoiTestPlayer()
        assertThrows(IllegalArgumentException::class.java) {
            accessor.setField(obj, accessor.fieldCount(), 1)
        }
    }

    println("\n--- 超大索引 ---")
    test("getField 超大索引") {
        val accessor = SpoiTestPlayerAccessor()
        val obj = SpoiTestPlayer()
        assertThrows(IllegalArgumentException::class.java) {
            accessor.getField(obj, 99999)
        }
    }
    test("setField 超大索引") {
        val accessor = SpoiTestPlayerAccessor()
        val obj = SpoiTestPlayer()
        assertThrows(IllegalArgumentException::class.java) {
            accessor.setField(obj, 99999, 1)
        }
    }

    println("\n--- 其他类型越界 ---")
    test("SpoiItem accessor getField 越界") {
        val accessor = SpoiItemAccessor()
        val obj = SpoiItem()
        assertThrows(IllegalArgumentException::class.java) {
            accessor.getField(obj, 99)
        }
    }
    test("SpoiInventory accessor setField 越界") {
        val accessor = SpoiInventoryAccessor()
        val obj = SpoiInventory()
        assertThrows(IllegalArgumentException::class.java) {
            accessor.setField(obj, 99, 1)
        }
    }

    println("\n--- 不同类型对象 ---")
    test("SpoiTestPlayer accessor 用于 SpoiItem（name 属性兼容）") {
        val accessor = SpoiTestPlayerAccessor()
        val obj = SpoiItem()
        // SpoiItem 也有 name 属性，但 accessor 会尝试 cast 为 SpoiTestPlayer
        assertThrows(ClassCastException::class.java) {
            accessor.getField(obj, 0)
        }
    }
}

// =============================== 5. Executor 组合操作测试 ===============================

fun testExecutorCombinations() {
    println("\n========================================")
    println("5. Executor 组合操作测试")
    println("========================================")

    val executor = SpoiExecutor(SpoiAccessorRegistry.registry)

    println("\n--- SET + PIPE + EXEC ---")
    test("SET hp + PIPE + EXEC 返回修改后的对象") {
        val player = SpoiTestPlayer()
        player.name = "Test"
        val data = buildSpoiStream(listOf(
            makeSetInst(listOf(1), makeSetOperandI32(TypeId.I32, 999)),
            makePipeInst(emptyList()),
            makeExecInst()
        ))
        val result = executor.execute(player, data)
        check(result["resultType"] == ResultType.SINGLE)
        val value = result["value"] as SpoiTestPlayer
        check(value.hp == 999)
        check(player.hp == 999)
    }

    println("\n--- 多层 SET ---")
    test("多层 SET：name, hp, level") {
        val player = SpoiTestPlayer()
        val data = buildSpoiStream(listOf(
            makeSetInst(listOf(0), makeSetOperandString(TypeId.STRING, "MultiSet")),
            makeSetInst(listOf(1), makeSetOperandI32(TypeId.I32, 500)),
            makeSetInst(listOf(2), makeSetOperandI32(TypeId.I32, 99)),
            makePipeInst(emptyList()),
            makeExecInst()
        ))
        val result = executor.execute(player, data)
        check(player.name == "MultiSet")
        check(player.hp == 500)
        check(player.level == 99)
        val value = result["value"] as SpoiTestPlayer
        check(value.name == "MultiSet")
    }

    println("\n--- FILTER ---")
    test("FILTER hp > 50") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer())
        items[0].hp = 100; items[0].name = "A"
        items[1].hp = 30;  items[1].name = "B"
        items[2].hp = 80;  items[2].name = "C"
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeFilterInst(emptyList(), makeFilterOperand(1, 3, TypeId.I32, i32Bytes(50))),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.VECTOR)
        val values = result["value"] as List<*>
        check(values.size == 2)
        check((values[0] as SpoiTestPlayer).name == "A")
        check((values[1] as SpoiTestPlayer).name == "C")
    }

    println("\n--- 双层 FILTER ---")
    test("双层 FILTER：hp > 30 且 level > 3") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer())
        items[0].hp = 100; items[0].level = 10; items[0].name = "A"
        items[1].hp = 50;  items[1].level = 2;  items[1].name = "B"
        items[2].hp = 80;  items[2].level = 5;  items[2].name = "C"
        items[3].hp = 20;  items[3].level = 8;  items[3].name = "D"
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeFilterInst(emptyList(), makeFilterOperand(1, 3, TypeId.I32, i32Bytes(30))),
            makeFilterInst(emptyList(), makeFilterOperand(2, 3, TypeId.I32, i32Bytes(3))),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.VECTOR)
        val values = result["value"] as List<*>
        check(values.size == 2)
        check((values[0] as SpoiTestPlayer).name == "A")
        check((values[1] as SpoiTestPlayer).name == "C")
    }

    println("\n--- 空结果 FILTER ---")
    test("FILTER 无匹配 => 空结果") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer())
        items[0].hp = 10; items[1].hp = 20
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeFilterInst(emptyList(), makeFilterOperand(1, 3, TypeId.I32, i32Bytes(999))),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.UNDEF)
    }

    println("\n--- COUNT ---")
    test("COUNT 有数据") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer())
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeCountInst(),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.SINGLE)
        check(result["value"] == 3)
    }

    test("空管道 COUNT => 0") {
        val items = emptyList<SpoiTestPlayer>()
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeCountInst(),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.SINGLE)
        check(result["value"] == 0)
    }

    println("\n--- SORT ---")
    test("SORT by name") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer())
        items[0].name = "Charlie"; items[0].hp = 30
        items[1].name = "Alice";   items[1].hp = 10
        items[2].name = "Bob";     items[2].hp = 20
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeSortInst(listOf(0)),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.VECTOR)
        val values = result["value"] as List<*>
        check(values.size == 3)
        check((values[0] as SpoiTestPlayer).name == "Alice")
        check((values[1] as SpoiTestPlayer).name == "Bob")
        check((values[2] as SpoiTestPlayer).name == "Charlie")
    }

    test("空管道 SORT 不报错") {
        val items = emptyList<SpoiTestPlayer>()
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeSortInst(listOf(0)),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.UNDEF)
    }

    println("\n--- TAKE ---")
    test("TAKE 2") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer())
        items[0].name = "A"; items[1].name = "B"; items[2].name = "C"
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeTakeInst(2),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.VECTOR)
        val values = result["value"] as List<*>
        check(values.size == 2)
        check((values[0] as SpoiTestPlayer).name == "A")
        check((values[1] as SpoiTestPlayer).name == "B")
    }

    test("TAKE 100（超过数量）") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer())
        items[0].name = "A"; items[1].name = "B"
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeTakeInst(100),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.VECTOR)
        val values = result["value"] as List<*>
        check(values.size == 2)
    }

    test("TAKE 0 => 空结果") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer())
        items[0].name = "A"; items[1].name = "B"
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeTakeInst(0),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.UNDEF)
    }

    println("\n--- DROP ---")
    test("DROP 1") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer())
        items[0].name = "A"; items[1].name = "B"; items[2].name = "C"
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeDropInst(1),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.VECTOR)
        val values = result["value"] as List<*>
        check(values.size == 2)
        check((values[0] as SpoiTestPlayer).name == "B")
        check((values[1] as SpoiTestPlayer).name == "C")
    }

    test("DROP 100（超过数量）=> 空结果") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer())
        items[0].name = "A"; items[1].name = "B"
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeDropInst(100),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.UNDEF)
    }

    println("\n--- SELECT ---")
    test("SELECT name") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer())
        items[0].name = "Hero"; items[1].name = "Villain"
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeSelectInst(listOf(0)),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.VECTOR)
        val values = result["value"] as List<*>
        check(values.size == 2)
        check(values[0] == "Hero")
        check(values[1] == "Villain")
    }

    test("空管道 SELECT 不报错") {
        val items = emptyList<SpoiTestPlayer>()
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeSelectInst(listOf(0)),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.UNDEF)
    }

    println("\n--- REVERSE ---")
    test("REVERSE 有数据") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer())
        items[0].name = "A"; items[1].name = "B"; items[2].name = "C"
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeReverseInst(),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.VECTOR)
        val values = result["value"] as List<*>
        check(values.size == 3)
        check((values[0] as SpoiTestPlayer).name == "C")
        check((values[1] as SpoiTestPlayer).name == "B")
        check((values[2] as SpoiTestPlayer).name == "A")
    }

    test("空管道 REVERSE 不报错") {
        val items = emptyList<SpoiTestPlayer>()
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeReverseInst(),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.UNDEF)
    }

    println("\n--- DISTINCT ---")
    test("DISTINCT 有重复") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer())
        items[0].name = "A"; items[0].hp = 10
        items[1].name = "A"; items[1].hp = 10
        items[2].name = "B"; items[2].hp = 20
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeDistinctInst(),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.VECTOR)
        val values = result["value"] as List<*>
        check(values.size == 2)
    }

    test("空管道 DISTINCT 不报错") {
        val items = emptyList<SpoiTestPlayer>()
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeDistinctInst(),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        check(result["resultType"] == ResultType.UNDEF)
    }

    println("\n--- 完整流水线 ---")
    test("PIPE→FILTER→SELECT→TAKE→EXEC") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer())
        items[0].name = "Sword";  items[0].hp = 100
        items[1].name = "Potion"; items[1].hp = 10
        items[2].name = "Shield"; items[2].hp = 50
        items[3].name = "Axe";    items[3].hp = 80
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeFilterInst(emptyList(), makeFilterOperand(1, 3, TypeId.I32, i32Bytes(20))),
            makeSelectInst(listOf(0)),
            makeTakeInst(2),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        val values = result["value"] as List<*>
        check(values.size == 2)
        check(values[0] == "Sword")
        check(values[1] == "Shield")
    }

    test("PIPE→SORT→REVERSE→DROP→TAKE→SELECT→EXEC") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer())
        items[0].name = "Sword";  items[0].hp = 100
        items[1].name = "Potion"; items[1].hp = 10
        items[2].name = "Shield"; items[2].hp = 50
        items[3].name = "Axe";    items[3].hp = 80
        items[4].name = "Dagger"; items[4].hp = 30
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeSortInst(listOf(1)),
            makeReverseInst(),
            makeDropInst(1),
            makeTakeInst(2),
            makeSelectInst(listOf(0)),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        val values = result["value"] as List<*>
        // Sort by hp: Potion(10), Dagger(30), Shield(50), Axe(80), Sword(100)
        // Reverse: Sword(100), Axe(80), Shield(50), Dagger(30), Potion(10)
        // Drop 1: Axe(80), Shield(50), Dagger(30), Potion(10)
        // Take 2: Axe(80), Shield(50)
        // Select name: "Axe", "Shield"
        check(values.size == 2)
        check(values[0] == "Axe")
        check(values[1] == "Shield")
    }

    test("PIPE→FILTER→COUNT→EXEC") {
        val items = listOf(SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer(), SpoiTestPlayer())
        items[0].hp = 100; items[1].hp = 10; items[2].hp = 50; items[3].hp = 80
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeFilterInst(emptyList(), makeFilterOperand(1, 3, TypeId.I32, i32Bytes(30))),
            makeCountInst(),
            makeExecInst()
        ))
        val result = executor.execute(items, data)
        // Filter hp > 30: 100, 50, 80 → count = 3
        check(result["value"] == 3)
    }
}

// =============================== 6. 跨类型 Executor 测试 ===============================

fun testCrossTypeExecutor() {
    println("\n========================================")
    println("6. 跨类型 Executor 测试")
    println("========================================")

    val executor = SpoiExecutor(SpoiAccessorRegistry.registry)

    println("\n--- Item ---")
    test("Item PIPE + EXEC") {
        val item = SpoiItem()
        item.name = "Potion"
        item.value = 50
        val data = buildSpoiStream(listOf(
            makePipeInst(emptyList()),
            makeExecInst()
        ))
        val result = executor.execute(item, data)
        check(result["resultType"] == ResultType.SINGLE)
        val value = result["value"] as SpoiItem
        check(value.name == "Potion")
        check(value.value == 50)
    }

    test("Item SET name") {
        val item = SpoiItem()
        val data = buildSpoiStream(listOf(
            makeSetInst(listOf(0), makeSetOperandString(TypeId.STRING, "Elixir")),
            makeExecInst()
        ))
        executor.execute(item, data)
        check(item.name == "Elixir")
    }

    println("\n--- Inventory ---")
    test("Inventory SET gold") {
        val inv = SpoiInventory()
        val data = buildSpoiStream(listOf(
            makeSetInst(listOf(2), makeSetOperandI32(TypeId.I32, 777)),
            makePipeInst(emptyList()),
            makeExecInst()
        ))
        val result = executor.execute(inv, data)
        check(inv.gold == 777)
        val value = result["value"] as SpoiInventory
        check(value.gold == 777)
    }

    println("\n--- Character ---")
    test("Character SET name + hp") {
        val char = SpoiCharacter()
        val data = buildSpoiStream(listOf(
            makeSetInst(listOf(0), makeSetOperandString(TypeId.STRING, "Mage")),
            makeSetInst(listOf(1), makeSetOperandI32(TypeId.I32, 300)),
            makePipeInst(emptyList()),
            makeExecInst()
        ))
        val result = executor.execute(char, data)
        check(char.name == "Mage")
        check(char.hp == 300)
        val value = result["value"] as SpoiCharacter
        check(value.name == "Mage")
    }

    println("\n--- World ---")
    test("World SET worldName + tick") {
        val world = SpoiWorld()
        val data = buildSpoiStream(listOf(
            makeSetInst(listOf(0), makeSetOperandString(TypeId.STRING, "Azeroth")),
            makeSetInst(listOf(1), makeSetOperandI32(TypeId.I32, 5000)),
            makePipeInst(emptyList()),
            makeExecInst()
        ))
        val result = executor.execute(world, data)
        check(world.worldName == "Azeroth")
        check(world.tick == 5000)
        val value = result["value"] as SpoiWorld
        check(value.worldName == "Azeroth")
    }
}

// =============================== 7. Registry 边界测试 ===============================

fun testRegistryBoundaries() {
    println("\n========================================")
    println("7. Registry 边界测试")
    println("========================================")

    println("\n--- size=6 ---")
    test("Registry 包含恰好 6 个类型") {
        check(SpoiAccessorRegistry.registry.size == 6)
    }
    test("Registry 包含 SpoiTestPlayer") {
        check(SpoiAccessorRegistry.registry.containsKey("SpoiTestPlayer"))
    }
    test("Registry 包含 SpoiTestState") {
        check(SpoiAccessorRegistry.registry.containsKey("SpoiTestState"))
    }
    test("Registry 包含 SpoiItem") {
        check(SpoiAccessorRegistry.registry.containsKey("SpoiItem"))
    }
    test("Registry 包含 SpoiInventory") {
        check(SpoiAccessorRegistry.registry.containsKey("SpoiInventory"))
    }
    test("Registry 包含 SpoiCharacter") {
        check(SpoiAccessorRegistry.registry.containsKey("SpoiCharacter"))
    }
    test("Registry 包含 SpoiWorld") {
        check(SpoiAccessorRegistry.registry.containsKey("SpoiWorld"))
    }

    println("\n--- missing key ---")
    test("get 不存在的 key 返回 null") {
        check(SpoiAccessorRegistry.get("NonExistentType") == null)
    }
    test("get 不存在的 key 返回 null（FakeType）") {
        check(SpoiAccessorRegistry.get("FakeType") == null)
    }

    println("\n--- all fieldCount > 0 ---")
    test("SpoiTestPlayerAccessor fieldCount > 0") {
        check(SpoiTestPlayerAccessor().fieldCount() > 0)
    }
    test("SpoiTestStateAccessor fieldCount > 0") {
        check(SpoiTestStateAccessor().fieldCount() > 0)
    }
    test("SpoiItemAccessor fieldCount > 0") {
        check(SpoiItemAccessor().fieldCount() > 0)
    }
    test("SpoiInventoryAccessor fieldCount > 0") {
        check(SpoiInventoryAccessor().fieldCount() > 0)
    }
    test("SpoiCharacterAccessor fieldCount > 0") {
        check(SpoiCharacterAccessor().fieldCount() > 0)
    }
    test("SpoiWorldAccessor fieldCount > 0") {
        check(SpoiWorldAccessor().fieldCount() > 0)
    }

    println("\n--- verify fieldCount values ---")
    test("SpoiTestPlayerAccessor fieldCount = 4") {
        check(SpoiTestPlayerAccessor().fieldCount() == 4)
    }
    test("SpoiTestStateAccessor fieldCount = 3") {
        check(SpoiTestStateAccessor().fieldCount() == 3)
    }
    test("SpoiItemAccessor fieldCount = 2") {
        check(SpoiItemAccessor().fieldCount() == 2)
    }
    test("SpoiInventoryAccessor fieldCount = 3") {
        check(SpoiInventoryAccessor().fieldCount() == 3)
    }
    test("SpoiCharacterAccessor fieldCount = 5") {
        check(SpoiCharacterAccessor().fieldCount() == 5)
    }
    test("SpoiWorldAccessor fieldCount = 3") {
        check(SpoiWorldAccessor().fieldCount() == 3)
    }
}

// =============================== 主入口 ===============================

fun main() {
    println("========================================")
    println("Stream-Punk SPOI 刁钻测试套件 (Kotlin)")
    println("========================================")

    testNumericalBoundaries()
    testStringBoundaries()
    testDeserializationErrors()
    testAccessorOutOfBounds()
    testExecutorCombinations()
    testCrossTypeExecutor()
    testRegistryBoundaries()

    println("\n========================================")
    println("结果: $passed 通过, $failed 失败")
    println("========================================")

    if (failures.isNotEmpty()) {
        println("\n失败详情:")
        for (f in failures) {
            println(f)
        }
    }

    if (failed > 0) {
        kotlin.system.exitProcess(1)
    }
}