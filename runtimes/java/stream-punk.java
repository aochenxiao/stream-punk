/**
 * Stream-Punk Java Runtime Library
 * Zero-dependency binary serialization/deserialization for Java.
 * Compatible with the C++ StreamPunk binary format (little-endian).
 *
 * Target: Java 8+
 */

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.function.Consumer;
import java.util.function.Supplier;

/* =================== 辅助类型 =================== */

class SpRef<T> {
    public T value;
    public long address;

    public SpRef(T value, long address) {
        this.value = value;
        this.address = address;
    }
}

/* =================== Base 类（由 sp-gen 生成具体实现） =================== */
// Base 类由 sp-gen 代码生成器产出，此处不定义桩
// 避免与生成器产出的实现冲突导致 javac 编译错误

/* =================== SpArray =================== */

class SpArray<T> {
    private Object[] data;

    public SpArray(int size) {
        this.data = new Object[size];
    }

    @SuppressWarnings("unchecked")
    public T at(int index) {
        return (T) data[index];
    }

    public void set(int index, T value) {
        data[index] = value;
    }

    public int size() {
        return data.length;
    }
}

/* =================== 反序列化读取器 =================== */

class I {
    private byte[] buf;
    public int off;
    private Map<Long, Object> objMap;

    public I(byte[] data) {
        this(data, 0);
    }

    public I(byte[] data, int offset) {
        this.buf = data;
        this.off = offset;
        this.objMap = new HashMap<>();
    }

    public boolean hasMoreData() {
        return off < buf.length;
    }

    public int read_u8() {
        return buf[off++] & 0xFF;
    }

    public int read_u16() {
        int v = ByteBuffer.wrap(buf, off, 2).order(ByteOrder.LITTLE_ENDIAN).getShort() & 0xFFFF;
        off += 2;
        return v;
    }

    public long read_u32() {
        long v = ByteBuffer.wrap(buf, off, 4).order(ByteOrder.LITTLE_ENDIAN).getInt() & 0xFFFFFFFFL;
        off += 4;
        return v;
    }

    public long read_u64() {
        long v = ByteBuffer.wrap(buf, off, 8).order(ByteOrder.LITTLE_ENDIAN).getLong();
        off += 8;
        return v;
    }

    public byte read_i8() {
        return buf[off++];
    }

    public short read_i16() {
        short v = ByteBuffer.wrap(buf, off, 2).order(ByteOrder.LITTLE_ENDIAN).getShort();
        off += 2;
        return v;
    }

    public int read_i32() {
        int v = ByteBuffer.wrap(buf, off, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
        off += 4;
        return v;
    }

    public long read_i64() {
        long v = ByteBuffer.wrap(buf, off, 8).order(ByteOrder.LITTLE_ENDIAN).getLong();
        off += 8;
        return v;
    }

    public float read_f32() {
        float v = ByteBuffer.wrap(buf, off, 4).order(ByteOrder.LITTLE_ENDIAN).getFloat();
        off += 4;
        return v;
    }

    public double read_f64() {
        double v = ByteBuffer.wrap(buf, off, 8).order(ByteOrder.LITTLE_ENDIAN).getDouble();
        off += 8;
        return v;
    }

    public char read_ch() {
        return (char) (buf[off++] & 0xFF);
    }

    public char read_ch8() {
        return (char) (buf[off++] & 0xFF);
    }

    public char read_ch16() {
        char v = ByteBuffer.wrap(buf, off, 2).order(ByteOrder.LITTLE_ENDIAN).getChar();
        off += 2;
        return v;
    }

    public int read_ch32() {
        int cp = ByteBuffer.wrap(buf, off, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
        off += 4;
        return cp;
    }

    public boolean read_bl() {
        return buf[off++] != 0;
    }

    public int read_sz() {
        return (int) read_u32();
    }

    public void read_bytes(int length) {
        off += length;
    }

    public String read_string() {
        int length = read_sz();
        if (length == 0) return "";
        String v = new String(buf, off, length, StandardCharsets.UTF_8);
        off += length;
        return v;
    }

    public String read_u8string() {
        return read_string();
    }

    public String read_u16string() {
        int length = read_sz();
        int byteLen = length * 2;
        String v = new String(buf, off, byteLen, StandardCharsets.UTF_16LE);
        off += byteLen;
        return v;
    }

    public byte[] read_u32string() {
        int length = read_sz();
        int byteLen = length * 4;
        byte[] v = Arrays.copyOfRange(buf, off, off + byteLen);
        off += byteLen;
        return v;
    }

    public SpRef<Base> read_ptr_with_typeID() {
        long addr = read_u64();
        if (addr == 0) return new SpRef<>(null, 0);
        if (objMap.containsKey(addr)) return (SpRef<Base>) objMap.get(addr);
        SpRef<Base> ref = new SpRef<>(null, addr);
        objMap.put(addr, ref);
        ref.value = Base.read_obj(this);
        return ref;
    }

    public <T> SpRef<T> read_ptr(Supplier<T> reader) {
        long addr = read_u64();
        if (addr == 0) return new SpRef<>(null, 0);
        if (objMap.containsKey(addr)) return (SpRef<T>) objMap.get(addr);
        T value = reader.get();
        SpRef<T> ref = new SpRef<>(value, addr);
        objMap.put(addr, ref);
        return ref;
    }

    public <T> ArrayList<T> read_array(int count, Supplier<T> reader) {
        ArrayList<T> result = new ArrayList<>(count);
        for (int i = 0; i < count; i++) {
            result.add(reader.get());
        }
        return result;
    }

    public <T> ArrayList<T> read_Array(Supplier<T> reader) {
        int size = read_sz();
        return read_array(size, reader);
    }

    public <T> HashSet<T> read_set(Supplier<T> reader) {
        ArrayList<T> arr = read_Array(reader);
        try {
            return new HashSet<>(arr);
        } catch (Exception e) {
            return new HashSet<>(arr);
        }
    }

    public <T> HashSet<T> read_unordered_set(Supplier<T> reader) {
        return read_set(reader);
    }

    public <K, V> HashMap<K, V> read_map(Supplier<K> keyReader, Supplier<V> valueReader) {
        int size = read_sz();
        HashMap<K, V> result = new HashMap<>();
        for (int i = 0; i < size; i++) {
            K k = keyReader.get();
            V v = valueReader.get();
            result.put(k, v);
        }
        return result;
    }

    public <K, V> HashMap<K, V> read_unordered_map(Supplier<K> keyReader, Supplier<V> valueReader) {
        return read_map(keyReader, valueReader);
    }

    public <T> ArrayList<T> read_vector(Supplier<T> reader) {
        return read_Array(reader);
    }

    public <T> ArrayList<T> read_deque(Supplier<T> reader) {
        return read_Array(reader);
    }

    public <T> ArrayList<T> read_list(Supplier<T> reader) {
        return read_Array(reader);
    }

    public <T> ArrayList<T> read_forward_list(Supplier<T> reader) {
        return read_Array(reader);
    }

    public <T> SpArray<T> read_SpArray(int size, Supplier<T> reader) {
        SpArray<T> arr = new SpArray<>(size);
        for (int i = 0; i < size; i++) {
            arr.set(i, reader.get());
        }
        return arr;
    }

    public String read_std_string() {
        return read_string();
    }
}

/* =================== 序列化写入器 =================== */

class O {
    private ByteArrayOutputStream buf;

    public O() {
        this.buf = new ByteArrayOutputStream();
    }

    public byte[] to_bytes() {
        return buf.toByteArray();
    }

    private void writeLE(byte[] data) {
        try { buf.write(data); } catch (Exception e) { throw new RuntimeException(e); }
    }

    public void write_u8(int v) { buf.write(v & 0xFF); }

    public void write_u16(int v) {
        writeLE(ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort((short) (v & 0xFFFF)).array());
    }

    public void write_u32(long v) {
        writeLE(ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt((int) (v & 0xFFFFFFFFL)).array());
    }

    public void write_u64(long v) {
        writeLE(ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(v).array());
    }

    public void write_i8(byte v) { buf.write(v); }

    public void write_i16(short v) {
        writeLE(ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort(v).array());
    }

    public void write_i32(int v) {
        writeLE(ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array());
    }

    public void write_i64(long v) {
        writeLE(ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(v).array());
    }

    public void write_f32(float v) {
        writeLE(ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putFloat(v).array());
    }

    public void write_f64(double v) {
        writeLE(ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putDouble(v).array());
    }

    public void write_ch(char v) { buf.write(v & 0xFF); }

    public void write_ch8(char v) { buf.write(v & 0xFF); }

    public void write_ch16(char v) {
        writeLE(ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putChar(v).array());
    }

    public void write_ch32(int v) {
        writeLE(ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array());
    }

    public void write_bl(boolean v) { buf.write(v ? 1 : 0); }

    public void write_sz(int v) { write_u32(v); }

    public void write_string(String s) {
        if (s == null) {
            write_sz(0);
            return;
        }
        byte[] bytes = s.getBytes(StandardCharsets.UTF_8);
        write_sz(bytes.length);
        try { buf.write(bytes); } catch (Exception e) { throw new RuntimeException(e); }
    }

    public void write_u8string(String s) { write_string(s); }

    public void write_u16string(String s) {
        if (s == null) {
            write_sz(0);
            return;
        }
        byte[] bytes = s.getBytes(StandardCharsets.UTF_16LE);
        write_sz(s.length());
        try { buf.write(bytes); } catch (Exception e) { throw new RuntimeException(e); }
    }

    public void write_u32string(byte[] b) {
        int length = b == null ? 0 : b.length / 4;
        write_sz(length);
        if (b != null) try { buf.write(b); } catch (Exception e) { throw new RuntimeException(e); }
    }

    public void write_ptr_with_typeID(Base value) {
        if (value == null) {
            write_u64(0);
            return;
        }
        write_u64(System.identityHashCode(value));
    }

    public <T> void write_ptr(T value, long address, Consumer<T> writer) {
        if (value == null) {
            write_u64(0);
            return;
        }
        write_u64(address);
        writer.accept(value);
    }

    public <T> void write_Array(ArrayList<T> arr, Consumer<T> writer) {
        write_sz(arr.size());
        for (T v : arr) {
            writer.accept(v);
        }
    }

    public <T> void write_set(HashSet<T> set, Consumer<T> writer) {
        write_sz(set.size());
        for (T v : set) {
            writer.accept(v);
        }
    }

    public <K, V> void write_map(HashMap<K, V> map, Consumer<K> keyWriter, Consumer<V> valueWriter) {
        write_sz(map.size());
        for (Map.Entry<K, V> e : map.entrySet()) {
            keyWriter.accept(e.getKey());
            valueWriter.accept(e.getValue());
        }
    }
}