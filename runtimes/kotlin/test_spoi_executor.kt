/**
 * Stream-Punk SPOI Executor 测试套件
 * 编译: kotlinc spoi_executor.kt test_spoi_executor.kt -include-runtime -d test_spoi_executor.jar
 * 运行: java -jar test_spoi_executor.jar TestSpoiExecutorKt
 */

import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

// =============================== 测试数据类 ===============================

class Item(val name: String, val price: Int)

class Player(val name: String, var level: Int, var health: Int) {
    val items: ArrayList<Item> = ArrayList()
    val metadata: HashMap<String, Any?> = HashMap()
}

// =============================== 类型注册表 ===============================

val typeRegistry = mapOf(
    "Player" to listOf("name", "level", "health", "items", "metadata"),
    "Item" to listOf("name", "price")
)

// =============================== 测试基础设施 ===============================

var passed = 0
var failed = 0

fun test(name: String, fn: () -> Unit) {
    try {
        fn()
        passed++
        println("   ✔ $name")
    } catch (e: Throwable) {
        failed++
        println("   ✘ $name")
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

/** 从执行结果中安全提取列表：SINGLE 包装为单元素列表，UNDEF 返回空列表，VECTOR 直接返回 */
fun getResultList(result: Map<String, Any?>): List<Any?> {
    return when (result["resultType"]) {
        ResultType.SINGLE -> listOf(result["value"])
        ResultType.VECTOR -> result["value"] as List<*>
        else -> emptyList()
    }
}

// =============================== 辅助函数 ===============================

fun buildSpoiStream(instructions: List<SpoiInstruction>): ByteArray {
    val buf = ByteArrayOutputStream()
    writeVarint(buf, instructions.size)
    for (inst in instructions) {
        buf.write(inst.op)
        writeVarint(buf, inst.path.size)
        for (seg in inst.path) {
            writeVarint(buf, seg)
        }
        writeVarint(buf, inst.operand.size)
        buf.write(inst.operand)
    }
    return buf.toByteArray()
}

fun makeInst(op: Int, path: List<Int>, operand: ByteArray): SpoiInstruction {
    return SpoiInstruction(op, path.toMutableList(), operand)
}

fun setInt(path: List<Int>, value: Int): SpoiInstruction {
    val buf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
    buf.putInt(value)
    return makeInst(Op.SET, path, buf.array())
}

fun setStr(path: List<Int>, value: String): SpoiInstruction {
    return makeInst(Op.SET, path, value.toByteArray(Charsets.UTF_8))
}

fun pipeInst(path: List<Int>): SpoiInstruction {
    return makeInst(Op.PIPE, path, ByteArray(0))
}

fun filterGt(path: List<Int>, memberIdx: Int, value: Int): SpoiInstruction {
    val buf = ByteBuffer.allocate(9).order(ByteOrder.LITTLE_ENDIAN)
    buf.putInt(memberIdx)
    buf.put(3.toByte())  // cmpOp: e_gt = 3
    buf.putInt(value)
    return makeInst(Op.FILTER, path, buf.array())
}

fun filterEq(path: List<Int>, memberIdx: Int, value: Int): SpoiInstruction {
    val buf = ByteBuffer.allocate(9).order(ByteOrder.LITTLE_ENDIAN)
    buf.putInt(memberIdx)
    buf.put(0.toByte())  // cmpOp: e_eq = 0
    buf.putInt(value)
    return makeInst(Op.FILTER, path, buf.array())
}

fun selectInst(path: List<Int>): SpoiInstruction {
    return makeInst(Op.SELECT, path, ByteArray(0))
}

fun sortInst(path: List<Int>): SpoiInstruction {
    return makeInst(Op.SORT, path, ByteArray(0))
}

fun takeInst(n: Int): SpoiInstruction {
    val buf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
    buf.putInt(n)
    return makeInst(Op.TAKE, emptyList<Int>(), buf.array())
}

fun dropInst(n: Int): SpoiInstruction {
    val buf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
    buf.putInt(n)
    return makeInst(Op.DROP, emptyList<Int>(), buf.array())
}

fun reverseInst(): SpoiInstruction {
    return makeInst(Op.REVERSE, emptyList<Int>(), ByteArray(0))
}

fun distinctInst(): SpoiInstruction {
    return makeInst(Op.DISTINCT, emptyList<Int>(), ByteArray(0))
}

fun countInst(): SpoiInstruction {
    return makeInst(Op.COUNT, emptyList<Int>(), ByteArray(0))
}

fun anyInst(memberIdx: Int, cmpOp: Int, value: Int): SpoiInstruction {
    val buf = ByteBuffer.allocate(9).order(ByteOrder.LITTLE_ENDIAN)
    buf.putInt(memberIdx)
    buf.put(cmpOp.toByte())
    buf.putInt(value)
    return makeInst(Op.ANY, emptyList<Int>(), buf.array())
}

fun allInst(memberIdx: Int, cmpOp: Int, value: Int): SpoiInstruction {
    val buf = ByteBuffer.allocate(9).order(ByteOrder.LITTLE_ENDIAN)
    buf.putInt(memberIdx)
    buf.put(cmpOp.toByte())
    buf.putInt(value)
    return makeInst(Op.ALL, emptyList<Int>(), buf.array())
}

fun findInst(memberIdx: Int, cmpOp: Int, value: Int): SpoiInstruction {
    val buf = ByteBuffer.allocate(9).order(ByteOrder.LITTLE_ENDIAN)
    buf.putInt(memberIdx)
    buf.put(cmpOp.toByte())
    buf.putInt(value)
    return makeInst(Op.FIND, emptyList<Int>(), buf.array())
}

fun execInst(): SpoiInstruction {
    return makeInst(Op.EXEC, emptyList<Int>(), ByteArray(0))
}

// =============================== 测试用例 ===============================

fun main() {
    println("\nVarint 编解码")
    test("varint roundtrip") {
        for (v in listOf(0, 1, 127, 128, 255, 256, 16383, 16384, 2097151, 2097152, 268435455, 268435456)) {
            val buf = ByteArrayOutputStream()
            writeVarint(buf, v)
            val offset = intArrayOf(0)
            val result = readVarint(buf.toByteArray(), offset)
            assertEqual(result, v, "varint: $v")
        }
    }

    println("\n指令流解析")
    test("parse instruction stream") {
        val insts = listOf(
            pipeInst(listOf(3)),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val parsed = parseSpoiStream(data)
        assertEqual(parsed.size, 2, "instruction count")
        assertEqual(parsed[0].op, Op.PIPE, "first op")
        assertEqual(parsed[0].path.toList(), listOf(3), "first path")
        assertEqual(parsed[1].op, Op.EXEC, "second op")
    }

    test("parse instruction stream with operand") {
        val insts = listOf(
            setInt(listOf(1), 42),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val parsed = parseSpoiStream(data)
        assertEqual(parsed.size, 2, "instruction count")
        assertEqual(parsed[0].op, Op.SET, "first op")
        assertEqual(parsed[0].path.toList(), listOf(1), "first path")
        val operandVal = ByteBuffer.wrap(parsed[0].operand).order(ByteOrder.LITTLE_ENDIAN).int
        assertEqual(operandVal, 42, "operand value")
    }

    println("\n基本导航")
    test("navigate to name field") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        val result = executor.navigate(player, listOf(0))
        assertEqual(result, "Alice", "navigate to name")
    }

    test("navigate to level field") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        val result = executor.navigate(player, listOf(1))
        assertEqual(result, 10, "navigate to level")
    }

    test("navigate to nested item field") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        val items = executor.navigate(player, listOf(3)) as List<*>
        val item = items[0] as Item
        val price = executor.navigate(item, listOf(1))
        assertEqual(price, 100, "navigate to item price")
    }

    println("\nSET 操作")
    test("SET string field") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        val insts = listOf(
            setStr(listOf(0), "Bob"),
            execInst()
        )
        val data = buildSpoiStream(insts)
        executor.execute(player, data)
        assertEqual(player.name, "Bob", "name set")
    }

    test("SET int field") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        val insts = listOf(
            setInt(listOf(1), 20),
            execInst()
        )
        val data = buildSpoiStream(insts)
        executor.execute(player, data)
        assertEqual(player.level, 20, "level set")
    }

    test("SET multiple fields") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        val insts = listOf(
            setStr(listOf(0), "Charlie"),
            setInt(listOf(1), 30),
            setInt(listOf(2), 200),
            execInst()
        )
        val data = buildSpoiStream(insts)
        executor.execute(player, data)
        assertEqual(player.name, "Charlie", "name set")
        assertEqual(player.level, 30, "level set")
        assertEqual(player.health, 200, "health set")
    }

    println("\nADD 操作")
    test("ADD to level") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        val addBuf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
        addBuf.putInt(5)
        val insts = listOf(
            makeInst(Op.ADD, listOf(1), addBuf.array()),
            execInst()
        )
        val data = buildSpoiStream(insts)
        executor.execute(player, data)
        assertEqual(player.level, 15, "level after add")
    }

    test("ADD to health") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        val addBuf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
        addBuf.putInt(-20)
        val insts = listOf(
            makeInst(Op.ADD, listOf(2), addBuf.array()),
            execInst()
        )
        val data = buildSpoiStream(insts)
        executor.execute(player, data)
        assertEqual(player.health, 80, "health after add")
    }

    println("\nPIPE 操作")
    test("PIPE list of items") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Potion", 10))

        val insts = listOf(
            pipeInst(listOf(3)),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["resultType"], ResultType.VECTOR, "result type")
        val values = result["value"] as List<*>
        assertEqual(values.size, 3, "item count")
    }

    test("PIPE single value") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)

        val insts = listOf(
            pipeInst(listOf(1)),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["resultType"], ResultType.SINGLE, "result type")
        assertEqual(result["value"], 10, "level value")
    }

    println("\nFILTER 操作")
    test("FILTER gt") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Potion", 10))

        val insts = listOf(
            pipeInst(listOf(3)),
            filterGt(emptyList(), 1, 50),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["resultType"], ResultType.SINGLE, "result type")
        val values = getResultList(result)
        assertEqual(values.size, 1, "filtered count")
        assertEqual((values[0] as Item).name, "Sword", "filtered item")
    }

    test("FILTER eq") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Potion", 10))

        val insts = listOf(
            pipeInst(listOf(3)),
            filterEq(emptyList(), 1, 50),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["resultType"], ResultType.SINGLE, "result type")
        val values = getResultList(result)
        assertEqual(values.size, 1, "filtered count")
        assertEqual((values[0] as Item).name, "Shield", "filtered item")
    }

    test("FILTER no match") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))

        val insts = listOf(
            pipeInst(listOf(3)),
            filterGt(emptyList(), 1, 999),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["resultType"], ResultType.UNDEF, "result type")
        val values = getResultList(result)
        assertEqual(values.size, 0, "empty filtered")
    }

    println("\nSELECT 操作")
    test("SELECT name from items") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))

        val insts = listOf(
            pipeInst(listOf(3)),
            selectInst(listOf(0)),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = result["value"] as List<*>
        assertEqual(values[0], "Sword", "first name")
        assertEqual(values[1], "Shield", "second name")
    }

    test("SELECT price from items") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Potion", 10))

        val insts = listOf(
            pipeInst(listOf(3)),
            selectInst(listOf(1)),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = result["value"] as List<*>
        assertEqual(values[0], 100, "first price")
        assertEqual(values[1], 10, "second price")
    }

    println("\nSORT 操作")
    test("SORT items by price ascending") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Potion", 10))
        player.items.add(Item("Shield", 50))

        val insts = listOf(
            pipeInst(listOf(3)),
            sortInst(listOf(1)),
            selectInst(listOf(0)),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = result["value"] as List<*>
        assertEqual(values[0], "Potion", "cheapest first")
        assertEqual(values[1], "Shield", "middle")
        assertEqual(values[2], "Sword", "most expensive last")
    }

    test("SORT items by name") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Axe", 80))
        player.items.add(Item("Potion", 10))

        val insts = listOf(
            pipeInst(listOf(3)),
            sortInst(listOf(0)),
            selectInst(listOf(0)),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = result["value"] as List<*>
        assertEqual(values[0], "Axe", "alphabetically first")
        assertEqual(values[1], "Potion", "alphabetically second")
        assertEqual(values[2], "Sword", "alphabetically third")
    }

    println("\nTAKE 操作")
    test("TAKE first 2") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Potion", 10))

        val insts = listOf(
            pipeInst(listOf(3)),
            takeInst(2),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = result["value"] as List<*>
        assertEqual(values.size, 2, "taken count")
    }

    test("TAKE more than available") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))

        val insts = listOf(
            pipeInst(listOf(3)),
            takeInst(10),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = getResultList(result)
        assertEqual(values.size, 1, "taken count")
    }

    test("TAKE zero") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))

        val insts = listOf(
            pipeInst(listOf(3)),
            takeInst(0),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = getResultList(result)
        assertEqual(values.size, 0, "empty taken")
    }

    println("\nDROP 操作")
    test("DROP first 1") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Potion", 10))

        val insts = listOf(
            pipeInst(listOf(3)),
            dropInst(1),
            selectInst(listOf(0)),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = result["value"] as List<*>
        assertEqual(values.size, 2, "remaining count")
        assertEqual(values[0], "Shield", "first after drop")
        assertEqual(values[1], "Potion", "second after drop")
    }

    test("DROP all") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))

        val insts = listOf(
            pipeInst(listOf(3)),
            dropInst(5),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = getResultList(result)
        assertEqual(values.size, 0, "all dropped")
    }

    println("\nREVERSE 操作")
    test("REVERSE item order") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Potion", 10))

        val insts = listOf(
            pipeInst(listOf(3)),
            reverseInst(),
            selectInst(listOf(0)),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = result["value"] as List<*>
        assertEqual(values[0], "Potion", "first after reverse")
        assertEqual(values[1], "Shield", "second after reverse")
        assertEqual(values[2], "Sword", "third after reverse")
    }

    test("REVERSE empty") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)

        val insts = listOf(
            pipeInst(listOf(3)),
            reverseInst(),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = getResultList(result)
        assertEqual(values.size, 0, "empty reversed")
    }

    println("\nDISTINCT 操作")
    test("DISTINCT on names") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Sword", 200))
        player.items.add(Item("Shield", 50))

        val insts = listOf(
            pipeInst(listOf(3)),
            selectInst(listOf(0)),
            distinctInst(),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = result["value"] as List<*>
        assertEqual(values.size, 2, "distinct count")
        assertTrue(values.contains("Sword"), "contains Sword")
        assertTrue(values.contains("Shield"), "contains Shield")
    }

    test("DISTINCT no duplicates") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Potion", 10))

        val insts = listOf(
            pipeInst(listOf(3)),
            selectInst(listOf(0)),
            distinctInst(),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = result["value"] as List<*>
        assertEqual(values.size, 3, "distinct count unchanged")
    }

    println("\nCOUNT 操作")
    test("COUNT items") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Potion", 10))

        val insts = listOf(
            pipeInst(listOf(3)),
            countInst(),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["resultType"], ResultType.SINGLE, "result type")
        assertEqual(result["value"], 3, "count")
    }

    test("COUNT empty") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)

        val insts = listOf(
            pipeInst(listOf(3)),
            countInst(),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["value"], 0, "count zero")
    }

    println("\nANY 操作")
    test("ANY price == 100") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))

        val insts = listOf(
            pipeInst(listOf(3)),
            anyInst(1, 0, 100),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["resultType"], ResultType.SINGLE, "result type")
        assertEqual(result["value"], true, "any found")
    }

    test("ANY price > 200 (none)") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))

        val insts = listOf(
            pipeInst(listOf(3)),
            anyInst(1, 3, 200),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["value"], false, "none found")
    }

    println("\nALL 操作")
    test("ALL price > 0") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Potion", 10))

        val insts = listOf(
            pipeInst(listOf(3)),
            allInst(1, 3, 0),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["resultType"], ResultType.SINGLE, "result type")
        assertEqual(result["value"], true, "all match")
    }

    test("ALL price > 50 (not all)") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Potion", 10))

        val insts = listOf(
            pipeInst(listOf(3)),
            allInst(1, 3, 50),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["value"], false, "not all match")
    }

    println("\nFIND 操作")
    test("FIND price == 50") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Potion", 10))

        val insts = listOf(
            pipeInst(listOf(3)),
            findInst(1, 0, 50),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["resultType"], ResultType.SINGLE, "result type")
        val found = result["value"] as Item
        assertEqual(found.name, "Shield", "found item name")
        assertEqual(found.price, 50, "found item price")
    }

    test("FIND no match") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Shield", 50))

        val insts = listOf(
            pipeInst(listOf(3)),
            findInst(1, 0, 999),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["resultType"], ResultType.UNDEF, "no match result type")
    }

    println("\n完整流水线")
    test("Full pipeline: PIPE→FILTER→SELECT→TAKE→EXEC") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Potion", 10))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Axe", 80))

        val insts = listOf(
            pipeInst(listOf(3)),
            filterGt(emptyList(), 1, 20),
            selectInst(listOf(0)),
            takeInst(2),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = result["value"] as List<*>
        assertEqual(values.size, 2, "result count")
        assertEqual(values[0], "Sword", "first result")
        assertEqual(values[1], "Shield", "second result")
    }

    test("Full pipeline: PIPE→SORT→REVERSE→DROP→TAKE→EXEC") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Potion", 10))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Axe", 80))
        player.items.add(Item("Dagger", 30))

        val insts = listOf(
            pipeInst(listOf(3)),
            sortInst(listOf(1)),
            reverseInst(),
            dropInst(1),
            takeInst(2),
            selectInst(listOf(0)),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        val values = result["value"] as List<*>
        // After sort by price: Potion(10), Dagger(30), Shield(50), Axe(80), Sword(100)
        // After reverse: Sword(100), Axe(80), Shield(50), Dagger(30), Potion(10)
        // After drop 1: Axe(80), Shield(50), Dagger(30), Potion(10)
        // After take 2: Axe(80), Shield(50)
        // After select name: "Axe", "Shield"
        assertEqual(values.size, 2, "result count")
        assertEqual(values[0], "Axe", "first result")
        assertEqual(values[1], "Shield", "second result")
    }

    test("Full pipeline: PIPE→FILTER→COUNT→EXEC") {
        val executor = SpoiExecutor(typeRegistry)
        val player = Player("Alice", 10, 100)
        player.items.add(Item("Sword", 100))
        player.items.add(Item("Potion", 10))
        player.items.add(Item("Shield", 50))
        player.items.add(Item("Axe", 80))

        val insts = listOf(
            pipeInst(listOf(3)),
            filterGt(emptyList(), 1, 30),
            countInst(),
            execInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        // Filter price > 30: Sword(100), Shield(50), Axe(80) → count = 3
        assertEqual(result["value"], 3, "filtered count")
    }

    // ======================== 总结 ========================

    println("\n========================================")
    println("通过: $passed, 失败: $failed")
    println("========================================")
    if (failed > 0) {
        kotlin.system.exitProcess(1)
    }
}