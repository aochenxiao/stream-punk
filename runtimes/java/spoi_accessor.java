// ============================================================
// SPOI Accessor — Java 类型特化访问器（自动生成）
// 由 sp-gen spoi-java-accessor 从 C++ 元数据生成
// 替代反射机制，直接通过字段索引访问/设置值
// ============================================================

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.*;

// ============================================================
// 基本类型 ID（与 C++ E_type 枚举值一致）
// ============================================================

class TypeId {
    public static final long U8     = 26L;
    public static final long U16    = 27L;
    public static final long U32    = 28L;
    public static final long U64    = 29L;
    public static final long I8     = 30L;
    public static final long I16    = 31L;
    public static final long I32    = 32L;
    public static final long I64    = 33L;
    public static final long F32    = 34L;
    public static final long F64    = 35L;
    public static final long STRING = 9L;
    public static final long BOOL   = 40L;
}

// ============================================================
// SpoiAccessor — 类型特化访问器接口
// ============================================================

interface SpoiAccessor {
    int fieldCount();
    Object getField(Object obj, int idx);
    void setField(Object obj, int idx, Object val);
}

// ============================================================
// DeserializeValue — 通用值反序列化（基于 type_id 前缀）
// 格式: [type_id(u32 LE) + value_bytes]
// ============================================================

class SpoiDeserializer {
    static Object deserializeValue(byte[] data) {
        if (data == null || data.length < 4) return null;
        long typeId = Integer.toUnsignedLong(
            ByteBuffer.wrap(data, 0, 4).order(ByteOrder.LITTLE_ENDIAN).getInt());
        byte[] valueBytes = Arrays.copyOfRange(data, 4, data.length);
        if (typeId == TypeId.U8)   return valueBytes[0] & 0xFF;
        if (typeId == TypeId.U16)  return (int)(ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getShort() & 0xFFFF);
        if (typeId == TypeId.U32)  return Integer.toUnsignedLong(ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getInt());
        if (typeId == TypeId.U64)  return ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getLong();
        if (typeId == TypeId.I8)   return valueBytes[0];
        if (typeId == TypeId.I16)  return ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getShort();
        if (typeId == TypeId.I32)  return ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getInt();
        if (typeId == TypeId.I64)  return ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getLong();
        if (typeId == TypeId.F32)  return ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getFloat();
        if (typeId == TypeId.F64)  return ByteBuffer.wrap(valueBytes).order(ByteOrder.LITTLE_ENDIAN).getDouble();
        if (typeId == TypeId.STRING) return new String(valueBytes, StandardCharsets.UTF_8);
        if (typeId == TypeId.BOOL) return valueBytes[0] != 0;
        return valueBytes;
    }
}

// ============================================================
// SpoiTestPlayerAccessor
// ============================================================

class SpoiTestPlayerAccessor implements SpoiAccessor {

    public int fieldCount() { return 4; }

    public Object getField(Object obj, int idx) {
        SpoiTestPlayer o = (SpoiTestPlayer) obj;
        switch (idx) {
            case 0: return o.name;
            case 1: return Integer.valueOf(o.hp);
            case 2: return Integer.valueOf(o.level);
            case 3: return Double.valueOf(o.posX);
            default: throw new IllegalArgumentException("invalid field index for SpoiTestPlayer: " + idx);
        }
    }

    public void setField(Object obj, int idx, Object val) {
        SpoiTestPlayer o = (SpoiTestPlayer) obj;
        switch (idx) {
            case 0: o.name = val != null ? val.toString() : ""; break;
            case 1: o.hp = val instanceof Number ? ((Number)val).intValue() : 0; break;
            case 2: o.level = val instanceof Number ? ((Number)val).intValue() : 0; break;
            case 3: o.posX = val instanceof Number ? ((Number)val).doubleValue() : 0.0; break;
            default: throw new IllegalArgumentException("invalid field index for SpoiTestPlayer: " + idx);
        }
    }
}

// ============================================================
// SpoiTestStateAccessor
// ============================================================

class SpoiTestStateAccessor implements SpoiAccessor {

    public int fieldCount() { return 3; }

    public Object getField(Object obj, int idx) {
        SpoiTestState o = (SpoiTestState) obj;
        switch (idx) {
            case 0: return Integer.valueOf(o.tick);
            case 1: return o.currentMap;
            case 2: return o.players;
            default: throw new IllegalArgumentException("invalid field index for SpoiTestState: " + idx);
        }
    }

    public void setField(Object obj, int idx, Object val) {
        SpoiTestState o = (SpoiTestState) obj;
        switch (idx) {
            case 0: o.tick = val instanceof Number ? ((Number)val).intValue() : 0; break;
            case 1: o.currentMap = val != null ? val.toString() : ""; break;
            case 2: o.players = val; break;
            default: throw new IllegalArgumentException("invalid field index for SpoiTestState: " + idx);
        }
    }
}

// ============================================================
// SpoiItemAccessor
// ============================================================

class SpoiItemAccessor implements SpoiAccessor {

    public int fieldCount() { return 2; }

    public Object getField(Object obj, int idx) {
        SpoiItem o = (SpoiItem) obj;
        switch (idx) {
            case 0: return o.name;
            case 1: return Integer.valueOf(o.value);
            default: throw new IllegalArgumentException("invalid field index for SpoiItem: " + idx);
        }
    }

    public void setField(Object obj, int idx, Object val) {
        SpoiItem o = (SpoiItem) obj;
        switch (idx) {
            case 0: o.name = val != null ? val.toString() : ""; break;
            case 1: o.value = val instanceof Number ? ((Number)val).intValue() : 0; break;
            default: throw new IllegalArgumentException("invalid field index for SpoiItem: " + idx);
        }
    }
}

// ============================================================
// SpoiInventoryAccessor
// ============================================================

class SpoiInventoryAccessor implements SpoiAccessor {

    public int fieldCount() { return 3; }

    public Object getField(Object obj, int idx) {
        SpoiInventory o = (SpoiInventory) obj;
        switch (idx) {
            case 0: return o.items;
            case 1: return o.equipped;
            case 2: return Integer.valueOf(o.gold);
            default: throw new IllegalArgumentException("invalid field index for SpoiInventory: " + idx);
        }
    }

    public void setField(Object obj, int idx, Object val) {
        SpoiInventory o = (SpoiInventory) obj;
        switch (idx) {
            case 0: o.items = val; break;
            case 1: o.equipped = val; break;
            case 2: o.gold = val instanceof Number ? ((Number)val).intValue() : 0; break;
            default: throw new IllegalArgumentException("invalid field index for SpoiInventory: " + idx);
        }
    }
}

// ============================================================
// SpoiCharacterAccessor
// ============================================================

class SpoiCharacterAccessor implements SpoiAccessor {

    public int fieldCount() { return 5; }

    public Object getField(Object obj, int idx) {
        SpoiCharacter o = (SpoiCharacter) obj;
        switch (idx) {
            case 0: return o.name;
            case 1: return Integer.valueOf(o.hp);
            case 2: return o.inventory;
            case 3: return o.weapon;
            case 4: return Integer.valueOf(o.petLevel);
            default: throw new IllegalArgumentException("invalid field index for SpoiCharacter: " + idx);
        }
    }

    public void setField(Object obj, int idx, Object val) {
        SpoiCharacter o = (SpoiCharacter) obj;
        switch (idx) {
            case 0: o.name = val != null ? val.toString() : ""; break;
            case 1: o.hp = val instanceof Number ? ((Number)val).intValue() : 0; break;
            case 2: o.inventory = val; break;
            case 3: o.weapon = val; break;
            case 4: o.petLevel = val instanceof Number ? ((Number)val).intValue() : 0; break;
            default: throw new IllegalArgumentException("invalid field index for SpoiCharacter: " + idx);
        }
    }
}

// ============================================================
// SpoiWorldAccessor
// ============================================================

class SpoiWorldAccessor implements SpoiAccessor {

    public int fieldCount() { return 3; }

    public Object getField(Object obj, int idx) {
        SpoiWorld o = (SpoiWorld) obj;
        switch (idx) {
            case 0: return o.worldName;
            case 1: return Integer.valueOf(o.tick);
            case 2: return o.characters;
            default: throw new IllegalArgumentException("invalid field index for SpoiWorld: " + idx);
        }
    }

    public void setField(Object obj, int idx, Object val) {
        SpoiWorld o = (SpoiWorld) obj;
        switch (idx) {
            case 0: o.worldName = val != null ? val.toString() : ""; break;
            case 1: o.tick = val instanceof Number ? ((Number)val).intValue() : 0; break;
            case 2: o.characters = val; break;
            default: throw new IllegalArgumentException("invalid field index for SpoiWorld: " + idx);
        }
    }
}

// ============================================================
// SpoiAccessorRegistry — 静态类型注册表
// 替代运行时 Map<String, List<String>>（反射版本）
// ============================================================

class SpoiAccessorRegistry {
    static final Map<String, SpoiAccessor> registry = new HashMap<>();
    static {
        registry.put("SpoiTestPlayer", new SpoiTestPlayerAccessor());
        registry.put("SpoiTestState", new SpoiTestStateAccessor());
        registry.put("SpoiItem", new SpoiItemAccessor());
        registry.put("SpoiInventory", new SpoiInventoryAccessor());
        registry.put("SpoiCharacter", new SpoiCharacterAccessor());
        registry.put("SpoiWorld", new SpoiWorldAccessor());
    }
    static SpoiAccessor get(String typeName) { return registry.get(typeName); }
}
