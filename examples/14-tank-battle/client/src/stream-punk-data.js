/* eslint-disable @typescript-eslint/no-array-constructor */
export class SpRef {
    constructor(initialValue = null, address = 0n) {
        this.value = initialValue;
        this.address = address;
    }
}
export class SpArray {
    constructor(size) {
        this.size = size;
        this._data = new Array(size);
    }
    at(index) {
        if (index < 0 || index >= this.size)
            throw new RangeError("SpArray index out of bounds");
        return this._data[index];
    }
    set(index, value) {
        if (index < 0 || index >= this.size)
            throw new RangeError("SpArray index out of bounds");
        this._data[index] = value;
    }
}
export class SpVariant {
    constructor(value, typeIndex) {
        this.value = value;
        this.typeIndex = typeIndex;
    }
}
export class I {
    constructor(buffer, initialOffset = 0) {
        this.view = new DataView(buffer);
        this.offset = initialOffset;
        this.objectMap = new Map();
        this.decoder = new TextDecoder("utf-8");
        this.decoder16 = new TextDecoder("utf-16le");
    }
    hasMoreData() { return this.offset < this.view.byteLength; }
    skip(bytes) { this.offset += bytes; }
    readU8() { const v = this.view.getUint8(this.offset); this.offset += 1; return v; }
    readU16() { const v = this.view.getUint16(this.offset, true); this.offset += 2; return v; }
    readU32() { const v = this.view.getUint32(this.offset, true); this.offset += 4; return v; }
    readU64() { const v = this.view.getBigUint64(this.offset, true); this.offset += 8; return v; }
    readI8() { const v = this.view.getInt8(this.offset); this.offset += 1; return v; }
    readI16() { const v = this.view.getInt16(this.offset, true); this.offset += 2; return v; }
    readI32() { const v = this.view.getInt32(this.offset, true); this.offset += 4; return v; }
    readI64() { const v = this.view.getBigInt64(this.offset, true); this.offset += 8; return v; }
    readF32() { const v = this.view.getFloat32(this.offset, true); this.offset += 4; return v; }
    readF64() { const v = this.view.getFloat64(this.offset, true); this.offset += 8; return v; }
    readCh() { const v = String.fromCharCode(this.view.getUint8(this.offset)); this.offset += 1; return v; }
    readCh8() { return this.readCh(); }
    readCh16() { const v = String.fromCharCode(this.view.getUint16(this.offset, true)); this.offset += 2; return v; }
    readCh32() {
        const cp = this.view.getUint32(this.offset, true);
        this.offset += 4;
        return (cp >= 0 && cp <= 0x10FFFF) ? String.fromCodePoint(cp) : "";
    }
    readBl() { const v = this.view.getUint8(this.offset) !== 0; this.offset += 1; return v; }
    readSz() { return this.readU32(); }
    readString() {
        const len = Number(this.readSz());
        if (len === 0)
            return "";
        const v = this.decoder.decode(new Uint8Array(this.view.buffer, this.offset, len));
        this.offset += len;
        return v;
    }
    readU8String() {
        const len = Number(this.readSz());
        const v = this.decoder.decode(new Uint8Array(this.view.buffer, this.offset, len));
        this.offset += len;
        return v;
    }
    readU16String() {
        const len = Number(this.readSz());
        const byteLen = len * 2;
        const v = this.decoder16.decode(new Uint8Array(this.view.buffer, this.offset, byteLen));
        this.offset += byteLen;
        return v;
    }
    readU32String() {
        const len = Number(this.readSz());
        const byteLen = len * 4;
        const bytes = new Uint8Array(this.view.buffer, this.offset, byteLen);
        this.offset += byteLen;
        let result = "";
        for (let i = 0; i < byteLen; i += 4) {
            const cp = new DataView(bytes.buffer, bytes.byteOffset + i, 4).getUint32(0, true);
            result += String.fromCodePoint(cp);
        }
        return result;
    }
    readPtrWithTypeID() {
        const address = this.readU64();
        if (address === 0n)
            return new SpRef(null, 0n);
        if (this.objectMap.has(address))
            return this.objectMap.get(address);
        const ref = new SpRef(null, address);
        this.objectMap.set(address, ref);
        ref.value = readObj(this);
        return ref;
    }
    readPtr(reader) {
        const address = this.readU64();
        if (address === 0n)
            return new SpRef(null, 0n);
        if (this.objectMap.has(address))
            return this.objectMap.get(address);
        const value = reader();
        const ref = new SpRef(value, address);
        this.objectMap.set(address, ref);
        return ref;
    }
    readArray(reader) {
        const size = Number(this.readSz());
        const arr = new Array(size);
        for (let i = 0; i < size; i++)
            arr[i] = reader();
        return arr;
    }
    readSet(reader) {
        return new Set(this.readArray(reader));
    }
    readMap(keyReader, valueReader) {
        const size = Number(this.readSz());
        const map = new Map();
        for (let i = 0; i < size; i++) {
            map.set(keyReader(), valueReader());
        }
        return map;
    }
    readSpArray(size, reader) {
        const arr = new SpArray(size);
        for (let i = 0; i < size; i++)
            arr.set(i, reader());
        return arr;
    }
    readBitset() {
        const size = this.readU32();
        const byteLen = Math.ceil(size / 8);
        const bytes = new Uint8Array(this.view.buffer, this.offset, byteLen);
        this.offset += byteLen;
        return Array.from({ length: size }, (_, i) => {
            return ((bytes[Math.floor(i / 8)] >> (i % 8)) & 1) === 1;
        });
    }
    readOptional(reader) {
        return this.readBl() ? reader() : null;
    }
    readVariant(readers) {
        const index = this.readU32();
        if (index >= readers.length)
            throw new Error(`Variant index ${index} out of range`);
        return new SpVariant(readers[index](), index);
    }
    readTime() {
        const sec = this.readI64();
        const attoSec = this.readI64();
        return new Date(Number(sec) * 1000 + Number(attoSec / 1000000000000000n));
    }
}
export class O {
    constructor() {
        this._buffers = [];
        this._objectMap = new Map();
        this._encoder = new TextEncoder();
        this._nextAddr = 1n;
    }
    _pushBuffer(buf) {
        this._buffers.push(buf);
    }
    _pushView(fn, byteLen) {
        const buf = new ArrayBuffer(byteLen);
        fn(new DataView(buf));
        this._pushBuffer(buf);
    }
    writeU8(v) { this._pushView(dv => dv.setUint8(0, v), 1); }
    writeU16(v) { this._pushView(dv => dv.setUint16(0, v, true), 2); }
    writeU32(v) { this._pushView(dv => dv.setUint32(0, v, true), 4); }
    writeU64(v) { this._pushView(dv => dv.setBigUint64(0, v, true), 8); }
    writeI8(v) { this._pushView(dv => dv.setInt8(0, v), 1); }
    writeI16(v) { this._pushView(dv => dv.setInt16(0, v, true), 2); }
    writeI32(v) { this._pushView(dv => dv.setInt32(0, v, true), 4); }
    writeI64(v) { this._pushView(dv => dv.setBigInt64(0, v, true), 8); }
    writeF32(v) { this._pushView(dv => dv.setFloat32(0, v, true), 4); }
    writeF64(v) { this._pushView(dv => dv.setFloat64(0, v, true), 8); }
    writeCh(v) { this.writeU16(v.length > 0 ? v.charCodeAt(0) : 0); }
    writeCh8(v) { this.writeU8(v.length > 0 ? v.charCodeAt(0) : 0); }
    writeCh16(v) { this.writeU16(v.length > 0 ? v.charCodeAt(0) : 0); }
    writeCh32(v) { this.writeU32(v.length > 0 ? (v.codePointAt(0) ?? 0) : 0); }
    writeBl(v) { this.writeU8(v ? 1 : 0); }
    writeSz(v) { this.writeU32(v); }
    _writeBytes(data) {
        const buf = new ArrayBuffer(data.length);
        new Uint8Array(buf).set(data);
        this._buffers.push(buf);
    }
    writeString(v) {
        const encoded = this._encoder.encode(v);
        this.writeSz(encoded.length);
        this._writeBytes(encoded);
    }
    writeU8String(v) { this.writeString(v); }
    writeU16String(v) {
        this.writeSz(v.length);
        const bytes = new Uint8Array(v.length * 2);
        for (let i = 0; i < v.length; i++) {
            const cu = v.charCodeAt(i);
            bytes[i * 2] = cu & 0xFF;
            bytes[i * 2 + 1] = (cu >> 8) & 0xFF;
        }
        this._writeBytes(bytes);
    }
    writeU32String(v) {
        const len = v.length;
        this.writeSz(len);
        const bytes = new Uint8Array(len * 4);
        for (let i = 0; i < len; i++) {
            const cp = v.codePointAt(i) ?? 0;
            const off = i * 4;
            bytes[off] = cp & 0xFF;
            bytes[off + 1] = (cp >> 8) & 0xFF;
            bytes[off + 2] = (cp >> 16) & 0xFF;
            bytes[off + 3] = (cp >> 24) & 0xFF;
            if (cp > 0xFFFF)
                i++;
        }
        this._writeBytes(bytes);
    }
    writePtr(value, address, writer) {
        if (value === null) {
            this.writeU64(0n);
            return;
        }
        if (address === 0n) {
            address = this._nextAddr++;
        }
        if (this._objectMap.has(address)) {
            this.writeU64(address);
            return;
        }
        this._objectMap.set(address, value);
        this.writeU64(address);
        writer(value);
    }
    writePtrWithTypeID(value) {
        if (value === null) {
            this.writeU64(0n);
            return;
        }
        const address = this._nextAddr++;
        if (this._objectMap.has(address)) {
            this.writeU64(address);
            return;
        }
        this._objectMap.set(address, value);
        this.writeU64(address);
        writeObj(this, value);
    }
    writeArray(arr, writer) {
        this.writeSz(arr.length);
        for (let i = 0; i < arr.length; i++)
            writer(arr[i]);
    }
    writeSet(set, writer) {
        this.writeSz(set.size);
        set.forEach(v => writer(v));
    }
    writeMap(map, keyWriter, valueWriter) {
        this.writeSz(map.size);
        map.forEach((v, k) => { keyWriter(k); valueWriter(v); });
    }
    writeSpArray(arr, writer) {
        for (let i = 0; i < arr.size; i++)
            writer(arr.at(i));
    }
    writeBitset(bits) {
        this.writeU32(bits.length);
        const byteLen = Math.ceil(bits.length / 8);
        const bytes = new Uint8Array(byteLen);
        for (let i = 0; i < bits.length; i++) {
            if (bits[i])
                bytes[Math.floor(i / 8)] |= (1 << (i % 8));
        }
        this._writeBytes(bytes);
    }
    writeOptional(value, writer) {
        this.writeBl(value !== null);
        if (value !== null)
            writer(value);
    }
    writeVariant(value, typeIndex, writers) {
        this.writeU32(typeIndex);
        writers[typeIndex](value);
    }
    writeTime(date) {
        const totalMs = date.getTime();
        const sec = BigInt(Math.floor(totalMs / 1000));
        const attoSec = BigInt((totalMs % 1000) * 1000000000000000);
        this.writeI64(sec);
        this.writeI64(attoSec);
    }
    toBytes() {
        const totalLength = this._buffers.reduce((sum, buf) => sum + buf.byteLength, 0);
        const result = new Uint8Array(totalLength);
        let offset = 0;
        for (const buf of this._buffers) {
            result.set(new Uint8Array(buf), offset);
            offset += buf.byteLength;
        }
        return result;
    }
}
export var E_StreamPunkType;
(function (E_StreamPunkType) {
    E_StreamPunkType[E_StreamPunkType["Base"] = 46] = "Base";
    E_StreamPunkType[E_StreamPunkType["Vec2"] = 47] = "Vec2";
    E_StreamPunkType[E_StreamPunkType["PlayerInput"] = 48] = "PlayerInput";
    E_StreamPunkType[E_StreamPunkType["Bullet"] = 49] = "Bullet";
    E_StreamPunkType[E_StreamPunkType["PlayerState"] = 50] = "PlayerState";
    E_StreamPunkType[E_StreamPunkType["GameState"] = 51] = "GameState";
})(E_StreamPunkType || (E_StreamPunkType = {}));
export class Base {
    from(i) { void i; return this; }
    to(o) { void o; return this; }
}
Base.typeID = E_StreamPunkType.Base;
const __typeFactory = new Map();
__typeFactory.set(E_StreamPunkType.Vec2, () => new Vec2());
__typeFactory.set(E_StreamPunkType.PlayerInput, () => new PlayerInput());
__typeFactory.set(E_StreamPunkType.Bullet, () => new Bullet());
__typeFactory.set(E_StreamPunkType.PlayerState, () => new PlayerState());
__typeFactory.set(E_StreamPunkType.GameState, () => new GameState());
export function readObj(i) {
    const id = i.readU32();
    const factory = __typeFactory.get(id);
    if (!factory)
        return null;
    const obj = factory();
    obj.from(i);
    return obj;
}
export function writeObj(o, obj) {
    o.writeU32(obj.constructor.typeID);
    obj.to(o);
}
export class Vec2 extends Base {
    constructor() {
        super();
        this.x = 0.0;
        this.y = 0.0;
    }
    from(i) {
        super.from(i);
        this.x = i.readF64();
        this.y = i.readF64();
        return this;
    }
    to(o) {
        super.to(o);
        o.writeF64(this.x);
        o.writeF64(this.y);
        return this;
    }
}
Vec2.typeID = E_StreamPunkType.Vec2;
export class PlayerInput extends Base {
    constructor() {
        super();
        this.up = false;
        this.down = false;
        this.left = false;
        this.right = false;
        this.fire = false;
    }
    from(i) {
        super.from(i);
        this.up = i.readBl();
        this.down = i.readBl();
        this.left = i.readBl();
        this.right = i.readBl();
        this.fire = i.readBl();
        return this;
    }
    to(o) {
        super.to(o);
        o.writeBl(this.up);
        o.writeBl(this.down);
        o.writeBl(this.left);
        o.writeBl(this.right);
        o.writeBl(this.fire);
        return this;
    }
}
PlayerInput.typeID = E_StreamPunkType.PlayerInput;
export class Bullet extends Base {
    constructor() {
        super();
        this.x = 0.0;
        this.y = 0.0;
        this.vx = 0.0;
        this.vy = 0.0;
        this.ownerId = 0;
    }
    from(i) {
        super.from(i);
        this.x = i.readF64();
        this.y = i.readF64();
        this.vx = i.readF64();
        this.vy = i.readF64();
        this.ownerId = i.readI32();
        return this;
    }
    to(o) {
        super.to(o);
        o.writeF64(this.x);
        o.writeF64(this.y);
        o.writeF64(this.vx);
        o.writeF64(this.vy);
        o.writeI32(this.ownerId);
        return this;
    }
}
Bullet.typeID = E_StreamPunkType.Bullet;
export class PlayerState extends Base {
    constructor() {
        super();
        this.id = 0;
        this.x = 0.0;
        this.y = 0.0;
        this.rotation = 0.0;
        this.hp = 0;
    }
    from(i) {
        super.from(i);
        this.id = i.readI32();
        this.x = i.readF64();
        this.y = i.readF64();
        this.rotation = i.readF64();
        this.hp = i.readI32();
        return this;
    }
    to(o) {
        super.to(o);
        o.writeI32(this.id);
        o.writeF64(this.x);
        o.writeF64(this.y);
        o.writeF64(this.rotation);
        o.writeI32(this.hp);
        return this;
    }
}
PlayerState.typeID = E_StreamPunkType.PlayerState;
export class GameState extends Base {
    constructor() {
        super();
        this.players = [];
        this.bullets = [];
    }
    from(i) {
        super.from(i);
        this.players = i.readArray(() => new PlayerState().from(i));
        this.bullets = i.readArray(() => new Bullet().from(i));
        return this;
    }
    to(o) {
        super.to(o);
        o.writeArray(this.players, (v) => { writeObj(o, v); });
        o.writeArray(this.bullets, (v) => { writeObj(o, v); });
        return this;
    }
}
GameState.typeID = E_StreamPunkType.GameState;
