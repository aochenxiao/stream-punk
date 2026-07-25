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

  readPtrWithTypeID(): SpRef<any> {
    const address = this.readU64();
    if (address === 0n) return new SpRef(null, 0n);
    if (this.objectMap.has(address)) return this.objectMap.get(address) as SpRef<any>;
    const ref = new SpRef(null, address);
    this.objectMap.set(address, ref);
    ref.value = readObj(this);
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
  private _data: number[] = [];
  private _objectMap: Map<bigint, unknown> = new Map();
  private _encoder: TextEncoder = new TextEncoder();
  private _nextAddr: bigint = 1n;

  private _pushBuffer(buf: ArrayBuffer): void {
    const bytes = new Uint8Array(buf);
    for (let i = 0; i < bytes.length; i++) this._data.push(bytes[i]);
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
    for (let i = 0; i < data.length; i++) this._data.push(data[i]);
  }

  writeString(v: string): void {
    const encoded = this._encoder.encode(v);
    this.writeSz(encoded.length);
    this._writeBytes(encoded);
  }

  writeU8String(v: string): void { this.writeString(v); }
  writeU16String(v: string): void {
    this.writeSz(v.length * 2);
    for (let i = 0; i < v.length; i++) {
      const cu = v.charCodeAt(i);
      this._data.push(cu & 0xFF);
      this._data.push((cu >> 8) & 0xFF);
    }
  }

  writeU32String(v: string): void {
    const len = v.length;
    this.writeSz(len);
    for (let i = 0; i < len; i++) {
      const cp = v.codePointAt(i) ?? 0;
      this._data.push(cp & 0xFF);
      this._data.push((cp >> 8) & 0xFF);
      this._data.push((cp >> 16) & 0xFF);
      this._data.push((cp >> 24) & 0xFF);
      if (cp > 0xFFFF) i++;
    }
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

  writePtrWithTypeID(value: any): void {
    if (value === null) { this.writeU64(0n); return; }
    const address = this._nextAddr++;
    if (this._objectMap.has(address)) { this.writeU64(address); return; }
    this._objectMap.set(address, value);
    this.writeU64(address);
    writeObj(this, value);
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

  toArrayBuffer(): ArrayBuffer {
    return new Uint8Array(this._data).buffer;
  }
}

export enum E_StreamPunkType {
  e_unknowType = 0,
  e_op_position = 1,
  e_op_select = 2,
  e_op_deptr = 3,
  e_op_ranges_insert_one = 4,
  bg = 5,
  ed = 6,
  vector = 7,
  array = 8,
  string = 9,
  bitset = 10,
  deque = 11,
  list = 12,
  flist = 13,
  set = 14,
  uset = 15,
  map = 16,
  umap = 17,
  sptr = 18,
  wptr = 19,
  uptr = 20,
  opt = 21,
  path = 22,
  atomic = 23,
  variant = 24,
  tuple = 25,
  u8 = 26,
  u16 = 27,
  u32 = 28,
  u64 = 29,
  i8 = 30,
  i16 = 31,
  i32 = 32,
  i64 = 33,
  f32 = 34,
  f64 = 35,
  ch = 36,
  ch8 = 37,
  ch16 = 38,
  ch32 = 39,
  bl = 40,
  ptr = 41,
  voidPtr = 42,
  cst = 43,
  dur = 44,
  timepoint = 45,
  Base = 46,
  AllBasicTypes = 47,
  TemplateContainer = 48,
  PointerContainer = 49,
  ComplexTemplateNesting = 50,
  ComprehensiveContainer = 51,
  Child = 52,
  SelfReferential = 53,
  TemplateAndPointer = 54,
  InheritanceAndSelfReference = 55,
  MegaComplexClass = 56,
  SuperComplexContainer = 57,
  Test = 58,
  MQTT = 59,
  PointerDemo = 60,
  ContainerDemo = 61,
  NetworkSystem = 62,
  Device = 63,
  NetworkDevice = 64,
  Sensor = 65,
  TemperatureSensor = 66,
  SmartHomeSystem = 67,
  MultiLevelContainer = 68,
  SptrTest = 69,
  MousePosition = 70,
  e_customType = 71,
}

export class Base {
  static readonly typeID = E_StreamPunkType.Base;
  from(i: I): this { void i; return this; }
  to(o: O): this  { void o; return this; }
}

const __typeFactory = new Map<number, () => Base>();
__typeFactory.set(E_StreamPunkType.AllBasicTypes, () => new AllBasicTypes());
__typeFactory.set(E_StreamPunkType.TemplateContainer, () => new TemplateContainer());
__typeFactory.set(E_StreamPunkType.PointerContainer, () => new PointerContainer());
__typeFactory.set(E_StreamPunkType.ComplexTemplateNesting, () => new ComplexTemplateNesting());
__typeFactory.set(E_StreamPunkType.ComprehensiveContainer, () => new ComprehensiveContainer());
__typeFactory.set(E_StreamPunkType.Child, () => new Child());
__typeFactory.set(E_StreamPunkType.SelfReferential, () => new SelfReferential());
__typeFactory.set(E_StreamPunkType.TemplateAndPointer, () => new TemplateAndPointer());
__typeFactory.set(E_StreamPunkType.InheritanceAndSelfReference, () => new InheritanceAndSelfReference());
__typeFactory.set(E_StreamPunkType.MegaComplexClass, () => new MegaComplexClass());
__typeFactory.set(E_StreamPunkType.SuperComplexContainer, () => new SuperComplexContainer());
__typeFactory.set(E_StreamPunkType.Test, () => new Test());
__typeFactory.set(E_StreamPunkType.MQTT, () => new MQTT());
__typeFactory.set(E_StreamPunkType.PointerDemo, () => new PointerDemo());
__typeFactory.set(E_StreamPunkType.ContainerDemo, () => new ContainerDemo());
__typeFactory.set(E_StreamPunkType.NetworkSystem, () => new NetworkSystem());
__typeFactory.set(E_StreamPunkType.Device, () => new Device());
__typeFactory.set(E_StreamPunkType.NetworkDevice, () => new NetworkDevice());
__typeFactory.set(E_StreamPunkType.Sensor, () => new Sensor());
__typeFactory.set(E_StreamPunkType.TemperatureSensor, () => new TemperatureSensor());
__typeFactory.set(E_StreamPunkType.SmartHomeSystem, () => new SmartHomeSystem());
__typeFactory.set(E_StreamPunkType.MultiLevelContainer, () => new MultiLevelContainer());
__typeFactory.set(E_StreamPunkType.SptrTest, () => new SptrTest());
__typeFactory.set(E_StreamPunkType.MousePosition, () => new MousePosition());

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

export class AllBasicTypes extends Base {
  static readonly typeID = E_StreamPunkType.AllBasicTypes;
  b: boolean = false;
  i8_v: number = 0;
  u8_v: number = 0;
  i16_v: number = 0;
  u16_v: number = 0;
  i32_v: number = 0;
  u32_v: number = 0;
  i64_v: bigint = 0n;
  u64_v: bigint = 0n;
  f: number = 0.0;
  d: number = 0.0;
  c: string = "";
  c8: string = "";
  c16: string = "";
  c32: string = "";

  from(i: I): this {
    super.from(i);
    this.b = i.readBl();
    this.i8_v = i.readI8();
    this.u8_v = i.readU8();
    this.i16_v = i.readI16();
    this.u16_v = i.readU16();
    this.i32_v = i.readI32();
    this.u32_v = i.readU32();
    this.i64_v = i.readI64();
    this.u64_v = i.readU64();
    this.f = i.readF32();
    this.d = i.readF64();
    this.c = i.readCh();
    this.c8 = i.readCh8();
    this.c16 = i.readCh16();
    this.c32 = i.readCh32();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeBl(this.b);
    o.writeI8(this.i8_v);
    o.writeU8(this.u8_v);
    o.writeI16(this.i16_v);
    o.writeU16(this.u16_v);
    o.writeI32(this.i32_v);
    o.writeU32(this.u32_v);
    o.writeI64(this.i64_v);
    o.writeU64(this.u64_v);
    o.writeF32(this.f);
    o.writeF64(this.d);
    o.writeCh(this.c);
    o.writeCh8(this.c8);
    o.writeCh16(this.c16);
    o.writeCh32(this.c32);
    return this;
  }
}
export class TemplateContainer extends Base {
  static readonly typeID = E_StreamPunkType.TemplateContainer;
  s: string = "";
  u8s: string = "";
  vec: Array<number> = [];
  deq: Array<number> = [];
  lst: Array<string> = [];
  shortForwardList: Array<number> = [];
  uintSet: Set<number> = new Set();
  stringHashSet: Set<string> = new Set();
  intStringMap: Map<number, string> = new Map();
  stringFloatHashMap: Map<string, number> = new Map();

  from(i: I): this {
    super.from(i);
    this.s = i.readString();
    this.u8s = i.readU8String();
    this.vec = i.readArray(() => i.readI32());
    this.deq = i.readArray(() => i.readF64());
    this.lst = i.readArray(() => i.readString());
    this.shortForwardList = i.readArray(() => i.readU16());
    this.uintSet = i.readSet(() => i.readU32());
    this.stringHashSet = i.readSet(() => i.readString());
    this.intStringMap = i.readMap(() => i.readI32(), () => i.readString());
    this.stringFloatHashMap = i.readMap(() => i.readString(), () => i.readF32());
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeString(this.s);
    o.writeU8String(this.u8s);
    o.writeArray(this.vec, (v) => o.writeI32(v));
    o.writeArray(this.deq, (v) => o.writeF64(v));
    o.writeArray(this.lst, (v) => o.writeString(v));
    o.writeArray(this.shortForwardList, (v) => o.writeU16(v));
    o.writeSet(this.uintSet, (v) => o.writeU32(v));
    o.writeSet(this.stringHashSet, (v) => o.writeString(v));
    o.writeMap(this.intStringMap, (k) => o.writeI32(k), (v) => o.writeString(v));
    o.writeMap(this.stringFloatHashMap, (k) => o.writeString(k), (v) => o.writeF32(v));
    return this;
  }
}
export class PointerContainer extends Base {
  static readonly typeID = E_StreamPunkType.PointerContainer;
  raw_ptr: SpRef<number | null> = new SpRef(null);
  shared_ptr_int: SpRef<number | null> = new SpRef(null);
  unique_ptr_int: SpRef<number | null> = new SpRef(null);

  from(i: I): this {
    super.from(i);
    this.raw_ptr = i.readPtr(() => i.readI32());
    this.shared_ptr_int = i.readPtr(() => i.readI32());
    this.unique_ptr_int = i.readPtr(() => i.readI32());
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writePtr(this.raw_ptr.value, this.raw_ptr.address, (v) => o.writeI32(v));
    o.writePtr(this.shared_ptr_int.value, this.shared_ptr_int.address, (v) => o.writeI32(v));
    o.writePtr(this.unique_ptr_int.value, this.unique_ptr_int.address, (v) => o.writeI32(v));
    return this;
  }
}
export class ComplexTemplateNesting extends Base {
  static readonly typeID = E_StreamPunkType.ComplexTemplateNesting;
  nestedVectors: Array<Array<Array<bigint>>> = [];
  arrayVectors: Array<Array<number>> = [];
  mapVectors: Map<string, Array<number>> = new Map();
  setVecs: Set<Array<bigint>> = new Set();

  from(i: I): this {
    super.from(i);
    this.nestedVectors = i.readArray(() => i.readArray(() => i.readArray(() => i.readU64())));
    this.arrayVectors = i.readArray(() => i.readArray(() => i.readF32()));
    this.mapVectors = i.readMap(() => i.readString(), () => i.readArray(() => i.readU32()));
    this.setVecs = i.readSet(() => i.readArray(() => i.readI64()));
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeArray(this.nestedVectors, (v) => o.writeArray(v, (v) => o.writeArray(v, (v) => o.writeU64(v))));
    o.writeArray(this.arrayVectors, (v) => o.writeArray(v, (v) => o.writeF32(v)));
    o.writeMap(this.mapVectors, (k) => o.writeString(k), (v) => o.writeArray(v, (v) => o.writeU32(v)));
    o.writeSet(this.setVecs, (v) => o.writeArray(v, (v) => o.writeI64(v)));
    return this;
  }
}
export class ComprehensiveContainer extends Base {
  static readonly typeID = E_StreamPunkType.ComprehensiveContainer;
  vec_sptr_all_basic: Array<SpRef<AllBasicTypes | null>> = [];
  deq_uptr_template_container: Array<SpRef<TemplateContainer | null>> = [];
  list_string: Array<string> = [];
  flist_sptr_complex: Array<SpRef<ComplexTemplateNesting | null>> = [];
  set_int: Set<number> = new Set();
  uset_string: Set<string> = new Set();
  map_str_sptr_all_basic: Map<string, SpRef<AllBasicTypes | null>> = new Map();
  umap_int_uptr_template_container: Map<number, SpRef<TemplateContainer | null>> = new Map();
  self_wptr: SpRef<ComprehensiveContainer | null> = new SpRef(null);
  self_sptr: SpRef<ComprehensiveContainer | null> = new SpRef(null);

  from(i: I): this {
    super.from(i);
    this.vec_sptr_all_basic = i.readArray(() => i.readPtrWithTypeID());
    this.deq_uptr_template_container = i.readArray(() => i.readPtrWithTypeID());
    this.list_string = i.readArray(() => i.readString());
    this.flist_sptr_complex = i.readArray(() => i.readPtrWithTypeID());
    this.set_int = i.readSet(() => i.readI32());
    this.uset_string = i.readSet(() => i.readString());
    this.map_str_sptr_all_basic = i.readMap(() => i.readString(), () => i.readPtrWithTypeID());
    this.umap_int_uptr_template_container = i.readMap(() => i.readI32(), () => i.readPtrWithTypeID());
    this.self_wptr = i.readPtrWithTypeID();
    this.self_sptr = i.readPtrWithTypeID();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeArray(this.vec_sptr_all_basic, (v) => o.writePtrWithTypeID(v.value));
    o.writeArray(this.deq_uptr_template_container, (v) => o.writePtrWithTypeID(v.value));
    o.writeArray(this.list_string, (v) => o.writeString(v));
    o.writeArray(this.flist_sptr_complex, (v) => o.writePtrWithTypeID(v.value));
    o.writeSet(this.set_int, (v) => o.writeI32(v));
    o.writeSet(this.uset_string, (v) => o.writeString(v));
    o.writeMap(this.map_str_sptr_all_basic, (k) => o.writeString(k), (v) => o.writePtrWithTypeID(v.value));
    o.writeMap(this.umap_int_uptr_template_container, (k) => o.writeI32(k), (v) => o.writePtrWithTypeID(v.value));
    o.writePtrWithTypeID(this.self_wptr.value);
    o.writePtrWithTypeID(this.self_sptr.value);
    return this;
  }
}
export class Child extends AllBasicTypes {
  static readonly typeID = E_StreamPunkType.Child;
  child_field: number = 0;

  from(i: I): this {
    super.from(i);
    this.child_field = i.readI32();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeI32(this.child_field);
    return this;
  }
}
export class SelfReferential extends Base {
  static readonly typeID = E_StreamPunkType.SelfReferential;
  self_ptr: SpRef<SelfReferential | null> = new SpRef(null);

  from(i: I): this {
    super.from(i);
    this.self_ptr = i.readPtrWithTypeID();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writePtrWithTypeID(this.self_ptr.value);
    return this;
  }
}
export class TemplateAndPointer extends Base {
  static readonly typeID = E_StreamPunkType.TemplateAndPointer;
  v_raw_ptr: Array<SpRef<number | null>> = [];
  m_str_shared_ptr: Map<string, SpRef<number | null>> = new Map();

  from(i: I): this {
    super.from(i);
    this.v_raw_ptr = i.readArray(() => i.readPtr(() => i.readI32()));
    this.m_str_shared_ptr = i.readMap(() => i.readString(), () => i.readPtr(() => i.readI32()));
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeArray(this.v_raw_ptr, (v) => o.writePtr(v.value, v.address, (v) => o.writeI32(v)));
    o.writeMap(this.m_str_shared_ptr, (k) => o.writeString(k), (v) => o.writePtr(v.value, v.address, (v) => o.writeI32(v)));
    return this;
  }
}
export class InheritanceAndSelfReference extends Child {
  static readonly typeID = E_StreamPunkType.InheritanceAndSelfReference;
  self_ptr: SpRef<InheritanceAndSelfReference | null> = new SpRef(null);

  from(i: I): this {
    super.from(i);
    this.self_ptr = i.readPtrWithTypeID();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writePtrWithTypeID(this.self_ptr.value);
    return this;
  }
}
export class MegaComplexClass extends Child {
  static readonly typeID = E_StreamPunkType.MegaComplexClass;
  complex_vector: Array<SpRef<TemplateAndPointer | null>> = [];
  raw_self_ref_ptr: SpRef<SelfReferential | null> = new SpRef(null);
  self_ptr: SpRef<MegaComplexClass | null> = new SpRef(null);

  from(i: I): this {
    super.from(i);
    this.complex_vector = i.readArray(() => i.readPtrWithTypeID());
    this.raw_self_ref_ptr = i.readPtrWithTypeID();
    this.self_ptr = i.readPtrWithTypeID();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeArray(this.complex_vector, (v) => o.writePtrWithTypeID(v.value));
    o.writePtrWithTypeID(this.raw_self_ref_ptr.value);
    o.writePtrWithTypeID(this.self_ptr.value);
    return this;
  }
}
export class SuperComplexContainer extends Base {
  static readonly typeID = E_StreamPunkType.SuperComplexContainer;
  bits: Array<boolean> = [];
  opt_int: number | null = null;
  file_path: string = "";
  atomic_placeholder: number = 0;
  sptr_test: SpRef<Test | null> = new SpRef(null);
  wptr_test: SpRef<Test | null> = new SpRef(null);
  uptr_test: SpRef<Test | null> = new SpRef(null);
  tuple_member: [number, string, boolean] = [0.0, "", false];
  map_of_umaps: Map<string, Map<number, number>> = new Map();
  variant_member: SpVariant<[number | string | SpRef<AllBasicTypes | null>]> = new SpVariant(0, 0);
  deq_list_flist: Array<Array<Array<number>>> = [];
  vec_arr_str: Array<SpArray<string>> = [];
  set_of_usets: Set<Array<string>> = new Set();
  kitchen_sink: Map<string, Array<[SpRef<SelfReferential | null>, SpRef<PointerContainer | null>, Array<SpVariant<[SpArray<string> | Array<Set<string>>]>>] | null>> = new Map();

  from(i: I): this {
    super.from(i);
    this.bits = i.readBitset();
    this.opt_int = i.readOptional(() => i.readI32());
    this.file_path = i.readString();
    this.atomic_placeholder = i.readI32();
    this.sptr_test = i.readPtrWithTypeID();
    this.wptr_test = i.readPtrWithTypeID();
    this.uptr_test = i.readPtrWithTypeID();
    this.tuple_member = [i.readF64(), i.readString(), i.readBl()];
    this.map_of_umaps = i.readMap(() => i.readString(), () => i.readMap(() => i.readI32(), () => i.readF64()));
    this.variant_member = i.readVariant([() => i.readI32(), () => i.readString(), () => i.readPtrWithTypeID()]);
    this.deq_list_flist = i.readArray(() => i.readArray(() => i.readArray(() => i.readI32())));
    this.vec_arr_str = i.readArray(() => i.readSpArray(5, () => i.readString()));
    this.set_of_usets = i.readSet(() => i.readArray(() => i.readString()));
    this.kitchen_sink = i.readMap(() => i.readString(), () => i.readArray(() => i.readOptional(() => [i.readPtrWithTypeID(), i.readPtrWithTypeID(), i.readArray(() => i.readVariant([() => i.readSpArray(8, () => i.readCh()), () => i.readArray(() => i.readSet(() => i.readString()))]))])));
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeBitset(this.bits);
    o.writeOptional(this.opt_int, (v) => { o.writeI32(v); });
    o.writeString(this.file_path);
    o.writeI32(this.atomic_placeholder);
    o.writePtrWithTypeID(this.sptr_test.value);
    o.writePtrWithTypeID(this.wptr_test.value);
    o.writePtrWithTypeID(this.uptr_test.value);
    o.writeF64(this.tuple_member[0]); o.writeString(this.tuple_member[1]); o.writeBl(this.tuple_member[2]); ;
    o.writeMap(this.map_of_umaps, (k) => o.writeString(k), (v) => o.writeMap(v, (k) => o.writeI32(k), (v) => o.writeF64(v)));
    o.writeVariant(this.variant_member.value, this.variant_member.typeIndex, [(v: any) => o.writeI32(v), (v: any) => o.writeString(v), (v: any) => o.writePtrWithTypeID(v.value)]);
    o.writeArray(this.deq_list_flist, (v) => o.writeArray(v, (v) => o.writeArray(v, (v) => o.writeI32(v))));
    o.writeArray(this.vec_arr_str, (v) => o.writeSpArray(v, (v) => o.writeString(v)));
    o.writeSet(this.set_of_usets, (v) => o.writeArray(v, (v) => o.writeString(v)));
    o.writeMap(this.kitchen_sink, (k) => o.writeString(k), (v) => o.writeArray(v, (v) => o.writeOptional(v, (v) => { o.writePtrWithTypeID(v[0].value); o.writePtrWithTypeID(v[1].value); o.writeArray(v[2], (v) => o.writeVariant(v.value, v.typeIndex, [(v: any) => o.writeSpArray(v, (v) => o.writeCh(v)), (v: any) => o.writeArray(v, (v) => o.writeSet(v, (v) => o.writeString(v)))])); ; })));
    return this;
  }
}
export class Test extends Base {
  static readonly typeID = E_StreamPunkType.Test;
  name: string = "";
  pwd: string = "";
  gateWay: string = "";
  mask: string = "";
  ip: string = "";
  dns1: string = "";
  dns2: string = "";

  from(i: I): this {
    super.from(i);
    this.name = i.readString();
    this.pwd = i.readString();
    this.gateWay = i.readString();
    this.mask = i.readString();
    this.ip = i.readString();
    this.dns1 = i.readString();
    this.dns2 = i.readString();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeString(this.name);
    o.writeString(this.pwd);
    o.writeString(this.gateWay);
    o.writeString(this.mask);
    o.writeString(this.ip);
    o.writeString(this.dns1);
    o.writeString(this.dns2);
    return this;
  }
}
export class MQTT extends Base {
  static readonly typeID = E_StreamPunkType.MQTT;
  host: string = "";
  user: string = "";
  pwd: string = "";

  from(i: I): this {
    super.from(i);
    this.host = i.readString();
    this.user = i.readString();
    this.pwd = i.readString();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeString(this.host);
    o.writeString(this.user);
    o.writeString(this.pwd);
    return this;
  }
}
export class PointerDemo extends Base {
  static readonly typeID = E_StreamPunkType.PointerDemo;
  rawPtr: SpRef<Test | null> = new SpRef(null);
  sharedPtr: SpRef<MQTT | null> = new SpRef(null);
  uniquePtr: SpRef<Test | null> = new SpRef(null);
  weakSelf: SpRef<PointerDemo | null> = new SpRef(null);

  from(i: I): this {
    super.from(i);
    this.rawPtr = i.readPtrWithTypeID();
    this.sharedPtr = i.readPtrWithTypeID();
    this.uniquePtr = i.readPtrWithTypeID();
    this.weakSelf = i.readPtrWithTypeID();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writePtrWithTypeID(this.rawPtr.value);
    o.writePtrWithTypeID(this.sharedPtr.value);
    o.writePtrWithTypeID(this.uniquePtr.value);
    o.writePtrWithTypeID(this.weakSelf.value);
    return this;
  }
}
export class ContainerDemo extends Base {
  static readonly typeID = E_StreamPunkType.ContainerDemo;
  testPtrs: Array<SpRef<Test | null>> = [];
  selfContainer: SpRef<ContainerDemo | null> = new SpRef(null);
  allObjects: Set<SpRef<Base | null>> = new Set();
  mqttConfigs: Map<string, SpRef<MQTT | null>> = new Map();
  mqttConfigs2: Map<string, SpRef<MQTT | null>> = new Map();

  from(i: I): this {
    super.from(i);
    this.testPtrs = i.readArray(() => i.readPtrWithTypeID());
    this.selfContainer = i.readPtrWithTypeID();
    this.allObjects = i.readSet(() => i.readPtrWithTypeID());
    this.mqttConfigs = i.readMap(() => i.readString(), () => i.readPtrWithTypeID());
    this.mqttConfigs2 = i.readMap(() => i.readString(), () => i.readPtrWithTypeID());
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeArray(this.testPtrs, (v) => o.writePtrWithTypeID(v.value));
    o.writePtrWithTypeID(this.selfContainer.value);
    o.writeSet(this.allObjects, (v) => o.writePtrWithTypeID(v.value));
    o.writeMap(this.mqttConfigs, (k) => o.writeString(k), (v) => o.writePtrWithTypeID(v.value));
    o.writeMap(this.mqttConfigs2, (k) => o.writeString(k), (v) => o.writePtrWithTypeID(v.value));
    return this;
  }
}
export class NetworkSystem extends Base {
  static readonly typeID = E_StreamPunkType.NetworkSystem;
  mainContainer: SpRef<ContainerDemo | null> = new SpRef(null);
  activeTests: Array<SpRef<Test | null>> = [];
  mqttInstances: Array<SpRef<MQTT | null>> = [];
  demos: Array<SpRef<PointerDemo | null>> = [];

  from(i: I): this {
    super.from(i);
    this.mainContainer = i.readPtrWithTypeID();
    this.activeTests = i.readArray(() => i.readPtrWithTypeID());
    this.mqttInstances = i.readArray(() => i.readPtrWithTypeID());
    this.demos = i.readArray(() => i.readPtrWithTypeID());
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writePtrWithTypeID(this.mainContainer.value);
    o.writeArray(this.activeTests, (v) => o.writePtrWithTypeID(v.value));
    o.writeArray(this.mqttInstances, (v) => o.writePtrWithTypeID(v.value));
    o.writeArray(this.demos, (v) => o.writePtrWithTypeID(v.value));
    return this;
  }
}
export class Device extends Base {
  static readonly typeID = E_StreamPunkType.Device;
  deviceId: string = "";
  manufacturer: string = "";
  lastSeen: Date = new Date(0);

  from(i: I): this {
    super.from(i);
    this.deviceId = i.readString();
    this.manufacturer = i.readString();
    this.lastSeen = i.readTime();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeString(this.deviceId);
    o.writeString(this.manufacturer);
    o.writeTime(this.lastSeen);
    return this;
  }
}
export class NetworkDevice extends Device {
  static readonly typeID = E_StreamPunkType.NetworkDevice;
  ipAddress: string = "";
  macAddress: string = "";
  port: number = 0;

  from(i: I): this {
    super.from(i);
    this.ipAddress = i.readString();
    this.macAddress = i.readString();
    this.port = i.readU16();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeString(this.ipAddress);
    o.writeString(this.macAddress);
    o.writeU16(this.port);
    return this;
  }
}
export class Sensor extends Device {
  static readonly typeID = E_StreamPunkType.Sensor;
  currentValue: number = 0.0;
  minValue: number = 0.0;
  maxValue: number = 0.0;
  samplingInterval: Date = new Date(0);

  from(i: I): this {
    super.from(i);
    this.currentValue = i.readF64();
    this.minValue = i.readF64();
    this.maxValue = i.readF64();
    this.samplingInterval = i.readTime();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeF64(this.currentValue);
    o.writeF64(this.minValue);
    o.writeF64(this.maxValue);
    o.writeTime(this.samplingInterval);
    return this;
  }
}
export class TemperatureSensor extends Sensor {
  static readonly typeID = E_StreamPunkType.TemperatureSensor;
  isCelsius: boolean = false;
  calibrationOffset: number = 0.0;

  from(i: I): this {
    super.from(i);
    this.isCelsius = i.readBl();
    this.calibrationOffset = i.readF64();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeBl(this.isCelsius);
    o.writeF64(this.calibrationOffset);
    return this;
  }
}
export class SmartHomeSystem extends Base {
  static readonly typeID = E_StreamPunkType.SmartHomeSystem;
  allDevices: Array<SpRef<Device | null>> = [];
  sensors: Map<string, SpRef<Sensor | null>> = new Map();
  mainThermostat: SpRef<TemperatureSensor | null> = new SpRef(null);
  network: SpRef<NetworkSystem | null> = new SpRef(null);

  from(i: I): this {
    super.from(i);
    this.allDevices = i.readArray(() => i.readPtrWithTypeID());
    this.sensors = i.readMap(() => i.readString(), () => i.readPtrWithTypeID());
    this.mainThermostat = i.readPtrWithTypeID();
    this.network = i.readPtrWithTypeID();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeArray(this.allDevices, (v) => o.writePtrWithTypeID(v.value));
    o.writeMap(this.sensors, (k) => o.writeString(k), (v) => o.writePtrWithTypeID(v.value));
    o.writePtrWithTypeID(this.mainThermostat.value);
    o.writePtrWithTypeID(this.network.value);
    return this;
  }
}
export class MultiLevelContainer extends Base {
  static readonly typeID = E_StreamPunkType.MultiLevelContainer;
  baseObj: SpRef<Base | null> = new SpRef(null);
  deviceList: Array<SpRef<Device | null>> = [];
  sensorMap: Map<string, SpRef<Sensor | null>> = new Map();
  selfRef: SpRef<MultiLevelContainer | null> = new SpRef(null);

  from(i: I): this {
    super.from(i);
    this.baseObj = i.readPtrWithTypeID();
    this.deviceList = i.readArray(() => i.readPtrWithTypeID());
    this.sensorMap = i.readMap(() => i.readString(), () => i.readPtrWithTypeID());
    this.selfRef = i.readPtrWithTypeID();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writePtrWithTypeID(this.baseObj.value);
    o.writeArray(this.deviceList, (v) => o.writePtrWithTypeID(v.value));
    o.writeMap(this.sensorMap, (k) => o.writeString(k), (v) => o.writePtrWithTypeID(v.value));
    o.writePtrWithTypeID(this.selfRef.value);
    return this;
  }
}
export class SptrTest extends Base {
  static readonly typeID = E_StreamPunkType.SptrTest;
  test1: SpRef<Array<SpRef<Device | null>> | null> = new SpRef(null);

  from(i: I): this {
    super.from(i);
    this.test1 = i.readPtr(() => i.readArray(() => i.readPtrWithTypeID()));
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writePtr(this.test1.value, this.test1.address, (v) => o.writeArray(v, (v) => o.writePtrWithTypeID(v.value)));
    return this;
  }
}
export class MousePosition extends Base {
  static readonly typeID = E_StreamPunkType.MousePosition;
  x: number = 0;
  y: number = 0;

  from(i: I): this {
    super.from(i);
    this.x = i.readI32();
    this.y = i.readI32();
    return this;
  }

  to(o: O): this {
    super.to(o);
    o.writeI32(this.x);
    o.writeI32(this.y);
    return this;
  }
}

