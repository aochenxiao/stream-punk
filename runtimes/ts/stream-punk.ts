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

    write_optional(value: any, writeValue: (val: any) => void): void {
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

