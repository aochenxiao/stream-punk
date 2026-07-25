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
  Ingredient = 47,
  MenuItem = 48,
  RecipeItem = 49,
  Recipe = 50,
  OrderItem = 51,
  Table = 52,
  Order = 53,
  ServerState = 54,
  PlaceOrderRequest = 55,
  UpdateStatusRequest = 56,
  PaymentRequest = 57,
  RoleLoginRequest = 58,
  Notification = 59,
  ChangeTableRequest = 60,
  UrgeDishRequest = 61,
  MergeOrdersRequest = 62,
  RoleAssigned = 63,
}

export class Base {
  static readonly typeID: E_StreamPunkType = E_StreamPunkType.Base;
  from(i: I): this { void i; return this; }
  to(o: O): this  { void o; return this; }
}

const __typeFactory = new Map<number, () => Base>();
__typeFactory.set(E_StreamPunkType.Ingredient, () => new Ingredient());
__typeFactory.set(E_StreamPunkType.MenuItem, () => new MenuItem());
__typeFactory.set(E_StreamPunkType.RecipeItem, () => new RecipeItem());
__typeFactory.set(E_StreamPunkType.Recipe, () => new Recipe());
__typeFactory.set(E_StreamPunkType.OrderItem, () => new OrderItem());
__typeFactory.set(E_StreamPunkType.Table, () => new Table());
__typeFactory.set(E_StreamPunkType.Order, () => new Order());
__typeFactory.set(E_StreamPunkType.ServerState, () => new ServerState());
__typeFactory.set(E_StreamPunkType.PlaceOrderRequest, () => new PlaceOrderRequest());
__typeFactory.set(E_StreamPunkType.UpdateStatusRequest, () => new UpdateStatusRequest());
__typeFactory.set(E_StreamPunkType.PaymentRequest, () => new PaymentRequest());
__typeFactory.set(E_StreamPunkType.RoleLoginRequest, () => new RoleLoginRequest());
__typeFactory.set(E_StreamPunkType.Notification, () => new Notification());
__typeFactory.set(E_StreamPunkType.ChangeTableRequest, () => new ChangeTableRequest());
__typeFactory.set(E_StreamPunkType.UrgeDishRequest, () => new UrgeDishRequest());
__typeFactory.set(E_StreamPunkType.MergeOrdersRequest, () => new MergeOrdersRequest());
__typeFactory.set(E_StreamPunkType.RoleAssigned, () => new RoleAssigned());

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

export class Ingredient extends Base {
    static readonly typeID = E_StreamPunkType.Ingredient;

    id: number;
    name: string;
    unit: string;
    stock: number;
    minStock: number;

    constructor() {
        super();
        this.id = 0;
        this.name = "";
        this.unit = "";
        this.stock = 0.0;
        this.minStock = 0.0;
    }

    from(i: I): this {
        super.from(i);
        this.id = i.readI32();
        this.name = i.readString();
        this.unit = i.readString();
        this.stock = i.readF64();
        this.minStock = i.readF64();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.id);
        o.writeString(this.name);
        o.writeString(this.unit);
        o.writeF64(this.stock);
        o.writeF64(this.minStock);
        return this;
    }
}
export class MenuItem extends Base {
    static readonly typeID = E_StreamPunkType.MenuItem;

    id: number;
    name: string;
    price: number;
    category: string;
    available: boolean;

    constructor() {
        super();
        this.id = 0;
        this.name = "";
        this.price = 0.0;
        this.category = "";
        this.available = false;
    }

    from(i: I): this {
        super.from(i);
        this.id = i.readI32();
        this.name = i.readString();
        this.price = i.readF64();
        this.category = i.readString();
        this.available = i.readBl();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.id);
        o.writeString(this.name);
        o.writeF64(this.price);
        o.writeString(this.category);
        o.writeBl(this.available);
        return this;
    }
}
export class RecipeItem extends Base {
    static readonly typeID = E_StreamPunkType.RecipeItem;

    ingredientId: number;
    ingredientName: string;
    quantity: number;

    constructor() {
        super();
        this.ingredientId = 0;
        this.ingredientName = "";
        this.quantity = 0.0;
    }

    from(i: I): this {
        super.from(i);
        this.ingredientId = i.readI32();
        this.ingredientName = i.readString();
        this.quantity = i.readF64();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.ingredientId);
        o.writeString(this.ingredientName);
        o.writeF64(this.quantity);
        return this;
    }
}
export class Recipe extends Base {
    static readonly typeID = E_StreamPunkType.Recipe;

    menuItemId: number;
    items: Array<RecipeItem>;

    constructor() {
        super();
        this.menuItemId = 0;
        this.items = [];
    }

    from(i: I): this {
        super.from(i);
        this.menuItemId = i.readI32();
        this.items = i.readArray(() => new RecipeItem().from(i));
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.menuItemId);
        o.writeArray(this.items, (v) => { v.to(o) });
        return this;
    }
}
export class OrderItem extends Base {
    static readonly typeID = E_StreamPunkType.OrderItem;

    menuItemId: number;
    menuItemName: string;
    quantity: number;
    status: number;
    note: string;

    constructor() {
        super();
        this.menuItemId = 0;
        this.menuItemName = "";
        this.quantity = 0;
        this.status = 0;
        this.note = "";
    }

    from(i: I): this {
        super.from(i);
        this.menuItemId = i.readI32();
        this.menuItemName = i.readString();
        this.quantity = i.readI32();
        this.status = i.readI32();
        this.note = i.readString();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.menuItemId);
        o.writeString(this.menuItemName);
        o.writeI32(this.quantity);
        o.writeI32(this.status);
        o.writeString(this.note);
        return this;
    }
}
export class Table extends Base {
    static readonly typeID = E_StreamPunkType.Table;

    id: number;
    name: string;
    type: number;
    status: number;
    capacity: number;

    constructor() {
        super();
        this.id = 0;
        this.name = "";
        this.type = 0;
        this.status = 0;
        this.capacity = 0;
    }

    from(i: I): this {
        super.from(i);
        this.id = i.readI32();
        this.name = i.readString();
        this.type = i.readI32();
        this.status = i.readI32();
        this.capacity = i.readI32();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.id);
        o.writeString(this.name);
        o.writeI32(this.type);
        o.writeI32(this.status);
        o.writeI32(this.capacity);
        return this;
    }
}
export class Order extends Base {
    static readonly typeID = E_StreamPunkType.Order;

    id: number;
    tableId: number;
    tableName: string;
    items: Array<OrderItem>;
    status: number;
    totalPrice: number;
    discount: number;
    paymentMethod: string;
    note: string;

    constructor() {
        super();
        this.id = 0;
        this.tableId = 0;
        this.tableName = "";
        this.items = [];
        this.status = 0;
        this.totalPrice = 0.0;
        this.discount = 0.0;
        this.paymentMethod = "";
        this.note = "";
    }

    from(i: I): this {
        super.from(i);
        this.id = i.readI32();
        this.tableId = i.readI32();
        this.tableName = i.readString();
        this.items = i.readArray(() => new OrderItem().from(i));
        this.status = i.readI32();
        this.totalPrice = i.readF64();
        this.discount = i.readF64();
        this.paymentMethod = i.readString();
        this.note = i.readString();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.id);
        o.writeI32(this.tableId);
        o.writeString(this.tableName);
        o.writeArray(this.items, (v) => { v.to(o) });
        o.writeI32(this.status);
        o.writeF64(this.totalPrice);
        o.writeF64(this.discount);
        o.writeString(this.paymentMethod);
        o.writeString(this.note);
        return this;
    }
}
export class ServerState extends Base {
    static readonly typeID = E_StreamPunkType.ServerState;

    orders: Array<Order>;
    tables: Array<Table>;
    menu: Array<MenuItem>;
    ingredients: Array<Ingredient>;
    recipes: Array<Recipe>;

    constructor() {
        super();
        this.orders = [];
        this.tables = [];
        this.menu = [];
        this.ingredients = [];
        this.recipes = [];
    }

    from(i: I): this {
        super.from(i);
        this.orders = i.readArray(() => new Order().from(i));
        this.tables = i.readArray(() => new Table().from(i));
        this.menu = i.readArray(() => new MenuItem().from(i));
        this.ingredients = i.readArray(() => new Ingredient().from(i));
        this.recipes = i.readArray(() => new Recipe().from(i));
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeArray(this.orders, (v) => { v.to(o) });
        o.writeArray(this.tables, (v) => { v.to(o) });
        o.writeArray(this.menu, (v) => { v.to(o) });
        o.writeArray(this.ingredients, (v) => { v.to(o) });
        o.writeArray(this.recipes, (v) => { v.to(o) });
        return this;
    }
}
export class PlaceOrderRequest extends Base {
    static readonly typeID = E_StreamPunkType.PlaceOrderRequest;

    tableId: number;
    items: Array<OrderItem>;

    constructor() {
        super();
        this.tableId = 0;
        this.items = [];
    }

    from(i: I): this {
        super.from(i);
        this.tableId = i.readI32();
        this.items = i.readArray(() => new OrderItem().from(i));
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.tableId);
        o.writeArray(this.items, (v) => { v.to(o) });
        return this;
    }
}
export class UpdateStatusRequest extends Base {
    static readonly typeID = E_StreamPunkType.UpdateStatusRequest;

    orderId: number;
    itemIndex: number;
    newStatus: number;
    note: string;

    constructor() {
        super();
        this.orderId = 0;
        this.itemIndex = 0;
        this.newStatus = 0;
        this.note = "";
    }

    from(i: I): this {
        super.from(i);
        this.orderId = i.readI32();
        this.itemIndex = i.readI32();
        this.newStatus = i.readI32();
        this.note = i.readString();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.orderId);
        o.writeI32(this.itemIndex);
        o.writeI32(this.newStatus);
        o.writeString(this.note);
        return this;
    }
}
export class PaymentRequest extends Base {
    static readonly typeID = E_StreamPunkType.PaymentRequest;

    tableId: number;
    paymentMethod: string;
    discount: number;
    authCode: string;

    constructor() {
        super();
        this.tableId = 0;
        this.paymentMethod = "";
        this.discount = 0.0;
        this.authCode = "";
    }

    from(i: I): this {
        super.from(i);
        this.tableId = i.readI32();
        this.paymentMethod = i.readString();
        this.discount = i.readF64();
        this.authCode = i.readString();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.tableId);
        o.writeString(this.paymentMethod);
        o.writeF64(this.discount);
        o.writeString(this.authCode);
        return this;
    }
}
export class RoleLoginRequest extends Base {
    static readonly typeID = E_StreamPunkType.RoleLoginRequest;

    role: number;
    name: string;

    constructor() {
        super();
        this.role = 0;
        this.name = "";
    }

    from(i: I): this {
        super.from(i);
        this.role = i.readI32();
        this.name = i.readString();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.role);
        o.writeString(this.name);
        return this;
    }
}
export class Notification extends Base {
    static readonly typeID = E_StreamPunkType.Notification;

    message: string;
    type: number;
    targetRole: number;

    constructor() {
        super();
        this.message = "";
        this.type = 0;
        this.targetRole = 0;
    }

    from(i: I): this {
        super.from(i);
        this.message = i.readString();
        this.type = i.readI32();
        this.targetRole = i.readI32();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeString(this.message);
        o.writeI32(this.type);
        o.writeI32(this.targetRole);
        return this;
    }
}
export class ChangeTableRequest extends Base {
    static readonly typeID = E_StreamPunkType.ChangeTableRequest;

    fromTableId: number;
    toTableId: number;

    constructor() {
        super();
        this.fromTableId = 0;
        this.toTableId = 0;
    }

    from(i: I): this {
        super.from(i);
        this.fromTableId = i.readI32();
        this.toTableId = i.readI32();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.fromTableId);
        o.writeI32(this.toTableId);
        return this;
    }
}
export class UrgeDishRequest extends Base {
    static readonly typeID = E_StreamPunkType.UrgeDishRequest;

    orderId: number;

    constructor() {
        super();
        this.orderId = 0;
    }

    from(i: I): this {
        super.from(i);
        this.orderId = i.readI32();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.orderId);
        return this;
    }
}
export class MergeOrdersRequest extends Base {
    static readonly typeID = E_StreamPunkType.MergeOrdersRequest;

    tableId1: number;
    tableId2: number;

    constructor() {
        super();
        this.tableId1 = 0;
        this.tableId2 = 0;
    }

    from(i: I): this {
        super.from(i);
        this.tableId1 = i.readI32();
        this.tableId2 = i.readI32();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.tableId1);
        o.writeI32(this.tableId2);
        return this;
    }
}
export class RoleAssigned extends Base {
    static readonly typeID = E_StreamPunkType.RoleAssigned;

    role: number;
    name: string;
    tableId: number;

    constructor() {
        super();
        this.role = 0;
        this.name = "";
        this.tableId = 0;
    }

    from(i: I): this {
        super.from(i);
        this.role = i.readI32();
        this.name = i.readString();
        this.tableId = i.readI32();
        return this;
    }

    to(o: O): this {
        super.to(o);
        o.writeI32(this.role);
        o.writeString(this.name);
        o.writeI32(this.tableId);
        return this;
    }
}

