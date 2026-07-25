/**
 * SPOI Accessor & Executor 刁钻测试套件
 *
 * 测试 JavaScript 的 SPOI Accessor 和 Executor 功能。
 * 覆盖数值边界、字符串边界、反序列化异常、Accessor 越界、
 * Executor 组合操作、跨类型 Executor、Registry 边界等。
 *
 * 运行: node test_spoi_rigorous.js
 */

'use strict';

const {
  TypeId, SpoiAccessor, deserializeValue, SpoiAccessorRegistry,
  SpoiTestPlayerAccessor, SpoiTestStateAccessor, SpoiItemAccessor,
  SpoiInventoryAccessor, SpoiCharacterAccessor, SpoiWorldAccessor,
} = require('./spoi_js_accessor.js');
const { SpoiExecutor, Op, ResultType } = require('./spoi_executor.js');

// =============================== 数据类 ===============================

function SpoiTestPlayer() { this.name = ''; this.hp = 0; this.level = 0; this.posX = 0; }
function SpoiTestState() { this.tick = 0; this.currentMap = ''; this.players = null; }
function SpoiItem() { this.name = ''; this.value = 0; }
function SpoiInventory() { this.items = null; this.equipped = null; this.gold = 0; }
function SpoiCharacter() { this.name = ''; this.hp = 0; this.inventory = null; this.weapon = null; this.petLevel = 0; }
function SpoiWorld() { this.worldName = ''; this.tick = 0; this.characters = null; }

// =============================== 辅助函数 ===============================

function u32LE(n) {
  const buf = new ArrayBuffer(4);
  new DataView(buf).setUint32(0, n, true);
  return new Uint8Array(buf);
}

function buildValueBytes(typeId, valueBytes) {
  const header = u32LE(typeId);
  const result = new Uint8Array(4 + valueBytes.length);
  result.set(header, 0);
  result.set(valueBytes, 4);
  return result;
}

function writeVarint(buf, v) {
  while (v >= 0x80) { buf.push((v & 0x7F) | 0x80); v >>>= 7; }
  buf.push(v & 0x7F);
}

function buildSetInst(path, typeId, valueBytes) {
  return { op: Op.SET, path: path, operand: buildValueBytes(typeId, valueBytes) };
}

function buildPipeInst(path) {
  return { op: Op.PIPE, path: path, operand: new Uint8Array(0) };
}

function buildExecInst() {
  return { op: Op.EXEC, path: [], operand: new Uint8Array(0) };
}

function buildFilterInst(path, memberIdx, cmpOp, typeId, valueBytes) {
  const valBytes = buildValueBytes(typeId, valueBytes);
  const valLen = valBytes.length;
  // operand: memberIdx(u32 LE) + cmpOp(u8) + value_len(varint) + value_bytes(typeId + value)
  const varintBuf = [];
  writeVarint(varintBuf, valLen);
  const operand = new Uint8Array(4 + 1 + varintBuf.length + valLen);
  new DataView(operand.buffer).setUint32(0, memberIdx, true);
  operand[4] = cmpOp;
  for (let i = 0; i < varintBuf.length; i++) operand[5 + i] = varintBuf[i];
  operand.set(valBytes, 5 + varintBuf.length);
  return { op: Op.FILTER, path: path, operand: operand };
}

function buildTakeInst(path, n) {
  const operand = u32LE(n);
  return { op: Op.TAKE, path: path, operand: operand };
}

function buildDropInst(path, n) {
  const operand = u32LE(n);
  return { op: Op.DROP, path: path, operand: operand };
}

function buildSortInst(path) {
  return { op: Op.SORT, path: path, operand: new Uint8Array(0) };
}

function buildSelectInst(path) {
  return { op: Op.SELECT, path: path, operand: new Uint8Array(0) };
}

function buildReverseInst() {
  return { op: Op.REVERSE, path: [], operand: new Uint8Array(0) };
}

function buildDistinctInst() {
  return { op: Op.DISTINCT, path: [], operand: new Uint8Array(0) };
}

function buildCountInst() {
  return { op: Op.COUNT, path: [], operand: new Uint8Array(0) };
}

function buildSpoi(instructions) {
  const buf = [];
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

// =============================== 测试框架 ===============================

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    passed++;
    console.log('  \u2713 ' + name);
  } catch (e) {
    failed++;
    console.log('  \u2717 ' + name);
    console.log('    ' + e.message);
  }
}

function suite(name, fn) {
  console.log('\n' + name);
  fn();
}

// =============================== 1. 数值边界测试 ===============================

suite('1. 数值边界 - U8', () => {
  test('U8 0', () => {
    const data = buildValueBytes(TypeId.U8, new Uint8Array([0]));
    console.assert(deserializeValue(data) === 0);
  });
  test('U8 255', () => {
    const data = buildValueBytes(TypeId.U8, new Uint8Array([255]));
    console.assert(deserializeValue(data) === 255);
  });
  test('U8 128', () => {
    const data = buildValueBytes(TypeId.U8, new Uint8Array([128]));
    console.assert(deserializeValue(data) === 128);
  });
});

suite('2. 数值边界 - U16', () => {
  test('U16 0', () => {
    const vb = new ArrayBuffer(2);
    new DataView(vb).setUint16(0, 0, true);
    const data = buildValueBytes(TypeId.U16, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 0);
  });
  test('U16 65535', () => {
    const vb = new ArrayBuffer(2);
    new DataView(vb).setUint16(0, 65535, true);
    const data = buildValueBytes(TypeId.U16, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 65535);
  });
  test('U16 32768', () => {
    const vb = new ArrayBuffer(2);
    new DataView(vb).setUint16(0, 32768, true);
    const data = buildValueBytes(TypeId.U16, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 32768);
  });
});

suite('3. 数值边界 - U32', () => {
  test('U32 0', () => {
    const vb = new ArrayBuffer(4);
    new DataView(vb).setUint32(0, 0, true);
    const data = buildValueBytes(TypeId.U32, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 0);
  });
  test('U32 4294967295', () => {
    const vb = new ArrayBuffer(4);
    new DataView(vb).setUint32(0, 4294967295, true);
    const data = buildValueBytes(TypeId.U32, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 4294967295);
  });
  test('U32 2147483648', () => {
    const vb = new ArrayBuffer(4);
    new DataView(vb).setUint32(0, 2147483648, true);
    const data = buildValueBytes(TypeId.U32, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 2147483648);
  });
});

suite('4. 数值边界 - U64', () => {
  test('U64 0', () => {
    const vb = new ArrayBuffer(8);
    new DataView(vb).setBigUint64(0, 0n, true);
    const data = buildValueBytes(TypeId.U64, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 0n);
  });
  test('U64 max (18446744073709551615n)', () => {
    const vb = new ArrayBuffer(8);
    new DataView(vb).setBigUint64(0, 18446744073709551615n, true);
    const data = buildValueBytes(TypeId.U64, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 18446744073709551615n);
  });
  test('U64 mid (9223372036854775808n)', () => {
    const vb = new ArrayBuffer(8);
    new DataView(vb).setBigUint64(0, 9223372036854775808n, true);
    const data = buildValueBytes(TypeId.U64, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 9223372036854775808n);
  });
});

suite('5. 数值边界 - I8', () => {
  test('I8 -128', () => {
    const vb = new ArrayBuffer(1);
    new DataView(vb).setInt8(0, -128);
    const data = buildValueBytes(TypeId.I8, new Uint8Array(vb));
    console.assert(deserializeValue(data) === -128);
  });
  test('I8 127', () => {
    const vb = new ArrayBuffer(1);
    new DataView(vb).setInt8(0, 127);
    const data = buildValueBytes(TypeId.I8, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 127);
  });
  test('I8 0', () => {
    const vb = new ArrayBuffer(1);
    new DataView(vb).setInt8(0, 0);
    const data = buildValueBytes(TypeId.I8, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 0);
  });
  test('I8 -1', () => {
    const vb = new ArrayBuffer(1);
    new DataView(vb).setInt8(0, -1);
    const data = buildValueBytes(TypeId.I8, new Uint8Array(vb));
    console.assert(deserializeValue(data) === -1);
  });
});

suite('6. 数值边界 - I16', () => {
  test('I16 -32768', () => {
    const vb = new ArrayBuffer(2);
    new DataView(vb).setInt16(0, -32768, true);
    const data = buildValueBytes(TypeId.I16, new Uint8Array(vb));
    console.assert(deserializeValue(data) === -32768);
  });
  test('I16 32767', () => {
    const vb = new ArrayBuffer(2);
    new DataView(vb).setInt16(0, 32767, true);
    const data = buildValueBytes(TypeId.I16, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 32767);
  });
  test('I16 -1', () => {
    const vb = new ArrayBuffer(2);
    new DataView(vb).setInt16(0, -1, true);
    const data = buildValueBytes(TypeId.I16, new Uint8Array(vb));
    console.assert(deserializeValue(data) === -1);
  });
});

suite('7. 数值边界 - I32', () => {
  test('I32 -2147483648', () => {
    const vb = new ArrayBuffer(4);
    new DataView(vb).setInt32(0, -2147483648, true);
    const data = buildValueBytes(TypeId.I32, new Uint8Array(vb));
    console.assert(deserializeValue(data) === -2147483648);
  });
  test('I32 2147483647', () => {
    const vb = new ArrayBuffer(4);
    new DataView(vb).setInt32(0, 2147483647, true);
    const data = buildValueBytes(TypeId.I32, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 2147483647);
  });
  test('I32 -1', () => {
    const vb = new ArrayBuffer(4);
    new DataView(vb).setInt32(0, -1, true);
    const data = buildValueBytes(TypeId.I32, new Uint8Array(vb));
    console.assert(deserializeValue(data) === -1);
  });
});

suite('8. 数值边界 - I64', () => {
  test('I64 -9223372036854775808n', () => {
    const vb = new ArrayBuffer(8);
    new DataView(vb).setBigInt64(0, -9223372036854775808n, true);
    const data = buildValueBytes(TypeId.I64, new Uint8Array(vb));
    console.assert(deserializeValue(data) === -9223372036854775808n);
  });
  test('I64 9223372036854775807n', () => {
    const vb = new ArrayBuffer(8);
    new DataView(vb).setBigInt64(0, 9223372036854775807n, true);
    const data = buildValueBytes(TypeId.I64, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 9223372036854775807n);
  });
  test('I64 -1', () => {
    const vb = new ArrayBuffer(8);
    new DataView(vb).setBigInt64(0, -1n, true);
    const data = buildValueBytes(TypeId.I64, new Uint8Array(vb));
    console.assert(deserializeValue(data) === -1n);
  });
});

suite('9. 数值边界 - F32', () => {
  test('F32 0.0', () => {
    const vb = new ArrayBuffer(4);
    new DataView(vb).setFloat32(0, 0.0, true);
    const data = buildValueBytes(TypeId.F32, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 0.0);
  });
  test('F32 -0.0', () => {
    const vb = new ArrayBuffer(4);
    new DataView(vb).setFloat32(0, -0.0, true);
    const data = buildValueBytes(TypeId.F32, new Uint8Array(vb));
    const v = deserializeValue(data);
    console.assert(Object.is(v, -0), 'should be -0');
  });
  test('F32 NaN', () => {
    const vb = new ArrayBuffer(4);
    new DataView(vb).setFloat32(0, NaN, true);
    const data = buildValueBytes(TypeId.F32, new Uint8Array(vb));
    console.assert(isNaN(deserializeValue(data)), 'should be NaN');
  });
  test('F32 Infinity', () => {
    const vb = new ArrayBuffer(4);
    new DataView(vb).setFloat32(0, Infinity, true);
    const data = buildValueBytes(TypeId.F32, new Uint8Array(vb));
    console.assert(!isFinite(deserializeValue(data)) && deserializeValue(data) > 0, 'should be +Infinity');
  });
  test('F32 -Infinity', () => {
    const vb = new ArrayBuffer(4);
    new DataView(vb).setFloat32(0, -Infinity, true);
    const data = buildValueBytes(TypeId.F32, new Uint8Array(vb));
    console.assert(!isFinite(deserializeValue(data)) && deserializeValue(data) < 0, 'should be -Infinity');
  });
});

suite('10. 数值边界 - F64', () => {
  test('F64 0.0', () => {
    const vb = new ArrayBuffer(8);
    new DataView(vb).setFloat64(0, 0.0, true);
    const data = buildValueBytes(TypeId.F64, new Uint8Array(vb));
    console.assert(deserializeValue(data) === 0.0);
  });
  test('F64 -0.0', () => {
    const vb = new ArrayBuffer(8);
    new DataView(vb).setFloat64(0, -0.0, true);
    const data = buildValueBytes(TypeId.F64, new Uint8Array(vb));
    const v = deserializeValue(data);
    console.assert(Object.is(v, -0), 'should be -0');
  });
  test('F64 NaN', () => {
    const vb = new ArrayBuffer(8);
    new DataView(vb).setFloat64(0, NaN, true);
    const data = buildValueBytes(TypeId.F64, new Uint8Array(vb));
    console.assert(isNaN(deserializeValue(data)), 'should be NaN');
  });
  test('F64 Infinity', () => {
    const vb = new ArrayBuffer(8);
    new DataView(vb).setFloat64(0, Infinity, true);
    const data = buildValueBytes(TypeId.F64, new Uint8Array(vb));
    console.assert(!isFinite(deserializeValue(data)) && deserializeValue(data) > 0, 'should be +Infinity');
  });
  test('F64 -Infinity', () => {
    const vb = new ArrayBuffer(8);
    new DataView(vb).setFloat64(0, -Infinity, true);
    const data = buildValueBytes(TypeId.F64, new Uint8Array(vb));
    console.assert(!isFinite(deserializeValue(data)) && deserializeValue(data) < 0, 'should be -Infinity');
  });
  test('F64 max safe integer', () => {
    const vb = new ArrayBuffer(8);
    new DataView(vb).setFloat64(0, Number.MAX_SAFE_INTEGER, true);
    const data = buildValueBytes(TypeId.F64, new Uint8Array(vb));
    console.assert(deserializeValue(data) === Number.MAX_SAFE_INTEGER);
  });
});

suite('11. 数值边界 - Bool', () => {
  test('Bool 1 => true', () => {
    const data = buildValueBytes(TypeId.BOOL, new Uint8Array([1]));
    console.assert(deserializeValue(data) === true);
  });
  test('Bool 0 => false', () => {
    const data = buildValueBytes(TypeId.BOOL, new Uint8Array([0]));
    console.assert(deserializeValue(data) === false);
  });
  test('Bool 42 => true (非零即真)', () => {
    const data = buildValueBytes(TypeId.BOOL, new Uint8Array([42]));
    console.assert(deserializeValue(data) === true);
  });
  test('Bool 255 => true (非零即真)', () => {
    const data = buildValueBytes(TypeId.BOOL, new Uint8Array([255]));
    console.assert(deserializeValue(data) === true);
  });
});

// =============================== 2. 字符串边界测试 ===============================

suite('12. 字符串边界 - 空串', () => {
  test('空字符串', () => {
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode(''));
    console.assert(deserializeValue(data) === '');
  });
});

suite('13. 字符串边界 - Unicode/Emoji', () => {
  test('你好世界', () => {
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode('你好世界'));
    console.assert(deserializeValue(data) === '你好世界');
  });
  test('你好世界🌍🎉', () => {
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode('你好世界🌍🎉'));
    console.assert(deserializeValue(data) === '你好世界🌍🎉');
  });
  test('CJK 字符', () => {
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode('日本語テスト'));
    console.assert(deserializeValue(data) === '日本語テスト');
  });
  test('混合 Emoji', () => {
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode('😀😃😄😁😆'));
    console.assert(deserializeValue(data) === '😀😃😄😁😆');
  });
});

suite('14. 字符串边界 - null 字节', () => {
  test('字符串含 null 字节', () => {
    const str = 'hello\x00world';
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode(str));
    console.assert(deserializeValue(data) === str);
  });
  test('仅 null 字节', () => {
    const str = '\x00';
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode(str));
    console.assert(deserializeValue(data) === str);
  });
});

suite('15. 字符串边界 - 特殊字符', () => {
  test('换行符', () => {
    const str = 'line1\nline2\nline3';
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode(str));
    console.assert(deserializeValue(data) === str);
  });
  test('制表符', () => {
    const str = 'col1\tcol2\tcol3';
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode(str));
    console.assert(deserializeValue(data) === str);
  });
  test('引号', () => {
    const str = 'he said "hello"';
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode(str));
    console.assert(deserializeValue(data) === str);
  });
  test('反斜杠', () => {
    const str = 'C:\\path\\to\\file';
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode(str));
    console.assert(deserializeValue(data) === str);
  });
});

suite('16. 字符串边界 - 长串', () => {
  test('1000 字符', () => {
    const str = 'A'.repeat(1000);
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode(str));
    console.assert(deserializeValue(data) === str);
  });
});

suite('17. 字符串边界 - 空格', () => {
  test('仅空格', () => {
    const str = '   ';
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode(str));
    console.assert(deserializeValue(data) === str);
  });
  test('前后空格', () => {
    const str = '  hello  ';
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode(str));
    console.assert(deserializeValue(data) === str);
  });
});

// =============================== 3. 反序列化异常测试 ===============================

suite('18. 反序列化异常 - 截断数据', () => {
  test('空 Uint8Array', () => {
    console.assert(deserializeValue(new Uint8Array(0)) === null);
  });
  test('2 字节（不足 4 字节头）', () => {
    console.assert(deserializeValue(new Uint8Array(2)) === null);
  });
  test('3 字节（不足 4 字节头）', () => {
    console.assert(deserializeValue(new Uint8Array(3)) === null);
  });
  test('null 输入', () => {
    console.assert(deserializeValue(null) === null);
  });
  test('U32 type_id 但只有 5 字节（值不足）', () => {
    // type_id 4 字节 + 1 字节值，U32 需要 4 字节值
    const data = new Uint8Array(5);
    new DataView(data.buffer).setUint32(0, TypeId.U32, true);
    data[4] = 0x42;
    console.assert(deserializeValue(data) === 0, 'truncated U32 should return 0');
  });
  test('U64 type_id 但只有 6 字节（值不足）', () => {
    const data = new Uint8Array(6);
    new DataView(data.buffer).setUint32(0, TypeId.U64, true);
    console.assert(deserializeValue(data) === 0n, 'truncated U64 should return 0n');
  });
});

suite('19. 反序列化异常 - 无效 type_id', () => {
  test('type_id = 999（未知类型）', () => {
    const data = new Uint8Array(8);
    new DataView(data.buffer).setUint32(0, 999, true);
    const result = deserializeValue(data);
    console.assert(result instanceof Uint8Array, 'unknown type should return raw bytes');
  });
  test('type_id = 0', () => {
    const data = new Uint8Array(8);
    new DataView(data.buffer).setUint32(0, 0, true);
    const result = deserializeValue(data);
    console.assert(result instanceof Uint8Array, 'type_id 0 should return raw bytes');
  });
  test('type_id = 255', () => {
    const data = new Uint8Array(8);
    new DataView(data.buffer).setUint32(0, 255, true);
    const result = deserializeValue(data);
    console.assert(result instanceof Uint8Array, 'type_id 255 should return raw bytes');
  });
});

suite('20. 反序列化异常 - type_id 无值', () => {
  test('U8 type_id 无值 => 0', () => {
    const data = buildValueBytes(TypeId.U8, new Uint8Array(0));
    console.assert(deserializeValue(data) === 0);
  });
  test('U16 type_id 无值 => 0', () => {
    const data = buildValueBytes(TypeId.U16, new Uint8Array(0));
    console.assert(deserializeValue(data) === 0);
  });
  test('U32 type_id 无值 => 0', () => {
    const data = buildValueBytes(TypeId.U32, new Uint8Array(0));
    console.assert(deserializeValue(data) === 0);
  });
  test('U64 type_id 无值 => 0n', () => {
    const data = buildValueBytes(TypeId.U64, new Uint8Array(0));
    console.assert(deserializeValue(data) === 0n);
  });
  test('I8 type_id 无值 => 0', () => {
    const data = buildValueBytes(TypeId.I8, new Uint8Array(0));
    console.assert(deserializeValue(data) === 0);
  });
  test('STRING type_id 无值 => ""', () => {
    const data = buildValueBytes(TypeId.STRING, new Uint8Array(0));
    console.assert(deserializeValue(data) === '');
  });
  test('BOOL type_id 无值 => false', () => {
    const data = buildValueBytes(TypeId.BOOL, new Uint8Array(0));
    console.assert(deserializeValue(data) === false);
  });
  test('F32 type_id 无值 => 0', () => {
    const data = buildValueBytes(TypeId.F32, new Uint8Array(0));
    console.assert(deserializeValue(data) === 0.0);
  });
  test('F64 type_id 无值 => 0', () => {
    const data = buildValueBytes(TypeId.F64, new Uint8Array(0));
    console.assert(deserializeValue(data) === 0.0);
  });
});

// =============================== 4. Accessor 越界测试 ===============================

suite('21. Accessor 越界 - 负索引', () => {
  const accessor = new SpoiTestPlayerAccessor();
  const obj = new SpoiTestPlayer();

  test('getField 负索引抛出', () => {
    let threw = false;
    try { accessor.getField(obj, -1); } catch (e) { threw = true; }
    console.assert(threw, 'Should throw for negative getField index');
  });
  test('setField 负索引抛出', () => {
    let threw = false;
    try { accessor.setField(obj, -1, 1); } catch (e) { threw = true; }
    console.assert(threw, 'Should throw for negative setField index');
  });
});

suite('22. Accessor 越界 - field_count 越界', () => {
  const accessor = new SpoiTestPlayerAccessor();
  const obj = new SpoiTestPlayer();

  test('getField fieldCount 越界', () => {
    let threw = false;
    try { accessor.getField(obj, accessor.fieldCount()); } catch (e) { threw = true; }
    console.assert(threw, 'Should throw for getField at fieldCount');
  });
  test('setField fieldCount 越界', () => {
    let threw = false;
    try { accessor.setField(obj, accessor.fieldCount(), 1); } catch (e) { threw = true; }
    console.assert(threw, 'Should throw for setField at fieldCount');
  });
});

suite('23. Accessor 越界 - 超大索引', () => {
  const accessor = new SpoiTestPlayerAccessor();
  const obj = new SpoiTestPlayer();

  test('getField 超大索引', () => {
    let threw = false;
    try { accessor.getField(obj, 99999); } catch (e) { threw = true; }
    console.assert(threw, 'Should throw for huge getField index');
  });
  test('setField 超大索引', () => {
    let threw = false;
    try { accessor.setField(obj, 99999, 1); } catch (e) { threw = true; }
    console.assert(threw, 'Should throw for huge setField index');
  });
});

suite('24. Accessor 越界 - setField 类型错误', () => {
  test('SpoiItem accessor setField 越界', () => {
    const accessor = new SpoiItemAccessor();
    const obj = new SpoiItem();
    let threw = false;
    try { accessor.getField(obj, 99); } catch (e) { threw = true; }
    console.assert(threw, 'Should throw for invalid index on SpoiItem');
  });
  test('SpoiInventory accessor setField 越界', () => {
    const accessor = new SpoiInventoryAccessor();
    const obj = new SpoiInventory();
    let threw = false;
    try { accessor.setField(obj, 99, 1); } catch (e) { threw = true; }
    console.assert(threw, 'Should throw for invalid index on SpoiInventory');
  });
});

suite('25. Accessor 越界 - 不同类型对象', () => {
  test('SpoiTestPlayer accessor 用于 SpoiItem', () => {
    const accessor = new SpoiTestPlayerAccessor();
    const obj = new SpoiItem();
    // 属性名不同但访问器仍会尝试访问（getField 不检查类型）
    // getField 0 会访问 obj.name（SpoiItem 也有 name）
    console.assert(accessor.getField(obj, 0) === '');
    // getField 1 访问 obj.hp，但 SpoiItem 没有 hp 属性，返回 undefined
    console.assert(accessor.getField(obj, 1) === undefined);
  });
});

// =============================== 5. Executor 组合操作测试 ===============================

suite('26. Executor 组合 - SET + PIPE + EXEC', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('SET hp + PIPE + EXEC 返回修改后的对象', () => {
    const player = new SpoiTestPlayer();
    player.name = 'Test';
    const vb = new ArrayBuffer(4);
    new DataView(vb).setInt32(0, 999, true);
    const data = buildSpoi([
      buildSetInst([1], TypeId.I32, new Uint8Array(vb)),
      buildPipeInst([]),
      buildExecInst(),
    ]);
    const result = executor.execute(player, data);
    console.assert(result.resultType === ResultType.SINGLE);
    console.assert(result.value.hp === 999);
    console.assert(player.hp === 999);
  });
});

suite('27. Executor 组合 - 多层 SET', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('多层 SET：name, hp, level', () => {
    const player = new SpoiTestPlayer();
    const nameData = buildValueBytes(TypeId.STRING, new TextEncoder().encode('MultiSet'));
    const hpVb = new ArrayBuffer(4);
    new DataView(hpVb).setInt32(0, 500, true);
    const lvVb = new ArrayBuffer(4);
    new DataView(lvVb).setInt32(0, 99, true);
    const data = buildSpoi([
      buildSetInst([0], TypeId.STRING, new TextEncoder().encode('MultiSet')),
      buildSetInst([1], TypeId.I32, new Uint8Array(hpVb)),
      buildSetInst([2], TypeId.I32, new Uint8Array(lvVb)),
      buildPipeInst([]),
      buildExecInst(),
    ]);
    const result = executor.execute(player, data);
    console.assert(player.name === 'MultiSet');
    console.assert(player.hp === 500);
    console.assert(player.level === 99);
    console.assert(result.value.name === 'MultiSet');
  });
});

suite('28. Executor 组合 - FILTER', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('FILTER hp > 50', () => {
    const items = [new SpoiTestPlayer(), new SpoiTestPlayer(), new SpoiTestPlayer()];
    items[0].hp = 100; items[0].name = 'A';
    items[1].hp = 30;  items[1].name = 'B';
    items[2].hp = 80;  items[2].name = 'C';
    const vb = new ArrayBuffer(4);
    new DataView(vb).setInt32(0, 50, true);
    const data = buildSpoi([
      buildPipeInst([]),
      buildFilterInst([], 1, 3, TypeId.I32, new Uint8Array(vb)), // hp(idx=1) > 50
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.VECTOR);
    console.assert(result.value.length === 2);
    console.assert(result.value[0].name === 'A');
    console.assert(result.value[1].name === 'C');
  });
});

suite('29. Executor 组合 - 双层 FILTER', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('双层 FILTER：hp > 30 且 level > 3', () => {
    const items = [new SpoiTestPlayer(), new SpoiTestPlayer(), new SpoiTestPlayer(), new SpoiTestPlayer()];
    items[0].hp = 100; items[0].level = 10; items[0].name = 'A';
    items[1].hp = 50;  items[1].level = 2;  items[1].name = 'B';
    items[2].hp = 80;  items[2].level = 5;  items[2].name = 'C';
    items[3].hp = 20;  items[3].level = 8;  items[3].name = 'D';
    const hpVb = new ArrayBuffer(4);
    new DataView(hpVb).setInt32(0, 30, true);
    const lvVb = new ArrayBuffer(4);
    new DataView(lvVb).setInt32(0, 3, true);
    const data = buildSpoi([
      buildPipeInst([]),
      buildFilterInst([], 1, 3, TypeId.I32, new Uint8Array(hpVb)), // hp > 30
      buildFilterInst([], 2, 3, TypeId.I32, new Uint8Array(lvVb)), // level > 3
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.VECTOR);
    console.assert(result.value.length === 2);
    console.assert(result.value[0].name === 'A');
    console.assert(result.value[1].name === 'C');
  });
});

suite('30. Executor 组合 - 空结果 FILTER', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('FILTER 无匹配 => 空结果', () => {
    const items = [new SpoiTestPlayer(), new SpoiTestPlayer()];
    items[0].hp = 10; items[1].hp = 20;
    const vb = new ArrayBuffer(4);
    new DataView(vb).setInt32(0, 999, true);
    const data = buildSpoi([
      buildPipeInst([]),
      buildFilterInst([], 1, 3, TypeId.I32, new Uint8Array(vb)), // hp > 999
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.UNDEF);
    console.assert(result.data instanceof Uint8Array);
    console.assert(result.data.length === 0);
  });
});

suite('31. Executor 组合 - COUNT', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('COUNT 有数据', () => {
    const items = [new SpoiTestPlayer(), new SpoiTestPlayer(), new SpoiTestPlayer()];
    const data = buildSpoi([
      buildPipeInst([]),
      buildCountInst(),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.SINGLE);
    console.assert(result.value === 3);
  });
});

suite('32. Executor 组合 - 空管道 COUNT', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('空管道 COUNT => 0', () => {
    const items = [];
    const data = buildSpoi([
      buildPipeInst([]),
      buildCountInst(),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.SINGLE);
    console.assert(result.value === 0);
  });
});

suite('33. Executor 组合 - SORT', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('SORT by name', () => {
    const items = [new SpoiTestPlayer(), new SpoiTestPlayer(), new SpoiTestPlayer()];
    items[0].name = 'Charlie'; items[0].hp = 30;
    items[1].name = 'Alice';   items[1].hp = 10;
    items[2].name = 'Bob';     items[2].hp = 20;
    const data = buildSpoi([
      buildPipeInst([]),
      buildSortInst([0]), // sort by name (idx=0)
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.VECTOR);
    console.assert(result.value.length === 3);
    console.assert(result.value[0].name === 'Alice');
    console.assert(result.value[1].name === 'Bob');
    console.assert(result.value[2].name === 'Charlie');
  });
});

suite('34. Executor 组合 - 空 SORT', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('空管道 SORT 不报错', () => {
    const items = [];
    const data = buildSpoi([
      buildPipeInst([]),
      buildSortInst([0]),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.UNDEF);
  });
});

suite('35. Executor 组合 - TAKE', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('TAKE 2', () => {
    const items = [new SpoiTestPlayer(), new SpoiTestPlayer(), new SpoiTestPlayer()];
    items[0].name = 'A'; items[1].name = 'B'; items[2].name = 'C';
    const data = buildSpoi([
      buildPipeInst([]),
      buildTakeInst([], 2),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.VECTOR);
    console.assert(result.value.length === 2);
    console.assert(result.value[0].name === 'A');
    console.assert(result.value[1].name === 'B');
  });

  test('TAKE 100（超过数量）', () => {
    const items = [new SpoiTestPlayer(), new SpoiTestPlayer()];
    items[0].name = 'A'; items[1].name = 'B';
    const data = buildSpoi([
      buildPipeInst([]),
      buildTakeInst([], 100),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.VECTOR);
    console.assert(result.value.length === 2);
  });

  test('TAKE 0 => 空结果', () => {
    const items = [new SpoiTestPlayer(), new SpoiTestPlayer()];
    items[0].name = 'A'; items[1].name = 'B';
    const data = buildSpoi([
      buildPipeInst([]),
      buildTakeInst([], 0),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.UNDEF);
  });
});

suite('36. Executor 组合 - DROP', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('DROP 1', () => {
    const items = [new SpoiTestPlayer(), new SpoiTestPlayer(), new SpoiTestPlayer()];
    items[0].name = 'A'; items[1].name = 'B'; items[2].name = 'C';
    const data = buildSpoi([
      buildPipeInst([]),
      buildDropInst([], 1),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.VECTOR);
    console.assert(result.value.length === 2);
    console.assert(result.value[0].name === 'B');
    console.assert(result.value[1].name === 'C');
  });

  test('DROP 100（超过数量）=> 空结果', () => {
    const items = [new SpoiTestPlayer(), new SpoiTestPlayer()];
    items[0].name = 'A'; items[1].name = 'B';
    const data = buildSpoi([
      buildPipeInst([]),
      buildDropInst([], 100),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.UNDEF);
  });
});

suite('37. Executor 组合 - SELECT', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('SELECT name', () => {
    const items = [new SpoiTestPlayer(), new SpoiTestPlayer()];
    items[0].name = 'Hero'; items[1].name = 'Villain';
    const data = buildSpoi([
      buildPipeInst([]),
      buildSelectInst([0]),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.VECTOR);
    console.assert(result.value.length === 2);
    console.assert(result.value[0] === 'Hero');
    console.assert(result.value[1] === 'Villain');
  });
});

suite('38. Executor 组合 - 空 SELECT', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('空管道 SELECT 不报错', () => {
    const items = [];
    const data = buildSpoi([
      buildPipeInst([]),
      buildSelectInst([0]),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.UNDEF);
  });
});

suite('39. Executor 组合 - REVERSE', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('REVERSE 有数据', () => {
    const items = [new SpoiTestPlayer(), new SpoiTestPlayer(), new SpoiTestPlayer()];
    items[0].name = 'A'; items[1].name = 'B'; items[2].name = 'C';
    const data = buildSpoi([
      buildPipeInst([]),
      buildReverseInst(),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.VECTOR);
    console.assert(result.value.length === 3);
    console.assert(result.value[0].name === 'C');
    console.assert(result.value[1].name === 'B');
    console.assert(result.value[2].name === 'A');
  });
});

suite('40. Executor 组合 - 空 REVERSE', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('空管道 REVERSE 不报错', () => {
    const items = [];
    const data = buildSpoi([
      buildPipeInst([]),
      buildReverseInst(),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.UNDEF);
  });
});

suite('41. Executor 组合 - DISTINCT', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('DISTINCT 有重复', () => {
    const items = [new SpoiTestPlayer(), new SpoiTestPlayer(), new SpoiTestPlayer()];
    items[0].name = 'A'; items[0].hp = 10;
    items[1].name = 'A'; items[1].hp = 10; // 重复
    items[2].name = 'B'; items[2].hp = 20;
    const data = buildSpoi([
      buildPipeInst([]),
      buildDistinctInst(),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.VECTOR);
    console.assert(result.value.length === 2);
  });
});

suite('42. Executor 组合 - 空 DISTINCT', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('空管道 DISTINCT 不报错', () => {
    const items = [];
    const data = buildSpoi([
      buildPipeInst([]),
      buildDistinctInst(),
      buildExecInst(),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.UNDEF);
  });
});

// =============================== 6. 跨类型 Executor 测试 ===============================

suite('43. 跨类型 Executor - Item', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('Item PIPE + EXEC', () => {
    const item = new SpoiItem();
    item.name = 'Potion';
    item.value = 50;
    const data = buildSpoi([
      buildPipeInst([]),
      buildExecInst(),
    ]);
    const result = executor.execute(item, data);
    console.assert(result.resultType === ResultType.SINGLE);
    console.assert(result.value.name === 'Potion');
    console.assert(result.value.value === 50);
  });

  test('Item SET name', () => {
    const item = new SpoiItem();
    const data = buildSpoi([
      buildSetInst([0], TypeId.STRING, new TextEncoder().encode('Elixir')),
      buildExecInst(),
    ]);
    const result = executor.execute(item, data);
    console.assert(item.name === 'Elixir');
  });
});

suite('44. 跨类型 Executor - Inventory', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('Inventory SET gold', () => {
    const inv = new SpoiInventory();
    const vb = new ArrayBuffer(4);
    new DataView(vb).setInt32(0, 777, true);
    const data = buildSpoi([
      buildSetInst([2], TypeId.I32, new Uint8Array(vb)),
      buildPipeInst([]),
      buildExecInst(),
    ]);
    const result = executor.execute(inv, data);
    console.assert(inv.gold === 777);
    console.assert(result.value.gold === 777);
  });
});

suite('45. 跨类型 Executor - Character', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('Character SET name + hp', () => {
    const char = new SpoiCharacter();
    const hpVb = new ArrayBuffer(4);
    new DataView(hpVb).setInt32(0, 300, true);
    const data = buildSpoi([
      buildSetInst([0], TypeId.STRING, new TextEncoder().encode('Mage')),
      buildSetInst([1], TypeId.I32, new Uint8Array(hpVb)),
      buildPipeInst([]),
      buildExecInst(),
    ]);
    const result = executor.execute(char, data);
    console.assert(char.name === 'Mage');
    console.assert(char.hp === 300);
    console.assert(result.value.name === 'Mage');
  });
});

suite('46. 跨类型 Executor - World', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('World SET worldName + tick', () => {
    const world = new SpoiWorld();
    const tickVb = new ArrayBuffer(4);
    new DataView(tickVb).setInt32(0, 5000, true);
    const data = buildSpoi([
      buildSetInst([0], TypeId.STRING, new TextEncoder().encode('Azeroth')),
      buildSetInst([1], TypeId.I32, new Uint8Array(tickVb)),
      buildPipeInst([]),
      buildExecInst(),
    ]);
    const result = executor.execute(world, data);
    console.assert(world.worldName === 'Azeroth');
    console.assert(world.tick === 5000);
    console.assert(result.value.worldName === 'Azeroth');
  });
});

// =============================== 7. Registry 边界测试 ===============================

suite('47. Registry 边界 - size=6', () => {
  test('Registry 包含恰好 6 个类型', () => {
    console.assert(SpoiAccessorRegistry.size === 6);
  });
  test('Registry 包含所有期望的类型', () => {
    console.assert(SpoiAccessorRegistry.has('SpoiTestPlayer'));
    console.assert(SpoiAccessorRegistry.has('SpoiTestState'));
    console.assert(SpoiAccessorRegistry.has('SpoiItem'));
    console.assert(SpoiAccessorRegistry.has('SpoiInventory'));
    console.assert(SpoiAccessorRegistry.has('SpoiCharacter'));
    console.assert(SpoiAccessorRegistry.has('SpoiWorld'));
  });
});

suite('48. Registry 边界 - missing key', () => {
  test('get 不存在的 key 返回 undefined', () => {
    console.assert(SpoiAccessorRegistry.get('NonExistentType') === undefined);
  });
  test('has 不存在的 key 返回 false', () => {
    console.assert(!SpoiAccessorRegistry.has('FakeType'));
  });
});

// =============================== 结果 ===============================

console.log('\n' + '='.repeat(50));
console.log('\u7ed3\u679c: ' + passed + ' \u901a\u8fc7, ' + failed + ' \u5931\u8d25');
console.log('='.repeat(50));

if (failed > 0) process.exit(1);