/* eslint-disable @typescript-eslint/no-array-constructor */

export type SpStdArray<T> = T[];

export class SpRef<T> {
  value: T | null;
  address: bigint;
  constructor(initialValue: T | null = null, address: bigint = 0n) {
    this.value = initialValue;
    this.address = address;
  }
}

export class SpArray<T> {
  private _data: T[];
  size: number;
  constructor(size: number) {
    this.size = size;
    this._data = new Array(size);
  }
  at(index: number): T {
    if (index < 0 || index >= this.size) throw new RangeError("SpArray index out of bounds");
    return this._data[index];
  }
  set(index: number, value: T): void {
    if (index < 0 || index >= this.size) throw new RangeError("SpArray index out of bounds");
    this._data[index] = value;
  }
}

export class SpVariant<Types extends any[]> {
  value: Types[number];
  typeIndex: number;
  constructor(value: Types[number], typeIndex: number) {
    this.value = value;
    this.typeIndex = typeIndex;
  }
}

export class I {
  view: DataView;
  offset: number;
  objectMap: Map<bigint, unknown>;
  decoder: TextDecoder;
  decoder16: TextDecoder;

  constructor(buffer: ArrayBuffer, initialOffset: number = 0) {
    this.view = new DataView(buffer);
    this.offset = initialOffset;
    this.objectMap = new Map();
    this.decoder = new TextDecoder("utf-8");
    this.decoder16 = new TextDecoder("utf-16le");
  }

  hasMoreData(): boolean { return this.offset < this.view.byteLength; }
  skip(bytes: number): void { this.offset += bytes; }

  readU8(): number  { const v = this.view.getUint8(this.offset);    this.offset += 1; return v; }
  readU16(): number { const v = this.view.getUint16(this.offset, true); this.offset += 2; return v; }
  readU32(): number { const v = this.view.getUint32(this.offset, true); this.offset += 4; return v; }
  readU64(): bigint { const v = this.view.getBigUint64(this.offset, true); this.offset += 8; return v; }
  readI8(): number  { const v = this.view.getInt8(this.offset);    this.offset += 1; return v; }
  readI16(): number { const v = this.view.getInt16(this.offset, true); this.offset += 2; return v; }
  readI32(): number { const v = this.view.getInt32(this.offset, true); this.offset += 4; return v; }
  readI64(): bigint { const v = this.view.getBigInt64(this.offset, true); this.offset += 8; return v; }
  readF32(): number { const v = this.view.getFloat32(this.offset, true); this.offset += 4; return v; }
  readF64(): number { const v = this.view.getFloat64(this.offset, true); this.offset += 8; return v; }

  readCh(): string  { const v = String.fromCharCode(this.view.getUint8(this.offset)); this.offset += 1; return v; }
  readCh8(): string { return this.readCh(); }
  readCh16(): string { const v = String.fromCharCode(this.view.getUint16(this.offset, true)); this.offset += 2; return v; }
  readCh32(): string {
    const cp = this.view.getUint32(this.offset, true);
    this.offset += 4;
    return (cp >= 0 && cp <= 0x10FFFF) ? String.fromCodePoint(cp) : "";
  }

  readBl(): boolean  { const v = this.view.getUint8(this.offset) !== 0; this.offset += 1; return v; }
  readSz(): number   { return this.readU32(); }

  readString(): string {
    const len = Number(this.readSz());
    if (len === 0) return "";
    const v = this.decoder.decode(new Uint8Array(this.view.buffer, this.offset, len));
    this.offset += len;
    return v;
  }

  readBytes(): Uint8Array {
    const len = Number(this.readSz());
    if (len === 0) return new Uint8Array(0);
    const v = new Uint8Array(this.view.buffer.slice(this.offset, this.offset + len));
    this.offset += len;
    return v;
  }

  readU8String(): string {
    const len = Number(this.readSz());
    const v = this.decoder.decode(new Uint8Array(this.view.buffer, this.offset, len));
    this.offset += len;
    return v;
  }

  readU16String(): string {
    const len = Number(this.readSz());
    const byteLen = len * 2;
    const v = this.decoder16.decode(new Uint8Array(this.view.buffer, this.offset, byteLen));
    this.offset += byteLen;
    return v;
  }

  readU32String(): string {
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

  readPtrWithTypeID<T>(readObjFn: (i: I) => T | null): SpRef<T | null> {
    const address = this.readU64();
    if (address === 0n) return new SpRef<T | null>(null, 0n);
    if (this.objectMap.has(address)) return this.objectMap.get(address) as SpRef<T | null>;
    const ref = new SpRef<T | null>(null, address);
    this.objectMap.set(address, ref);
    ref.value = readObjFn(this);
    return ref;
  }

  readPtr<T>(reader: () => T): SpRef<T> {
    const address = this.readU64();
    if (address === 0n) return new SpRef<T>(null, 0n);
    if (this.objectMap.has(address)) return this.objectMap.get(address) as SpRef<T>;
    const value = reader();
    const ref = new SpRef<T>(value, address);
    this.objectMap.set(address, ref);
    return ref;
  }

  readArray<T>(reader: () => T): T[] {
    const size = Number(this.readSz());
    const arr: T[] = new Array(size);
    for (let i = 0; i < size; i++) arr[i] = reader();
    return arr;
  }

  readSet<T>(reader: () => T): Set<T> {
    return new Set(this.readArray(reader));
  }

  readMap<K, V>(keyReader: () => K, valueReader: () => V): Map<K, V> {
    const size = Number(this.readSz());
    const map = new Map<K, V>();
    for (let i = 0; i < size; i++) {
      map.set(keyReader(), valueReader());
    }
    return map;
  }

  readSpArray<T>(size: number, reader: () => T): SpArray<T> {
    const arr = new SpArray<T>(size);
    for (let i = 0; i < size; i++) arr.set(i, reader());
    return arr;
  }

  readBitset(): boolean[] {
    const size = this.readU32();
    const byteLen = Math.ceil(size / 8);
    const bytes = new Uint8Array(this.view.buffer, this.offset, byteLen);
    this.offset += byteLen;
    return Array.from({ length: size }, (_, i) => {
      return ((bytes[Math.floor(i / 8)] >> (i % 8)) & 1) === 1;
    });
  }

  readOptional<T>(reader: () => T): T | null {
    return this.readBl() ? reader() : null;
  }

  readVariant(readers: Array<() => any>): SpVariant<any[]> {
    const index = this.readU32();
    if (index >= readers.length) throw new Error(`Variant index ${index} out of range`);
    return new SpVariant(readers[index](), index);
  }

  readTime(): Date {
    const sec = this.readI64();
    const attoSec = this.readI64();
    return new Date(Number(sec) * 1000 + Number(attoSec / 1000000000000000n));
  }
}

export class O {
  private _buffers: ArrayBuffer[] = [];
  private _objectMap: Map<bigint, unknown> = new Map();
  private _encoder: TextEncoder = new TextEncoder();
  private _nextAddr: bigint = 1n;

  private _pushBuffer(buf: ArrayBuffer): void {
    this._buffers.push(buf);
  }

  private _pushView(fn: (dv: DataView) => void, byteLen: number): void {
    const buf = new ArrayBuffer(byteLen);
    fn(new DataView(buf));
    this._pushBuffer(buf);
  }

  writeU8(v: number): void  { this._pushView(dv => dv.setUint8(0, v), 1); }
  writeU16(v: number): void { this._pushView(dv => dv.setUint16(0, v, true), 2); }
  writeU32(v: number): void { this._pushView(dv => dv.setUint32(0, v, true), 4); }
  writeU64(v: bigint): void { this._pushView(dv => dv.setBigUint64(0, v, true), 8); }
  writeI8(v: number): void  { this._pushView(dv => dv.setInt8(0, v), 1); }
  writeI16(v: number): void { this._pushView(dv => dv.setInt16(0, v, true), 2); }
  writeI32(v: number): void { this._pushView(dv => dv.setInt32(0, v, true), 4); }
  writeI64(v: bigint): void { this._pushView(dv => dv.setBigInt64(0, v, true), 8); }
  writeF32(v: number): void { this._pushView(dv => dv.setFloat32(0, v, true), 4); }
  writeF64(v: number): void { this._pushView(dv => dv.setFloat64(0, v, true), 8); }

  writeCh(v: string): void  { this.writeU16(v.length > 0 ? v.charCodeAt(0) : 0); }
  writeCh8(v: string): void { this.writeU8(v.length > 0 ? v.charCodeAt(0) : 0); }
  writeCh16(v: string): void { this.writeU16(v.length > 0 ? v.charCodeAt(0) : 0); }
  writeCh32(v: string): void { this.writeU32(v.length > 0 ? (v.codePointAt(0) ?? 0) : 0); }

  writeBl(v: boolean): void { this.writeU8(v ? 1 : 0); }
  writeSz(v: number): void  { this.writeU32(v); }

  private _writeBytes(data: Uint8Array): void {
    const buf = new ArrayBuffer(data.length);
    new Uint8Array(buf).set(data);
    this._buffers.push(buf);
  }

  writeString(v: string): void {
    const encoded = this._encoder.encode(v);
    this.writeSz(encoded.length);
    this._writeBytes(encoded);
  }

  writeBytes(bytes: Uint8Array): void {
    this.writeSz(bytes.length);
    this._writeBytes(bytes);
  }

  writeU8String(v: string): void { this.writeString(v); }
  writeU16String(v: string): void {
    this.writeSz(v.length);
    const bytes = new Uint8Array(v.length * 2);
    for (let i = 0; i < v.length; i++) {
      const cu = v.charCodeAt(i);
      bytes[i * 2] = cu & 0xFF;
      bytes[i * 2 + 1] = (cu >> 8) & 0xFF;
    }
    this._writeBytes(bytes);
  }

  writeU32String(v: string): void {
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
      if (cp > 0xFFFF) i++;
    }
    this._writeBytes(bytes);
  }

  writePtr<T>(value: T | null, address: bigint, writer: (v: T) => void): void {
    if (value === null) { this.writeU64(0n); return; }
    if (address === 0n) {
      address = this._nextAddr++;
    }
    if (this._objectMap.has(address)) { this.writeU64(address); return; }
    this._objectMap.set(address, value);
    this.writeU64(address);
    writer(value);
  }

  writePtrWithTypeID<T>(value: T | null, address: bigint, writeObjFn: (o: O, v: T) => void): void {
    if (value === null) { this.writeU64(0n); return; }
    if (address === 0n) {
      address = this._nextAddr++;
    }
    if (this._objectMap.has(address)) { this.writeU64(address); return; }
    this._objectMap.set(address, value);
    this.writeU64(address);
    writeObjFn(this, value);
  }

  writeArray<T>(arr: T[], writer: (v: T) => void): void {
    this.writeSz(arr.length);
    for (let i = 0; i < arr.length; i++) writer(arr[i]);
  }

  writeSet<T>(set: Set<T>, writer: (v: T) => void): void {
    this.writeSz(set.size);
    set.forEach(v => writer(v));
  }

  writeMap<K, V>(map: Map<K, V>, keyWriter: (k: K) => void, valueWriter: (v: V) => void): void {
    this.writeSz(map.size);
    map.forEach((v, k) => { keyWriter(k); valueWriter(v); });
  }

  writeSpArray<T>(arr: SpArray<T>, writer: (v: T) => void): void {
    for (let i = 0; i < arr.size; i++) writer(arr.at(i));
  }

  writeBitset(bits: boolean[]): void {
    this.writeU32(bits.length);
    const byteLen = Math.ceil(bits.length / 8);
    const bytes = new Uint8Array(byteLen);
    for (let i = 0; i < bits.length; i++) {
      if (bits[i]) bytes[Math.floor(i / 8)] |= (1 << (i % 8));
    }
    this._writeBytes(bytes);
  }

  writeOptional<T>(value: T | null, writer: (v: T) => void): void {
    this.writeBl(value !== null);
    if (value !== null) writer(value);
  }

  writeVariant(value: any, typeIndex: number, writers: Array<(v: any) => void>): void {
    this.writeU32(typeIndex);
    writers[typeIndex](value);
  }

  writeTime(date: Date): void {
    const totalMs = date.getTime();
    const sec = BigInt(Math.floor(totalMs / 1000));
    const attoSec = BigInt((totalMs % 1000) * 1000000000000000);
    this.writeI64(sec);
    this.writeI64(attoSec);
  }

  toBytes(): Uint8Array {
    const totalLength = this._buffers.reduce((sum, buf) => sum + buf.byteLength, 0);
    const result = new Uint8Array(totalLength);
    let offset = 0;
    for (const buf of this._buffers) {
      result.set(new Uint8Array(buf), offset);
      offset += buf.byteLength;
    }
    return result;
  }
}export enum E_StreamPunkType {
  Base = 46,
  TextOp = 47,
  CursorInfo = 48,
  JoinRequest = 49,
  JoinResponse = 50,
  UserListUpdate = 51,
}

export class Base {
  static readonly typeID: E_StreamPunkType = E_StreamPunkType.Base;
  from(i: I): this { void i; return this; }
  to(o: O): this  { void o; return this; }
}

const __typeFactory = new Map<number, () => Base>();
__typeFactory.set(E_StreamPunkType.TextOp, () => new TextOp());
__typeFactory.set(E_StreamPunkType.CursorInfo, () => new CursorInfo());
__typeFactory.set(E_StreamPunkType.JoinRequest, () => new JoinRequest());
__typeFactory.set(E_StreamPunkType.JoinResponse, () => new JoinResponse());
__typeFactory.set(E_StreamPunkType.UserListUpdate, () => new UserListUpdate());

export function readObj(i: I): Base | null {
  const id = i.readU32();
  const factory = __typeFactory.get(id);
  if (!factory) return null;
  const obj = factory();
  obj.from(i);
  return obj;
}

export function writeObj(o: O, obj: Base): void {
  o.writeU32((obj.constructor as typeof Base).typeID);
  obj.to(o);
}

export class TextOp extends Base {
    static readonly typeID = E_StreamPunkType.TextOp;

    opType: number;
    position: number;
    text: string;
    userId: number;
    version: number;

    constructor() {
        super();
        this.opType = 0;
        this.position = 0;
        this.text = "";
        this.userId = 0;
        this.version = 0;
    }

    from(i: I): this {
        super.from(i);
        this.opType = i.readI32();
        this.position = i.readI32();
        this.text = i.readString();
        this.userId = i.readI32();
        this.version = i.readI32();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.opType);
        o.writeI32(this.position);
        o.writeString(this.text);
        o.writeI32(this.userId);
        o.writeI32(this.version);
        return this;
    }
}
export class CursorInfo extends Base {
    static readonly typeID = E_StreamPunkType.CursorInfo;

    userId: number;
    userName: string;
    position: number;
    color: string;

    constructor() {
        super();
        this.userId = 0;
        this.userName = "";
        this.position = 0;
        this.color = "";
    }

    from(i: I): this {
        super.from(i);
        this.userId = i.readI32();
        this.userName = i.readString();
        this.position = i.readI32();
        this.color = i.readString();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.userId);
        o.writeString(this.userName);
        o.writeI32(this.position);
        o.writeString(this.color);
        return this;
    }
}
export class JoinRequest extends Base {
    static readonly typeID = E_StreamPunkType.JoinRequest;

    userName: string;

    constructor() {
        super();
        this.userName = "";
    }

    from(i: I): this {
        super.from(i);
        this.userName = i.readString();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeString(this.userName);
        return this;
    }
}
export class JoinResponse extends Base {
    static readonly typeID = E_StreamPunkType.JoinResponse;

    userId: number;
    document: string;
    users: Array<CursorInfo>;

    constructor() {
        super();
        this.userId = 0;
        this.document = "";
        this.users = [];
    }

    from(i: I): this {
        super.from(i);
        this.userId = i.readI32();
        this.document = i.readString();
        this.users = i.readArray(() => new CursorInfo().from(i));
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.userId);
        o.writeString(this.document);
        o.writeArray(this.users, (v) => { writeObj(o, v) });
        return this;
    }
}
export class UserListUpdate extends Base {
    static readonly typeID = E_StreamPunkType.UserListUpdate;

    users: Array<CursorInfo>;

    constructor() {
        super();
        this.users = [];
    }

    from(i: I): this {
        super.from(i);
        this.users = i.readArray(() => new CursorInfo().from(i));
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeArray(this.users, (v) => { writeObj(o, v) });
        return this;
    }
}

// ============================================================================
// SPOI 指令绑定（与 include/stream-punk/StreamPunkSPOI.hpp 的序列化格式对齐）
// ============================================================================

// 操作码（C++ 端 Xt_SPOI_ops 枚举值）
export const SPOI_OP = {
  e_nav: 0x00, e_idx: 0x01, e_deref: 0x02, e_unwrap: 0x03,
  e_set: 0x04, e_add: 0x05, e_append: 0x06, e_remove: 0x07,
  e_insert: 0x08, e_replace: 0x09, e_reset: 0x0A, e_setnull: 0x0B,
  e_move: 0x23,
} as const;

// 单条 SPOI 指令：op(u8) + path(vector<u32>) + typeDesc(vector<Sz>) + operand(vector<u8>)
export class SpoiInstruction {
  op: number;
  path: number[];
  typeDesc: number[];
  operand: Uint8Array;

  constructor(op: number = 0, path: number[] = [], typeDesc: number[] = [], operand: Uint8Array = new Uint8Array(0)) {
    this.op = op;
    this.path = path;
    this.typeDesc = typeDesc;
    this.operand = operand;
  }

  from(i: I): this {
    this.op = i.readU8();
    this.path = i.readArray(() => i.readU32());
    this.typeDesc = i.readArray(() => i.readU32());
    this.operand = i.readBytes();
    return this;
  }

  to(o: O): this {
    o.writeU8(this.op);
    o.writeArray(this.path, (v) => o.writeU32(v));
    o.writeArray(this.typeDesc, (v) => o.writeU32(v));
    o.writeBytes(this.operand);
    return this;
  }
}

// 指令流：vector<SpoiInstruction>
export class SpoiStream {
  instructions: SpoiInstruction[];

  constructor(instructions: SpoiInstruction[] = []) {
    this.instructions = instructions;
  }

  from(i: I): this {
    this.instructions = i.readArray(() => new SpoiInstruction().from(i));
    return this;
  }

  to(o: O): this {
    o.writeArray(this.instructions, (v) => v.to(o));
    return this;
  }
}

// 编码辅助（operand 用 SP 格式：u32 裸 4 字节 LE，string 为 u32 长度 + UTF-8 字节）
function encodeU32(v: number): Uint8Array {
  const out = new Uint8Array(4);
  new DataView(out.buffer).setUint32(0, v, true);
  return out;
}

function encodeStringBytes(s: string): Uint8Array {
  const enc = new TextEncoder();
  const bytes = enc.encode(s);
  const out = new Uint8Array(4 + bytes.length);
  new DataView(out.buffer).setUint32(0, bytes.length, true);
  out.set(bytes, 4);
  return out;
}

function decodeU32(bytes: Uint8Array, off: number): number {
  return new DataView(bytes.buffer, bytes.byteOffset + off, 4).getUint32(0, true);
}

// 生成字符串增量指令（本地编辑 → SPOI）
export function makeSpoiOps(oldText: string, newText: string): SpoiInstruction[] {
  // 最长公共前缀 / 后缀剥离，中间段 = 本次编辑
  let p = 0;
  while (p < oldText.length && p < newText.length && oldText[p] === newText[p]) p++;
  let s = 0;
  while (s < oldText.length - p && s < newText.length - p &&
         oldText[oldText.length - 1 - s] === newText[newText.length - 1 - s]) s++;

  const oldMid = oldText.slice(p, oldText.length - s);
  const newMid = newText.slice(p, newText.length - s);

  const ops: SpoiInstruction[] = [];
  // 先删后插（若同时存在，插入位置仍为 p，与 C++ 顺序应用语义一致）
  if (oldMid.length > 0) {
    ops.push(new SpoiInstruction(SPOI_OP.e_remove, [0, p], [], encodeU32(oldMid.length)));
  }
  if (newMid.length > 0) {
    ops.push(new SpoiInstruction(SPOI_OP.e_insert, [0, p], [], encodeStringBytes(newMid)));
  }
  return ops;
}

// 把 SPOI 指令流应用到本地字符串（与 C++ _applyContainerOp 的 is_string_v 分支 clamp 语义一致）
export function applySpoiOps(content: string, ops: SpoiInstruction[]): string {
  let s = content;
  const dec = new TextDecoder();
  for (const op of ops) {
    if (op.path.length === 0 || op.path[0] !== 0) continue; // 仅处理 content 字段（字段索引 0）
    const path = op.path;
    const a0 = path.length > 1 ? path[path.length - 1] : 0;

    switch (op.op) {
      case SPOI_OP.e_append: {
        // operand = string chunk
        const chunk = dec.decode(op.operand.subarray(4));
        s += chunk;
        break;
      }
      case SPOI_OP.e_insert: {
        const chunk = dec.decode(op.operand.subarray(4));
        const pos = Math.min(a0, s.length);
        s = s.slice(0, pos) + chunk + s.slice(pos);
        break;
      }
      case SPOI_OP.e_remove: {
        // operand = u32 len（可为空 → 默认 1）
        let len = 1;
        if (op.operand.length >= 4) len = decodeU32(op.operand, 0);
        const pos = Math.min(a0, s.length);
        len = Math.min(len, s.length - pos);
        s = s.slice(0, pos) + s.slice(pos + len);
        break;
      }
      case SPOI_OP.e_replace: {
        // operand = u32 len + string chunk
        const len = decodeU32(op.operand, 0);
        const chunk = dec.decode(op.operand.subarray(8));
        const pos = Math.min(a0, s.length);
        const effLen = Math.min(len, s.length - pos);
        s = s.slice(0, pos) + chunk + s.slice(pos + effLen);
        break;
      }
      case SPOI_OP.e_move: {
        // path = [fieldIdx, from, len, to]
        if (path.length < 4) break;
        const to = path[path.length - 1];
        const len = path[path.length - 2];
        const from = a0;
        const f = Math.min(from, s.length);
        let l = Math.min(len, s.length - f);
        const chunk = s.slice(f, f + l);
        s = s.slice(0, f) + s.slice(f + l);
        const t = Math.min(to, s.length); // 以擦除后的字符串为基准
        s = s.slice(0, t) + chunk + s.slice(t);
        break;
      }
      default:
        break; // 其他操作（set/add 等）本示例不涉及
    }
  }
  return s;
}

