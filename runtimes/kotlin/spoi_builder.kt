// ============================================================
// SPOI — StreamPunk Operation Instruction
// Kotlin 查询/更新 Builder（自动生成）
// ============================================================

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.StandardCharsets

// 操作码
object Op {
    const val SET       = 0x04
    const val ADD       = 0x05
    const val APPEND    = 0x06
    const val REMOVE    = 0x07
    const val INSERT    = 0x08
    const val REPLACE   = 0x09
    const val RESET     = 0x0A
    const val SETNULL   = 0x0B
    const val FILTER    = 0x0C
    const val SELECT    = 0x0D
    const val SORT      = 0x0E
    const val REVERSE   = 0x0F
    const val TAKE      = 0x10
    const val DROP      = 0x11
    const val TAKEWHILE = 0x12
    const val DROPWHILE = 0x13
    const val DISTINCT  = 0x14
    const val COUNT     = 0x15
    const val ANY       = 0x16
    const val ALL       = 0x17
    const val FIND      = 0x18
    const val KEYS      = 0x19
    const val VALUES    = 0x1A
    const val JOIN      = 0x1B
    const val ENUMERATE = 0x1C
    const val CHUNK     = 0x1D
    const val SLIDE     = 0x1E
    const val STRIDE    = 0x1F
    const val ADJACENT  = 0x20
    const val EXEC      = 0x21
}

// 比较运算符
object Cmp {
    const val EQ = 0
    const val NE = 1
    const val LT = 2
    const val GT = 3
    const val LE = 4
    const val GE = 5
}

const val PATH_DEREF = 0xFFFF

// 类型成员索引常量
// SpoiTestPlayer
const val SpoiTestPlayer_name = 0
const val SpoiTestPlayer_hp = 1
const val SpoiTestPlayer_level = 2
const val SpoiTestPlayer_posX = 3

// SpoiTestState
const val SpoiTestState_tick = 0
const val SpoiTestState_currentMap = 1
const val SpoiTestState_players = 2

// SpoiItem
const val SpoiItem_name = 0
const val SpoiItem_value = 1

// SpoiInventory
const val SpoiInventory_items = 0
const val SpoiInventory_equipped = 1
const val SpoiInventory_gold = 2

// SpoiCharacter
const val SpoiCharacter_name = 0
const val SpoiCharacter_hp = 1
const val SpoiCharacter_inventory = 2
const val SpoiCharacter_weapon = 3
const val SpoiCharacter_petLevel = 4

// SpoiWorld
const val SpoiWorld_worldName = 0
const val SpoiWorld_tick = 1
const val SpoiWorld_characters = 2

// Varint 编码
fun writeVarint(buf: ArrayList<Byte>, v: Int) {
    var value = v
    while (value >= 0x80) {
        buf.add(((value and 0x7F) or 0x80).toByte())
        value = value ushr 7
    }
    buf.add((value and 0x7F).toByte())
}

fun writeU32(buf: ArrayList<Byte>, v: Int) {
    buf.add((v and 0xFF).toByte())
    buf.add(((v ushr 8) and 0xFF).toByte())
    buf.add(((v ushr 16) and 0xFF).toByte())
    buf.add(((v ushr 24) and 0xFF).toByte())
}

// SpoiInstruction
class SpoiInstruction(
    val op: Int,
    val path: IntArray,
    val operand: ByteArray = ByteArray(0)
) {
    fun serialize(): ByteArray {
        val buf = ArrayList<Byte>()
        buf.add(op.toByte())
        writeVarint(buf, path.size)
        for (seg in path) writeU32(buf, seg)
        writeVarint(buf, operand.size)
        for (b in operand) buf.add(b)
        return ByteArray(buf.size) { buf[it] }
    }
}

// SpoiStream
class SpoiStream {
    val instructions = ArrayList<SpoiInstruction>()

    fun build(): ByteArray {
        val buf = ArrayList<Byte>()
        writeVarint(buf, instructions.size)
        for (inst in instructions) {
            for (b in inst.serialize()) buf.add(b)
        }
        return ByteArray(buf.size) { buf[it] }
    }

    fun buildHex(): String {
        return build().joinToString("") { "%02x".format(it) }
    }
}

// SpoiUpdate — 写操作 Builder
class SpoiUpdate {
    private val stream = SpoiStream()

    fun set(path: IntArray, value: ByteArray): SpoiUpdate {
        stream.instructions.add(SpoiInstruction(Op.SET, path, value))
        return this
    }

    fun setI32(path: IntArray, value: Int): SpoiUpdate {
        return set(path, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(value).array())
    }

    fun setU32(path: IntArray, value: Int): SpoiUpdate {
        return set(path, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(value).array())
    }

    fun setF64(path: IntArray, value: Double): SpoiUpdate {
        return set(path, ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putDouble(value).array())
    }

    fun setStr(path: IntArray, value: String): SpoiUpdate {
        val data = value.toByteArray(StandardCharsets.UTF_8)
        val buf = ArrayList<Byte>()
        writeVarint(buf, data.size)
        for (b in data) buf.add(b)
        return set(path, ByteArray(buf.size) { buf[it] })
    }

    fun setBool(path: IntArray, value: Boolean): SpoiUpdate {
        return set(path, byteArrayOf(if (value) 1 else 0))
    }

    fun addI32(path: IntArray, delta: Int): SpoiUpdate {
        stream.instructions.add(SpoiInstruction(Op.ADD, path,
            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(delta).array()))
        return this
    }

    fun add(path: IntArray, value: ByteArray): SpoiUpdate {
        stream.instructions.add(SpoiInstruction(Op.ADD, path, value))
        return this
    }

    fun append(path: IntArray, value: ByteArray): SpoiUpdate {
        stream.instructions.add(SpoiInstruction(Op.APPEND, path, value))
        return this
    }

    fun remove(path: IntArray, value: ByteArray): SpoiUpdate {
        stream.instructions.add(SpoiInstruction(Op.REMOVE, path, value))
        return this
    }

    fun insert(path: IntArray, value: ByteArray): SpoiUpdate {
        stream.instructions.add(SpoiInstruction(Op.INSERT, path, value))
        return this
    }

    fun replace(path: IntArray, value: ByteArray): SpoiUpdate {
        stream.instructions.add(SpoiInstruction(Op.REPLACE, path, value))
        return this
    }

    fun reset(path: IntArray): SpoiUpdate {
        stream.instructions.add(SpoiInstruction(Op.RESET, path, ByteArray(0)))
        return this
    }

    fun setnull(path: IntArray): SpoiUpdate {
        stream.instructions.add(SpoiInstruction(Op.SETNULL, path, ByteArray(0)))
        return this
    }

    fun build(): ByteArray = stream.build()
    fun buildHex(): String = stream.buildHex()
}

// SpoiQuery — 查询 Builder
class SpoiQuery {
    private val stream = SpoiStream()

    fun nav(field: Int): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.FILTER, intArrayOf(field), ByteArray(0)))
        return this
    }

    fun filter(field: Int, cmpOp: Int, value: ByteArray): SpoiQuery {
        val buf = ArrayList<Byte>()
        writeU32(buf, field)
        buf.add(cmpOp.toByte())
        writeVarint(buf, value.size)
        for (b in value) buf.add(b)
        stream.instructions.add(SpoiInstruction(Op.FILTER, IntArray(0),
            ByteArray(buf.size) { buf[it] }))
        return this
    }

    fun filterI32(field: Int, cmpOp: Int, value: Int): SpoiQuery {
        return filter(field, cmpOp,
            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(value).array())
    }

    fun filterStr(field: Int, cmpOp: Int, value: String): SpoiQuery {
        val data = value.toByteArray(StandardCharsets.UTF_8)
        val buf = ArrayList<Byte>()
        writeVarint(buf, data.size)
        for (b in data) buf.add(b)
        return filter(field, cmpOp, ByteArray(buf.size) { buf[it] })
    }

    fun select(vararg fields: Int): SpoiQuery {
        val buf = ArrayList<Byte>()
        writeU32(buf, fields.size)
        for (f in fields) writeU32(buf, f)
        stream.instructions.add(SpoiInstruction(Op.SELECT, IntArray(0),
            ByteArray(buf.size) { buf[it] }))
        return this
    }

    fun sort(field: Int, ascending: Boolean = true): SpoiQuery {
        val buf = ArrayList<Byte>()
        writeU32(buf, field)
        buf.add((if (ascending) 1 else 0).toByte())
        stream.instructions.add(SpoiInstruction(Op.SORT, IntArray(0),
            ByteArray(buf.size) { buf[it] }))
        return this
    }

    fun reverse(): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.REVERSE, IntArray(0), ByteArray(0)))
        return this
    }

    fun take(count: Int): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.TAKE, IntArray(0),
            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(count).array()))
        return this
    }

    fun drop(count: Int): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.DROP, IntArray(0),
            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(count).array()))
        return this
    }

    fun distinct(): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.DISTINCT, IntArray(0), ByteArray(0)))
        return this
    }

    fun count(): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.COUNT, IntArray(0), ByteArray(0)))
        return this
    }

    fun keys(): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.KEYS, IntArray(0), ByteArray(0)))
        return this
    }

    fun values(): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.VALUES, IntArray(0), ByteArray(0)))
        return this
    }

    fun join(field: Int): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.JOIN, IntArray(0),
            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(field).array()))
        return this
    }

    fun enumerate(start: Int = 0): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.ENUMERATE, IntArray(0),
            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(start).array()))
        return this
    }

    fun chunk(size: Int): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.CHUNK, IntArray(0),
            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(size).array()))
        return this
    }

    fun stride(step: Int): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.STRIDE, IntArray(0),
            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(step).array()))
        return this
    }

    fun takewhile(field: Int, cmpOp: Int, value: ByteArray): SpoiQuery {
        val buf = ArrayList<Byte>()
        writeU32(buf, field)
        buf.add(cmpOp.toByte())
        writeVarint(buf, value.size)
        for (b in value) buf.add(b)
        stream.instructions.add(SpoiInstruction(Op.TAKEWHILE, IntArray(0),
            ByteArray(buf.size) { buf[it] }))
        return this
    }

    fun dropwhile(field: Int, cmpOp: Int, value: ByteArray): SpoiQuery {
        val buf = ArrayList<Byte>()
        writeU32(buf, field)
        buf.add(cmpOp.toByte())
        writeVarint(buf, value.size)
        for (b in value) buf.add(b)
        stream.instructions.add(SpoiInstruction(Op.DROPWHILE, IntArray(0),
            ByteArray(buf.size) { buf[it] }))
        return this
    }

    fun any(field: Int, cmpOp: Int, value: ByteArray): SpoiQuery {
        val buf = ArrayList<Byte>()
        writeU32(buf, field)
        buf.add(cmpOp.toByte())
        writeVarint(buf, value.size)
        for (b in value) buf.add(b)
        stream.instructions.add(SpoiInstruction(Op.ANY, IntArray(0),
            ByteArray(buf.size) { buf[it] }))
        return this
    }

    fun all(field: Int, cmpOp: Int, value: ByteArray): SpoiQuery {
        val buf = ArrayList<Byte>()
        writeU32(buf, field)
        buf.add(cmpOp.toByte())
        writeVarint(buf, value.size)
        for (b in value) buf.add(b)
        stream.instructions.add(SpoiInstruction(Op.ALL, IntArray(0),
            ByteArray(buf.size) { buf[it] }))
        return this
    }

    fun find(field: Int, cmpOp: Int, value: ByteArray): SpoiQuery {
        val buf = ArrayList<Byte>()
        writeU32(buf, field)
        buf.add(cmpOp.toByte())
        writeVarint(buf, value.size)
        for (b in value) buf.add(b)
        stream.instructions.add(SpoiInstruction(Op.FIND, IntArray(0),
            ByteArray(buf.size) { buf[it] }))
        return this
    }

    fun slide(size: Int): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.SLIDE, IntArray(0),
            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(size).array()))
        return this
    }

    fun adjacent(n: Int): SpoiQuery {
        stream.instructions.add(SpoiInstruction(Op.ADJACENT, IntArray(0),
            ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(n).array()))
        return this
    }

    fun build(): ByteArray {
        stream.instructions.add(SpoiInstruction(Op.EXEC, IntArray(0), ByteArray(0)))
        return stream.build()
    }
}
