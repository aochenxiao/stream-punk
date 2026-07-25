/**
 * StreamPunk SPOI Executor — Kotlin Runtime（v2: 访问器驱动，零反射）
 *
 * SPOI = StreamPunk Operation Instruction
 * 执行 SPOI 指令流，对 Kotlin/JVM 对象进行查询/更新操作。
 *
 * 与 v1 的区别：
 *   - 使用 SpoiAccessor 接口替代 java.lang.reflect.Field
 *   - 使用 DeserializeValue（基于 type_id 前缀）替代字节长度启发式
 *   - 导航和字段设置通过访问器的 when 跳转表，O(1) 且无反射开销
 *
 * 用法：
 *     val executor = SpoiExecutor(accessorRegistry)
 *     val result = executor.execute(rootObj, instructionBytes)
 */

import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.StandardCharsets

// =============================== 操作码常量 ===============================

object Op {
    // 导航
    const val NAV        = 0x00
    const val IDX        = 0x01
    const val DEREF      = 0x02
    const val UNWRAP     = 0x03
    // 写操作
    const val SET        = 0x04
    const val ADD        = 0x05
    const val APPEND     = 0x06
    const val REMOVE     = 0x07
    const val INSERT     = 0x08
    const val REPLACE    = 0x09
    const val RESET      = 0x0A
    const val SETNULL    = 0x0B
    // 读操作
    const val FILTER     = 0x0C
    const val SELECT     = 0x0D
    const val SORT       = 0x0E
    const val REVERSE    = 0x0F
    const val TAKE       = 0x10
    const val DROP       = 0x11
    const val TAKEWHILE  = 0x12
    const val DROPWHILE  = 0x13
    const val DISTINCT   = 0x14
    // 聚合
    const val COUNT      = 0x15
    const val ANY        = 0x16
    const val ALL        = 0x17
    const val FIND       = 0x18
    // 容器
    const val KEYS       = 0x19
    const val VALUES     = 0x1A
    const val JOIN       = 0x1B
    // 控制
    const val EXEC       = 0x21
    const val PIPE       = 0x22
}

// 路径特殊标记
const val PATH_DEREF  = 0xFFFF
const val PATH_MAPKEY = 0xFFFE

// 结果类型
object ResultType {
    const val UNDEF    = 0
    const val SINGLE   = 1
    const val VECTOR   = 2
    const val COUNT    = 3
    const val BOOL     = 4
    const val OPTIONAL = 5
    const val ERROR    = 6
}

// =============================== Varint 编解码 ===============================

fun readVarint(data: ByteArray, offset: IntArray): Int {
    var result = 0
    var shift = 0
    while (offset[0] < data.size) {
        val b = data[offset[0]].toInt() and 0xFF
        offset[0]++
        result = result or ((b and 0x7F) shl shift)
        if ((b and 0x80) == 0) {
            return result
        }
        shift += 7
    }
    return result
}

fun writeVarint(buf: ByteArrayOutputStream, v: Int) {
    var value = v
    while (value >= 0x80) {
        buf.write((value and 0x7F) or 0x80)
        value = value ushr 7
    }
    buf.write(value and 0x7F)
}

// =============================== SPOI 指令 ===============================

data class SpoiInstruction(
    val op: Int = 0,
    val path: MutableList<Int> = mutableListOf(),
    val operand: ByteArray = ByteArray(0)
)

fun parseSpoiStream(data: ByteArray): List<SpoiInstruction> {
    val offset = intArrayOf(0)
    val count = readVarint(data, offset)
    val instructions = mutableListOf<SpoiInstruction>()

    repeat(count) {
        // op
        val op = data[offset[0]].toInt() and 0xFF
        offset[0]++

        // path
        val pathLen = readVarint(data, offset)
        val path = mutableListOf<Int>()
        repeat(pathLen) {
            path.add(readVarint(data, offset))
        }

        // operand
        val operandLen = readVarint(data, offset)
        val operand = data.copyOfRange(offset[0], offset[0] + operandLen)
        offset[0] += operandLen

        instructions.add(SpoiInstruction(op, path, operand))
    }

    return instructions
}

// =============================== SPOI 执行器（v2: 访问器驱动） ===============================

class SpoiExecutor(
    private val accessors: Map<String, SpoiAccessor>
) {
    var pipeData: MutableList<Any?> = mutableListOf()
        private set

    fun execute(root: Any?, data: ByteArray): Map<String, Any?> {
        val instructions = parseSpoiStream(data)
        pipeData = mutableListOf()

        for (inst in instructions) {
            dispatch(inst, root)
        }

        return makeResult()
    }

    private fun makeResult(): Map<String, Any?> {
        if (pipeData.isEmpty()) {
            return mapOf("resultType" to ResultType.UNDEF, "data" to ByteArray(0))
        }

        return if (pipeData.size == 1) {
            mapOf("resultType" to ResultType.SINGLE, "value" to pipeData[0])
        } else {
            mapOf("resultType" to ResultType.VECTOR, "value" to pipeData.toList())
        }
    }

    // =============================== 调度 ===============================

    private fun dispatch(inst: SpoiInstruction, root: Any?) {
        when (inst.op) {
            // 写操作
            Op.SET     -> opSet(root, inst.path, inst.operand)
            Op.ADD     -> opAdd(root, inst.path, inst.operand)
            Op.APPEND  -> opAppend(root, inst.path, inst.operand)
            Op.REMOVE  -> opRemove(root, inst.path, inst.operand)
            Op.INSERT  -> opInsert(root, inst.path, inst.operand)
            Op.REPLACE -> opReplace(root, inst.path, inst.operand)
            Op.RESET   -> opReset(root, inst.path)
            Op.SETNULL -> opSetnull(root, inst.path)
            // 读操作
            Op.FILTER    -> opFilter(root, inst.path, inst.operand)
            Op.SELECT    -> opSelect(root, inst.path)
            Op.SORT      -> opSort(inst.path)
            Op.REVERSE   -> opReverse()
            Op.TAKE      -> opTake(inst.operand)
            Op.DROP      -> opDrop(inst.operand)
            Op.TAKEWHILE -> opTakewhile(root, inst.path, inst.operand)
            Op.DROPWHILE -> opDropwhile(root, inst.path, inst.operand)
            Op.DISTINCT  -> opDistinct()
            // 聚合
            Op.COUNT -> opCount()
            Op.ANY   -> opAny(root, inst.path, inst.operand)
            Op.ALL   -> opAll(root, inst.path, inst.operand)
            Op.FIND  -> opFind(root, inst.path, inst.operand)
            // 容器
            Op.KEYS   -> opKeys()
            Op.VALUES -> opValues()
            Op.JOIN   -> opJoin()
            // 控制
            Op.EXEC -> { /* 执行结束，结果已在 pipeData 中 */ }
            Op.PIPE -> opPipe(root, inst.path)
            else -> throw IllegalArgumentException(
                "Unknown SPOI opcode: 0x${inst.op.toString(16).padStart(2, '0').uppercase()}")
        }
    }

    // =============================== 导航（访问器驱动） ===============================

    fun navigate(obj: Any?, path: List<Int>): Any? {
        var current = obj
        for (seg in path) {
            current = navStep(current, seg)
        }
        return current
    }

    private fun navStep(obj: Any?, seg: Int): Any? {
        // null 处理
        if (obj == null) {
            throw RuntimeException("Cannot navigate on null")
        }

        // 指针解引用
        if (seg == PATH_DEREF) {
            // 基本类型返回自身
            if (obj is String || obj is Number || obj is Boolean) {
                return obj
            }
            val acc = getAccessor(obj)
            if (acc != null) {
                return acc.getField(obj, 0)
            }
            return obj
        }

        // 容器索引访问
        if (obj is List<*>) {
            return obj[seg]
        }
        if (obj is Map<*, *>) {
            return (obj as Map<*, *>).values.elementAt(seg)
        }

        // 基本类型：seg == 0 时返回自身
        if (obj is String || obj is Number || obj is Boolean) {
            if (seg == 0) {
                return obj
            }
            throw RuntimeException(
                "Cannot navigate segment $seg on ${obj.javaClass.simpleName}")
        }

        // 结构体成员访问 — 使用访问器
        val acc = getAccessor(obj)
        if (acc != null) {
            return acc.getField(obj, seg)
        }

        throw RuntimeException(
            "Cannot navigate segment $seg on ${obj.javaClass.simpleName}")
    }

    private fun navSet(obj: Any?, path: List<Int>, value: Any?) {
        if (path.isEmpty()) {
            return
        }
        if (path.size == 1) {
            setField(obj, path[0], value)
            return
        }

        var target: Any? = obj
        for (i in 0 until path.size - 1) {
            target = navStep(target, path[i])
        }
        setField(target, path.last(), value)
    }

    private fun setField(obj: Any?, seg: Int, value: Any?) {
        if (obj is MutableList<*>) {
            @Suppress("UNCHECKED_CAST")
            (obj as MutableList<Any?>)[seg] = value
            return
        }
        if (obj is MutableMap<*, *>) {
            @Suppress("UNCHECKED_CAST")
            val key = (obj as MutableMap<Any?, Any?>).keys.elementAt(seg)
            (obj as MutableMap<Any?, Any?>)[key] = value
            return
        }

        // 结构体 — 使用访问器
        val acc = getAccessor(obj)
        if (acc != null) {
            acc.setField(obj!!, seg, value)
            return
        }

        throw RuntimeException(
            "Cannot set field $seg on ${obj?.javaClass?.simpleName}")
    }

    /** 获取对象对应的访问器 */
    private fun getAccessor(obj: Any?): SpoiAccessor? {
        if (obj == null || accessors.isEmpty()) {
            return null
        }
        val typeName = obj.javaClass.simpleName
        return accessors[typeName]
    }

    // =============================== 写操作 ===============================

    private fun opSet(root: Any?, path: List<Int>, operand: ByteArray) {
        val value = SpoiDeserializer.deserializeValue(operand)
        navSet(root, path, value)
    }

    private fun opAdd(root: Any?, path: List<Int>, operand: ByteArray) {
        val delta = SpoiDeserializer.deserializeValue(operand)
        val target = navigate(root, path)
        val result = addValues(target, delta)
        navSet(root, path, result)
    }

    private fun addValues(a: Any?, b: Any?): Any? {
        if (a is Number && b is Number) {
            val da = a.toDouble()
            val db = b.toDouble()
            val sum = da + db
            return when {
                a is Int || a is Short || a is Byte -> {
                    if (b is Int || b is Short || b is Byte) sum.toInt()
                    else sum
                }
                a is Long -> sum.toLong()
                a is Float -> sum.toFloat()
                else -> sum
            }
        }
        if (a is String || b is String) {
            return a.toString() + b.toString()
        }
        throw RuntimeException(
            "Cannot add ${a?.javaClass?.simpleName} and ${b?.javaClass?.simpleName}")
    }

    private fun opAppend(root: Any?, path: List<Int>, operand: ByteArray) {
        val value = SpoiDeserializer.deserializeValue(operand)
        val target = navigate(root, path)
        if (target is MutableList<*>) {
            @Suppress("UNCHECKED_CAST")
            (target as MutableList<Any?>).add(value)
        } else {
            throw RuntimeException(
                "Cannot append to ${target?.javaClass?.simpleName}")
        }
    }

    private fun opRemove(root: Any?, path: List<Int>, operand: ByteArray) {
        val target = navigate(root, path)
        if (target is MutableList<*>) {
            val idx = ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).int
            target.removeAt(idx)
        } else {
            throw RuntimeException(
                "Cannot remove from ${target?.javaClass?.simpleName}")
        }
    }

    private fun opInsert(root: Any?, path: List<Int>, operand: ByteArray) {
        val idx = ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).int
        val value = SpoiDeserializer.deserializeValue(operand.copyOfRange(4, operand.size))
        val target = navigate(root, path)
        if (target is MutableList<*>) {
            @Suppress("UNCHECKED_CAST")
            (target as MutableList<Any?>).add(idx, value)
        } else {
            throw RuntimeException(
                "Cannot insert into ${target?.javaClass?.simpleName}")
        }
    }

    private fun opReplace(root: Any?, path: List<Int>, operand: ByteArray) {
        val idx = ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).int
        val value = SpoiDeserializer.deserializeValue(operand.copyOfRange(4, operand.size))
        val target = navigate(root, path)
        if (target is MutableList<*>) {
            @Suppress("UNCHECKED_CAST")
            (target as MutableList<Any?>)[idx] = value
        } else {
            throw RuntimeException(
                "Cannot replace in ${target?.javaClass?.simpleName}")
        }
    }

    private fun opReset(root: Any?, path: List<Int>) {
        navSet(root, path, null)
    }

    private fun opSetnull(root: Any?, path: List<Int>) {
        navSet(root, path, null)
    }

    // =============================== 读操作 ===============================

    private fun opPipe(root: Any?, path: List<Int>) {
        val data: Any? = if (path.isNotEmpty()) navigate(root, path) else root

        pipeData = when (data) {
            is List<*> -> data.toMutableList() as MutableList<Any?>
            is Map<*, *> -> (data as Map<*, *>).values.toMutableList()
            else -> mutableListOf(data)
        }
    }

    /** 检查对象是否匹配比较表达式（v2: 访问器驱动） */
    private fun matches(obj: Any?, path: List<Int>, operand: ByteArray): Boolean {
        // operand 格式: memberIdx(u32) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
        if (operand.size < 9) {
            return true
        }
        val memberIdx = ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).int
        val cmpOp = operand[4].toInt() and 0xFF
        // 跳过 value_len（varint 编码）
        val valueOffset = intArrayOf(5)
        readVarint(operand, valueOffset)
        val valueBytes = operand.copyOfRange(valueOffset[0], operand.size)

        // 先按路径导航到目标对象，再访问成员字段
        var target: Any? = obj
        if (path.isNotEmpty()) {
            target = navigate(obj, path)
        }
        // 对于基本类型，memberIdx=0 时直接比较值本身
        val fieldValue: Any? = if (memberIdx == 0 && (target is Number || target is String || target is Boolean)) {
            target
        } else {
            navStep(target, memberIdx)
        }
        val expected = SpoiDeserializer.deserializeValue(valueBytes)

        return compareValues(fieldValue, cmpOp, expected)
    }

    private fun compareValues(fieldValue: Any?, cmpOp: Int, expected: Any?): Boolean {
        if (fieldValue == null && expected == null) {
            return cmpOp == 0 // eq
        }
        if (fieldValue == null || expected == null) {
            return cmpOp == 1 // ne
        }

        val cmp: Int = when {
            fieldValue is Comparable<*> && expected is Comparable<*> -> {
                try {
                    @Suppress("UNCHECKED_CAST")
                    (fieldValue as Comparable<Any?>).compareTo(expected)
                } catch (e: Exception) {
                    fieldValue.toString().compareTo(expected.toString())
                }
            }
            else -> fieldValue.toString().compareTo(expected.toString())
        }

        return when (cmpOp) {
            0 -> cmp == 0  // eq
            1 -> cmp != 0  // ne
            2 -> cmp < 0   // lt
            3 -> cmp > 0   // gt
            4 -> cmp <= 0  // le
            5 -> cmp >= 0  // ge
            else -> true
        }
    }

    private fun opFilter(root: Any?, path: List<Int>, operand: ByteArray) {
        val filtered = pipeData.filter { matches(it, path, operand) }
        pipeData = filtered.toMutableList()
    }

    private fun opSelect(root: Any?, path: List<Int>) {
        if (path.isNotEmpty()) {
            val selected = pipeData.map { navigate(it, path) }
            pipeData = selected.toMutableList()
        }
    }

    private fun opSort(path: List<Int>) {
        if (path.isNotEmpty()) {
            pipeData.sortWith(Comparator { a, b ->
                val va = navigate(a, path)
                val vb = navigate(b, path)
                compareSpValues(va, vb)
            })
        } else {
            pipeData.sortWith(Comparator { a, b ->
                a.toString().compareTo(b.toString())
            })
        }
    }

    private fun compareSpValues(a: Any?, b: Any?): Int {
        if (a == null && b == null) return 0
        if (a == null) return -1
        if (b == null) return 1
        return when {
            a is Number && b is Number -> a.toDouble().compareTo(b.toDouble())
            a is String && b is String -> a.compareTo(b)
            else -> a.toString().compareTo(b.toString())
        }
    }

    private fun opReverse() {
        pipeData.reverse()
    }

    private fun opTake(operand: ByteArray) {
        var n: Long = 0
        if (operand.size >= 4) {
            n = ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).int.toLong() and 0xFFFFFFFFL
        }
        if (n > pipeData.size) {
            n = pipeData.size.toLong()
        }
        pipeData = pipeData.take(n.toInt()).toMutableList()
    }

    private fun opDrop(operand: ByteArray) {
        var n: Long = 0
        if (operand.size >= 4) {
            n = ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).int.toLong() and 0xFFFFFFFFL
        }
        if (n >= pipeData.size) {
            pipeData = mutableListOf()
        } else if (n > 0) {
            pipeData = pipeData.drop(n.toInt()).toMutableList()
        }
    }

    private fun opTakewhile(root: Any?, path: List<Int>, operand: ByteArray) {
        val result = mutableListOf<Any?>()
        for (obj in pipeData) {
            if (matches(obj, path, operand)) {
                result.add(obj)
            } else {
                break
            }
        }
        pipeData = result
    }

    private fun opDropwhile(root: Any?, path: List<Int>, operand: ByteArray) {
        var idx = pipeData.size
        for (i in pipeData.indices) {
            if (!matches(pipeData[i], path, operand)) {
                idx = i
                break
            }
        }
        pipeData = pipeData.drop(idx).toMutableList()
    }

    private fun opDistinct() {
        val seen = linkedSetOf<Any?>()
        val result = mutableListOf<Any?>()
        for (obj in pipeData) {
            val key: Any? = when (obj) {
                is Number, is String, is Boolean -> obj
                else -> obj.toString()
            }
            if (seen.add(key)) {
                result.add(obj)
            }
        }
        pipeData = result
    }

    // =============================== 聚合 ===============================

    private fun opCount() {
        val count = pipeData.size
        pipeData = mutableListOf(count)
    }

    private fun opAny(root: Any?, path: List<Int>, operand: ByteArray) {
        val any = pipeData.any { matches(it, path, operand) }
        pipeData = mutableListOf(any)
    }

    private fun opAll(root: Any?, path: List<Int>, operand: ByteArray) {
        val all = pipeData.all { matches(it, path, operand) }
        pipeData = mutableListOf(all)
    }

    private fun opFind(root: Any?, path: List<Int>, operand: ByteArray) {
        for (obj in pipeData) {
            if (matches(obj, path, operand)) {
                pipeData = mutableListOf(obj)
                return
            }
        }
        pipeData = mutableListOf()
    }

    // =============================== 容器操作 ===============================

    private fun opKeys() {
        val result = mutableListOf<Any?>()
        for (obj in pipeData) {
            if (obj is Map<*, *>) {
                result.addAll(obj.keys)
            }
        }
        pipeData = result
    }

    private fun opValues() {
        val result = mutableListOf<Any?>()
        for (obj in pipeData) {
            if (obj is Map<*, *>) {
                result.addAll(obj.values)
            }
        }
        pipeData = result
    }

    private fun opJoin() {
        val result = mutableListOf<Any?>()
        for (obj in pipeData) {
            if (obj is List<*>) {
                result.addAll(obj)
            } else {
                result.add(obj)
            }
        }
        pipeData = result
    }
}