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
    CH:     36,
    CH8:    37,
    CH16:   38,
    CH32:   39,
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
    if (typeId === TypeId.CH)   return valueBytes.length > 0 ? valueBytes[0] : 0;
    if (typeId === TypeId.CH8)  return valueBytes.length > 0 ? valueBytes[0] : 0;
    if (typeId === TypeId.CH16) return valueBytes.length >= 2 ? view.getUint16(4, true) : 0;
    if (typeId === TypeId.CH32) return valueBytes.length >= 4 ? view.getUint32(4, true) : 0;
    return valueBytes;
}

// ============================================================
// MousePositionAccessor
// ============================================================

class MousePositionAccessor extends SpoiAccessor {
    fieldCount() { return 2; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.x;
            case 1: return obj.y;
            default: throw new Error('invalid field index for MousePosition: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.x = val | 0; break;
            case 1: obj.y = val | 0; break;
            default: throw new Error('invalid field index for MousePosition: ' + idx);
        }
    }
}

// ============================================================
// MQTTAccessor
// ============================================================

class MQTTAccessor extends SpoiAccessor {
    fieldCount() { return 3; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.host;
            case 1: return obj.user;
            case 2: return obj.pwd;
            default: throw new Error('invalid field index for MQTT: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.host = String(val); break;
            case 1: obj.user = String(val); break;
            case 2: obj.pwd = String(val); break;
            default: throw new Error('invalid field index for MQTT: ' + idx);
        }
    }
}

// ============================================================
// TestAccessor
// ============================================================

class TestAccessor extends SpoiAccessor {
    fieldCount() { return 7; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.name;
            case 1: return obj.pwd;
            case 2: return obj.gateWay;
            case 3: return obj.mask;
            case 4: return obj.ip;
            case 5: return obj.dns1;
            case 6: return obj.dns2;
            default: throw new Error('invalid field index for Test: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.name = String(val); break;
            case 1: obj.pwd = String(val); break;
            case 2: obj.gateWay = String(val); break;
            case 3: obj.mask = String(val); break;
            case 4: obj.ip = String(val); break;
            case 5: obj.dns1 = String(val); break;
            case 6: obj.dns2 = String(val); break;
            default: throw new Error('invalid field index for Test: ' + idx);
        }
    }
}

// ============================================================
// AllBasicTypesAccessor
// ============================================================

class AllBasicTypesAccessor extends SpoiAccessor {
    fieldCount() { return 15; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.b;
            case 1: return obj.i8_v;
            case 2: return obj.u8_v;
            case 3: return obj.i16_v;
            case 4: return obj.u16_v;
            case 5: return obj.i32_v;
            case 6: return obj.u32_v;
            case 7: return obj.i64_v;
            case 8: return obj.u64_v;
            case 9: return obj.f;
            case 10: return obj.d;
            case 11: return obj.c;
            case 12: return obj.c8;
            case 13: return obj.c16;
            case 14: return obj.c32;
            default: throw new Error('invalid field index for AllBasicTypes: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.b = !!val; break;
            case 1: obj.i8_v = (val << 24) >> 24; break;
            case 2: obj.u8_v = val & 0xFF; break;
            case 3: obj.i16_v = (val << 16) >> 16; break;
            case 4: obj.u16_v = val & 0xFFFF; break;
            case 5: obj.i32_v = val | 0; break;
            case 6: obj.u32_v = val >>> 0; break;
            case 7: obj.i64_v = BigInt(val); break;
            case 8: obj.u64_v = BigInt(val); break;
            case 9: obj.f = Math.fround(val); break;
            case 10: obj.d = Number(val); break;
            case 11: obj.c = val & 0xFF; break;
            case 12: obj.c8 = val & 0xFF; break;
            case 13: obj.c16 = val & 0xFFFF; break;
            case 14: obj.c32 = val >>> 0; break;
            default: throw new Error('invalid field index for AllBasicTypes: ' + idx);
        }
    }
}

// ============================================================
// ChildAccessor
// ============================================================

class ChildAccessor extends SpoiAccessor {
    fieldCount() { return 1; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.child_field;
            default: throw new Error('invalid field index for Child: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.child_field = val | 0; break;
            default: throw new Error('invalid field index for Child: ' + idx);
        }
    }
}

// ============================================================
// PointerDemoAccessor
// ============================================================

class PointerDemoAccessor extends SpoiAccessor {
    fieldCount() { return 4; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.rawPtr;
            case 1: return obj.sharedPtr;
            case 2: return obj.uniquePtr;
            case 3: return obj.weakSelf;
            default: throw new Error('invalid field index for PointerDemo: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.rawPtr = val; break;
            case 1: obj.sharedPtr = val; break;
            case 2: obj.uniquePtr = val; break;
            case 3: obj.weakSelf = val; break;
            default: throw new Error('invalid field index for PointerDemo: ' + idx);
        }
    }
}

// ============================================================
// ShadowTestDataAccessor
// ============================================================

class ShadowTestDataAccessor extends SpoiAccessor {
    fieldCount() { return 4; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.numbers;
            case 1: return obj.items;
            case 2: return obj.optVal;
            case 3: return obj.pos;
            default: throw new Error('invalid field index for ShadowTestData: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.numbers = val | 0; break;
            case 1: obj.items = val & 0xFF; break;
            case 2: obj.optVal = val | 0; break;
            case 3: obj.pos = val; break;
            default: throw new Error('invalid field index for ShadowTestData: ' + idx);
        }
    }
}

// ============================================================
// DeviceAccessor
// ============================================================

class DeviceAccessor extends SpoiAccessor {
    fieldCount() { return 3; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.deviceId;
            case 1: return obj.manufacturer;
            case 2: return obj.lastSeen;
            default: throw new Error('invalid field index for Device: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.deviceId = String(val); break;
            case 1: obj.manufacturer = String(val); break;
            case 2: obj.lastSeen = val; break;
            default: throw new Error('invalid field index for Device: ' + idx);
        }
    }
}

// ============================================================
// NetworkDeviceAccessor
// ============================================================

class NetworkDeviceAccessor extends SpoiAccessor {
    fieldCount() { return 3; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.ipAddress;
            case 1: return obj.macAddress;
            case 2: return obj.port;
            default: throw new Error('invalid field index for NetworkDevice: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.ipAddress = String(val); break;
            case 1: obj.macAddress = String(val); break;
            case 2: obj.port = val & 0xFFFF; break;
            default: throw new Error('invalid field index for NetworkDevice: ' + idx);
        }
    }
}

// ============================================================
// TemplateContainerAccessor
// ============================================================

class TemplateContainerAccessor extends SpoiAccessor {
    fieldCount() { return 10; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.s;
            case 1: return obj.u8s;
            case 2: return obj.vec;
            case 3: return obj.deq;
            case 4: return obj.lst;
            case 5: return obj.shortForwardList;
            case 6: return obj.uintSet;
            case 7: return obj.stringHashSet;
            case 8: return obj.intStringMap;
            case 9: return obj.stringFloatHashMap;
            default: throw new Error('invalid field index for TemplateContainer: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.s = String(val); break;
            case 1: obj.u8s = String(val); break;
            case 2: obj.vec = val | 0; break;
            case 3: obj.deq = Number(val); break;
            case 4: obj.lst = val & 0xFF; break;
            case 5: obj.shortForwardList = val & 0xFFFF; break;
            case 6: obj.uintSet = val >>> 0; break;
            case 7: obj.stringHashSet = val & 0xFF; break;
            case 8: obj.intStringMap = val & 0xFF; break;
            case 9: obj.stringFloatHashMap = Math.fround(val); break;
            default: throw new Error('invalid field index for TemplateContainer: ' + idx);
        }
    }
}

// ============================================================
// PointerContainerAccessor
// ============================================================

class PointerContainerAccessor extends SpoiAccessor {
    fieldCount() { return 3; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.raw_ptr;
            case 1: return obj.shared_ptr_int;
            case 2: return obj.unique_ptr_int;
            default: throw new Error('invalid field index for PointerContainer: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.raw_ptr = val | 0; break;
            case 1: obj.shared_ptr_int = val | 0; break;
            case 2: obj.unique_ptr_int = val | 0; break;
            default: throw new Error('invalid field index for PointerContainer: ' + idx);
        }
    }
}

// ============================================================
// ComplexTemplateNestingAccessor
// ============================================================

class ComplexTemplateNestingAccessor extends SpoiAccessor {
    fieldCount() { return 4; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.nestedVectors;
            case 1: return obj.arrayVectors;
            case 2: return obj.mapVectors;
            case 3: return obj.setVecs;
            default: throw new Error('invalid field index for ComplexTemplateNesting: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.nestedVectors = BigInt(val); break;
            case 1: obj.arrayVectors = Math.fround(val); break;
            case 2: obj.mapVectors = val >>> 0; break;
            case 3: obj.setVecs = BigInt(val); break;
            default: throw new Error('invalid field index for ComplexTemplateNesting: ' + idx);
        }
    }
}

// ============================================================
// TemplateAndPointerAccessor
// ============================================================

class TemplateAndPointerAccessor extends SpoiAccessor {
    fieldCount() { return 2; }

    getField(obj, idx) {
        switch (idx) {
            case 0: return obj.v_raw_ptr;
            case 1: return obj.m_str_shared_ptr;
            default: throw new Error('invalid field index for TemplateAndPointer: ' + idx);
        }
    }

    setField(obj, idx, val) {
        switch (idx) {
            case 0: obj.v_raw_ptr = val | 0; break;
            case 1: obj.m_str_shared_ptr = val | 0; break;
            default: throw new Error('invalid field index for TemplateAndPointer: ' + idx);
        }
    }
}

// ============================================================
// SpoiAccessorRegistry — 静态类型注册表
// 替代运行时 Record<string, string[]>（反射版本）
// ============================================================

const SpoiAccessorRegistry = new Map([
    ['MousePosition', new MousePositionAccessor()],
    ['MQTT', new MQTTAccessor()],
    ['Test', new TestAccessor()],
    ['AllBasicTypes', new AllBasicTypesAccessor()],
    ['Child', new ChildAccessor()],
    ['PointerDemo', new PointerDemoAccessor()],
    ['ShadowTestData', new ShadowTestDataAccessor()],
    ['Device', new DeviceAccessor()],
    ['NetworkDevice', new NetworkDeviceAccessor()],
    ['TemplateContainer', new TemplateContainerAccessor()],
    ['PointerContainer', new PointerContainerAccessor()],
    ['ComplexTemplateNesting', new ComplexTemplateNestingAccessor()],
    ['TemplateAndPointer', new TemplateAndPointerAccessor()],
]);

// ============================================================
// 导出（同时支持 CommonJS 和 ES Module）
// ============================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = { TypeId, SpoiAccessor, deserializeValue, SpoiAccessorRegistry };
    module.exports.MousePositionAccessor = MousePositionAccessor;
    module.exports.MQTTAccessor = MQTTAccessor;
    module.exports.TestAccessor = TestAccessor;
    module.exports.AllBasicTypesAccessor = AllBasicTypesAccessor;
    module.exports.ChildAccessor = ChildAccessor;
    module.exports.PointerDemoAccessor = PointerDemoAccessor;
    module.exports.ShadowTestDataAccessor = ShadowTestDataAccessor;
    module.exports.DeviceAccessor = DeviceAccessor;
    module.exports.NetworkDeviceAccessor = NetworkDeviceAccessor;
    module.exports.TemplateContainerAccessor = TemplateContainerAccessor;
    module.exports.PointerContainerAccessor = PointerContainerAccessor;
    module.exports.ComplexTemplateNestingAccessor = ComplexTemplateNestingAccessor;
    module.exports.TemplateAndPointerAccessor = TemplateAndPointerAccessor;
}
