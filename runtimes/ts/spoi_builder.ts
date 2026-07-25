// ============================================================
// SPOI — StreamPunk Operation Instruction
// TypeScript 查询/更新 Builder（自动生成）
// ============================================================

// ============================================================
// 操作码
// ============================================================

export const Op = {
  SET:       0x04,
  ADD:       0x05,
  APPEND:    0x06,
  REMOVE:    0x07,
  INSERT:    0x08,
  REPLACE:   0x09,
  RESET:     0x0A,
  SETNULL:   0x0B,
  FILTER:    0x0C,
  SELECT:    0x0D,
  SORT:      0x0E,
  REVERSE:   0x0F,
  TAKE:      0x10,
  DROP:      0x11,
  TAKEWHILE: 0x12,
  DROPWHILE: 0x13,
  DISTINCT:  0x14,
  COUNT:     0x15,
  ANY:       0x16,
  ALL:       0x17,
  FIND:      0x18,
  KEYS:      0x19,
  VALUES:    0x1A,
  JOIN:      0x1B,
  ENUMERATE: 0x1C,
  CHUNK:     0x1D,
  SLIDE:     0x1E,
  STRIDE:    0x1F,
  ADJACENT:  0x20,
  EXEC:      0x21,
} as const;

export const Cmp = {
  EQ: 0, NE: 1, LT: 2, GT: 3, LE: 4, GE: 5,
} as const;

export const PATH_DEREF = 0xFFFF;

// ============================================================
// 类型成员索引常量
// ============================================================

// SpoiTestPlayer
export const SpoiTestPlayer = {
  name: 0,
  hp: 1,
  level: 2,
  posX: 3,
} as const;

// SpoiTestState
export const SpoiTestState = {
  tick: 0,
  currentMap: 1,
  players: 2,
} as const;

// SpoiItem
export const SpoiItem = {
  name: 0,
  value: 1,
} as const;

// SpoiInventory
export const SpoiInventory = {
  items: 0,
  equipped: 1,
  gold: 2,
} as const;

// SpoiCharacter
export const SpoiCharacter = {
  name: 0,
  hp: 1,
  inventory: 2,
  weapon: 3,
  petLevel: 4,
} as const;

// SpoiWorld
export const SpoiWorld = {
  worldName: 0,
  tick: 1,
  characters: 2,
} as const;

// ============================================================
// Varint 编码
// ============================================================

function writeVarint(buf: number[], v: number): void {
  while (v >= 0x80) {
    buf.push((v & 0x7F) | 0x80);
    v >>>= 7;
  }
  buf.push(v & 0x7F);
}

function writeU32(buf: number[], v: number): void {
  buf.push(v & 0xFF, (v >>> 8) & 0xFF, (v >>> 16) & 0xFF, (v >>> 24) & 0xFF);
}

// ============================================================
// SpoiInstruction
// ============================================================

export class SpoiInstruction {
  constructor(
    public op: number,
    public path: number[],
    public operand: Uint8Array = new Uint8Array(0)
  ) {}

  serialize(): Uint8Array {
    const buf: number[] = [];
    buf.push(this.op);
    writeVarint(buf, this.path.length);
    for (const seg of this.path) writeU32(buf, seg);
    writeVarint(buf, this.operand.length);
    for (const b of this.operand) buf.push(b);
    return new Uint8Array(buf);
  }
}

// ============================================================
// SpoiStream
// ============================================================

export class SpoiStream {
  instructions: SpoiInstruction[] = [];

  build(): Uint8Array {
    const buf: number[] = [];
    writeVarint(buf, this.instructions.length);
    for (const inst of this.instructions) {
      for (const b of inst.serialize()) buf.push(b);
    }
    return new Uint8Array(buf);
  }

  buildHex(): string {
    return Array.from(this.build()).map(b => b.toString(16).padStart(2, '0')).join('');
  }
}

// ============================================================
// SpoiUpdate — 写操作 Builder
// ============================================================

export class SpoiUpdate {
  private _stream = new SpoiStream();

  set(path: number[], value: Uint8Array): this {
    this._stream.instructions.push(new SpoiInstruction(Op.SET, path, value));
    return this;
  }

  setI32(path: number[], value: number): this {
    const buf = new Uint8Array(4);
    new DataView(buf.buffer).setInt32(0, value, true);
    return this.set(path, buf);
  }

  setU32(path: number[], value: number): this {
    const buf = new Uint8Array(4);
    new DataView(buf.buffer).setUint32(0, value, true);
    return this.set(path, buf);
  }

  setF64(path: number[], value: number): this {
    const buf = new Uint8Array(8);
    new DataView(buf.buffer).setFloat64(0, value, true);
    return this.set(path, buf);
  }

  setStr(path: number[], value: string): this {
    const enc = new TextEncoder().encode(value);
    const buf: number[] = [];
    writeVarint(buf, enc.length);
    for (const b of enc) buf.push(b);
    return this.set(path, new Uint8Array(buf));
  }

  setBool(path: number[], value: boolean): this {
    return this.set(path, new Uint8Array([value ? 1 : 0]));
  }

  addI32(path: number[], delta: number): this {
    const buf = new Uint8Array(4);
    new DataView(buf.buffer).setInt32(0, delta, true);
    this._stream.instructions.push(new SpoiInstruction(Op.ADD, path, buf));
    return this;
  }

  add(path: number[], value: Uint8Array): this {
    this._stream.instructions.push(new SpoiInstruction(Op.ADD, path, value));
    return this;
  }

  append(path: number[], value: Uint8Array): this {
    this._stream.instructions.push(new SpoiInstruction(Op.APPEND, path, value));
    return this;
  }

  remove(path: number[], value: Uint8Array): this {
    this._stream.instructions.push(new SpoiInstruction(Op.REMOVE, path, value));
    return this;
  }

  insert(path: number[], value: Uint8Array): this {
    this._stream.instructions.push(new SpoiInstruction(Op.INSERT, path, value));
    return this;
  }

  replace(path: number[], value: Uint8Array): this {
    this._stream.instructions.push(new SpoiInstruction(Op.REPLACE, path, value));
    return this;
  }

  reset(path: number[]): this {
    this._stream.instructions.push(new SpoiInstruction(Op.RESET, path));
    return this;
  }

  setnull(path: number[]): this {
    this._stream.instructions.push(new SpoiInstruction(Op.SETNULL, path));
    return this;
  }

  build(): Uint8Array { return this._stream.build(); }
  buildHex(): string { return this._stream.buildHex(); }
}

// ============================================================
// SpoiQuery — 查询 Builder
// ============================================================

export class SpoiQuery {
  private _stream = new SpoiStream();

  nav(field: number): this {
    this._stream.instructions.push(new SpoiInstruction(Op.FILTER, [field]));
    return this;
  }

  filter(field: number, cmpOp: number, value: Uint8Array): this {
    const buf: number[] = [];
    writeU32(buf, field);
    buf.push(cmpOp);
    writeVarint(buf, value.length);
    for (const b of value) buf.push(b);
    this._stream.instructions.push(new SpoiInstruction(Op.FILTER, [], new Uint8Array(buf)));
    return this;
  }

  filterI32(field: number, cmpOp: number, value: number): this {
    const buf = new Uint8Array(4);
    new DataView(buf.buffer).setInt32(0, value, true);
    return this.filter(field, cmpOp, buf);
  }

  filterStr(field: number, cmpOp: number, value: string): this {
    const enc = new TextEncoder().encode(value);
    const buf: number[] = [];
    writeVarint(buf, enc.length);
    for (const b of enc) buf.push(b);
    return this.filter(field, cmpOp, new Uint8Array(buf));
  }

  select(...fields: number[]): this {
    const buf: number[] = [];
    writeU32(buf, fields.length);
    for (const f of fields) writeU32(buf, f);
    this._stream.instructions.push(new SpoiInstruction(Op.SELECT, [], new Uint8Array(buf)));
    return this;
  }

  sort(field: number, ascending: boolean = true): this {
    const buf: number[] = [];
    writeU32(buf, field);
    buf.push(ascending ? 1 : 0);
    this._stream.instructions.push(new SpoiInstruction(Op.SORT, [], new Uint8Array(buf)));
    return this;
  }

  reverse(): this {
    this._stream.instructions.push(new SpoiInstruction(Op.REVERSE, []));
    return this;
  }

  take(count: number): this {
    const buf = new Uint8Array(4);
    new DataView(buf.buffer).setUint32(0, count, true);
    this._stream.instructions.push(new SpoiInstruction(Op.TAKE, [], buf));
    return this;
  }

  drop(count: number): this {
    const buf = new Uint8Array(4);
    new DataView(buf.buffer).setUint32(0, count, true);
    this._stream.instructions.push(new SpoiInstruction(Op.DROP, [], buf));
    return this;
  }

  distinct(): this {
    this._stream.instructions.push(new SpoiInstruction(Op.DISTINCT, []));
    return this;
  }

  count(): this {
    this._stream.instructions.push(new SpoiInstruction(Op.COUNT, []));
    return this;
  }

  keys(): this {
    this._stream.instructions.push(new SpoiInstruction(Op.KEYS, []));
    return this;
  }

  values(): this {
    this._stream.instructions.push(new SpoiInstruction(Op.VALUES, []));
    return this;
  }

  join(field: number): this {
    const buf = new Uint8Array(4);
    new DataView(buf.buffer).setUint32(0, field, true);
    this._stream.instructions.push(new SpoiInstruction(Op.JOIN, [], buf));
    return this;
  }

  enumerate(start: number = 0): this {
    const buf = new Uint8Array(4);
    new DataView(buf.buffer).setUint32(0, start, true);
    this._stream.instructions.push(new SpoiInstruction(Op.ENUMERATE, [], buf));
    return this;
  }

  chunk(size: number): this {
    const buf = new Uint8Array(4);
    new DataView(buf.buffer).setUint32(0, size, true);
    this._stream.instructions.push(new SpoiInstruction(Op.CHUNK, [], buf));
    return this;
  }

  stride(step: number): this {
    const buf = new Uint8Array(4);
    new DataView(buf.buffer).setUint32(0, step, true);
    this._stream.instructions.push(new SpoiInstruction(Op.STRIDE, [], buf));
    return this;
  }

  takewhile(field: number, cmpOp: number, value: Uint8Array): this {
    const buf: number[] = [];
    writeU32(buf, field);
    buf.push(cmpOp);
    writeVarint(buf, value.length);
    for (const b of value) buf.push(b);
    this._stream.instructions.push(new SpoiInstruction(Op.TAKEWHILE, [], new Uint8Array(buf)));
    return this;
  }

  dropwhile(field: number, cmpOp: number, value: Uint8Array): this {
    const buf: number[] = [];
    writeU32(buf, field);
    buf.push(cmpOp);
    writeVarint(buf, value.length);
    for (const b of value) buf.push(b);
    this._stream.instructions.push(new SpoiInstruction(Op.DROPWHILE, [], new Uint8Array(buf)));
    return this;
  }

  any(field: number, cmpOp: number, value: Uint8Array): this {
    const buf: number[] = [];
    writeU32(buf, field);
    buf.push(cmpOp);
    writeVarint(buf, value.length);
    for (const b of value) buf.push(b);
    this._stream.instructions.push(new SpoiInstruction(Op.ANY, [], new Uint8Array(buf)));
    return this;
  }

  all(field: number, cmpOp: number, value: Uint8Array): this {
    const buf: number[] = [];
    writeU32(buf, field);
    buf.push(cmpOp);
    writeVarint(buf, value.length);
    for (const b of value) buf.push(b);
    this._stream.instructions.push(new SpoiInstruction(Op.ALL, [], new Uint8Array(buf)));
    return this;
  }

  find(field: number, cmpOp: number, value: Uint8Array): this {
    const buf: number[] = [];
    writeU32(buf, field);
    buf.push(cmpOp);
    writeVarint(buf, value.length);
    for (const b of value) buf.push(b);
    this._stream.instructions.push(new SpoiInstruction(Op.FIND, [], new Uint8Array(buf)));
    return this;
  }

  slide(size: number): this {
    const buf = new Uint8Array(4);
    new DataView(buf.buffer).setUint32(0, size, true);
    this._stream.instructions.push(new SpoiInstruction(Op.SLIDE, [], buf));
    return this;
  }

  adjacent(n: number): this {
    const buf = new Uint8Array(4);
    new DataView(buf.buffer).setUint32(0, n, true);
    this._stream.instructions.push(new SpoiInstruction(Op.ADJACENT, [], buf));
    return this;
  }

  build(): Uint8Array {
    this._stream.instructions.push(new SpoiInstruction(Op.EXEC, []));
    return this._stream.build();
  }
}

