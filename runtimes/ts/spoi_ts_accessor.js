"use strict";
// ============================================================
// SPOI Accessor — TypeScript 类型特化访问器（自动生成）
// 由 sp-gen spoi-ts-accessor 从 C++ 元数据生成
// 替代反射机制，直接通过字段索引访问/设置值
// ============================================================
Object.defineProperty(exports, "__esModule", { value: true });
exports.SpoiAccessorRegistry = exports.TypeId = void 0;
exports.deserializeValue = deserializeValue;
// ============================================================
// 基本类型 ID（与 C++ E_type 枚举值一致）
// ============================================================
exports.TypeId = {
    U8: 26,
    U16: 27,
    U32: 28,
    U64: 29,
    I8: 30,
    I16: 31,
    I32: 32,
    I64: 33,
    F32: 34,
    F64: 35,
    STRING: 9,
    BOOL: 40,
    CUSTOM: 0,
};
// ============================================================
// deserializeValue — 通用值反序列化（基于 type_id 前缀）
// 格式: [type_id(u32 LE) + value_bytes]
// ============================================================
function deserializeValue(data) {
    if (data.length < 4)
        return null;
    const view = new DataView(data.buffer, data.byteOffset, data.length);
    const typeId = view.getUint32(0, true);
    const valueBytes = data.slice(4);
    if (typeId === exports.TypeId.U8)
        return valueBytes[0];
    if (typeId === exports.TypeId.U16)
        return view.getUint16(4, true);
    if (typeId === exports.TypeId.U32)
        return view.getUint32(4, true);
    if (typeId === exports.TypeId.U64)
        return view.getBigUint64(4, true);
    if (typeId === exports.TypeId.I8)
        return view.getInt8(4);
    if (typeId === exports.TypeId.I16)
        return view.getInt16(4, true);
    if (typeId === exports.TypeId.I32)
        return view.getInt32(4, true);
    if (typeId === exports.TypeId.I64)
        return view.getBigInt64(4, true);
    if (typeId === exports.TypeId.F32)
        return view.getFloat32(4, true);
    if (typeId === exports.TypeId.F64)
        return view.getFloat64(4, true);
    if (typeId === exports.TypeId.STRING)
        return new TextDecoder('utf-8').decode(valueBytes);
    if (typeId === exports.TypeId.BOOL)
        return valueBytes[0] !== 0;
    return valueBytes;
}
// ============================================================
// SpoiTestPlayerAccessor
// ============================================================
class SpoiTestPlayerAccessor {
    fieldCount() {
        return 4;
    }
    getField(obj, idx) {
        const o = obj;
        switch (idx) {
            case 0: return o.name;
            case 1: return o.hp;
            case 2: return o.level;
            case 3: return o.posX;
            default: throw new Error(`invalid field index for SpoiTestPlayer: ${idx}`);
        }
    }
    setField(obj, idx, val) {
        const o = obj;
        switch (idx) {
            case 0:
                o.name = val;
                break;
            case 1:
                o.hp = val;
                break;
            case 2:
                o.level = val;
                break;
            case 3:
                o.posX = val;
                break;
            default: throw new Error(`invalid field index for SpoiTestPlayer: ${idx}`);
        }
    }
}
// ============================================================
// SpoiTestStateAccessor
// ============================================================
class SpoiTestStateAccessor {
    fieldCount() {
        return 3;
    }
    getField(obj, idx) {
        const o = obj;
        switch (idx) {
            case 0: return o.tick;
            case 1: return o.currentMap;
            case 2: return o.players;
            default: throw new Error(`invalid field index for SpoiTestState: ${idx}`);
        }
    }
    setField(obj, idx, val) {
        const o = obj;
        switch (idx) {
            case 0:
                o.tick = val;
                break;
            case 1:
                o.currentMap = val;
                break;
            case 2:
                o.players = val;
                break;
            default: throw new Error(`invalid field index for SpoiTestState: ${idx}`);
        }
    }
}
// ============================================================
// SpoiItemAccessor
// ============================================================
class SpoiItemAccessor {
    fieldCount() {
        return 2;
    }
    getField(obj, idx) {
        const o = obj;
        switch (idx) {
            case 0: return o.name;
            case 1: return o.value;
            default: throw new Error(`invalid field index for SpoiItem: ${idx}`);
        }
    }
    setField(obj, idx, val) {
        const o = obj;
        switch (idx) {
            case 0:
                o.name = val;
                break;
            case 1:
                o.value = val;
                break;
            default: throw new Error(`invalid field index for SpoiItem: ${idx}`);
        }
    }
}
// ============================================================
// SpoiInventoryAccessor
// ============================================================
class SpoiInventoryAccessor {
    fieldCount() {
        return 3;
    }
    getField(obj, idx) {
        const o = obj;
        switch (idx) {
            case 0: return o.items;
            case 1: return o.equipped;
            case 2: return o.gold;
            default: throw new Error(`invalid field index for SpoiInventory: ${idx}`);
        }
    }
    setField(obj, idx, val) {
        const o = obj;
        switch (idx) {
            case 0:
                o.items = val;
                break;
            case 1:
                o.equipped = val;
                break;
            case 2:
                o.gold = val;
                break;
            default: throw new Error(`invalid field index for SpoiInventory: ${idx}`);
        }
    }
}
// ============================================================
// SpoiCharacterAccessor
// ============================================================
class SpoiCharacterAccessor {
    fieldCount() {
        return 5;
    }
    getField(obj, idx) {
        const o = obj;
        switch (idx) {
            case 0: return o.name;
            case 1: return o.hp;
            case 2: return o.inventory;
            case 3: return o.weapon;
            case 4: return o.petLevel;
            default: throw new Error(`invalid field index for SpoiCharacter: ${idx}`);
        }
    }
    setField(obj, idx, val) {
        const o = obj;
        switch (idx) {
            case 0:
                o.name = val;
                break;
            case 1:
                o.hp = val;
                break;
            case 2:
                o.inventory = val;
                break;
            case 3:
                o.weapon = val;
                break;
            case 4:
                o.petLevel = val;
                break;
            default: throw new Error(`invalid field index for SpoiCharacter: ${idx}`);
        }
    }
}
// ============================================================
// SpoiWorldAccessor
// ============================================================
class SpoiWorldAccessor {
    fieldCount() {
        return 3;
    }
    getField(obj, idx) {
        const o = obj;
        switch (idx) {
            case 0: return o.worldName;
            case 1: return o.tick;
            case 2: return o.characters;
            default: throw new Error(`invalid field index for SpoiWorld: ${idx}`);
        }
    }
    setField(obj, idx, val) {
        const o = obj;
        switch (idx) {
            case 0:
                o.worldName = val;
                break;
            case 1:
                o.tick = val;
                break;
            case 2:
                o.characters = val;
                break;
            default: throw new Error(`invalid field index for SpoiWorld: ${idx}`);
        }
    }
}
// ============================================================
// SpoiAccessorRegistry — 静态类型注册表
// 替代运行时 Record<string, string[]>（反射版本）
// ============================================================
exports.SpoiAccessorRegistry = new Map([
    ["SpoiTestPlayer", new SpoiTestPlayerAccessor()],
    ["SpoiTestState", new SpoiTestStateAccessor()],
    ["SpoiItem", new SpoiItemAccessor()],
    ["SpoiInventory", new SpoiInventoryAccessor()],
    ["SpoiCharacter", new SpoiCharacterAccessor()],
    ["SpoiWorld", new SpoiWorldAccessor()],
]);
