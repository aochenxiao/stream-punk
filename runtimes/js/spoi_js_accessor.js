// ============================================================
// SPOI Accessor — JavaScript 类型特化访问器（自动生成）
// 由 sp-gen spoi-js-accessor 从 C++ 元数据生成
// 替代反射机制，直接通过字段索引访问/设置值
// ============================================================

'use strict';

// ============================================================
// 基本类型 ID（与 C++ E_type 枚举值一致）
// ============================================================

const TypeId = {
    U8:     26,
    U16:    27,
    U32:    28,
    U64:    29,
    I8:     30,
    I16:    31,
    I32:    32,
    I64:    33,
    F32:    34,
    F64:    35,
    STRING: 9,
    BOOL:   40,
};

// ============================================================
// SpoiAccessor — 类型特化访问器基类
// ============================================================

class SpoiAccessor {
    /** @returns {number} */
    fieldCount() { return 0; }

    /**
     * @param {*} obj
     * @param {number} idx
     * @returns {*}
     */
    getField(obj, idx) { return undefined; }

    /**
     * @param {*} obj
     * @param {number} idx
     * @param {*} val
     */
    setField(obj, idx, val) {}
}

// ============================================================
// DeserializeValue — 通用值反序列化（基于 type_id 前缀）
// 格式: [type_id(u32 LE) + value_bytes]
// ============================================================

/**
 * @param {Uint8Array} data
 * @returns {*}
 */
function deserializeValue(data) {
    if (!data || data.length < 4) return null;
    const view = new DataView(data.buffer, data.byteOffset, data.length);
    const typeId = view.getUint32(0, true);
    const valueBytes = data.length > 4 ? data.slice(4) : new Uint8Array(0);
    if (typeId === TypeId.U8)   return valueBytes.length > 0 ? valueBytes[0] : 0;
    if (typeId === TypeId.U16)  return valueBytes.length >= 2 ? view.getUint16(4, true) : 0;
    if (typeId === TypeId.U32)  return valueBytes.length >= 4 ? view.getUint32(4, true) : 0;
    if (typeId === TypeId.U64)  return valueBytes.length >= 8 ? view.getBigUint64(4, true) : 0n;
    if (typeId === TypeId.I8)   return valueBytes.length > 0 ? view.getInt8(4) : 0;
    if (typeId === TypeId.I16)  return valueBytes.length >= 2 ? view.getInt16(4, true) : 0;
    if (typeId === TypeId.I32)  return valueBytes.length >= 4 ? view.getInt32(4, true) : 0;
    if (typeId === TypeId.I64)  return valueBytes.length >= 8 ? view.getBigInt64(4, true) : 0n;
    if (typeId === TypeId.F32)  return valueBytes.length >= 4 ? view.getFloat32(4, true) : 0.0;
    if (typeId === TypeId.F64)  return valueBytes.length >= 8 ? view.getFloat64(4, true) : 0.0;
    if (typeId === TypeId.STRING) return new TextDecoder('utf-8').decode(valueBytes);
    if (typeId === TypeId.BOOL) return valueBytes.length > 0 ? valueBytes[0] !== 0 : false;
    return valueBytes;
}

// ============================================================
// SpoiTestPlayerAccessor
// ============================================================

class SpoiTestPlayerAccessor extends SpoiAccessor {
    fieldCount() { return 4; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.name;
            case 1: return obj.hp;
            case 2: return obj.level;
            case 3: return obj.posX;
            default: throw new Error('invalid field index for SpoiTestPlayer: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.name = val; break;
            case 1: obj.hp = val; break;
            case 2: obj.level = val; break;
            case 3: obj.posX = val; break;
            default: throw new Error('invalid field index for SpoiTestPlayer: ' + idx);
        }
    }
}

// ============================================================
// SpoiTestStateAccessor
// ============================================================

class SpoiTestStateAccessor extends SpoiAccessor {
    fieldCount() { return 3; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.tick;
            case 1: return obj.currentMap;
            case 2: return obj.players;
            default: throw new Error('invalid field index for SpoiTestState: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.tick = val; break;
            case 1: obj.currentMap = val; break;
            case 2: obj.players = val; break;
            default: throw new Error('invalid field index for SpoiTestState: ' + idx);
        }
    }
}

// ============================================================
// SpoiItemAccessor
// ============================================================

class SpoiItemAccessor extends SpoiAccessor {
    fieldCount() { return 2; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.name;
            case 1: return obj.value;
            default: throw new Error('invalid field index for SpoiItem: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.name = val; break;
            case 1: obj.value = val; break;
            default: throw new Error('invalid field index for SpoiItem: ' + idx);
        }
    }
}

// ============================================================
// SpoiInventoryAccessor
// ============================================================

class SpoiInventoryAccessor extends SpoiAccessor {
    fieldCount() { return 3; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.items;
            case 1: return obj.equipped;
            case 2: return obj.gold;
            default: throw new Error('invalid field index for SpoiInventory: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.items = val; break;
            case 1: obj.equipped = val; break;
            case 2: obj.gold = val; break;
            default: throw new Error('invalid field index for SpoiInventory: ' + idx);
        }
    }
}

// ============================================================
// SpoiCharacterAccessor
// ============================================================

class SpoiCharacterAccessor extends SpoiAccessor {
    fieldCount() { return 5; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.name;
            case 1: return obj.hp;
            case 2: return obj.inventory;
            case 3: return obj.weapon;
            case 4: return obj.petLevel;
            default: throw new Error('invalid field index for SpoiCharacter: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.name = val; break;
            case 1: obj.hp = val; break;
            case 2: obj.inventory = val; break;
            case 3: obj.weapon = val; break;
            case 4: obj.petLevel = val; break;
            default: throw new Error('invalid field index for SpoiCharacter: ' + idx);
        }
    }
}

// ============================================================
// SpoiWorldAccessor
// ============================================================

class SpoiWorldAccessor extends SpoiAccessor {
    fieldCount() { return 3; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.worldName;
            case 1: return obj.tick;
            case 2: return obj.characters;
            default: throw new Error('invalid field index for SpoiWorld: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.worldName = val; break;
            case 1: obj.tick = val; break;
            case 2: obj.characters = val; break;
            default: throw new Error('invalid field index for SpoiWorld: ' + idx);
        }
    }
}

// ============================================================
// SpoiAccessorRegistry — 静态类型注册表
// 替代运行时 Record<string, string[]>（反射版本）
// ============================================================

const SpoiAccessorRegistry = new Map([
    ['SpoiTestPlayer', new SpoiTestPlayerAccessor()],
    ['SpoiTestState', new SpoiTestStateAccessor()],
    ['SpoiItem', new SpoiItemAccessor()],
    ['SpoiInventory', new SpoiInventoryAccessor()],
    ['SpoiCharacter', new SpoiCharacterAccessor()],
    ['SpoiWorld', new SpoiWorldAccessor()],
]);

// ============================================================
// 导出（同时支持 CommonJS 和 ES Module）
// ============================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = { TypeId, SpoiAccessor, deserializeValue, SpoiAccessorRegistry };
    module.exports.SpoiTestPlayerAccessor = SpoiTestPlayerAccessor;
    module.exports.SpoiTestStateAccessor = SpoiTestStateAccessor;
    module.exports.SpoiItemAccessor = SpoiItemAccessor;
    module.exports.SpoiInventoryAccessor = SpoiInventoryAccessor;
    module.exports.SpoiCharacterAccessor = SpoiCharacterAccessor;
    module.exports.SpoiWorldAccessor = SpoiWorldAccessor;
}
