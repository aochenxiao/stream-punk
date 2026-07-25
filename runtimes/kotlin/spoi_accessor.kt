// ============================================================
// SPOI Accessor — Kotlin 类型特化访问器（自动生成）
// 由 sp-gen spoi-kotlin-accessor 从 C++ 元数据生成
// 替代反射机制，直接通过字段索引访问/设置值
// ============================================================

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.StandardCharsets

// ============================================================
// 基本类型 ID（与 C++ E_type 枚举值一致）
// ============================================================

object TypeId {
    const val U8: Long     = 26L
    const val U16: Long    = 27L
    const val U32: Long    = 28L
    const val U64: Long    = 29L
    const val I8: Long     = 30L
    const val I16: Long    = 31L
    const val I32: Long    = 32L
    const val I64: Long    = 33L
    const val F32: Long    = 34L
    const val F64: Long    = 35L
    const val STRING: Long = 9L
    const val BOOL: Long   = 40L
}

// ============================================================
// SpoiAccessor — 类型特化访问器接口
// ============================================================

interface SpoiAccessor {
    fun fieldCount(): Int
    fun getField(obj: Any, idx: Int): Any?
    fun setField(obj: Any, idx: Int, value: Any?)
}

// ============================================================
// DeserializeValue — 通用值反序列化（基于 type_id 前缀）
// 格式: [type_id(u32 LE) + value_bytes]
// ============================================================

object SpoiDeserializer {
    fun deserializeValue(data: ByteArray): Any? {
        if (data.size < 4) return null
        val typeId = ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).int.toLong() and 0xFFFFFFFFL
        val valueBytes = data.copyOfRange(4, data.size)
        return when (typeId) {
            TypeId.U8     -> valueBytes[0].toInt() and 0xFF
            TypeId.U16    -> (ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).short.toInt() and 0xFFFF)
            TypeId.U32    -> (ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).int.toLong() and 0xFFFFFFFFL)
            TypeId.U64    -> ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).long
            TypeId.I8     -> valueBytes[0]
            TypeId.I16    -> ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).short
            TypeId.I32    -> ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).int
            TypeId.I64    -> ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).long
            TypeId.F32    -> ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).float
            TypeId.F64    -> ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).double
            TypeId.STRING -> String(valueBytes, StandardCharsets.UTF_8)
            TypeId.BOOL   -> valueBytes[0].toInt() != 0
            else          -> valueBytes
        }
    }
}

// ============================================================
// SpoiTestPlayerAccessor
// ============================================================

class SpoiTestPlayerAccessor : SpoiAccessor {

    override fun fieldCount(): Int = 4

    override fun getField(obj: Any, idx: Int): Any? {
        val o = obj as SpoiTestPlayer
        return when (idx) {
            0 -> o.name
            1 -> o.hp
            2 -> o.level
            3 -> o.posX
            else -> throw IllegalArgumentException("invalid field index for SpoiTestPlayer: $idx")
        }
    }

    override fun setField(obj: Any, idx: Int, value: Any?) {
        val o = obj as SpoiTestPlayer
        when (idx) {
            0 -> o.name = value?.toString() ?: ""
            1 -> o.hp = (value as? Number)?.toInt() ?: 0
            2 -> o.level = (value as? Number)?.toInt() ?: 0
            3 -> o.posX = (value as? Number)?.toDouble() ?: 0.0
            else -> throw IllegalArgumentException("invalid field index for SpoiTestPlayer: $idx")
        }
    }
}

// ============================================================
// SpoiTestStateAccessor
// ============================================================

class SpoiTestStateAccessor : SpoiAccessor {

    override fun fieldCount(): Int = 3

    override fun getField(obj: Any, idx: Int): Any? {
        val o = obj as SpoiTestState
        return when (idx) {
            0 -> o.tick
            1 -> o.currentMap
            2 -> o.players
            else -> throw IllegalArgumentException("invalid field index for SpoiTestState: $idx")
        }
    }

    override fun setField(obj: Any, idx: Int, value: Any?) {
        val o = obj as SpoiTestState
        when (idx) {
            0 -> o.tick = (value as? Number)?.toInt() ?: 0
            1 -> o.currentMap = value?.toString() ?: ""
            2 -> o.players = value
            else -> throw IllegalArgumentException("invalid field index for SpoiTestState: $idx")
        }
    }
}

// ============================================================
// SpoiItemAccessor
// ============================================================

class SpoiItemAccessor : SpoiAccessor {

    override fun fieldCount(): Int = 2

    override fun getField(obj: Any, idx: Int): Any? {
        val o = obj as SpoiItem
        return when (idx) {
            0 -> o.name
            1 -> o.value
            else -> throw IllegalArgumentException("invalid field index for SpoiItem: $idx")
        }
    }

    override fun setField(obj: Any, idx: Int, value: Any?) {
        val o = obj as SpoiItem
        when (idx) {
            0 -> o.name = value?.toString() ?: ""
            1 -> o.value = (value as? Number)?.toInt() ?: 0
            else -> throw IllegalArgumentException("invalid field index for SpoiItem: $idx")
        }
    }
}

// ============================================================
// SpoiInventoryAccessor
// ============================================================

class SpoiInventoryAccessor : SpoiAccessor {

    override fun fieldCount(): Int = 3

    override fun getField(obj: Any, idx: Int): Any? {
        val o = obj as SpoiInventory
        return when (idx) {
            0 -> o.items
            1 -> o.equipped
            2 -> o.gold
            else -> throw IllegalArgumentException("invalid field index for SpoiInventory: $idx")
        }
    }

    override fun setField(obj: Any, idx: Int, value: Any?) {
        val o = obj as SpoiInventory
        when (idx) {
            0 -> o.items = value
            1 -> o.equipped = value
            2 -> o.gold = (value as? Number)?.toInt() ?: 0
            else -> throw IllegalArgumentException("invalid field index for SpoiInventory: $idx")
        }
    }
}

// ============================================================
// SpoiCharacterAccessor
// ============================================================

class SpoiCharacterAccessor : SpoiAccessor {

    override fun fieldCount(): Int = 5

    override fun getField(obj: Any, idx: Int): Any? {
        val o = obj as SpoiCharacter
        return when (idx) {
            0 -> o.name
            1 -> o.hp
            2 -> o.inventory
            3 -> o.weapon
            4 -> o.petLevel
            else -> throw IllegalArgumentException("invalid field index for SpoiCharacter: $idx")
        }
    }

    override fun setField(obj: Any, idx: Int, value: Any?) {
        val o = obj as SpoiCharacter
        when (idx) {
            0 -> o.name = value?.toString() ?: ""
            1 -> o.hp = (value as? Number)?.toInt() ?: 0
            2 -> o.inventory = value
            3 -> o.weapon = value
            4 -> o.petLevel = (value as? Number)?.toInt() ?: 0
            else -> throw IllegalArgumentException("invalid field index for SpoiCharacter: $idx")
        }
    }
}

// ============================================================
// SpoiWorldAccessor
// ============================================================

class SpoiWorldAccessor : SpoiAccessor {

    override fun fieldCount(): Int = 3

    override fun getField(obj: Any, idx: Int): Any? {
        val o = obj as SpoiWorld
        return when (idx) {
            0 -> o.worldName
            1 -> o.tick
            2 -> o.characters
            else -> throw IllegalArgumentException("invalid field index for SpoiWorld: $idx")
        }
    }

    override fun setField(obj: Any, idx: Int, value: Any?) {
        val o = obj as SpoiWorld
        when (idx) {
            0 -> o.worldName = value?.toString() ?: ""
            1 -> o.tick = (value as? Number)?.toInt() ?: 0
            2 -> o.characters = value
            else -> throw IllegalArgumentException("invalid field index for SpoiWorld: $idx")
        }
    }
}

// ============================================================
// SpoiAccessorRegistry — 静态类型注册表
// 替代运行时 Map<String, List<String>>（反射版本）
// ============================================================

object SpoiAccessorRegistry {
    val registry: Map<String, SpoiAccessor> = mapOf(
        "SpoiTestPlayer" to SpoiTestPlayerAccessor(),
        "SpoiTestState" to SpoiTestStateAccessor(),
        "SpoiItem" to SpoiItemAccessor(),
        "SpoiInventory" to SpoiInventoryAccessor(),
        "SpoiCharacter" to SpoiCharacterAccessor(),
        "SpoiWorld" to SpoiWorldAccessor()
    )
    fun get(typeName: String): SpoiAccessor? = registry[typeName]
}
