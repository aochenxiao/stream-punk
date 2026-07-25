/**
 * Stream-Punk Kotlin Runtime Library
 * Zero-dependency binary serialization/deserialization for Kotlin.
 * Compatible with the C++ StreamPunk binary format (little-endian).
 *
 * Target: Kotlin/JVM
 */

import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.StandardCharsets
import java.util.*

/* =================== 辅助类型 =================== */

class SpRef<T>(var value: T?, val address: Long)

class SpArray<T>(private val size: Int) {
    private val data = arrayOfNulls<Any?>(size)

    @Suppress("UNCHECKED_CAST")
    fun at(index: Int): T = data[index] as T

    operator fun set(index: Int, value: T) { data[index] = value }

    fun size(): Int = size
}

/* =================== 反序列化读取器 =================== */

class I(private val buf: ByteArray) {
    var off: Int = 0
        private set

    constructor(data: ByteArray, offset: Int) : this(data) {
        off = offset
    }
    private val objMap = HashMap<Long, Any?>()

    fun hasMoreData(): Boolean = off < buf.size

    fun read_u8(): Int = (buf[off++].toInt() and 0xFF)
    fun read_u16(): Int = ByteBuffer.wrap(buf, off, 2).order(ByteOrder.LITTLE_ENDIAN).short.toInt() and 0xFFFF.also { off += 2 }
    fun read_u32(): Long = ByteBuffer.wrap(buf, off, 4).order(ByteOrder.LITTLE_ENDIAN).int.toLong() and 0xFFFFFFFFL.also { off += 4 }
    fun read_u64(): Long = ByteBuffer.wrap(buf, off, 8).order(ByteOrder.LITTLE_ENDIAN).long.also { off += 8 }
    fun read_i8(): Byte = buf[off++]
    fun read_i16(): Short = ByteBuffer.wrap(buf, off, 2).order(ByteOrder.LITTLE_ENDIAN).short.also { off += 2 }
    fun read_i32(): Int = ByteBuffer.wrap(buf, off, 4).order(ByteOrder.LITTLE_ENDIAN).int.also { off += 4 }
    fun read_i64(): Long = ByteBuffer.wrap(buf, off, 8).order(ByteOrder.LITTLE_ENDIAN).long.also { off += 8 }
    fun read_f32(): Float = ByteBuffer.wrap(buf, off, 4).order(ByteOrder.LITTLE_ENDIAN).float.also { off += 4 }
    fun read_f64(): Double = ByteBuffer.wrap(buf, off, 8).order(ByteOrder.LITTLE_ENDIAN).double.also { off += 8 }
    fun read_ch(): Char = (buf[off++].toInt() and 0xFF).toChar()
    fun read_ch8(): Char = (buf[off++].toInt() and 0xFF).toChar()
    fun read_ch16(): Char = ByteBuffer.wrap(buf, off, 2).order(ByteOrder.LITTLE_ENDIAN).char.also { off += 2 }
    fun read_ch32(): Int = ByteBuffer.wrap(buf, off, 4).order(ByteOrder.LITTLE_ENDIAN).int.also { off += 4 }
    fun read_bl(): Boolean = buf[off++] != 0.toByte()

    fun read_sz(): Int = read_u32().toInt()

    fun readString(): String {
        val length = read_sz()
        if (length == 0) return ""
        return String(buf, off, length, StandardCharsets.UTF_8).also { off += length }
    }
    fun read_string(): String = readString()
    fun read_u8string(): String = readString()
    fun read_u16string(): String {
        val length = read_sz()
        val byteLen = length * 2
        return String(buf, off, byteLen, StandardCharsets.UTF_16LE).also { off += byteLen }
    }
    fun read_u32string(): ByteArray {
        val length = read_sz()
        val byteLen = length * 4
        return buf.copyOfRange(off, off + byteLen).also { off += byteLen }
    }

    fun read_ptr_with_typeID(): SpRef<Base?> {
        val addr = read_u64()
        if (addr == 0L) return SpRef(null, 0)
        if (objMap.containsKey(addr)) return objMap[addr] as SpRef<Base?>
        val ref = SpRef<Base?>(null, addr)
        objMap[addr] = ref
        ref.value = Base.read_obj(this)
        return ref
    }

    fun <T> read_ptr(reader: () -> T): SpRef<T?> {
        val addr = read_u64()
        if (addr == 0L) return SpRef(null, 0)
        if (objMap.containsKey(addr)) return objMap[addr] as SpRef<T?>
        val value = reader()
        val ref = SpRef<T?>(value, addr)
        objMap[addr] = ref
        return ref
    }

    fun <T> read_Array(reader: () -> T): ArrayList<T> {
        val size = read_sz()
        return ArrayList<T>(size).apply {
            repeat(size) { add(reader()) }
        }
    }

    fun <T> read_set(reader: () -> T): HashSet<T> {
        val arr = read_Array(reader)
        return HashSet(arr)
    }

    fun <K, V> read_map(keyReader: () -> K, valueReader: () -> V): HashMap<K, V> {
        val size = read_sz()
        return HashMap<K, V>().apply {
            repeat(size) {
                put(keyReader(), valueReader())
            }
        }
    }
}

/* =================== 序列化写入器 =================== */

class O {
    private val buf = ByteArrayOutputStream()

    fun toBytes(): ByteArray = buf.toByteArray()

    private fun writeLE(data: ByteArray) { buf.write(data) }

    fun write_u8(v: Int) { buf.write(v and 0xFF) }
    fun write_u16(v: Int) { writeLE(ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort((v and 0xFFFF).toShort()).array()) }
    fun write_u32(v: Long) { writeLE(ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt((v and 0xFFFFFFFFL).toInt()).array()) }
    fun write_u64(v: Long) { writeLE(ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(v).array()) }
    fun write_i8(v: Byte) { buf.write(v.toInt()) }
    fun write_i16(v: Short) { writeLE(ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort(v).array()) }
    fun write_i32(v: Int) { writeLE(ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array()) }
    fun write_i64(v: Long) { writeLE(ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(v).array()) }
    fun write_f32(v: Float) { writeLE(ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putFloat(v).array()) }
    fun write_f64(v: Double) { writeLE(ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putDouble(v).array()) }
    fun write_ch(v: Char) { buf.write(v.code and 0xFF) }
    fun write_ch8(v: Char) { buf.write(v.code and 0xFF) }
    fun write_ch16(v: Char) { writeLE(ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putChar(v).array()) }
    fun write_ch32(v: Int) { writeLE(ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array()) }
    fun write_bl(v: Boolean) { buf.write(if (v) 1 else 0) }
    fun write_sz(v: Int) { write_u32(v.toLong()) }

    fun writeString(s: String?) {
        if (s == null) { write_sz(0); return }
        val bytes = s.toByteArray(StandardCharsets.UTF_8)
        write_sz(bytes.size)
        buf.write(bytes)
    }
    fun write_string(s: String?) { writeString(s) }
    fun write_u8string(s: String?) { writeString(s) }
    fun write_u16string(s: String?) {
        if (s == null) { write_sz(0); return }
        val bytes = s.toByteArray(StandardCharsets.UTF_16LE)
        write_sz(s.length)
        buf.write(bytes)
    }
    fun write_u32string(b: ByteArray?) {
        val length = if (b == null) 0 else b.size / 4
        write_sz(length)
        if (b != null) buf.write(b)
    }

    fun write_ptr_with_typeID(value: Base?) {
        if (value == null) { write_u64(0); return }
        write_u64(System.identityHashCode(value).toLong())
    }

    fun <T> write_ptr(value: T?, address: Long, writer: (T) -> Unit) {
        if (value == null) { write_u64(0); return }
        write_u64(address)
        writer(value)
    }

    fun <T> write_Array(arr: ArrayList<T>, writer: (T) -> Unit) {
        write_sz(arr.size)
        for (v in arr) writer(v)
    }

    fun <T> write_set(set: HashSet<T>, writer: (T) -> Unit) {
        write_sz(set.size)
        for (v in set) writer(v)
    }

    fun <K, V> write_map(map: HashMap<K, V>, keyWriter: (K) -> Unit, valueWriter: (V) -> Unit) {
        write_sz(map.size)
        for ((k, v) in map) {
            keyWriter(k)
            valueWriter(v)
        }
    }
}

/* =================== 类型分发（由代码生成器输出具体实现） =================== */
// E_StreamPunkType / __typeFactory 由 sp-gen 代码生成器产出，此处不定义桩
// 避免与生成器产出的实现冲突导致 Kotlin 编译器报 redeclaration 错误

/* =================== Base 类 =================== */

open class Base {
    open val typeID: Int get() = E_StreamPunkType.Base

    open fun from_(i: I): Base = this
    open fun to(o: O) {}

    companion object {
        @JvmStatic
        fun read_obj(i: I): Base? {
            val id = i.read_u32().toInt()
            val factory = __typeFactory[id] ?: return null
            val obj = factory()
            obj.from_(i)
            return obj
        }
    }
}

fun write_obj(o: O, obj: Base?) {
    if (obj == null) { o.write_u32(0); return }
    o.write_u32(obj.typeID.toLong())
    obj.to(o)
}