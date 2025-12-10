

export class SpRef {
    value;
    address;
    constructor(initialValue, address) {
        this.value = initialValue
        this.address = address
    }
}

export class SpArray {
    _data;
    constructor(size, initializer) {
        this._data = new Array(size).fill(initializer);
    }
    get size() { return this._data.length; }
    at(index) {
        if (index < 0 || index >= this.size) {
            throw new RangeError("Index out of bounds");
        }
        return this._data[index];
    }
    set(index, value) {
        if (index < 0 || index >= this.size) {
            throw new RangeError("Index out of bounds");
        }
        this._data[index] = value;
    }
}

export class SpVariant {
    _value;
    _typeIndex = -1;
    constructor(value) {
        if (value !== undefined) {
            this.set(value);
        }
    }
    set(value) {
        this._value = value;
        this._updateTypeIndex();
    }
    get value() { return this._value; }
    get typeIndex() { return this._typeIndex; }
    _updateTypeIndex() {
        if (this._value === undefined || this._value === null) {
            this._typeIndex = -1;
            return;
        }
        // const currentConstructor = this._value.constructor;
        for (let i = 0; i < this.possibleTypes.length; i++) {
            const type = this.possibleTypes[i];
            if (typeof this._value === type) {
                this._typeIndex = i;
                return;
            }
            if (typeof type === 'function' && this._value && this._value instanceof type) {
                this._typeIndex = i;
                return;
            }
        }
        this._typeIndex = -1;
    }
    get possibleTypes() { return []; }
}

export class I {
    view;
    offset;
    objectMap;
    decoder;
    decoder16;

    hasMoreData() {
        return this.offset < this.view.byteLength;
    }

    constructor(buffer, initialOffset = 0) {
        this.view = new DataView(buffer)
        this.offset = initialOffset
        this.objectMap = new Map()
        this.decoder = new TextDecoder('utf-8')
        this.decoder16 = new TextDecoder('utf-16le')
    }

    read_u8() {
        const value = this.view.getUint8(this.offset)
        this.offset += 1
        return value
    }

    read_u16() {
        const value = this.view.getUint16(this.offset, true)
        this.offset += 2
        return value
    }

    read_u32() {
        const value = this.view.getUint32(this.offset, true)
        this.offset += 4
        return value
    }

    read_u64() {
        const value = this.view.getBigUint64(this.offset, true)
        this.offset += 8
        return value
    }

    read_i8() {
        const value = this.view.getInt8(this.offset)
        this.offset += 1
        return value
    }

    read_i16() {
        const value = this.view.getInt16(this.offset, true)
        this.offset += 2
        return value
    }

    read_i32() {
        const value = this.view.getInt32(this.offset, true)
        this.offset += 4
        return value
    }

    read_i64() {
        const value = this.view.getBigInt64(this.offset, true)
        this.offset += 8
        return value
    }

    read_f32() {
        const value = this.view.getFloat32(this.offset, true)
        this.offset += 4
        return value
    }

    read_f64() {
        const value = this.view.getFloat64(this.offset, true)
        this.offset += 8
        return value
    }

    read_ch() {
        const value = String.fromCharCode(this.view.getUint8(this.offset))
        this.offset += 1
        return value
    }

    read_ch8() {
        const value = String.fromCharCode(this.view.getUint8(this.offset))
        this.offset += 1
        return value
    }

    read_ch16() {
        const value = String.fromCharCode(this.view.getUint16(this.offset, true))
        this.offset += 2
        return value
    }

    read_ch32() {
        const codePoint = this.view.getUint32(this.offset, true)
        this.offset += 4
        if (codePoint >= 0 && codePoint <= 0x10FFFF) {
            return String.fromCodePoint(codePoint)
        }
        else {
            return ""
        }
    }

    read_bl() {
        const value = this.view.getUint8(this.offset) !== 0
        this.offset += 1
        return value
    }

    align(boundary) {
        this.offset += (boundary - (this.offset % boundary)) % boundary
    }

    read_sz() {
        return this.read_u32()
    }

    read_bytes(len) {
        this.offset += len;
    }

    read_string() {
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

    read_u8string() {
        const len = Number(this.read_sz())
        const value = this.decoder.decode(
            new Uint8Array(this.view.buffer, this.offset, len),
        )
        this.offset += len
        return value
    }

    read_u16string() {
        const len = Number(this.read_sz())
        const byteLength = len * 2
        const value = this.decoder16.decode(
            new Uint8Array(this.view.buffer, this.offset, byteLength),
        )
        this.offset += byteLength
        return value
    }

    read_u32string() {
        const len = Number(this.read_sz());
        const byteLength = len * 4;
        const dataSlice = new Uint8Array(this.view.buffer,this.offset,byteLength);
        this.offset += byteLength;
        return dataSlice;
    }

    read_ptr_with_typeID(Constructor, ...constructorArgs) {
        const address = this.read_u64();
        if (address === 0n) {
            return new SpRef(null, 0n);
        }
        if (this.objectMap.has(address)) {
            return this.objectMap.get(address);
        }
        const ref = new SpRef(null, address);
        this.objectMap.set(address, ref);
        var r = new Constructor(...constructorArgs);
        r.from(i);
        ref.value = r;
        return ref;
    }


    read_ptr(elementReader) {
        const address = this.read_u64();
        if (address === 0n) {
            return new SpRef(null, 0n);
        }
        if (this.objectMap.has(address)) {
            return this.objectMap.get(address);
        }
        const value = elementReader();
        const ref = new SpRef(value, address);
        this.objectMap.set(address, ref);
        return ref;
    }

    read_array(count, elementReader) {
        const arr = []
        for (let i = 0; i < count; i++) {
            arr.push(elementReader())
        }
        return arr
    }

    read_Array(elementReader) {
        const size = Number(this.read_sz())
        return this.read_array(size, elementReader)
    }

    read_set(elementReader) {
        const arr = this.read_Array(elementReader)
        return new Set(arr)
    }

    read_unordered_set(elementReader) {
        return this.read_set(elementReader)
    }

    read_map(keyReader, valueReader) {
        const size = Number(this.read_sz())
        const map = new Map()
        for (let i = 0; i < size; i++) {
            const key = keyReader()
            const value = valueReader()
            map.set(key, value)
        }
        return map
    }

    read_unordered_map(keyReader, valueReader) {
        return this.read_map(keyReader, valueReader)
    }

    read_vector(elementReader) {
        return this.read_Array(elementReader);
    }

    read_deque(elementReader) {
        return this.read_Array(elementReader)
    }

    read_list(elementReader) {
        return this.read_Array(elementReader)
    }

    read_forward_list(elementReader) {
        return this.read_Array(elementReader)
    }

    read_SpArray(size, elementReader) {
        const arr = new SpArray(size);
        for (let i = 0; i < size; i++) {
            arr.set(i, elementReader());
        }
        return arr;
    }

    read_std_string() {
        return this.read_string()
    }

    read_bitset() {
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

    read_optional(valueReader) {
        const hasValue = this.read_bl();
        if (hasValue) {
            return valueReader();
        }
        return null;
    }

    read_variant(readers) {
        const index = this.read_u32();
        const readVariantAt = (currIdx) => {
            if (currIdx === index) {
                return readers[currIdx]();
            }
            if (currIdx + 1 < readers.length) {
                return readVariantAt(currIdx + 1);
            }
            throw new Error(`Variant index ${index} out of range`);
        };
        var r = readVariantAt(0);
        return r;
    }

    read_stream_punk_time() {
        const sec = this.read_i64();
        const attoSec = this.read_i64();
        const totalMs = Number(sec) * 1000 + Number(attoSec / 1000000000000000n);
        return new Date(totalMs);
    }
}


export class O {
    buffers = [];
    objectMap;
    encoder;

    constructor() {
        this.encoder = new TextEncoder();
        this.objectMap = new Map();
    }

    write_u8(value) {
        const buffer = new ArrayBuffer(1);
        new DataView(buffer).setUint8(0, value);
        this.buffers.push(buffer);
    }

    write_u16(value) {
        const buffer = new ArrayBuffer(2);
        new DataView(buffer).setUint16(0, value, true);
        this.buffers.push(buffer);
    }

    write_u32(value) {
        const buffer = new ArrayBuffer(4);
        new DataView(buffer).setUint32(0, value, true);
        this.buffers.push(buffer);
    }

    write_u64(value) {
        const buffer = new ArrayBuffer(8);
        new DataView(buffer).setBigUint64(0, value, true);
        this.buffers.push(buffer);
    }

    write_i8(value) {
        const buffer = new ArrayBuffer(1);
        new DataView(buffer).setInt8(0, value);
        this.buffers.push(buffer);
    }

    write_i16(value) {
        const buffer = new ArrayBuffer(2);
        new DataView(buffer).setInt16(0, value, true);
        this.buffers.push(buffer);
    }

    write_i32(value) {
        const buffer = new ArrayBuffer(4);
        new DataView(buffer).setInt32(0, value, true);
        this.buffers.push(buffer);
    }

    write_i64(value) {
        const buffer = new ArrayBuffer(8);
        new DataView(buffer).setBigInt64(0, value, true);
        this.buffers.push(buffer);
    }

    write_f32(value) {
        const buffer = new ArrayBuffer(4);
        new DataView(buffer).setFloat32(0, value, true);
        this.buffers.push(buffer);
    }

    write_f64(value) {
        const buffer = new ArrayBuffer(8);
        new DataView(buffer).setFloat64(0, value, true);
        this.buffers.push(buffer);
    }

    write_ch(value) {
        this.write_ch16(value);
    }

    write_ch8(value) {
        if (value.length > 0) {
            this.write_u8(value.charCodeAt(0));
        } else {
            this.write_u8(0);
        }
    }

    write_ch16(value) {
        if (value.length > 0) {
            this.write_u16(value.charCodeAt(0));
        } else {
            this.write_u16(0);
        }
    }

    write_ch32(value) {
        if (value.length > 0) {
            const codePoint = value.codePointAt(0) || 0;
            this.write_u32(codePoint);
        } else {
            this.write_u32(0);
        }
    }

    write_bl(value) {
        this.write_u8(value ? 1 : 0);
    }

    write_sz(value) {
        this.write_u32(value)
    }

    write_string(value) {
        const encoded = this.encoder.encode(value);
        this.write_sz(encoded.length);
        this.write_uint8_array(encoded);
    }

    write_u8string(value) {
        const encoded = this.encoder.encode(value);
        this.write_sz(encoded.length);
        this.write_uint8_array(encoded);
    }

    write_u16string(value) {
        const bytes = new Uint8Array(value.length * 2);
        for (let i = 0; i < value.length; i++) {
            const codeUnit = value.charCodeAt(i);
            const offset = i * 2;
            bytes[offset] = codeUnit & 0xFF;
            bytes[offset + 1] = (codeUnit >> 8) & 0xFF;
        }
        this.write_uint8_array(bytes);
    }

    write_u32string(value) {
        this.write_sz(value.length/4);
        this.write_uint8_array(value);
    }

    write_uint8_array(data) {
        const buffer = new ArrayBuffer(data.length);
        new Uint8Array(buffer).set(data);
        this.buffers.push(buffer);
    }

    write_ptr(value, address, writeValue) {
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

    write_array(arr, writeElement) {
        for (let i = 0; i < arr.length; i++) {
            writeElement(arr[i])
        }
    }

    write_Array(arr, writeElement) {
        this.write_sz(arr.length)
        this.write_array(arr, writeElement)
    }

    write_set(set, writeElement) {
        const arr = Array.from(set)
        this.write_Array(arr, writeElement)
    }

    write_unordered_set(set, writeElement) {
        this.write_set(set, writeElement)
    }

    write_map(map, writeKey, writeValue) {
        this.write_sz(map.size)
        map.forEach((value, key) => {
            writeKey(key)
            writeValue(value)
        })
    }

    write_unordered_map(map, writeKey, writeValue) {
        this.write_map(map, writeKey, writeValue)
    }

    write_vector(vec, writeElement) {
        this.write_Array(vec, writeElement)
    }

    write_deque(deq, writeElement) {
        this.write_Array(deq, writeElement)
    }

    write_list(list, writeElement) {
        this.write_Array(list, writeElement)
    }

    write_forward_list(list, writeElement) {
        this.write_Array(list, writeElement)
    }

    write_SpArray(arr, writeElement) {
        for (let i = 0; i < arr.size; i++) {
            writeElement(arr.at(i))
        }
    }

    write_bitset(bits) {
        this.write_u32(bits.length)
        const byteLength = Math.ceil(bits.length / 8)
        const bytes = new Uint8Array(byteLength)
        for (let i = 0; i < bits.length; i++) {
            if (bits[i]) {
                const byteIndex = Math.floor(i / 8)
                const bitIndex = i % 8
                bytes[byteIndex] |= (1 << bitIndex)
            }
        }
        this.write_uint8_array(bytes)
    }

    write_optional(value, writeValue) {
        this.write_bl(value !== null)
        if (value !== null) {
            writeValue(value)
        }
    }

    write_variant(value, index, writers) {
        this.write_u32(index)
        writers[index](value)
    }

    write_stream_punk_time(date) {
        const totalMs = date.getTime()
        const sec = BigInt(Math.floor(totalMs / 1000))
        const attoSec = BigInt((totalMs % 1000) * 1000000000000000)
        this.write_i64(sec)
        this.write_i64(attoSec)
    }

    to_array_buffer() {
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
