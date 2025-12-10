/* eslint-disable @typescript-eslint/no-array-constructor */

/*
这是一个无外部依赖的ts文件，你可以按照下面方式很方便地编译它：
预备工作：
npm install -g typescript
具体操作：

基本编译
tsc .\stream-punk-data.ts --target ES2020 --skipLibCheck

指定输出文件
tsc .\stream-punk-data.ts --outFile .\stream-punk-data.js --target ES2020 --skipLibCheck

启用严格模式
tsc .\stream-punk-data.ts --strict --target ES2020 --skipLibCheck
*/

export type SpStdArray<T> = T[];

export class SpRef<T> {
    value: T | null
    address: bigint
    constructor(initialValue: T | null, address: bigint) {
        this.value = initialValue
        this.address = address
    }
}

export class SpArray<T> {
    _data: Array<T>;
    constructor(size: number, initializer?: T) {
        this._data = new Array(size).fill(initializer);
    }
    get size(): number { return this._data.length; }
    at(index: number): T {
        if (index < 0 || index >= this.size) {
            throw new RangeError("Index out of bounds");
        }
        return this._data[index];
    }
    set(index: number, value: T): void {
        if (index < 0 || index >= this.size) {
            throw new RangeError("Index out of bounds");
        }
        this._data[index] = value;
    }
}

export class SpVariant<Types extends any[]> {
    private _value!: Types[number];
    private _typeIndex: number = -1;
    constructor(value?: Types[number]) {
        if (value !== undefined) {
            this.set(value);
        }
    }
    set(value: Types[number]): void {
        this._value = value;
        this._updateTypeIndex();
    }
    get value(): Types[number] { return this._value; }
    get typeIndex(): number { return this._typeIndex; }
    private _updateTypeIndex(): void {
        if (this._value === undefined || this._value === null) {
            this._typeIndex = -1;
            return;
        }
        const currentConstructor = this._value.constructor;
        for (let i = 0; i < this.possibleTypes.length; i++) {
            const type = this.possibleTypes[i];
            if (typeof this._value === type) {
                this._typeIndex = i;
                return;
            }
            if (typeof type === 'function' && this._value && (this._value as any) instanceof type) {
                this._typeIndex = i;
                return;
            }
        }
        this._typeIndex = -1;
    }
    private get possibleTypes(): any[] { return [] as any as Types; }
}

export class I {
    view: DataView
    offset: number
    objectMap: Map<bigint, unknown>
    decoder: TextDecoder
    decoder16: TextDecoder

    hasMoreData(): boolean {
        return this.offset < this.view.byteLength;
    }

    constructor(buffer: ArrayBuffer, initialOffset = 0) {
        this.view = new DataView(buffer)
        this.offset = initialOffset
        this.objectMap = new Map<bigint, unknown>()
        this.decoder = new TextDecoder('utf-8')
        this.decoder16 = new TextDecoder('utf-16le')
    }

    read_u8(): number {
        const value = this.view.getUint8(this.offset)
        this.offset += 1
        return value
    }

    read_u16(): number {
        const value = this.view.getUint16(this.offset, true)
        this.offset += 2
        return value
    }

    read_u32(): number {
        const value = this.view.getUint32(this.offset, true)
        this.offset += 4
        return value
    }

    read_u64(): bigint {
        const value = this.view.getBigUint64(this.offset, true)
        this.offset += 8
        return value
    }

    read_i8(): number {
        const value = this.view.getInt8(this.offset)
        this.offset += 1
        return value
    }

    read_i16(): number {
        const value = this.view.getInt16(this.offset, true)
        this.offset += 2
        return value
    }

    read_i32(): number {
        const value = this.view.getInt32(this.offset, true)
        this.offset += 4
        return value
    }

    read_i64(): bigint {
        const value = this.view.getBigInt64(this.offset, true)
        this.offset += 8
        return value
    }

    read_f32(): number {
        const value = this.view.getFloat32(this.offset, true)
        this.offset += 4
        return value
    }

    read_f64(): number {
        const value = this.view.getFloat64(this.offset, true)
        this.offset += 8
        return value
    }

    read_ch(): string {
        const value = String.fromCharCode(this.view.getUint8(this.offset))
        this.offset += 1
        return value
    }

    read_ch8(): string {
        const value = String.fromCharCode(this.view.getUint8(this.offset))
        this.offset += 1
        return value
    }

    read_ch16(): string {
        const value = String.fromCharCode(this.view.getUint16(this.offset, true))
        this.offset += 2
        return value
    }

    read_ch32(): string {
        const codePoint = this.view.getUint32(this.offset, true)
        this.offset += 4
        // Unicode code points: 0x0 ~ 0x10FFFF
        if (codePoint >= 0 && codePoint <= 0x10FFFF) {
            return String.fromCodePoint(codePoint)
        }
        else {
            return ""
        }
    }

    read_bl(): boolean {
        const value = this.view.getUint8(this.offset) !== 0
        this.offset += 1
        return value
    }

    align(boundary: number) {
        this.offset += (boundary - (this.offset % boundary)) % boundary
    }

    read_sz(): number {
        return this.read_u32()
    }

    read_bytes(len: number) {
        this.offset += len;
    }

    read_string(): string {
        const stringLength = Number(this.read_sz());
        if (stringLength === 0) {
            return "";
        }
        const value = this.decoder.decode(
            new Uint8Array(this.view.buffer, this.offset, stringLength)
        );
        this.offset += stringLength;
        return value;
    }

    read_u8string(): string {
        const len = Number(this.read_sz())
        const value = this.decoder.decode(
            new Uint8Array(this.view.buffer, this.offset, len),
        )
        this.offset += len
        return value
    }

    read_u16string(): string {
        const len = Number(this.read_sz())
        const byteLength = len * 2
        const value = this.decoder16.decode(
            new Uint8Array(this.view.buffer, this.offset, byteLength),
        )
        this.offset += byteLength
        return value
    }

    read_u32string(): Uint8Array {
        const len = Number(this.read_sz());
        const byteLength = len * 4;
        const dataSlice = new Uint8Array(this.view.buffer,this.offset,byteLength);
        this.offset += byteLength;
        return dataSlice;
    }

    read_ptr_with_typeID<T>(): SpRef<T> {
        const address = this.read_u64();
        if (address === 0n) {
            return new SpRef<T>(null, 0n);
        }
        if (this.objectMap.has(address)) {
            return this.objectMap.get(address) as SpRef<T>;
        }
        const ref = new SpRef<T>(null, address);
        this.objectMap.set(address, ref);
        ref.value = read_obj(this) as unknown as T;
        return ref;
    }

    read_ptr<T>(elementReader: () => T): SpRef<T> {
        const address = this.read_u64();
        if (address === 0n) {
            return new SpRef<T>(null, 0n);
        }
        if (this.objectMap.has(address)) {
            return this.objectMap.get(address) as SpRef<T>;
        }
        const value = elementReader();
        const ref = new SpRef<T>(value, address);
        this.objectMap.set(address, ref);
        return ref;
    }

    read_array<T>(count: number, elementReader: () => T): T[] {
        const arr: T[] = []
        for (let i = 0; i < count; i++) {
            arr.push(elementReader())
        }
        return arr
    }

    read_Array<T>(elementReader: () => T): T[] {
        const size = Number(this.read_sz())
        return this.read_array(size, elementReader)
    }

    read_set<T>(elementReader: () => T): Set<T> {
        const arr = this.read_Array(elementReader)
        return new Set(arr)
    }

    read_unordered_set<T>(elementReader: () => T): Set<T> {
        return this.read_set(elementReader)
    }

    read_map<K, V>(keyReader: () => K, valueReader: () => V): Map<K, V> {
        const size = Number(this.read_sz())
        const map = new Map<K, V>()
        for (let i = 0; i < size; i++) {
            const key = keyReader()
            const value = valueReader()
            map.set(key, value)
        }
        return map
    }

    read_unordered_map<K, V>(keyReader: () => K, valueReader: () => V): Map<K, V> {
        return this.read_map(keyReader, valueReader)
    }

    read_vector<T>(elementReader: () => T): T[] {
        return this.read_Array(elementReader);
    }

    read_deque<T>(elementReader: () => T): T[] {
        return this.read_Array(elementReader)
    }

    read_list<T>(elementReader: () => T): T[] {
        return this.read_Array(elementReader)
    }

    read_forward_list<T>(elementReader: () => T): T[] {
        return this.read_Array(elementReader)
    }

    read_SpArray<T>(size: number, elementReader: () => T): SpArray<T> {
        const arr = new SpArray<T>(size);
        for (let i = 0; i < size; i++) {
            arr.set(i, elementReader());
        }
        return arr;
    }

    read_std_string(): string {
        return this.read_string()
    }

    read_bitset(): boolean[] {
        const size = this.read_u32();
        const byteLength = Math.ceil(size / 8);
        const bytes = new Uint8Array(this.view.buffer, this.offset, byteLength);
        this.offset += byteLength;
        return Array.from({ length: size }, (_, i) => {
            const byteIndex = Math.floor(i / 8);
            const bitIndex = i % 8;
            return ((bytes[byteIndex] >> bitIndex) & 1) === 1;
        });
    }

    read_optional<T>(valueReader: () => T): T | null {
        const hasValue = this.read_bl();
        if (hasValue) {
            return valueReader();
        }
        return null;
    }

    read_variant<Readers extends Array<() => any>>(readers: Readers): ReturnType<Readers[number]> {
        const index = this.read_u32();
        const readVariantAt = <I extends number>(currIdx: I): ReturnType<Readers[number]> => {
            if (currIdx === index) {
                return readers[currIdx]();
            }
            if (currIdx + 1 < readers.length) {
                return readVariantAt(currIdx + 1 as I);
            }
            throw new Error(`Variant index ${index} out of range`);
        };
        var r = readVariantAt(0 as const);
        return r;
    }

    read_stream_punk_time(): Date {
        const sec = this.read_i64();
        const attoSec = this.read_i64();
        const totalMs = Number(sec) * 1000 + Number(attoSec / 1000000000000000n);
        return new Date(totalMs);
    }
}


export class O {
    private buffers: ArrayBuffer[] = [];
    objectMap: Map<bigint, unknown>
    encoder: TextEncoder

    constructor() {
        this.encoder = new TextEncoder();
        this.objectMap = new Map<bigint, unknown>();
    }

    write_u8(value: number): void {
        const buffer = new ArrayBuffer(1);
        new DataView(buffer).setUint8(0, value);
        this.buffers.push(buffer);
    }

    write_u16(value: number): void {
        const buffer = new ArrayBuffer(2);
        new DataView(buffer).setUint16(0, value, true);
        this.buffers.push(buffer);
    }

    write_u32(value: number): void {
        const buffer = new ArrayBuffer(4);
        new DataView(buffer).setUint32(0, value, true);
        this.buffers.push(buffer);
    }

    write_u64(value: bigint): void {
        const buffer = new ArrayBuffer(8);
        new DataView(buffer).setBigUint64(0, value, true);
        this.buffers.push(buffer);
    }

    write_i8(value: number): void {
        const buffer = new ArrayBuffer(1);
        new DataView(buffer).setInt8(0, value);
        this.buffers.push(buffer);
    }

    write_i16(value: number): void {
        const buffer = new ArrayBuffer(2);
        new DataView(buffer).setInt16(0, value, true);
        this.buffers.push(buffer);
    }

    write_i32(value: number): void {
        const buffer = new ArrayBuffer(4);
        new DataView(buffer).setInt32(0, value, true);
        this.buffers.push(buffer);
    }

    write_i64(value: bigint): void {
        const buffer = new ArrayBuffer(8);
        new DataView(buffer).setBigInt64(0, value, true);
        this.buffers.push(buffer);
    }

    write_f32(value: number): void {
        const buffer = new ArrayBuffer(4);
        new DataView(buffer).setFloat32(0, value, true);
        this.buffers.push(buffer);
    }

    write_f64(value: number): void {
        const buffer = new ArrayBuffer(8);
        new DataView(buffer).setFloat64(0, value, true);
        this.buffers.push(buffer);
    }

    write_ch(value: string): void {
        this.write_ch16(value);
    }

    write_ch8(value: string): void {
        if (value.length > 0) {
            this.write_u8(value.charCodeAt(0));
        } else {
            this.write_u8(0);
        }
    }

    write_ch16(value: string): void {
        if (value.length > 0) {
            this.write_u16(value.charCodeAt(0));
        } else {
            this.write_u16(0);
        }
    }

    write_ch32(value: string): void {
        if (value.length > 0) {
            const codePoint = value.codePointAt(0) || 0;
            this.write_u32(codePoint);
        } else {
            this.write_u32(0);
        }
    }

    write_bl(value: boolean): void {
        this.write_u8(value ? 1 : 0);
    }

    write_sz(value: number): void {
        this.write_u32(value)
    }
    
    // 在write_string方法中使用成员encoder
    write_string(value: string) {
    const encoded = this.encoder.encode(value);
        this.write_sz(encoded.length);
        this.write_uint8_array(encoded);
    }

    write_u8string(value: string): void {
        const encoded = this.encoder.encode(value);
        this.write_sz(encoded.length);
        this.write_uint8_array(encoded);
    }

    write_u16string(value: string): void {
        const bytes = new Uint8Array(value.length * 2);
        for (let i = 0; i < value.length; i++) {
            const codeUnit = value.charCodeAt(i);
            const offset = i * 2;
            bytes[offset] = codeUnit & 0xFF;
            bytes[offset + 1] = (codeUnit >> 8) & 0xFF;
        }
        this.write_uint8_array(bytes);
    }

    write_u32string(value: Uint8Array): void {
        this.write_sz(value.length/4);
        this.write_uint8_array(value);
    }

    private write_uint8_array(data: Uint8Array): void {
        const buffer = new ArrayBuffer(data.length);
        new Uint8Array(buffer).set(data);
        this.buffers.push(buffer);
    }

    write_ptr<T>(value: T | null, address: bigint, writeValue: (val: T) => void): void {
        if (value === null) {
            this.write_u64(0n)
            return
        }
        if (address === 0n) {
            address = BigInt(this.objectMap.size + 1)
        }
        if (this.objectMap.has(address)) {
            this.write_u64(address)
            return
        }
        this.objectMap.set(address, value)
        this.write_u64(address)
        writeValue(value)
    }

    write_ptr_with_typeID<T extends Base>(value: T | null, address: bigint = 0n): void {
        this.write_ptr(value, address, (val) => write_obj(this, val))
    }

    write_array<T>(arr: T[], writeElement: (item: T) => void): void {
        for (let i = 0; i < arr.length; i++) {
            writeElement(arr[i])
        }
    }

    write_Array<T>(arr: T[], writeElement: (item: T) => void): void {
        this.write_sz(arr.length)
        this.write_array(arr, writeElement)
    }

    write_set<T>(set: Set<T>, writeElement: (item: T) => void): void {
        const arr = Array.from(set)
        this.write_Array<T>(arr, writeElement)
    }

    write_unordered_set<T>(set: Set<T>, writeElement: (item: T) => void): void {
        this.write_set(set, writeElement)
    }

    write_map<K, V>(map: Map<K, V>, writeKey: (key: K) => void, writeValue: (value: V) => void): void {
        this.write_sz(map.size)
        map.forEach((value, key) => {
            writeKey(key)
            writeValue(value)
        })
    }

    write_unordered_map<K, V>(map: Map<K, V>, writeKey: (key: K) => void, writeValue: (value: V) => void): void {
        this.write_map(map, writeKey, writeValue)
    }

    write_vector<T>(vec: T[], writeElement: (item: T) => void): void {
        this.write_Array<T>(vec, writeElement)
    }

    write_deque<T>(deq: T[], writeElement: (item: T) => void): void {
        this.write_Array<T>(deq, writeElement)
    }

    write_list<T>(list: T[], writeElement: (item: T) => void): void {
        this.write_Array<T>(list, writeElement)
    }

    write_forward_list<T>(list: T[], writeElement: (item: T) => void): void {
        this.write_Array<T>(list, writeElement)
    }

    write_SpArray<T>(arr: SpArray<T>, writeElement: (item: T) => void): void {
        for (let i = 0; i < arr.size; i++) {
            writeElement(arr.at(i))
        }
    }

    write_bitset(bits: boolean[]): void {
        this.write_u32(bits.length)
        const byteLength = Math.ceil(bits.length / 8)
        const bytes = new Uint8Array(byteLength)
        for (let i = 0; i < bits.length; i++) {
            if (bits[i]) {
                const byteIndex = Math.floor(i / 8)
                const bitIndex = i % 8
                // ECMAScript规范，bytes数据已经保证初始化为0了
                bytes[byteIndex] |= (1 << bitIndex)
            }
        }
        this.write_uint8_array(bytes)
    }

    write_optional<T>(value: T | null, writeValue: (val: T) => void): void {
        this.write_bl(value !== null)
        if (value !== null) {
            writeValue(value)
        }
    }

    write_variant<T>(value: T, index: number, writers: Array<(val: any) => void>): void {
        this.write_u32(index)
        writers[index](value)
    }

    write_stream_punk_time(date: Date): void {
        const totalMs = date.getTime()
        const sec = BigInt(Math.floor(totalMs / 1000))
        const attoSec = BigInt((totalMs % 1000) * 1000000000000000)
        this.write_i64(sec)
        this.write_i64(attoSec)
    }

    to_array_buffer(): ArrayBuffer {
        const totalLength = this.buffers.reduce((sum, buf) => sum + buf.byteLength, 0);
        const result = new Uint8Array(totalLength);
        let offset = 0;
        for (const buffer of this.buffers) {
            result.set(new Uint8Array(buffer), offset);
            offset += buffer.byteLength;
        }
        return result.buffer;
    }

}

export enum E_StreamPunkType {
  e_unknowType = 0,
  bg = 1,
  ed = 2,
  vector = 3,
  array = 4,
  string = 5,
  bitset = 6,
  deque = 7,
  list = 8,
  flist = 9,
  set = 10,
  uset = 11,
  map = 12,
  umap = 13,
  sptr = 14,
  wptr = 15,
  uptr = 16,
  opt = 17,
  path = 18,
  atomic = 19,
  variant = 20,
  tuple = 21,
  u8 = 22,
  u16 = 23,
  u32 = 24,
  u64 = 25,
  i8 = 26,
  i16 = 27,
  i32 = 28,
  i64 = 29,
  f32 = 30,
  f64 = 31,
  ch = 32,
  ch8 = 33,
  ch16 = 34,
  ch32 = 35,
  bl = 36,
  ptr = 37,
  voidPtr = 38,
  cst = 39,
  dur = 40,
  timepoint = 41,
  Base = 42,
  AllBasicTypes = 43,
  TemplateContainer = 44,
  PointerContainer = 45,
  ComplexTemplateNesting = 46,
  ComprehensiveContainer = 47,
  Child = 48,
  SelfReferential = 49,
  TemplateAndPointer = 50,
  InheritanceAndSelfReference = 51,
  MegaComplexClass = 52,
  SuperComplexContainer = 53,
  Test = 54,
  MQTT = 55,
  PointerDemo = 56,
  ContainerDemo = 57,
  NetworkSystem = 58,
  Device = 59,
  NetworkDevice = 60,
  Sensor = 61,
  TemperatureSensor = 62,
  SmartHomeSystem = 63,
  MultiLevelContainer = 64,
  SptrTest = 65,
  MousePosition = 66,
  e_customType = 67,
}

export class Base {
  static typeID = E_StreamPunkType.Base;
  from(i: I) : this { void i; return this; }
  to(o: O):this{ void o; return this; }
}

export function read_obj(i:I): Base | null {
  const id:number = i.read_u32();
  switch(id){
    case E_StreamPunkType.AllBasicTypes:{ const obj = new AllBasicTypes(); obj.from(i); return obj; }
    case E_StreamPunkType.TemplateContainer:{ const obj = new TemplateContainer(); obj.from(i); return obj; }
    case E_StreamPunkType.PointerContainer:{ const obj = new PointerContainer(); obj.from(i); return obj; }
    case E_StreamPunkType.ComplexTemplateNesting:{ const obj = new ComplexTemplateNesting(); obj.from(i); return obj; }
    case E_StreamPunkType.ComprehensiveContainer:{ const obj = new ComprehensiveContainer(); obj.from(i); return obj; }
    case E_StreamPunkType.Child:{ const obj = new Child(); obj.from(i); return obj; }
    case E_StreamPunkType.SelfReferential:{ const obj = new SelfReferential(); obj.from(i); return obj; }
    case E_StreamPunkType.TemplateAndPointer:{ const obj = new TemplateAndPointer(); obj.from(i); return obj; }
    case E_StreamPunkType.InheritanceAndSelfReference:{ const obj = new InheritanceAndSelfReference(); obj.from(i); return obj; }
    case E_StreamPunkType.MegaComplexClass:{ const obj = new MegaComplexClass(); obj.from(i); return obj; }
    case E_StreamPunkType.SuperComplexContainer:{ const obj = new SuperComplexContainer(); obj.from(i); return obj; }
    case E_StreamPunkType.Test:{ const obj = new Test(); obj.from(i); return obj; }
    case E_StreamPunkType.MQTT:{ const obj = new MQTT(); obj.from(i); return obj; }
    case E_StreamPunkType.PointerDemo:{ const obj = new PointerDemo(); obj.from(i); return obj; }
    case E_StreamPunkType.ContainerDemo:{ const obj = new ContainerDemo(); obj.from(i); return obj; }
    case E_StreamPunkType.NetworkSystem:{ const obj = new NetworkSystem(); obj.from(i); return obj; }
    case E_StreamPunkType.Device:{ const obj = new Device(); obj.from(i); return obj; }
    case E_StreamPunkType.NetworkDevice:{ const obj = new NetworkDevice(); obj.from(i); return obj; }
    case E_StreamPunkType.Sensor:{ const obj = new Sensor(); obj.from(i); return obj; }
    case E_StreamPunkType.TemperatureSensor:{ const obj = new TemperatureSensor(); obj.from(i); return obj; }
    case E_StreamPunkType.SmartHomeSystem:{ const obj = new SmartHomeSystem(); obj.from(i); return obj; }
    case E_StreamPunkType.MultiLevelContainer:{ const obj = new MultiLevelContainer(); obj.from(i); return obj; }
    case E_StreamPunkType.SptrTest:{ const obj = new SptrTest(); obj.from(i); return obj; }
    case E_StreamPunkType.MousePosition:{ const obj = new MousePosition(); obj.from(i); return obj; }
  }
  return null;
}

export function write_obj(o: O, obj: Base) {
  o.write_u32((obj.constructor as any).typeID);
  obj.to(o);
}

export class AllBasicTypes extends Base {
  static typeID = E_StreamPunkType.AllBasicTypes;
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
    this.b = i.read_bl();
    this.i8_v = i.read_i8();
    this.u8_v = i.read_u8();
    this.i16_v = i.read_i16();
    this.u16_v = i.read_u16();
    this.i32_v = i.read_i32();
    this.u32_v = i.read_u32();
    this.i64_v = i.read_i64();
    this.u64_v = i.read_u64();
    this.f = i.read_f32();
    this.d = i.read_f64();
    this.c = i.read_ch();
    this.c8 = i.read_ch8();
    this.c16 = i.read_ch16();
    this.c32 = i.read_ch32();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_bl(this.b);
    o.write_i8(this.i8_v);
    o.write_u8(this.u8_v);
    o.write_i16(this.i16_v);
    o.write_u16(this.u16_v);
    o.write_i32(this.i32_v);
    o.write_u32(this.u32_v);
    o.write_i64(this.i64_v);
    o.write_u64(this.u64_v);
    o.write_f32(this.f);
    o.write_f64(this.d);
    o.write_ch(this.c);
    o.write_ch8(this.c8 );
    o.write_ch16(this.c16);
    o.write_ch32(this.c32);
    return this;
  }
}
export class TemplateContainer extends Base {
  static typeID = E_StreamPunkType.TemplateContainer;
  s: string = "";
  u8s: string = "";
  vec: Array<number> = new Array();
  deq: Array<number> = new Array();
  lst: Array<string> = new Array();
  shortForwardList: Array<number> = new Array();
  uintSet: Set<number> = new Set();
  stringHashSet: Set<string> = new Set();
  intStringMap: Map<number,string> = new Map();
  stringFloatHashMap: Map<string,number> = new Map();
  from(i: I): this {
    super.from(i);
    this.s = i.read_string();
    this.u8s = i.read_u8string();
    this.vec = i.read_Array(()=>i.read_i32());
    this.deq = i.read_Array(()=>i.read_f64());
    this.lst = i.read_Array(()=>i.read_string());
    this.shortForwardList = i.read_Array(()=>i.read_u16());
    this.uintSet = i.read_set(()=>i.read_u32());
    this.stringHashSet = i.read_set(()=>i.read_string());
    this.intStringMap = i.read_map(()=>i.read_i32(),()=>i.read_string());
    this.stringFloatHashMap = i.read_map(()=>i.read_string(),()=>i.read_f32());
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_string(this.s);
    o.write_u8string(this.u8s);
    o.write_Array(this.vec, (v)=>o.write_i32(v));
    o.write_Array(this.deq, (v)=>o.write_f64(v));
    o.write_Array(this.lst, (v)=>o.write_string(v));
    o.write_Array(this.shortForwardList, (v)=>o.write_u16(v));
    o.write_set(this.uintSet, (v)=>o.write_u32(v));
    o.write_set(this.stringHashSet, (v)=>o.write_string(v));
    o.write_map(this.intStringMap, (k)=>o.write_i32(k), (v)=>o.write_string(v));
    o.write_map(this.stringFloatHashMap, (k)=>o.write_string(k), (v)=>o.write_f32(v));
    return this;
  }
}
export class PointerContainer extends Base {
  static typeID = E_StreamPunkType.PointerContainer;
  raw_ptr: SpRef<number| null> = new SpRef(null, 0n);
  shared_ptr_int: SpRef<number| null> = new SpRef(null, 0n);
  unique_ptr_int: SpRef<number| null> = new SpRef(null, 0n);
  from(i: I): this {
    super.from(i);
    this.raw_ptr = i.read_ptr(()=>i.read_i32());
    this.shared_ptr_int = i.read_ptr(()=>i.read_i32());
    this.unique_ptr_int = i.read_ptr(()=>i.read_i32());
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_ptr(this.raw_ptr.value, this.raw_ptr.address, (v)=>o.write_i32(v));
    o.write_ptr(this.shared_ptr_int.value, this.shared_ptr_int.address, (v)=>o.write_i32(v));
    o.write_ptr(this.unique_ptr_int.value, this.unique_ptr_int.address, (v)=>o.write_i32(v));
    return this;
  }
}
export class ComplexTemplateNesting extends Base {
  static typeID = E_StreamPunkType.ComplexTemplateNesting;
  nestedVectors: Array<Array<Array<bigint>>> = new Array();
  arrayVectors: Array<Array<number>> = new Array();
  mapVectors: Map<string,Array<number>> = new Map();
  setVecs: Set<Array<bigint>> = new Set();
  from(i: I): this {
    super.from(i);
    this.nestedVectors = i.read_Array(()=>i.read_Array(()=>i.read_Array(()=>i.read_u64())));
    this.arrayVectors = i.read_Array(()=>i.read_Array(()=>i.read_f32()));
    this.mapVectors = i.read_map(()=>i.read_string(),()=>i.read_Array(()=>i.read_u32()));
    this.setVecs = i.read_set(()=>i.read_Array(()=>i.read_i64()));
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_Array(this.nestedVectors, (v)=>o.write_Array(v, (v)=>o.write_Array(v, (v)=>o.write_u64(v))));
    o.write_Array(this.arrayVectors, (v)=>o.write_Array(v, (v)=>o.write_f32(v)));
    o.write_map(this.mapVectors, (k)=>o.write_string(k), (v)=>o.write_Array(v, (v)=>o.write_u32(v)));
    o.write_set(this.setVecs, (v)=>o.write_Array(v, (v)=>o.write_i64(v)));
    return this;
  }
}
export class ComprehensiveContainer extends Base {
  static typeID = E_StreamPunkType.ComprehensiveContainer;
  vec_sptr_all_basic: Array<SpRef<AllBasicTypes| null>> = new Array();
  deq_uptr_template_container: Array<SpRef<TemplateContainer| null>> = new Array();
  list_string: Array<string> = new Array();
  flist_sptr_complex: Array<SpRef<ComplexTemplateNesting| null>> = new Array();
  set_int: Set<number> = new Set();
  uset_string: Set<string> = new Set();
  map_str_sptr_all_basic: Map<string,SpRef<AllBasicTypes| null>> = new Map();
  umap_int_uptr_template_container: Map<number,SpRef<TemplateContainer| null>> = new Map();
  self_wptr: SpRef<ComprehensiveContainer| null> = new SpRef(null, 0n);
  self_sptr: SpRef<ComprehensiveContainer| null> = new SpRef(null, 0n);
  from(i: I): this {
    super.from(i);
    this.vec_sptr_all_basic = i.read_Array(()=>i.read_ptr_with_typeID<AllBasicTypes>());
    this.deq_uptr_template_container = i.read_Array(()=>i.read_ptr_with_typeID<TemplateContainer>());
    this.list_string = i.read_Array(()=>i.read_string());
    this.flist_sptr_complex = i.read_Array(()=>i.read_ptr_with_typeID<ComplexTemplateNesting>());
    this.set_int = i.read_set(()=>i.read_i32());
    this.uset_string = i.read_set(()=>i.read_string());
    this.map_str_sptr_all_basic = i.read_map(()=>i.read_string(),()=>i.read_ptr_with_typeID<AllBasicTypes>());
    this.umap_int_uptr_template_container = i.read_map(()=>i.read_i32(),()=>i.read_ptr_with_typeID<TemplateContainer>());
    this.self_wptr = i.read_ptr_with_typeID<ComprehensiveContainer>();
    this.self_sptr = i.read_ptr_with_typeID<ComprehensiveContainer>();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_Array(this.vec_sptr_all_basic, (v)=>o.write_ptr_with_typeID(v.value));
    o.write_Array(this.deq_uptr_template_container, (v)=>o.write_ptr_with_typeID(v.value));
    o.write_Array(this.list_string, (v)=>o.write_string(v));
    o.write_Array(this.flist_sptr_complex, (v)=>o.write_ptr_with_typeID(v.value));
    o.write_set(this.set_int, (v)=>o.write_i32(v));
    o.write_set(this.uset_string, (v)=>o.write_string(v));
    o.write_map(this.map_str_sptr_all_basic, (k)=>o.write_string(k), (v)=>o.write_ptr_with_typeID(v.value));
    o.write_map(this.umap_int_uptr_template_container, (k)=>o.write_i32(k), (v)=>o.write_ptr_with_typeID(v.value));
    o.write_ptr_with_typeID(this.self_wptr.value);
    o.write_ptr_with_typeID(this.self_sptr.value);
    return this;
  }
}
export class Child extends AllBasicTypes {
  static typeID = E_StreamPunkType.Child;
  child_field: number = 0;
  from(i: I): this {
    super.from(i);
    this.child_field = i.read_i32();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_i32(this.child_field);
    return this;
  }
}
export class SelfReferential extends Base {
  static typeID = E_StreamPunkType.SelfReferential;
  self_ptr: SpRef<SelfReferential| null> = new SpRef(null, 0n);
  from(i: I): this {
    super.from(i);
    this.self_ptr = i.read_ptr_with_typeID<SelfReferential>();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_ptr_with_typeID(this.self_ptr.value);
    return this;
  }
}
export class TemplateAndPointer extends Base {
  static typeID = E_StreamPunkType.TemplateAndPointer;
  v_raw_ptr: Array<SpRef<number| null>> = new Array();
  m_str_shared_ptr: Map<string,SpRef<number| null>> = new Map();
  from(i: I): this {
    super.from(i);
    this.v_raw_ptr = i.read_Array(()=>i.read_ptr(()=>i.read_i32()));
    this.m_str_shared_ptr = i.read_map(()=>i.read_string(),()=>i.read_ptr(()=>i.read_i32()));
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_Array(this.v_raw_ptr, (v)=>o.write_ptr(v.value, v.address, (v)=>o.write_i32(v)));
    o.write_map(this.m_str_shared_ptr, (k)=>o.write_string(k), (v)=>o.write_ptr(v.value, v.address, (v)=>o.write_i32(v)));
    return this;
  }
}
export class InheritanceAndSelfReference extends Child {
  static typeID = E_StreamPunkType.InheritanceAndSelfReference;
  self_ptr: SpRef<InheritanceAndSelfReference| null> = new SpRef(null, 0n);
  from(i: I): this {
    super.from(i);
    this.self_ptr = i.read_ptr_with_typeID<InheritanceAndSelfReference>();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_ptr_with_typeID(this.self_ptr.value);
    return this;
  }
}
export class MegaComplexClass extends Child {
  static typeID = E_StreamPunkType.MegaComplexClass;
  complex_vector: Array<SpRef<TemplateAndPointer| null>> = new Array();
  raw_self_ref_ptr: SpRef<SelfReferential| null> = new SpRef(null, 0n);
  self_ptr: SpRef<MegaComplexClass| null> = new SpRef(null, 0n);
  from(i: I): this {
    super.from(i);
    this.complex_vector = i.read_Array(()=>i.read_ptr_with_typeID<TemplateAndPointer>());
    this.raw_self_ref_ptr = i.read_ptr_with_typeID<SelfReferential>();
    this.self_ptr = i.read_ptr_with_typeID<MegaComplexClass>();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_Array(this.complex_vector, (v)=>o.write_ptr_with_typeID(v.value));
    o.write_ptr_with_typeID(this.raw_self_ref_ptr.value);
    o.write_ptr_with_typeID(this.self_ptr.value);
    return this;
  }
}
export class SuperComplexContainer extends Base {
  static typeID = E_StreamPunkType.SuperComplexContainer;
  bits: Array<boolean> = new Array();
  opt_int: number | null = null;
  file_path: string = '';
  atomic_placeholder: number = 0;
  sptr_test: SpRef<Test| null> = new SpRef(null, 0n);
  wptr_test: SpRef<Test| null> = new SpRef(null, 0n);
  uptr_test: SpRef<Test| null> = new SpRef(null, 0n);
  tuple_member: [number,string,boolean] = [0.0,"",false];
  map_of_umaps: Map<string,Map<number,number>> = new Map();
  variant_member: SpVariant<[number|string|SpRef<AllBasicTypes| null>]> = new SpVariant<[number|string|SpRef<AllBasicTypes| null>]>(0);
  deq_list_flist: Array<Array<Array<number>>> = new Array();
  vec_arr_str: Array<SpArray<string>> = new Array();
  set_of_usets: Set<Array<string>> = new Set();
  kitchen_sink: Map<string,Array<[SpRef<SelfReferential| null>,SpRef<PointerContainer| null>,Array<SpVariant<[SpArray<string>|Array<Set<string>>]>>] | null>> = new Map();
  from(i: I): this {
    super.from(i);
    this.bits = i.read_bitset();
    this.opt_int = i.read_optional(()=>i.read_i32());
    this.file_path = i.read_string();
    this.atomic_placeholder = i.read_i32();
    this.sptr_test = i.read_ptr_with_typeID<Test>();
    this.wptr_test = i.read_ptr_with_typeID<Test>();
    this.uptr_test = i.read_ptr_with_typeID<Test>();
    this.tuple_member = [i.read_f64(),i.read_string(),i.read_bl()];
    this.map_of_umaps = i.read_map(()=>i.read_string(),()=>i.read_map(()=>i.read_i32(),()=>i.read_f64()));
    this.variant_member = new SpVariant<[number|string|SpRef<AllBasicTypes| null>]>(i.read_variant([()=> i.read_i32(),()=> i.read_string(),()=> i.read_ptr_with_typeID<AllBasicTypes>()]));
    this.deq_list_flist = i.read_Array(()=>i.read_Array(()=>i.read_Array(()=>i.read_i32())));
    this.vec_arr_str = i.read_Array(()=>i.read_SpArray(5, ()=>i.read_string()));
    this.set_of_usets = i.read_set(()=>i.read_Array(()=>i.read_string()));
    this.kitchen_sink = i.read_map(()=>i.read_string(),()=>i.read_Array(()=>i.read_optional(()=>[i.read_ptr_with_typeID<SelfReferential>(),i.read_ptr_with_typeID<PointerContainer>(),i.read_Array(()=>new SpVariant<[SpArray<string>|Array<Set<string>>]>(i.read_variant([()=> i.read_SpArray(8, ()=>i.read_ch()),()=> i.read_Array(()=>i.read_set(()=>i.read_string()))])))])));
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_bitset(this.bits);
    o.write_optional(this.opt_int, (v)=>{o.write_i32(v)});
    o.write_string(this.file_path);
    o.write_i32(this.atomic_placeholder);
    o.write_ptr_with_typeID(this.sptr_test.value);
    o.write_ptr_with_typeID(this.wptr_test.value);
    o.write_ptr_with_typeID(this.uptr_test.value);
    o.write_f64(this.tuple_member[0]);o.write_string(this.tuple_member[1]);o.write_bl(this.tuple_member[2]);;
    o.write_map(this.map_of_umaps, (k)=>o.write_string(k), (v)=>o.write_map(v, (k)=>o.write_i32(k), (v)=>o.write_f64(v)));
    o.write_variant(this.variant_member.value, this.variant_member.typeIndex, [(v:any)=> o.write_i32(v),(v:any)=> o.write_string(v),(v:any)=> o.write_ptr_with_typeID(v.value)]);
    o.write_Array(this.deq_list_flist, (v)=>o.write_Array(v, (v)=>o.write_Array(v, (v)=>o.write_i32(v))));
    o.write_Array(this.vec_arr_str, (v)=>o.write_SpArray(v, (v)=>o.write_string(v)));
    o.write_set(this.set_of_usets, (v)=>o.write_Array(v, (v)=>o.write_string(v)));
    o.write_map(this.kitchen_sink, (k)=>o.write_string(k), (v)=>o.write_Array(v, (v)=>o.write_optional(v, (v)=>{o.write_ptr_with_typeID(v[0].value);o.write_ptr_with_typeID(v[1].value);o.write_Array(v[2], (v)=>o.write_variant(v.value, v.typeIndex, [(v:any)=> o.write_SpArray(v, (v:any)=>o.write_ch(v)),(v:any)=> o.write_Array(v, (v:any)=>o.write_set(v, (v:any)=>o.write_string(v)))]));})));
    return this;
  }
}
export class Test extends Base {
  static typeID = E_StreamPunkType.Test;
  name: string = "";
  pwd: string = "";
  gateWay: string = "";
  mask: string = "";
  ip: string = "";
  dns1: string = "";
  dns2: string = "";
  from(i: I): this {
    super.from(i);
    this.name = i.read_string();
    this.pwd = i.read_string();
    this.gateWay = i.read_string();
    this.mask = i.read_string();
    this.ip = i.read_string();
    this.dns1 = i.read_string();
    this.dns2 = i.read_string();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_string(this.name);
    o.write_string(this.pwd);
    o.write_string(this.gateWay);
    o.write_string(this.mask);
    o.write_string(this.ip);
    o.write_string(this.dns1);
    o.write_string(this.dns2);
    return this;
  }
}
export class MQTT extends Base {
  static typeID = E_StreamPunkType.MQTT;
  host: string = "";
  user: string = "";
  pwd: string = "";
  from(i: I): this {
    super.from(i);
    this.host = i.read_string();
    this.user = i.read_string();
    this.pwd = i.read_string();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_string(this.host);
    o.write_string(this.user);
    o.write_string(this.pwd);
    return this;
  }
}
export class PointerDemo extends Base {
  static typeID = E_StreamPunkType.PointerDemo;
  rawPtr: SpRef<Test| null> = new SpRef(null, 0n);
  sharedPtr: SpRef<MQTT| null> = new SpRef(null, 0n);
  uniquePtr: SpRef<Test| null> = new SpRef(null, 0n);
  weakSelf: SpRef<PointerDemo| null> = new SpRef(null, 0n);
  from(i: I): this {
    super.from(i);
    this.rawPtr = i.read_ptr_with_typeID<Test>();
    this.sharedPtr = i.read_ptr_with_typeID<MQTT>();
    this.uniquePtr = i.read_ptr_with_typeID<Test>();
    this.weakSelf = i.read_ptr_with_typeID<PointerDemo>();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_ptr_with_typeID(this.rawPtr.value);
    o.write_ptr_with_typeID(this.sharedPtr.value);
    o.write_ptr_with_typeID(this.uniquePtr.value);
    o.write_ptr_with_typeID(this.weakSelf.value);
    return this;
  }
}
export class ContainerDemo extends Base {
  static typeID = E_StreamPunkType.ContainerDemo;
  testPtrs: Array<SpRef<Test| null>> = new Array();
  selfContainer: SpRef<ContainerDemo| null> = new SpRef(null, 0n);
  allObjects: Set<SpRef<Base| null>> = new Set();
  mqttConfigs: Map<string,SpRef<MQTT| null>> = new Map();
  mqttConfigs2: Map<string,SpRef<MQTT| null>> = new Map();
  from(i: I): this {
    super.from(i);
    this.testPtrs = i.read_Array(()=>i.read_ptr_with_typeID<Test>());
    this.selfContainer = i.read_ptr_with_typeID<ContainerDemo>();
    this.allObjects = i.read_set(()=>i.read_ptr_with_typeID<Base>());
    this.mqttConfigs = i.read_map(()=>i.read_string(),()=>i.read_ptr_with_typeID<MQTT>());
    this.mqttConfigs2 = i.read_map(()=>i.read_string(),()=>i.read_ptr_with_typeID<MQTT>());
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_Array(this.testPtrs, (v)=>o.write_ptr_with_typeID(v.value));
    o.write_ptr_with_typeID(this.selfContainer.value);
    o.write_set(this.allObjects, (v)=>o.write_ptr_with_typeID(v.value));
    o.write_map(this.mqttConfigs, (k)=>o.write_string(k), (v)=>o.write_ptr_with_typeID(v.value));
    o.write_map(this.mqttConfigs2, (k)=>o.write_string(k), (v)=>o.write_ptr_with_typeID(v.value));
    return this;
  }
}
export class NetworkSystem extends Base {
  static typeID = E_StreamPunkType.NetworkSystem;
  mainContainer: SpRef<ContainerDemo| null> = new SpRef(null, 0n);
  activeTests: Array<SpRef<Test| null>> = new Array();
  mqttInstances: Array<SpRef<MQTT| null>> = new Array();
  demos: Array<SpRef<PointerDemo| null>> = new Array();
  from(i: I): this {
    super.from(i);
    this.mainContainer = i.read_ptr_with_typeID<ContainerDemo>();
    this.activeTests = i.read_Array(()=>i.read_ptr_with_typeID<Test>());
    this.mqttInstances = i.read_Array(()=>i.read_ptr_with_typeID<MQTT>());
    this.demos = i.read_Array(()=>i.read_ptr_with_typeID<PointerDemo>());
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_ptr_with_typeID(this.mainContainer.value);
    o.write_Array(this.activeTests, (v)=>o.write_ptr_with_typeID(v.value));
    o.write_Array(this.mqttInstances, (v)=>o.write_ptr_with_typeID(v.value));
    o.write_Array(this.demos, (v)=>o.write_ptr_with_typeID(v.value));
    return this;
  }
}
export class Device extends Base {
  static typeID = E_StreamPunkType.Device;
  deviceId: string = "";
  manufacturer: string = "";
  lastSeen: Date = new Date();
  from(i: I): this {
    super.from(i);
    this.deviceId = i.read_string();
    this.manufacturer = i.read_string();
    this.lastSeen = i.read_stream_punk_time();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_string(this.deviceId);
    o.write_string(this.manufacturer);
    o.write_stream_punk_time(this.lastSeen);
    return this;
  }
}
export class NetworkDevice extends Device {
  static typeID = E_StreamPunkType.NetworkDevice;
  ipAddress: string = "";
  macAddress: string = "";
  port: number = 0;
  from(i: I): this {
    super.from(i);
    this.ipAddress = i.read_string();
    this.macAddress = i.read_string();
    this.port = i.read_u16();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_string(this.ipAddress);
    o.write_string(this.macAddress);
    o.write_u16(this.port);
    return this;
  }
}
export class Sensor extends Device {
  static typeID = E_StreamPunkType.Sensor;
  currentValue: number = 0.0;
  minValue: number = 0.0;
  maxValue: number = 0.0;
  samplingInterval: Date = new Date();
  from(i: I): this {
    super.from(i);
    this.currentValue = i.read_f64();
    this.minValue = i.read_f64();
    this.maxValue = i.read_f64();
    this.samplingInterval = i.read_stream_punk_time();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_f64(this.currentValue);
    o.write_f64(this.minValue);
    o.write_f64(this.maxValue);
    o.write_stream_punk_time(this.samplingInterval);
    return this;
  }
}
export class TemperatureSensor extends Sensor {
  static typeID = E_StreamPunkType.TemperatureSensor;
  isCelsius: boolean = false;
  calibrationOffset: number = 0.0;
  from(i: I): this {
    super.from(i);
    this.isCelsius = i.read_bl();
    this.calibrationOffset = i.read_f64();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_bl(this.isCelsius);
    o.write_f64(this.calibrationOffset);
    return this;
  }
}
export class SmartHomeSystem extends Base {
  static typeID = E_StreamPunkType.SmartHomeSystem;
  allDevices: Array<SpRef<Device| null>> = new Array();
  sensors: Map<string,SpRef<Sensor| null>> = new Map();
  mainThermostat: SpRef<TemperatureSensor| null> = new SpRef(null, 0n);
  network: SpRef<NetworkSystem| null> = new SpRef(null, 0n);
  from(i: I): this {
    super.from(i);
    this.allDevices = i.read_Array(()=>i.read_ptr_with_typeID<Device>());
    this.sensors = i.read_map(()=>i.read_string(),()=>i.read_ptr_with_typeID<Sensor>());
    this.mainThermostat = i.read_ptr_with_typeID<TemperatureSensor>();
    this.network = i.read_ptr_with_typeID<NetworkSystem>();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_Array(this.allDevices, (v)=>o.write_ptr_with_typeID(v.value));
    o.write_map(this.sensors, (k)=>o.write_string(k), (v)=>o.write_ptr_with_typeID(v.value));
    o.write_ptr_with_typeID(this.mainThermostat.value);
    o.write_ptr_with_typeID(this.network.value);
    return this;
  }
}
export class MultiLevelContainer extends Base {
  static typeID = E_StreamPunkType.MultiLevelContainer;
  baseObj: SpRef<Base| null> = new SpRef(null, 0n);
  deviceList: Array<SpRef<Device| null>> = new Array();
  sensorMap: Map<string,SpRef<Sensor| null>> = new Map();
  selfRef: SpRef<MultiLevelContainer| null> = new SpRef(null, 0n);
  from(i: I): this {
    super.from(i);
    this.baseObj = i.read_ptr_with_typeID<Base>();
    this.deviceList = i.read_Array(()=>i.read_ptr_with_typeID<Device>());
    this.sensorMap = i.read_map(()=>i.read_string(),()=>i.read_ptr_with_typeID<Sensor>());
    this.selfRef = i.read_ptr_with_typeID<MultiLevelContainer>();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_ptr_with_typeID(this.baseObj.value);
    o.write_Array(this.deviceList, (v)=>o.write_ptr_with_typeID(v.value));
    o.write_map(this.sensorMap, (k)=>o.write_string(k), (v)=>o.write_ptr_with_typeID(v.value));
    o.write_ptr_with_typeID(this.selfRef.value);
    return this;
  }
}
export class SptrTest extends Base {
  static typeID = E_StreamPunkType.SptrTest;
  test1: SpRef<Array<SpRef<Device| null>>| null> = new SpRef(null, 0n);
  from(i: I): this {
    super.from(i);
    this.test1 = i.read_ptr(()=>i.read_Array(()=>i.read_ptr_with_typeID<Device>()));
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_ptr(this.test1.value, this.test1.address, (v)=>o.write_Array(v, (v)=>o.write_ptr_with_typeID(v.value)));
    return this;
  }
}
export class MousePosition extends Base {
  static typeID = E_StreamPunkType.MousePosition;
  x: number = 0;
  y: number = 0;
  from(i: I): this {
    super.from(i);
    this.x = i.read_i32();
    this.y = i.read_i32();
    return this;
  }
  to(o: O): this {
    super.to(o);
    o.write_i32(this.x);
    o.write_i32(this.y);
    return this;
  }
}
