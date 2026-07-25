/**
 * SPOI 刁钻测试 — TypeScript 版
 *
 * 测试覆盖：
 *   1. 数值边界：各类型的最大值/最小值/零值/NaN/Inf
 *   2. 字符串边界：空串、Unicode/emoji、null字节、长串、特殊字符
 *   3. 反序列化异常：截断数据、无效type_id、空数据、类型不匹配
 *   4. Accessor 越界/类型不匹配：负索引、超大索引、setField传入错误类型
 *   5. Executor 组合操作：多层FILTER、空管道、边界组合
 *   6. 跨类型 Executor：Item、Inventory、Character、World
 *   7. Registry 边界
 *
 * 运行：npx ts-node test_spoi_rigorous.ts
 */

import { TypeId, deserializeValue, SpoiAccessorRegistry, SpoiAccessor } from './spoi_ts_accessor';
import { SpoiExecutor } from './spoi_executor';
declare var process: any;

// Op 是 const enum，运行时不可用，使用字面量
const SET      = 0x04;
const PIPE     = 0x22;
const SELECT   = 0x0D;
const EXEC     = 0x21;
const FILTER   = 0x0C;
const COUNT    = 0x15;
const SORT     = 0x0E;
const REVERSE  = 0x0F;
const TAKE     = 0x10;
const DROP     = 0x11;
const DISTINCT = 0x14;

// ResultType 也是 const enum，使用字面量
const RT_UNDEF  = 0;
const RT_SINGLE = 1;
const RT_VECTOR = 2;

// ============================================================
// 测试数据类
// ============================================================

class SpoiTestPlayer {
  name: string = '';
  hp: number = 0;
  level: number = 0;
  posX: number = 0;
  constructor(init?: Partial<SpoiTestPlayer>) { Object.assign(this, init); }
}

class SpoiTestState {
  tick: number = 0;
  currentMap: string = '';
  players: any = null;
  constructor(init?: Partial<SpoiTestState>) { Object.assign(this, init); }
}

class SpoiItem {
  name: string = '';
  value: number = 0;
  constructor(init?: Partial<SpoiItem>) { Object.assign(this, init); }
}

class SpoiInventory {
  items: any = null;
  equipped: any = null;
  gold: number = 0;
  constructor(init?: Partial<SpoiInventory>) { Object.assign(this, init); }
}

class SpoiCharacter {
  name: string = '';
  hp: number = 0;
  inventory: any = null;
  weapon: any = null;
  petLevel: number = 0;
  constructor(init?: Partial<SpoiCharacter>) { Object.assign(this, init); }
}

class SpoiWorld {
  worldName: string = '';
  tick: number = 0;
  characters: any = null;
  constructor(init?: Partial<SpoiWorld>) { Object.assign(this, init); }
}

// ============================================================
// 辅助函数
// ============================================================

function makeTypedValue(typeId: number, valueBytes: number[]): Uint8Array {
  const total = 4 + valueBytes.length;
  const buf = new ArrayBuffer(total);
  const view = new DataView(buf);
  view.setUint32(0, typeId, true);
  const arr = new Uint8Array(buf);
  for (let i = 0; i < valueBytes.length; i++) arr[4 + i] = valueBytes[i];
  return arr;
}

function writeVarint(buf: number[], v: number): void {
  while (v >= 0x80) { buf.push((v & 0x7F) | 0x80); v >>>= 7; }
  buf.push(v & 0x7F);
}

function buildStream(instructions: { op: number; path: number[]; operand: Uint8Array }[]): Uint8Array {
  const buf: number[] = [];
  writeVarint(buf, instructions.length);
  for (const inst of instructions) {
    buf.push(inst.op);
    writeVarint(buf, inst.path.length);
    for (const seg of inst.path) writeVarint(buf, seg);
    writeVarint(buf, inst.operand.length);
    for (const b of inst.operand) buf.push(b);
  }
  return new Uint8Array(buf);
}

function inst(op: number, path: number[], operand: Uint8Array) {
  return { op, path, operand };
}

function buildSetInst(path: number[], typeId: number, valueBytes: number[]) {
  return inst(SET, path, makeTypedValue(typeId, valueBytes));
}

function buildPipeInst(path: number[] = []) {
  return inst(PIPE, path, new Uint8Array());
}

function buildExecInst() {
  return inst(EXEC, [], new Uint8Array());
}

function buildFilterInst(path: number[], memberIdx: number, cmpOp: number, typeId: number, valueBytes: number[]): { op: number; path: number[]; operand: Uint8Array } {
  const typedVal = makeTypedValue(typeId, valueBytes);
  const vlenBuf: number[] = [];
  writeVarint(vlenBuf, typedVal.length);

  const memberBytes = new Uint8Array(4);
  new DataView(memberBytes.buffer).setUint32(0, memberIdx, true);

  const operand = new Uint8Array(4 + 1 + vlenBuf.length + typedVal.length);
  operand.set(memberBytes, 0);
  operand[4] = cmpOp;
  for (let i = 0; i < vlenBuf.length; i++) operand[5 + i] = vlenBuf[i];
  operand.set(typedVal, 5 + vlenBuf.length);

  return inst(FILTER, path, operand);
}

// ============================================================
// 字节序列化辅助
// ============================================================

function u8Bytes(n: number): number[] { return [n & 0xFF]; }

function u16Bytes(n: number): number[] {
  const buf = new ArrayBuffer(2);
  new DataView(buf).setUint16(0, n, true);
  return Array.from(new Uint8Array(buf));
}

function u32Bytes(n: number): number[] {
  const buf = new ArrayBuffer(4);
  new DataView(buf).setUint32(0, n, true);
  return Array.from(new Uint8Array(buf));
}

function u64Bytes(n: bigint): number[] {
  const buf = new ArrayBuffer(8);
  new DataView(buf).setBigUint64(0, n, true);
  return Array.from(new Uint8Array(buf));
}

function i8Bytes(n: number): number[] {
  const buf = new ArrayBuffer(1);
  new DataView(buf).setInt8(0, n);
  return Array.from(new Uint8Array(buf));
}

function i16Bytes(n: number): number[] {
  const buf = new ArrayBuffer(2);
  new DataView(buf).setInt16(0, n, true);
  return Array.from(new Uint8Array(buf));
}

function i32Bytes(n: number): number[] {
  const buf = new ArrayBuffer(4);
  new DataView(buf).setInt32(0, n, true);
  return Array.from(new Uint8Array(buf));
}

function i64Bytes(n: bigint): number[] {
  const buf = new ArrayBuffer(8);
  new DataView(buf).setBigInt64(0, n, true);
  return Array.from(new Uint8Array(buf));
}

function f32Bytes(n: number): number[] {
  const buf = new ArrayBuffer(4);
  new DataView(buf).setFloat32(0, n, true);
  return Array.from(new Uint8Array(buf));
}

function f64Bytes(n: number): number[] {
  const buf = new ArrayBuffer(8);
  new DataView(buf).setFloat64(0, n, true);
  return Array.from(new Uint8Array(buf));
}

function strBytes(s: string): number[] {
  return Array.from(new TextEncoder().encode(s));
}

// ============================================================
// 测试框架
// ============================================================

let passed = 0;
let failed = 0;
const failures: string[] = [];

function check(cond: boolean, msg: string = ''): void {
  if (!cond) throw new Error(msg || 'assertion failed');
}

function test(name: string, fn: () => void): void {
  try {
    fn();
    passed++;
    console.log(`  \u2713 ${name}`);
  } catch (e) {
    failed++;
    failures.push(`  FAIL ${name}: ${(e as Error).message}`);
    console.log(`  \u2717 ${name}`);
    console.log(`    ${(e as Error).message}`);
  }
}

function assertThrows(fn: () => void, expectedMsg?: string): void {
  try {
    fn();
    throw new Error('Expected exception but none was thrown');
  } catch (e) {
    if (expectedMsg && !(e as Error).message.includes(expectedMsg)) {
      throw new Error(`Expected message containing "${expectedMsg}" but got "${(e as Error).message}"`);
    }
  }
}

function assertNoThrow(fn: () => void): void {
  try {
    fn();
  } catch (e) {
    throw new Error(`Expected no exception but got ${(e as Error).message}`);
  }
}

// ============================================================
// 1. 数值边界测试
// ============================================================

function testNumericBoundaries(): void {
  console.log('\n============================================================');
  console.log('  1. 数值边界');
  console.log('============================================================');

  test('U8=0',    () => { check(deserializeValue(makeTypedValue(TypeId.U8, u8Bytes(0))) === 0); });
  test('U8=255',  () => { check(deserializeValue(makeTypedValue(TypeId.U8, u8Bytes(255))) === 255); });
  test('U8=128',  () => { check(deserializeValue(makeTypedValue(TypeId.U8, u8Bytes(128))) === 128); });
  test('U16=0',   () => { check(deserializeValue(makeTypedValue(TypeId.U16, u16Bytes(0))) === 0); });
  test('U16=65535', () => { check(deserializeValue(makeTypedValue(TypeId.U16, u16Bytes(65535))) === 65535); });
  test('U16=32768', () => { check(deserializeValue(makeTypedValue(TypeId.U16, u16Bytes(32768))) === 32768); });
  test('U32=0',   () => { check(deserializeValue(makeTypedValue(TypeId.U32, u32Bytes(0))) === 0); });
  test('U32=4294967295', () => { check(deserializeValue(makeTypedValue(TypeId.U32, u32Bytes(4294967295))) === 4294967295); });
  test('U32=2147483648', () => { check(deserializeValue(makeTypedValue(TypeId.U32, u32Bytes(2147483648))) === 2147483648); });
  test('U64=0',   () => { check(deserializeValue(makeTypedValue(TypeId.U64, u64Bytes(0n))) === 0n); });
  test('U64=max', () => { check(deserializeValue(makeTypedValue(TypeId.U64, u64Bytes(18446744073709551615n))) === 18446744073709551615n); });
  test('U64=mid', () => { check(deserializeValue(makeTypedValue(TypeId.U64, u64Bytes(9223372036854775808n))) === 9223372036854775808n); });
  test('I8=-128', () => { check(deserializeValue(makeTypedValue(TypeId.I8, i8Bytes(-128))) === -128); });
  test('I8=127',  () => { check(deserializeValue(makeTypedValue(TypeId.I8, i8Bytes(127))) === 127); });
  test('I8=0',    () => { check(deserializeValue(makeTypedValue(TypeId.I8, i8Bytes(0))) === 0); });
  test('I8=-1',   () => { check(deserializeValue(makeTypedValue(TypeId.I8, i8Bytes(-1))) === -1); });
  test('I16=-32768', () => { check(deserializeValue(makeTypedValue(TypeId.I16, i16Bytes(-32768))) === -32768); });
  test('I16=32767',  () => { check(deserializeValue(makeTypedValue(TypeId.I16, i16Bytes(32767))) === 32767); });
  test('I16=0',    () => { check(deserializeValue(makeTypedValue(TypeId.I16, i16Bytes(0))) === 0); });
  test('I16=-1',   () => { check(deserializeValue(makeTypedValue(TypeId.I16, i16Bytes(-1))) === -1); });
  test('I32=min',  () => { check(deserializeValue(makeTypedValue(TypeId.I32, i32Bytes(-2147483648))) === -2147483648); });
  test('I32=max',  () => { check(deserializeValue(makeTypedValue(TypeId.I32, i32Bytes(2147483647))) === 2147483647); });
  test('I32=0',    () => { check(deserializeValue(makeTypedValue(TypeId.I32, i32Bytes(0))) === 0); });
  test('I32=-1',   () => { check(deserializeValue(makeTypedValue(TypeId.I32, i32Bytes(-1))) === -1); });
  test('I64=min',  () => { check(deserializeValue(makeTypedValue(TypeId.I64, i64Bytes(-9223372036854775808n))) === -9223372036854775808n); });
  test('I64=max',  () => { check(deserializeValue(makeTypedValue(TypeId.I64, i64Bytes(9223372036854775807n))) === 9223372036854775807n); });
  test('I64=0',    () => { check(deserializeValue(makeTypedValue(TypeId.I64, i64Bytes(0n))) === 0n); });
  test('I64=-1',   () => { check(deserializeValue(makeTypedValue(TypeId.I64, i64Bytes(-1n))) === -1n); });
  test('F32=0.0',  () => { check(deserializeValue(makeTypedValue(TypeId.F32, f32Bytes(0.0))) === 0.0); });
  test('F32=-0.0', () => { check(Object.is(deserializeValue(makeTypedValue(TypeId.F32, f32Bytes(-0.0))), -0)); });
  test('F32=NaN',  () => { check(Number.isNaN(deserializeValue(makeTypedValue(TypeId.F32, f32Bytes(NaN))) as number)); });
  test('F32=Inf',  () => { check((deserializeValue(makeTypedValue(TypeId.F32, f32Bytes(Infinity))) as number) > 0 && !isFinite(deserializeValue(makeTypedValue(TypeId.F32, f32Bytes(Infinity))) as number)); });
  test('F32=-Inf', () => { check((deserializeValue(makeTypedValue(TypeId.F32, f32Bytes(-Infinity))) as number) < 0 && !isFinite(deserializeValue(makeTypedValue(TypeId.F32, f32Bytes(-Infinity))) as number)); });
  test('F64=0.0',  () => { check(deserializeValue(makeTypedValue(TypeId.F64, f64Bytes(0.0))) === 0.0); });
  test('F64=-0.0', () => { check(Object.is(deserializeValue(makeTypedValue(TypeId.F64, f64Bytes(-0.0))), -0)); });
  test('F64=NaN',  () => { check(Number.isNaN(deserializeValue(makeTypedValue(TypeId.F64, f64Bytes(NaN))) as number)); });
  test('F64=Inf',  () => { check((deserializeValue(makeTypedValue(TypeId.F64, f64Bytes(Infinity))) as number) > 0 && !isFinite(deserializeValue(makeTypedValue(TypeId.F64, f64Bytes(Infinity))) as number)); });
  test('F64=-Inf', () => { check((deserializeValue(makeTypedValue(TypeId.F64, f64Bytes(-Infinity))) as number) < 0 && !isFinite(deserializeValue(makeTypedValue(TypeId.F64, f64Bytes(-Infinity))) as number)); });
  test('Bool=1',   () => { check(deserializeValue(makeTypedValue(TypeId.BOOL, [1])) === true); });
  test('Bool=0',   () => { check(deserializeValue(makeTypedValue(TypeId.BOOL, [0])) === false); });
  test('Bool=42',  () => { check(deserializeValue(makeTypedValue(TypeId.BOOL, [42])) === true); });
  test('Bool=255', () => { check(deserializeValue(makeTypedValue(TypeId.BOOL, [255])) === true); });
}

// ============================================================
// 2. 字符串边界测试
// ============================================================

function testStringBoundaries(): void {
  console.log('\n============================================================');
  console.log('  2. 字符串边界');
  console.log('============================================================');

  test('empty', () => { check(deserializeValue(makeTypedValue(TypeId.STRING, [])) === ''); });
  test('Unicode/emoji', () => { check(deserializeValue(makeTypedValue(TypeId.STRING, strBytes('你好世界🌍🎉'))) === '你好世界🌍🎉'); });
  test('null byte', () => {
    const bytes = [0x68, 0x65, 0x6C, 0x6C, 0x6F, 0x00, 0x77, 0x6F, 0x72, 0x6C, 0x64]; // "hello\x00world"
    check(deserializeValue(makeTypedValue(TypeId.STRING, bytes)) === 'hello\x00world');
  });
  test('special chars', () => {
    const bytes = [0x0A, 0x09, 0x0D, 0x08, 0x0C]; // \n\t\r\b\f
    check(deserializeValue(makeTypedValue(TypeId.STRING, bytes)) === '\n\t\r\b\f');
  });
  test('long str', () => {
    const s = 'x'.repeat(1000);
    check(deserializeValue(makeTypedValue(TypeId.STRING, strBytes(s))) === s);
  });
  test('non-ASCII', () => { check(deserializeValue(makeTypedValue(TypeId.STRING, strBytes('café'))) === 'café'); });
  test('CJK', () => { check(deserializeValue(makeTypedValue(TypeId.STRING, strBytes('日本語テスト'))) === '日本語テスト'); });
  test('spaces', () => { check(deserializeValue(makeTypedValue(TypeId.STRING, [0x20, 0x20, 0x20, 0x20, 0x20])) === '     '); });
}

// ============================================================
// 3. 反序列化异常/边界测试
// ============================================================

function testDeserializeEdgeCases(): void {
  console.log('\n============================================================');
  console.log('  3. 反序列化异常/边界');
  console.log('============================================================');

  test('truncated empty', () => { check(deserializeValue(new Uint8Array(0)) === null); });
  test('truncated 1 byte', () => { check(deserializeValue(new Uint8Array([1])) === null); });
  test('truncated 3 bytes', () => { check(deserializeValue(new Uint8Array([1, 2, 3])) === null); });

  // U32 声称 4 字节但只给 2 字节 → DataView.getUint32 会抛 RangeError (offset 超出)
  test('U32 claim 2 bytes', () => {
    const buf = new ArrayBuffer(4 + 2);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.U32, true);
    view.setUint8(4, 1);
    view.setUint8(5, 2);
    // DataView.getUint32 with offset 4 needs 4 bytes, but buffer total is 6, so it should work
    // Actually the buffer is 6 bytes, offset 4 + 4 = 8, but buffer byteLength is 6
    assertThrows(() => deserializeValue(new Uint8Array(buf)));
  });

  // U64 声称 8 字节但只给 3 字节
  test('U64 claim 3 bytes', () => {
    const buf = new ArrayBuffer(4 + 3);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.U64, true);
    view.setUint8(4, 1);
    view.setUint8(5, 2);
    view.setUint8(6, 3);
    assertThrows(() => deserializeValue(new Uint8Array(buf)));
  });

  // U16 声称 2 字节但只给 1 字节
  test('U16 claim 1 byte', () => {
    const buf = new ArrayBuffer(4 + 1);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.U16, true);
    view.setUint8(4, 0xAB);
    assertThrows(() => deserializeValue(new Uint8Array(buf)));
  });

  // 无效 type_id → 返回原始 valueBytes
  test('type_id=0', () => {
    const buf = new ArrayBuffer(4 + 2);
    const view = new DataView(buf);
    view.setUint32(0, 0, true);
    view.setUint8(4, 0xAA);
    view.setUint8(5, 0xBB);
    const result = deserializeValue(new Uint8Array(buf));
    check(result instanceof Uint8Array && (result as Uint8Array).length === 2);
  });

  test('type_id=999', () => {
    const buf = new ArrayBuffer(4 + 2);
    const view = new DataView(buf);
    view.setUint32(0, 999, true);
    view.setUint8(4, 0xCC);
    view.setUint8(5, 0xDD);
    const result = deserializeValue(new Uint8Array(buf));
    check(result instanceof Uint8Array && (result as Uint8Array).length === 2);
  });

  test('type_id=999 empty', () => {
    const buf = new ArrayBuffer(4);
    const view = new DataView(buf);
    view.setUint32(0, 999, true);
    const result = deserializeValue(new Uint8Array(buf));
    check(result instanceof Uint8Array && (result as Uint8Array).length === 0);
  });

  // U8 没有 value_bytes → valueBytes[0] 为 undefined
  test('U8 no value', () => {
    const buf = new ArrayBuffer(4);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.U8, true);
    const result = deserializeValue(new Uint8Array(buf));
    check(result === undefined);
  });

  // BOOL 没有 value_bytes → valueBytes[0] 为 undefined，undefined !== 0 → true
  test('BOOL no value', () => {
    const buf = new ArrayBuffer(4);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.BOOL, true);
    const result = deserializeValue(new Uint8Array(buf));
    check(result === true);
  });

  test('STRING no value', () => {
    const buf = new ArrayBuffer(4);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.STRING, true);
    check(deserializeValue(new Uint8Array(buf)) === '');
  });
}

// ============================================================
// 4. Accessor 越界/类型不匹配测试
// ============================================================

function testAccessorEdgeCases(): void {
  console.log('\n============================================================');
  console.log('  4. Accessor 越界/类型不匹配');
  console.log('============================================================');

  const acc = SpoiAccessorRegistry.get('SpoiTestPlayer')!;
  const obj = new SpoiTestPlayer({ name: 'Test', hp: 100, level: 50, posX: 12.5 });

  test('getField -1',  () => { assertThrows(() => acc.getField(obj, -1), 'invalid field index'); });
  test('getField -100', () => { assertThrows(() => acc.getField(obj, -100), 'invalid field index'); });
  test('setField -1',  () => { assertThrows(() => acc.setField(obj, -1, 0), 'invalid field index'); });
  test('setField -100', () => { assertThrows(() => acc.setField(obj, -100, 0), 'invalid field index'); });
  test('getField =fc',  () => { assertThrows(() => acc.getField(obj, 4), 'invalid field index'); });
  test('getField >fc',  () => { assertThrows(() => acc.getField(obj, 10), 'invalid field index'); });
  test('getField huge', () => { assertThrows(() => acc.getField(obj, 999999), 'invalid field index'); });
  test('setField =fc',  () => { assertThrows(() => acc.setField(obj, 4, 0), 'invalid field index'); });
  test('setField >fc',  () => { assertThrows(() => acc.setField(obj, 10, 0), 'invalid field index'); });
  test('setField huge', () => { assertThrows(() => acc.setField(obj, 999999, 0), 'invalid field index'); });

  // JS/TS 不检查类型，赋什么就是什么
  test('set str for int', () => { assertNoThrow(() => acc.setField(obj, 1, 'not a number')); });
  test('set int for str', () => { assertNoThrow(() => acc.setField(obj, 0, 42)); });
  test('verify str for int', () => { check((obj.hp as any) === 'not a number'); });
  test('verify int for str', () => { check((obj.name as any) === 42); });

  // 不同类型对象
  const item = new SpoiItem();
  test('get on wrong type', () => { assertThrows(() => acc.getField(item, 0)); });
  test('set on wrong type', () => { assertThrows(() => acc.setField(item, 0, 0)); });

  // State accessor
  const sa = SpoiAccessorRegistry.get('SpoiTestState')!;
  const s = new SpoiTestState();
  test('state get neg', () => { assertThrows(() => sa.getField(s, -1), 'invalid field index'); });
  test('state set neg', () => { assertThrows(() => sa.setField(s, -1, 0), 'invalid field index'); });
  test('state get fc', () => { assertThrows(() => sa.getField(s, 3), 'invalid field index'); });
  test('state set fc', () => { assertThrows(() => sa.setField(s, 3, 0), 'invalid field index'); });

  // Character
  const ca = SpoiAccessorRegistry.get('SpoiCharacter')!;
  const c = new SpoiCharacter({ name: 'A', hp: 1, inventory: null, weapon: null, petLevel: 0 });
  test('char fc=5', () => { check(ca.fieldCount() === 5); });
  test('char get 4', () => { check(ca.getField(c, 4) === 0); });
  test('char set 4', () => { assertNoThrow(() => ca.setField(c, 4, 99)); });
  test('char verify', () => { check(c.petLevel === 99); });
  test('char get 5 oob', () => { assertThrows(() => ca.getField(c, 5), 'invalid field index'); });

  // World
  const wa = SpoiAccessorRegistry.get('SpoiWorld')!;
  const w = new SpoiWorld({ worldName: 'X', tick: 0, characters: null });
  test('world fc=3', () => { check(wa.fieldCount() === 3); });
  test('world get 2', () => { check(wa.getField(w, 2) === null); });
  test('world set 2', () => { assertNoThrow(() => wa.setField(w, 2, 'test')); });
  test('world verify', () => { check(w.characters === 'test'); });
}

// ============================================================
// 5. Executor 组合操作/边界测试
// ============================================================

function testExecutorEdgeCases(): void {
  console.log('\n============================================================');
  console.log('  5. Executor 组合操作/边界');
  console.log('============================================================');

  const ex = new SpoiExecutor(SpoiAccessorRegistry);

  // SET + PIPE + EXEC
  {
    const p = new SpoiTestPlayer({ name: 'Hero', hp: 100, level: 5, posX: 0.0 });
    const data = buildStream([buildSetInst([1], TypeId.I32, i32Bytes(999)), buildPipeInst(), buildExecInst()]);
    const r = ex.execute(p, data);
    test('SET+PIPE hp=999', () => { check(p.hp === 999); });
    test('SET+PIPE result', () => { check((r.value as SpoiTestPlayer).hp === 999); });
  }

  // 多层 SET
  {
    const p2 = new SpoiTestPlayer({ name: 'X', hp: 0, level: 0, posX: 0.0 });
    const data = buildStream([
      buildSetInst([0], TypeId.STRING, strBytes('Warrior')),
      buildSetInst([1], TypeId.I32, i32Bytes(500)),
      buildSetInst([2], TypeId.I32, i32Bytes(99)),
      buildSetInst([3], TypeId.F32, f32Bytes(3.14)),
      buildPipeInst(), buildExecInst(),
    ]);
    ex.execute(p2, data);
    test('multi SET name', () => { check(p2.name === 'Warrior'); });
    test('multi SET hp', () => { check(p2.hp === 500); });
    test('multi SET level', () => { check(p2.level === 99); });
    test('multi SET posX', () => { check(Math.abs(p2.posX - 3.14) < 0.001); });
  }

  // 创建测试数据
  const p1 = new SpoiTestPlayer({ name: 'Alice', hp: 100, level: 1, posX: 0.0 });
  const p2 = new SpoiTestPlayer({ name: 'Bob', hp: 200, level: 5, posX: 0.0 });
  const p3 = new SpoiTestPlayer({ name: 'Carol', hp: 300, level: 10, posX: 0.0 });
  const st = new SpoiTestState({ tick: 0, currentMap: 'Test', players: [p1, p2, p3] });

  // FILTER: hp > 150 (cmpOp=3 即 gt, memberIdx=1)
  {
    const data = buildStream([buildPipeInst([2]), buildFilterInst([], 1, 3, TypeId.I32, i32Bytes(150)), buildExecInst()]);
    const r = ex.execute(st, data);
    test('FILTER hp>150 count', () => { check((r.value as any[]).length === 2); });
  }

  // 双层 FILTER
  {
    const data = buildStream([
      buildPipeInst([2]),
      buildFilterInst([], 1, 3, TypeId.I32, i32Bytes(150)),
      buildFilterInst([], 2, 2, TypeId.I32, i32Bytes(10)),
      buildExecInst(),
    ]);
    const r = ex.execute(st, data);
    test('double FILTER name', () => { check((r.value as SpoiTestPlayer).name === 'Bob'); });
    test('double FILTER resultType', () => { check(r.resultType === RT_SINGLE); });
  }

  // FILTER: 空结果 → resultType=UNDEF, 无 "value" 键
  {
    const data = buildStream([buildPipeInst([2]), buildFilterInst([], 1, 4, TypeId.I32, i32Bytes(0)), buildExecInst()]);
    const r = ex.execute(st, data);
    test('FILTER empty resultType', () => { check(r.resultType === 0); });
    test('FILTER empty no value', () => { check(!('value' in r)); });
  }

  // COUNT
  {
    const data = buildStream([buildPipeInst([2]), inst(COUNT, [], new Uint8Array()), buildExecInst()]);
    test('COUNT 3', () => { check(ex.execute(st, data).value === 3); });
  }

  // COUNT 空管道
  {
    const data = buildStream([inst(COUNT, [], new Uint8Array()), buildExecInst()]);
    test('COUNT empty', () => { check(ex.execute(st, data).value === 0); });
  }

  // SORT
  {
    const data = buildStream([buildPipeInst([2]), inst(SORT, [0], new Uint8Array()), buildExecInst()]);
    const r = ex.execute(st, data);
    test('SORT Alice first', () => { check((r.value as any[])[0].name === 'Alice'); });
    test('SORT Carol last', () => { check((r.value as any[])[2].name === 'Carol'); });
  }

  // SORT 空列表
  {
    const data = buildStream([
      buildPipeInst([2]),
      buildFilterInst([], 1, 4, TypeId.I32, i32Bytes(0)),
      inst(SORT, [0], new Uint8Array()),
      buildExecInst(),
    ]);
    test('SORT empty', () => { check(ex.execute(st, data).resultType === 0); });
  }

  // TAKE
  {
    const data = buildStream([buildPipeInst([2]), inst(TAKE, [], new Uint8Array(u32Bytes(2))), buildExecInst()]);
    test('TAKE 2', () => { check((ex.execute(st, data).value as any[]).length === 2); });
  }
  {
    const data = buildStream([buildPipeInst([2]), inst(TAKE, [], new Uint8Array(u32Bytes(100))), buildExecInst()]);
    test('TAKE 100', () => { check((ex.execute(st, data).value as any[]).length === 3); });
  }
  {
    const data = buildStream([buildPipeInst([2]), inst(TAKE, [], new Uint8Array(u32Bytes(0))), buildExecInst()]);
    test('TAKE 0', () => { check(ex.execute(st, data).resultType === 0); });
  }

  // DROP
  {
    const data = buildStream([buildPipeInst([2]), inst(DROP, [], new Uint8Array(u32Bytes(1))), buildExecInst()]);
    test('DROP 1', () => { check((ex.execute(st, data).value as any[]).length === 2); });
  }
  {
    const data = buildStream([buildPipeInst([2]), inst(DROP, [], new Uint8Array(u32Bytes(100))), buildExecInst()]);
    test('DROP 100', () => { check(ex.execute(st, data).resultType === 0); });
  }

  // SELECT
  {
    const data = buildStream([buildPipeInst([2]), inst(SELECT, [0], new Uint8Array()), buildExecInst()]);
    const r = ex.execute(st, data);
    test('SELECT count', () => { check((r.value as any[]).length === 3); });
    test('SELECT values', () => { check(JSON.stringify(r.value) === JSON.stringify(['Alice', 'Bob', 'Carol'])); });
  }

  // SELECT 空管道
  {
    const data = buildStream([
      buildPipeInst([2]),
      buildFilterInst([], 1, 4, TypeId.I32, i32Bytes(0)),
      inst(SELECT, [0], new Uint8Array()),
      buildExecInst(),
    ]);
    test('SELECT empty', () => { check(ex.execute(st, data).resultType === 0); });
  }

  // REVERSE
  {
    const data = buildStream([buildPipeInst([2]), inst(REVERSE, [], new Uint8Array()), buildExecInst()]);
    test('REVERSE Carol', () => { check((ex.execute(st, data).value as any[])[0].name === 'Carol'); });
  }

  // REVERSE 空
  {
    const data = buildStream([
      buildPipeInst([2]),
      buildFilterInst([], 1, 4, TypeId.I32, i32Bytes(0)),
      inst(REVERSE, [], new Uint8Array()),
      buildExecInst(),
    ]);
    test('REVERSE empty', () => { check(ex.execute(st, data).resultType === 0); });
  }

  // DISTINCT
  {
    const st2 = new SpoiTestState({ tick: 0, currentMap: '', players: [p1, p1, p2, p2, p3] });
    const data = buildStream([buildPipeInst([2]), inst(DISTINCT, [], new Uint8Array()), buildExecInst()]);
    test('DISTINCT 3', () => { check((ex.execute(st2, data).value as any[]).length === 3); });
  }

  // DISTINCT 空
  {
    const st2 = new SpoiTestState({ tick: 0, currentMap: '', players: [p1, p1, p2, p2, p3] });
    const data = buildStream([
      buildPipeInst([2]),
      buildFilterInst([], 1, 4, TypeId.I32, i32Bytes(0)),
      inst(DISTINCT, [], new Uint8Array()),
      buildExecInst(),
    ]);
    test('DISTINCT empty', () => { check(ex.execute(st2, data).resultType === 0); });
  }
}

// ============================================================
// 6. 跨类型 Executor 测试
// ============================================================

function testExecutorCrossType(): void {
  console.log('\n============================================================');
  console.log('  6. 跨类型 Executor');
  console.log('============================================================');

  const ex = new SpoiExecutor(SpoiAccessorRegistry);

  // Item
  {
    const item = new SpoiItem({ name: '', value: 0 });
    const data = buildStream([
      buildSetInst([0], TypeId.STRING, strBytes('Potion')),
      buildSetInst([1], TypeId.I32, i32Bytes(50)),
      buildPipeInst(), buildExecInst(),
    ]);
    ex.execute(item, data);
    test('Item name', () => { check(item.name === 'Potion'); });
    test('Item value', () => { check(item.value === 50); });
  }

  // Inventory
  {
    const inv = new SpoiInventory({ items: null, equipped: null, gold: 0 });
    const data = buildStream([
      buildSetInst([2], TypeId.I32, i32Bytes(1000)),
      buildPipeInst(), buildExecInst(),
    ]);
    ex.execute(inv, data);
    test('Inv gold', () => { check(inv.gold === 1000); });
  }

  // Character
  {
    const char = new SpoiCharacter({ name: '', hp: 0, inventory: null, weapon: null, petLevel: 0 });
    const data = buildStream([
      buildSetInst([0], TypeId.STRING, strBytes('Hero')),
      buildSetInst([1], TypeId.I32, i32Bytes(2000)),
      buildSetInst([4], TypeId.I32, i32Bytes(5)),
      buildPipeInst(), buildExecInst(),
    ]);
    ex.execute(char, data);
    test('Char name', () => { check(char.name === 'Hero'); });
    test('Char hp', () => { check(char.hp === 2000); });
    test('Char petLevel', () => { check(char.petLevel === 5); });
  }

  // World
  {
    const world = new SpoiWorld({ worldName: '', tick: 0, characters: null });
    const data = buildStream([
      buildSetInst([0], TypeId.STRING, strBytes('Azeroth')),
      buildSetInst([1], TypeId.I32, i32Bytes(9999)),
      buildPipeInst(), buildExecInst(),
    ]);
    ex.execute(world, data);
    test('World name', () => { check(world.worldName === 'Azeroth'); });
    test('World tick', () => { check(world.tick === 9999); });
  }
}

// ============================================================
// 7. Registry 边界测试
// ============================================================

function testRegistryEdgeCases(): void {
  console.log('\n============================================================');
  console.log('  7. Registry 边界');
  console.log('============================================================');

  test('size=6', () => { check(SpoiAccessorRegistry.size === 6); });
  test('missing key', () => { check(SpoiAccessorRegistry.get('NonExistentType') === undefined); });

  for (const [name, acc] of SpoiAccessorRegistry) {
    test(`registry ${name} fc>0`, () => { check(acc.fieldCount() > 0); });
  }
}

// ============================================================
// 测试入口
// ============================================================

console.log('============================================================');
console.log('  SPOI Accessor 刁钻测试 — TypeScript 版');
console.log('============================================================');

const suites: [string, () => void][] = [
  ['1. 数值边界', testNumericBoundaries],
  ['2. 字符串边界', testStringBoundaries],
  ['3. 反序列化异常/边界', testDeserializeEdgeCases],
  ['4. Accessor 越界/类型不匹配', testAccessorEdgeCases],
  ['5. Executor 组合操作/边界', testExecutorEdgeCases],
  ['6. 跨类型 Executor', testExecutorCrossType],
  ['7. Registry 边界', testRegistryEdgeCases],
];

for (const [name, fn] of suites) {
  const pBefore = passed;
  const fBefore = failed;
  fn();
  const pDelta = passed - pBefore;
  const fDelta = failed - fBefore;
  console.log(`\n  ${name}  [${pDelta}/${pDelta + fDelta} passed]`);
}

console.log(`\n============================================================`);
console.log(`  Total: ${passed + failed} tests | Passed: ${passed} | Failed: ${failed}`);
console.log(`============================================================`);

if (failures.length > 0) {
  console.log('\nFailures:');
  for (const f of failures) console.log(f);
}

if (failed > 0) {
  process.exit(1);
}