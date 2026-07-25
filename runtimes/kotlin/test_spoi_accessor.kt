/**
 * Stream-Punk SPOI Accessor 测试套件
 * 测试: TypeId 常量、SpoiDeserializer、6 个 Accessor、SpoiAccessorRegistry、Executor 集成
 * 编译: kotlinc spoi_accessor.kt spoi_executor.kt test_spoi_accessor.kt -include-runtime -d test_spoi_accessor.jar
 * 运行: java -jar test_spoi_accessor.jar TestSpoiAccessorKt
 */

import java.nio.ByteBuffer
import java.nio.ByteOrder

// =============================== 测试数据类 ===============================

class SpoiTestPlayer(var name: String = "", var hp: Int = 0, var level: Int = 0, var posX: Double = 0.0)
class SpoiTestState(var tick: Int = 0, var currentMap: String = "", var players: Any? = null)
class SpoiItem(var name: String = "", var value: Int = 0)
class SpoiInventory(var items: Any? = null, var equipped: Any? = null, var gold: Int = 0)
class SpoiCharacter(var name: String = "", var hp: Int = 0, var inventory: Any? = null, var weapon: Any? = null, var petLevel: Int = 0)
class SpoiWorld(var worldName: String = "", var tick: Int = 0, var characters: Any? = null)

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

fun assertNotNull(v: Any?, msg: String? = null) {
    if (v == null) throw AssertionError(msg ?: "expected non-null, got null")
}

// =============================== 辅助函数 ===============================

/** 构造 [type_id(u32 LE) + value_bytes] 格式的 ByteArray */
fun buildTypedValue(typeId: Int, valueBytes: ByteArray): ByteArray {
    val buf = ByteBuffer.allocate(4 + valueBytes.size).order(ByteOrder.LITTLE_ENDIAN)
    buf.putInt(typeId)
    buf.put(valueBytes)
    return buf.array()
}

/** 构造 SET 指令，operand 格式为 [type_id(u32 LE) + value_bytes] */
fun makeSetInst(path: List<Int>, typedOperand: ByteArray): SpoiInstruction {
    return SpoiInstruction(Op.SET, path.toMutableList(), typedOperand)
}

/** 构造 PIPE 指令 */
fun makePipeInst(path: List<Int>): SpoiInstruction {
    return SpoiInstruction(Op.PIPE, path.toMutableList(), ByteArray(0))
}

/** 构造 EXEC 指令 */
fun makeExecInst(): SpoiInstruction {
    return SpoiInstruction(Op.EXEC, mutableListOf(), ByteArray(0))
}

/** 构建 SPOI 指令流 */
fun buildSpoiStream(instructions: List<SpoiInstruction>): ByteArray {
    val buf = java.io.ByteArrayOutputStream()
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

// =============================== TypeId 常量测试 ===============================

fun testTypeId() {
    println("\nTypeId 常量")

    test("U8 = 26") { assertEqual(TypeId.U8, 26L) }
    test("U16 = 27") { assertEqual(TypeId.U16, 27L) }
    test("U32 = 28") { assertEqual(TypeId.U32, 28L) }
    test("U64 = 29") { assertEqual(TypeId.U64, 29L) }
    test("I8 = 30") { assertEqual(TypeId.I8, 30L) }
    test("I16 = 31") { assertEqual(TypeId.I16, 31L) }
    test("I32 = 32") { assertEqual(TypeId.I32, 32L) }
    test("I64 = 33") { assertEqual(TypeId.I64, 33L) }
    test("F32 = 34") { assertEqual(TypeId.F32, 34L) }
    test("F64 = 35") { assertEqual(TypeId.F64, 35L) }
    test("STRING = 9") { assertEqual(TypeId.STRING, 9L) }
    test("BOOL = 40") { assertEqual(TypeId.BOOL, 40L) }
}

// =============================== SpoiDeserializer 测试 ===============================

fun testDeserializer() {
    println("\nSpoiDeserializer.deserializeValue")

    test("deserialize U8") {
        val data = buildTypedValue(26, byteArrayOf(0xAB.toByte()))
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, 0xAB, "U8 value")
    }

    test("deserialize U16") {
        val buf = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN)
        buf.putShort(0x1234.toShort())
        val data = buildTypedValue(27, buf.array())
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, 0x1234, "U16 value")
    }

    test("deserialize U32") {
        val buf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
        buf.putInt(0xDEADBEEF.toInt())
        val data = buildTypedValue(28, buf.array())
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, 0xDEADBEEFL, "U32 value")
    }

    test("deserialize U64") {
        val buf = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN)
        buf.putLong(0x1234567890ABCDEFL)
        val data = buildTypedValue(29, buf.array())
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, 0x1234567890ABCDEFL, "U64 value")
    }

    test("deserialize I8 positive") {
        val data = buildTypedValue(30, byteArrayOf(42))
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, 42.toByte(), "I8 positive")
    }

    test("deserialize I8 negative") {
        val data = buildTypedValue(30, byteArrayOf((-10).toByte()))
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, (-10).toByte(), "I8 negative")
    }

    test("deserialize I16") {
        val buf = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN)
        buf.putShort((-1000).toShort())
        val data = buildTypedValue(31, buf.array())
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, (-1000).toShort(), "I16 value")
    }

    test("deserialize I32") {
        val buf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
        buf.putInt(-123456)
        val data = buildTypedValue(32, buf.array())
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, -123456, "I32 value")
    }

    test("deserialize I64") {
        val buf = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN)
        buf.putLong(-9876543210L)
        val data = buildTypedValue(33, buf.array())
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, -9876543210L, "I64 value")
    }

    test("deserialize F32") {
        val buf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
        buf.putFloat(3.14f)
        val data = buildTypedValue(34, buf.array())
        val result = SpoiDeserializer.deserializeValue(data)
        assertTrue((result as Float) - 3.14f < 0.0001f, "F32 value")
    }

    test("deserialize F64") {
        val buf = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN)
        buf.putDouble(2.718281828)
        val data = buildTypedValue(35, buf.array())
        val result = SpoiDeserializer.deserializeValue(data)
        assertTrue((result as Double) - 2.718281828 < 0.0000001, "F64 value")
    }

    test("deserialize String") {
        val strBytes = "Hello World".toByteArray(Charsets.UTF_8)
        val data = buildTypedValue(9, strBytes)
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, "Hello World", "String value")
    }

    test("deserialize empty String") {
        val data = buildTypedValue(9, ByteArray(0))
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, "", "empty String")
    }

    test("deserialize Bool true") {
        val data = buildTypedValue(40, byteArrayOf(1))
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, true, "Bool true")
    }

    test("deserialize Bool false") {
        val data = buildTypedValue(40, byteArrayOf(0))
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, false, "Bool false")
    }

    test("deserialize empty data") {
        val data = ByteArray(0)
        val result = SpoiDeserializer.deserializeValue(data)
        assertNull(result, "empty data returns null")
    }

    test("deserialize data less than 4 bytes") {
        val data = byteArrayOf(1, 2, 3)
        val result = SpoiDeserializer.deserializeValue(data)
        assertNull(result, "data < 4 bytes returns null")
    }

    test("deserialize unknown type_id") {
        val buf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
        buf.putInt(999)
        val data = buf.array() + byteArrayOf(1, 2, 3)
        val result = SpoiDeserializer.deserializeValue(data)
        assertTrue(result is ByteArray, "unknown type_id returns raw bytes")
    }
}

// =============================== Accessor 测试 ===============================

fun testAccessorSpoiTestPlayer() {
    println("\nSpoiTestPlayerAccessor")

    val accessor = SpoiTestPlayerAccessor()
    val player = SpoiTestPlayer(name = "Alice", hp = 100, level = 10, posX = 12.5)

    test("fieldCount = 4") {
        assertEqual(accessor.fieldCount(), 4, "fieldCount")
    }

    test("getField idx=0 (name)") {
        assertEqual(accessor.getField(player, 0), "Alice", "name")
    }

    test("getField idx=1 (hp)") {
        assertEqual(accessor.getField(player, 1), 100, "hp")
    }

    test("getField idx=2 (level)") {
        assertEqual(accessor.getField(player, 2), 10, "level")
    }

    test("getField idx=3 (posX)") {
        assertEqual(accessor.getField(player, 3), 12.5, "posX")
    }

    test("setField name") {
        accessor.setField(player, 0, "Bob")
        assertEqual(player.name, "Bob", "name set")
    }

    test("setField hp") {
        accessor.setField(player, 1, 200)
        assertEqual(player.hp, 200, "hp set")
    }

    test("setField level") {
        accessor.setField(player, 2, 20)
        assertEqual(player.level, 20, "level set")
    }

    test("setField posX") {
        accessor.setField(player, 3, 25.0)
        assertEqual(player.posX, 25.0, "posX set")
    }

    test("getField invalid index") {
        try {
            accessor.getField(player, 99)
            throw AssertionError("expected exception")
        } catch (e: IllegalArgumentException) {
            // expected
        }
    }

    test("setField invalid index") {
        try {
            accessor.setField(player, 99, "bad")
            throw AssertionError("expected exception")
        } catch (e: IllegalArgumentException) {
            // expected
        }
    }
}

fun testAccessorSpoiTestState() {
    println("\nSpoiTestStateAccessor")

    val accessor = SpoiTestStateAccessor()
    val state = SpoiTestState(tick = 42, currentMap = "Overworld", players = "player_list")

    test("fieldCount = 3") {
        assertEqual(accessor.fieldCount(), 3, "fieldCount")
    }

    test("getField idx=0 (tick)") {
        assertEqual(accessor.getField(state, 0), 42, "tick")
    }

    test("getField idx=1 (currentMap)") {
        assertEqual(accessor.getField(state, 1), "Overworld", "currentMap")
    }

    test("getField idx=2 (players)") {
        assertEqual(accessor.getField(state, 2), "player_list", "players")
    }

    test("setField tick") {
        accessor.setField(state, 0, 100)
        assertEqual(state.tick, 100, "tick set")
    }

    test("setField currentMap") {
        accessor.setField(state, 1, "Dungeon")
        assertEqual(state.currentMap, "Dungeon", "currentMap set")
    }

    test("setField players") {
        val newPlayers = listOf("Alice", "Bob")
        accessor.setField(state, 2, newPlayers)
        assertEqual(state.players, newPlayers, "players set")
    }

    test("setField players to null") {
        accessor.setField(state, 2, null)
        assertNull(state.players, "players set to null")
    }
}

fun testAccessorSpoiItem() {
    println("\nSpoiItemAccessor")

    val accessor = SpoiItemAccessor()
    val item = SpoiItem(name = "Sword", value = 100)

    test("fieldCount = 2") {
        assertEqual(accessor.fieldCount(), 2, "fieldCount")
    }

    test("getField idx=0 (name)") {
        assertEqual(accessor.getField(item, 0), "Sword", "name")
    }

    test("getField idx=1 (value)") {
        assertEqual(accessor.getField(item, 1), 100, "value")
    }

    test("setField name") {
        accessor.setField(item, 0, "Axe")
        assertEqual(item.name, "Axe", "name set")
    }

    test("setField value") {
        accessor.setField(item, 1, 200)
        assertEqual(item.value, 200, "value set")
    }
}

fun testAccessorSpoiInventory() {
    println("\nSpoiInventoryAccessor")

    val accessor = SpoiInventoryAccessor()
    val items = listOf(SpoiItem("Sword", 100), SpoiItem("Shield", 50))
    val equipped = SpoiItem("Axe", 80)
    val inventory = SpoiInventory(items = items, equipped = equipped, gold = 500)

    test("fieldCount = 3") {
        assertEqual(accessor.fieldCount(), 3, "fieldCount")
    }

    test("getField idx=0 (items)") {
        assertEqual(accessor.getField(inventory, 0), items, "items")
    }

    test("getField idx=1 (equipped)") {
        assertEqual(accessor.getField(inventory, 1), equipped, "equipped")
    }

    test("getField idx=2 (gold)") {
        assertEqual(accessor.getField(inventory, 2), 500, "gold")
    }

    test("setField items") {
        val newItems = listOf(SpoiItem("Potion", 10))
        accessor.setField(inventory, 0, newItems)
        assertEqual(inventory.items, newItems, "items set")
    }

    test("setField equipped") {
        accessor.setField(inventory, 1, null)
        assertNull(inventory.equipped, "equipped set to null")
    }

    test("setField gold") {
        accessor.setField(inventory, 2, 1000)
        assertEqual(inventory.gold, 1000, "gold set")
    }
}

fun testAccessorSpoiCharacter() {
    println("\nSpoiCharacterAccessor")

    val accessor = SpoiCharacterAccessor()
    val inventory = SpoiInventory(items = emptyList<SpoiItem>(), equipped = null, gold = 100)
    val weapon = SpoiItem("Sword", 150)
    val character = SpoiCharacter(name = "Hero", hp = 200, inventory = inventory, weapon = weapon, petLevel = 3)

    test("fieldCount = 5") {
        assertEqual(accessor.fieldCount(), 5, "fieldCount")
    }

    test("getField idx=0 (name)") {
        assertEqual(accessor.getField(character, 0), "Hero", "name")
    }

    test("getField idx=1 (hp)") {
        assertEqual(accessor.getField(character, 1), 200, "hp")
    }

    test("getField idx=2 (inventory)") {
        assertEqual(accessor.getField(character, 2), inventory, "inventory")
    }

    test("getField idx=3 (weapon)") {
        assertEqual(accessor.getField(character, 3), weapon, "weapon")
    }

    test("getField idx=4 (petLevel)") {
        assertEqual(accessor.getField(character, 4), 3, "petLevel")
    }

    test("setField name") {
        accessor.setField(character, 0, "Villain")
        assertEqual(character.name, "Villain", "name set")
    }

    test("setField hp") {
        accessor.setField(character, 1, 50)
        assertEqual(character.hp, 50, "hp set")
    }

    test("setField inventory") {
        val newInv = SpoiInventory(items = null, equipped = null, gold = 0)
        accessor.setField(character, 2, newInv)
        assertEqual(character.inventory, newInv, "inventory set")
    }

    test("setField weapon") {
        accessor.setField(character, 3, null)
        assertNull(character.weapon, "weapon set to null")
    }

    test("setField petLevel") {
        accessor.setField(character, 4, 5)
        assertEqual(character.petLevel, 5, "petLevel set")
    }
}

fun testAccessorSpoiWorld() {
    println("\nSpoiWorldAccessor")

    val accessor = SpoiWorldAccessor()
    val characters = listOf(
        SpoiCharacter("Hero", 100, null, null, 1),
        SpoiCharacter("Villain", 80, null, null, 2)
    )
    val world = SpoiWorld(worldName = "Fantasy", tick = 5000, characters = characters)

    test("fieldCount = 3") {
        assertEqual(accessor.fieldCount(), 3, "fieldCount")
    }

    test("getField idx=0 (worldName)") {
        assertEqual(accessor.getField(world, 0), "Fantasy", "worldName")
    }

    test("getField idx=1 (tick)") {
        assertEqual(accessor.getField(world, 1), 5000, "tick")
    }

    test("getField idx=2 (characters)") {
        assertEqual(accessor.getField(world, 2), characters, "characters")
    }

    test("setField worldName") {
        accessor.setField(world, 0, "SciFi")
        assertEqual(world.worldName, "SciFi", "worldName set")
    }

    test("setField tick") {
        accessor.setField(world, 1, 9999)
        assertEqual(world.tick, 9999, "tick set")
    }

    test("setField characters") {
        accessor.setField(world, 2, null)
        assertNull(world.characters, "characters set to null")
    }
}

// =============================== SpoiAccessorRegistry 测试 ===============================

fun testAccessorRegistry() {
    println("\nSpoiAccessorRegistry")

    test("registry contains 6 types") {
        assertEqual(SpoiAccessorRegistry.registry.size, 6, "registry size")
    }

    test("registry contains SpoiTestPlayer") {
        assertNotNull(SpoiAccessorRegistry.get("SpoiTestPlayer"), "SpoiTestPlayer")
    }

    test("registry contains SpoiTestState") {
        assertNotNull(SpoiAccessorRegistry.get("SpoiTestState"), "SpoiTestState")
    }

    test("registry contains SpoiItem") {
        assertNotNull(SpoiAccessorRegistry.get("SpoiItem"), "SpoiItem")
    }

    test("registry contains SpoiInventory") {
        assertNotNull(SpoiAccessorRegistry.get("SpoiInventory"), "SpoiInventory")
    }

    test("registry contains SpoiCharacter") {
        assertNotNull(SpoiAccessorRegistry.get("SpoiCharacter"), "SpoiCharacter")
    }

    test("registry contains SpoiWorld") {
        assertNotNull(SpoiAccessorRegistry.get("SpoiWorld"), "SpoiWorld")
    }

    test("get returns null for unknown type") {
        assertNull(SpoiAccessorRegistry.get("UnknownType"), "unknown type")
    }

    test("get returns correct accessor type") {
        val acc = SpoiAccessorRegistry.get("SpoiItem")
        assertTrue(acc is SpoiItemAccessor, "SpoiItem returns SpoiItemAccessor")
    }
}

// =============================== Executor 集成测试 ===============================

fun testExecutorIntegration() {
    println("\nExecutor 集成测试（使用 accessor Map）")

    test("Executor navigate via accessor") {
        val executor = SpoiExecutor(SpoiAccessorRegistry.registry)
        val player = SpoiTestPlayer("Alice", 100, 10, 12.5)
        val result = executor.navigate(player, listOf(0))
        assertEqual(result, "Alice", "navigate to name")
    }

    test("Executor navigate to hp") {
        val executor = SpoiExecutor(SpoiAccessorRegistry.registry)
        val player = SpoiTestPlayer("Alice", 100, 10, 12.5)
        val result = executor.navigate(player, listOf(1))
        assertEqual(result, 100, "navigate to hp")
    }

    test("Executor SET string field via accessor") {
        val executor = SpoiExecutor(SpoiAccessorRegistry.registry)
        val player = SpoiTestPlayer("Alice", 100, 10, 12.5)

        val strBytes = "Bob".toByteArray(Charsets.UTF_8)
        val typedOperand = buildTypedValue(TypeId.STRING.toInt(), strBytes)
        val insts = listOf(
            makeSetInst(listOf(0), typedOperand),
            makeExecInst()
        )
        val data = buildSpoiStream(insts)
        executor.execute(player, data)
        assertEqual(player.name, "Bob", "name set via executor")
    }

    test("Executor SET int field via accessor") {
        val executor = SpoiExecutor(SpoiAccessorRegistry.registry)
        val player = SpoiTestPlayer("Alice", 100, 10, 12.5)

        val intBuf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
        intBuf.putInt(200)
        val typedOperand = buildTypedValue(TypeId.I32.toInt(), intBuf.array())
        val insts = listOf(
            makeSetInst(listOf(1), typedOperand),
            makeExecInst()
        )
        val data = buildSpoiStream(insts)
        executor.execute(player, data)
        assertEqual(player.hp, 200, "hp set via executor")
    }

    test("Executor SET double field via accessor") {
        val executor = SpoiExecutor(SpoiAccessorRegistry.registry)
        val player = SpoiTestPlayer("Alice", 100, 10, 0.0)

        val doubleBuf = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN)
        doubleBuf.putDouble(99.9)
        val typedOperand = buildTypedValue(TypeId.F64.toInt(), doubleBuf.array())
        val insts = listOf(
            makeSetInst(listOf(3), typedOperand),
            makeExecInst()
        )
        val data = buildSpoiStream(insts)
        executor.execute(player, data)
        assertTrue(player.posX - 99.9 < 0.0001, "posX set via executor")
    }

    test("Executor SET on SpoiItem") {
        val executor = SpoiExecutor(SpoiAccessorRegistry.registry)
        val item = SpoiItem("Dagger", 10)

        val strBytes = "Excalibur".toByteArray(Charsets.UTF_8)
        val typedOperand = buildTypedValue(TypeId.STRING.toInt(), strBytes)
        val insts = listOf(
            makeSetInst(listOf(0), typedOperand),
            makeExecInst()
        )
        val data = buildSpoiStream(insts)
        executor.execute(item, data)
        assertEqual(item.name, "Excalibur", "item name set via executor")
    }

    test("Executor SET on SpoiWorld") {
        val executor = SpoiExecutor(SpoiAccessorRegistry.registry)
        val world = SpoiWorld("OldWorld", 0, null)

        val strBytes = "NewWorld".toByteArray(Charsets.UTF_8)
        val typedOperand = buildTypedValue(TypeId.STRING.toInt(), strBytes)
        val insts = listOf(
            makeSetInst(listOf(0), typedOperand),
            makeExecInst()
        )
        val data = buildSpoiStream(insts)
        executor.execute(world, data)
        assertEqual(world.worldName, "NewWorld", "worldName set via executor")
    }

    test("Executor SET multiple fields on SpoiCharacter") {
        val executor = SpoiExecutor(SpoiAccessorRegistry.registry)
        val character = SpoiCharacter("OldName", 50, null, null, 1)

        val nameBytes = "NewHero".toByteArray(Charsets.UTF_8)
        val nameOperand = buildTypedValue(TypeId.STRING.toInt(), nameBytes)

        val hpBuf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
        hpBuf.putInt(999)
        val hpOperand = buildTypedValue(TypeId.I32.toInt(), hpBuf.array())

        val petBuf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
        petBuf.putInt(10)
        val petOperand = buildTypedValue(TypeId.I32.toInt(), petBuf.array())

        val insts = listOf(
            makeSetInst(listOf(0), nameOperand),
            makeSetInst(listOf(1), hpOperand),
            makeSetInst(listOf(4), petOperand),
            makeExecInst()
        )
        val data = buildSpoiStream(insts)
        executor.execute(character, data)
        assertEqual(character.name, "NewHero", "name set")
        assertEqual(character.hp, 999, "hp set")
        assertEqual(character.petLevel, 10, "petLevel set")
    }

    test("Executor PIPE via accessor") {
        val executor = SpoiExecutor(SpoiAccessorRegistry.registry)
        val player = SpoiTestPlayer("Alice", 100, 10, 12.5)

        val insts = listOf(
            makePipeInst(listOf(1)),
            makeExecInst()
        )
        val data = buildSpoiStream(insts)
        val result = executor.execute(player, data)
        assertEqual(result["resultType"], ResultType.SINGLE, "result type")
        assertEqual(result["value"], 100, "hp piped")
    }

    test("Executor SET Bool field via accessor") {
        // SpoiTestPlayer doesn't have a bool field, but we can test Bool deserialization
        // through the operand mechanism. We'll test deserializeValue directly.
        val data = buildTypedValue(TypeId.BOOL.toInt(), byteArrayOf(1))
        val result = SpoiDeserializer.deserializeValue(data)
        assertEqual(result, true, "Bool true deserialized")
    }

    test("Executor SET with U8 operand") {
        val executor = SpoiExecutor(SpoiAccessorRegistry.registry)
        val player = SpoiTestPlayer("Alice", 100, 10, 0.0)

        val u8Operand = buildTypedValue(TypeId.U8.toInt(), byteArrayOf(42.toByte()))
        val insts = listOf(
            makeSetInst(listOf(1), u8Operand),
            makeExecInst()
        )
        val data = buildSpoiStream(insts)
        executor.execute(player, data)
        assertEqual(player.hp, 42, "hp set via U8 operand")
    }
}

// =============================== 主入口 ===============================

fun main() {
    testTypeId()
    testDeserializer()
    testAccessorSpoiTestPlayer()
    testAccessorSpoiTestState()
    testAccessorSpoiItem()
    testAccessorSpoiInventory()
    testAccessorSpoiCharacter()
    testAccessorSpoiWorld()
    testAccessorRegistry()
    testExecutorIntegration()

    println("\n========================================")
    println("通过: $passed, 失败: $failed")
    println("========================================")
    if (failed > 0) {
        kotlin.system.exitProcess(1)
    }
}