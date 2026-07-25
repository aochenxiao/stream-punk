/**
 * SPOI Accessor TypeScript 测试套件
 *
 * 测试覆盖：TypeId 常量、deserializeValue、各 Accessor 的 getField/setField、
 * SpoiAccessorRegistry、Executor 集成（v2 访问器驱动）
 * 零外部依赖，使用 node 内置 assert 模块
 *
 * 运行：npx ts-node test_spoi_accessor.ts
 * 或：tsc test_spoi_accessor.ts --target ES2020 && node test_spoi_accessor.js
 */

import { TypeId, deserializeValue, SpoiAccessorRegistry, SpoiAccessor } from './spoi_ts_accessor';
import { SpoiExecutor, ResultType, PATH_DEREF } from './spoi_executor';
declare var require: any;
declare var process: any;
const assert = require('assert');

// Op 是 const enum，运行时不可用，使用字面量
const SET    = 0x04;
const PIPE   = 0x22;
const SELECT = 0x0D;
const EXEC   = 0x21;

// ResultType 也是 const enum，使用字面量
const RT_UNDEF  = 0;
const RT_SINGLE = 1;
const RT_VECTOR = 2;

// =============================== 测试数据类（构造函数名需与 Registry 键一致） ===============================

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

// =============================== 辅助函数 ===============================

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

/** 构造带 type_id 前缀的 operand（v2 格式：[type_id(u32 LE) + value_bytes]） */
function typedOperand(typeId: number, valueBytes: number[]): Uint8Array {
  const total = 4 + valueBytes.length;
  const buf = new ArrayBuffer(total);
  const view = new DataView(buf);
  view.setUint32(0, typeId, true);
  const arr = new Uint8Array(buf);
  for (let i = 0; i < valueBytes.length; i++) arr[4 + i] = valueBytes[i];
  return arr;
}

function i32Bytes(n: number): number[] {
  const buf = new ArrayBuffer(4);
  new DataView(buf).setInt32(0, n, true);
  return Array.from(new Uint8Array(buf));
}

function u32Bytes(n: number): number[] {
  const buf = new ArrayBuffer(4);
  new DataView(buf).setUint32(0, n, true);
  return Array.from(new Uint8Array(buf));
}

function f32Bytes(n: number): number[] {
  const buf = new ArrayBuffer(4);
  new DataView(buf).setFloat32(0, n, true);
  return Array.from(new Uint8Array(buf));
}

function u8Bytes(n: number): number[] {
  return [n & 0xFF];
}

function strBytes(s: string): number[] {
  return Array.from(new TextEncoder().encode(s));
}

function setIntField(path: number[], value: number) {
  return inst(SET, path, typedOperand(TypeId.I32, i32Bytes(value)));
}

function setStrField(path: number[], value: string) {
  return inst(SET, path, typedOperand(TypeId.STRING, strBytes(value)));
}

function setBoolField(path: number[], value: boolean) {
  return inst(SET, path, typedOperand(TypeId.BOOL, [value ? 1 : 0]));
}

function pipeOp(path?: number[]) {
  return inst(PIPE, path || [], new Uint8Array());
}

function selectOp(path: number[]) {
  return inst(SELECT, path, new Uint8Array());
}

function execOp() {
  return inst(EXEC, [], new Uint8Array());
}

// =============================== 测试框架 ===============================

let passed = 0;
let failed = 0;

function test(name: string, fn: () => void) {
  try {
    fn();
    passed++;
    console.log(`  \u2713 ${name}`);
  } catch (e) {
    failed++;
    console.log(`  \u2717 ${name}`);
    console.log(`    ${(e as Error).message}`);
  }
}

function suite(name: string, fn: () => void) {
  console.log(`\n${name}`);
  fn();
}

// =============================== 1. TypeId 常量测试 ===============================

suite('TypeId 常量', () => {
  test('整数类型', () => {
    assert.strictEqual(TypeId.U8, 26);
    assert.strictEqual(TypeId.U16, 27);
    assert.strictEqual(TypeId.U32, 28);
    assert.strictEqual(TypeId.U64, 29);
  });

  test('有符号整数类型', () => {
    assert.strictEqual(TypeId.I8, 30);
    assert.strictEqual(TypeId.I16, 31);
    assert.strictEqual(TypeId.I32, 32);
    assert.strictEqual(TypeId.I64, 33);
  });

  test('浮点类型', () => {
    assert.strictEqual(TypeId.F32, 34);
    assert.strictEqual(TypeId.F64, 35);
  });

  test('字符串与布尔类型', () => {
    assert.strictEqual(TypeId.STRING, 9);
    assert.strictEqual(TypeId.BOOL, 40);
  });

  test('CUSTOM 类型', () => {
    assert.strictEqual(TypeId.CUSTOM, 0);
  });
});

// =============================== 2. deserializeValue 函数测试 ===============================

suite('deserializeValue', () => {
  test('U8', () => {
    const data = typedOperand(TypeId.U8, u8Bytes(42));
    assert.strictEqual(deserializeValue(data), 42);
  });

  test('U8 最大值', () => {
    const data = typedOperand(TypeId.U8, u8Bytes(255));
    assert.strictEqual(deserializeValue(data), 255);
  });

  test('U16', () => {
    const buf = new ArrayBuffer(4 + 2);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.U16, true);
    view.setUint16(4, 0x1234, true);
    assert.strictEqual(deserializeValue(new Uint8Array(buf)), 0x1234);
  });

  test('U32', () => {
    const buf = new ArrayBuffer(4 + 4);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.U32, true);
    view.setUint32(4, 0xDEADBEEF, true);
    assert.strictEqual(deserializeValue(new Uint8Array(buf)), 0xDEADBEEF);
  });

  test('U64', () => {
    const buf = new ArrayBuffer(4 + 8);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.U64, true);
    view.setBigUint64(4, 0x123456789ABCDEF0n, true);
    assert.strictEqual(deserializeValue(new Uint8Array(buf)), 0x123456789ABCDEF0n);
  });

  test('I8 正数', () => {
    const buf = new ArrayBuffer(4 + 1);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.I8, true);
    view.setInt8(4, 100);
    assert.strictEqual(deserializeValue(new Uint8Array(buf)), 100);
  });

  test('I8 负数', () => {
    const buf = new ArrayBuffer(4 + 1);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.I8, true);
    view.setInt8(4, -128);
    assert.strictEqual(deserializeValue(new Uint8Array(buf)), -128);
  });

  test('I16', () => {
    const buf = new ArrayBuffer(4 + 2);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.I16, true);
    view.setInt16(4, -1000, true);
    assert.strictEqual(deserializeValue(new Uint8Array(buf)), -1000);
  });

  test('I32', () => {
    const buf = new ArrayBuffer(4 + 4);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.I32, true);
    view.setInt32(4, -123456, true);
    assert.strictEqual(deserializeValue(new Uint8Array(buf)), -123456);
  });

  test('I64', () => {
    const buf = new ArrayBuffer(4 + 8);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.I64, true);
    view.setBigInt64(4, -1234567890123n, true);
    assert.strictEqual(deserializeValue(new Uint8Array(buf)), -1234567890123n);
  });

  test('F32', () => {
    const buf = new ArrayBuffer(4 + 4);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.F32, true);
    view.setFloat32(4, 3.14, true);
    assert.ok(Math.abs((deserializeValue(new Uint8Array(buf)) as number) - 3.14) < 0.001);
  });

  test('F64', () => {
    const buf = new ArrayBuffer(4 + 8);
    const view = new DataView(buf);
    view.setUint32(0, TypeId.F64, true);
    view.setFloat64(4, 3.141592653589793, true);
    assert.strictEqual(deserializeValue(new Uint8Array(buf)), 3.141592653589793);
  });

  test('String', () => {
    const text = 'hello world';
    const bytes = strBytes(text);
    const data = typedOperand(TypeId.STRING, bytes);
    assert.strictEqual(deserializeValue(data), text);
  });

  test('String 空串', () => {
    const data = typedOperand(TypeId.STRING, []);
    assert.strictEqual(deserializeValue(data), '');
  });

  test('String 中文', () => {
    const text = '你好世界';
    const bytes = strBytes(text);
    const data = typedOperand(TypeId.STRING, bytes);
    assert.strictEqual(deserializeValue(data), text);
  });

  test('Bool true', () => {
    const data = typedOperand(TypeId.BOOL, [1]);
    assert.strictEqual(deserializeValue(data), true);
  });

  test('Bool false', () => {
    const data = typedOperand(TypeId.BOOL, [0]);
    assert.strictEqual(deserializeValue(data), false);
  });

  test('空数据（长度不足）', () => {
    assert.strictEqual(deserializeValue(new Uint8Array(0)), null);
    assert.strictEqual(deserializeValue(new Uint8Array(1)), null);
    assert.strictEqual(deserializeValue(new Uint8Array(3)), null);
  });
});

// =============================== 3. Accessor 测试 ===============================

suite('SpoiTestPlayerAccessor', () => {
  const accessor = SpoiAccessorRegistry.get('SpoiTestPlayer')!;

  test('fieldCount', () => {
    assert.strictEqual(accessor.fieldCount(), 4);
  });

  test('getField', () => {
    const player = new SpoiTestPlayer({ name: 'Alice', hp: 100, level: 10, posX: 5.5 });
    assert.strictEqual(accessor.getField(player, 0), 'Alice');
    assert.strictEqual(accessor.getField(player, 1), 100);
    assert.strictEqual(accessor.getField(player, 2), 10);
    assert.strictEqual(accessor.getField(player, 3), 5.5);
  });

  test('getField 越界抛错', () => {
    const player = new SpoiTestPlayer();
    assert.throws(() => accessor.getField(player, 4), /invalid field index/);
    assert.throws(() => accessor.getField(player, -1), /invalid field index/);
  });

  test('setField', () => {
    const player = new SpoiTestPlayer();
    accessor.setField(player, 0, 'Bob');
    accessor.setField(player, 1, 200);
    accessor.setField(player, 2, 20);
    accessor.setField(player, 3, 10.5);
    assert.strictEqual(player.name, 'Bob');
    assert.strictEqual(player.hp, 200);
    assert.strictEqual(player.level, 20);
    assert.strictEqual(player.posX, 10.5);
  });

  test('setField 越界抛错', () => {
    const player = new SpoiTestPlayer();
    assert.throws(() => accessor.setField(player, 4, 'x'), /invalid field index/);
  });
});

suite('SpoiTestStateAccessor', () => {
  const accessor = SpoiAccessorRegistry.get('SpoiTestState')!;

  test('fieldCount', () => {
    assert.strictEqual(accessor.fieldCount(), 3);
  });

  test('getField', () => {
    const state = new SpoiTestState({ tick: 42, currentMap: 'overworld', players: [1, 2, 3] });
    assert.strictEqual(accessor.getField(state, 0), 42);
    assert.strictEqual(accessor.getField(state, 1), 'overworld');
    assert.deepStrictEqual(accessor.getField(state, 2), [1, 2, 3]);
  });

  test('setField', () => {
    const state = new SpoiTestState();
    accessor.setField(state, 0, 99);
    accessor.setField(state, 1, 'dungeon');
    accessor.setField(state, 2, null);
    assert.strictEqual(state.tick, 99);
    assert.strictEqual(state.currentMap, 'dungeon');
    assert.strictEqual(state.players, null);
  });
});

suite('SpoiItemAccessor', () => {
  const accessor = SpoiAccessorRegistry.get('SpoiItem')!;

  test('fieldCount', () => {
    assert.strictEqual(accessor.fieldCount(), 2);
  });

  test('getField', () => {
    const item = new SpoiItem({ name: 'Sword', value: 150 });
    assert.strictEqual(accessor.getField(item, 0), 'Sword');
    assert.strictEqual(accessor.getField(item, 1), 150);
  });

  test('setField', () => {
    const item = new SpoiItem();
    accessor.setField(item, 0, 'Shield');
    accessor.setField(item, 1, 200);
    assert.strictEqual(item.name, 'Shield');
    assert.strictEqual(item.value, 200);
  });
});

suite('SpoiInventoryAccessor', () => {
  const accessor = SpoiAccessorRegistry.get('SpoiInventory')!;

  test('fieldCount', () => {
    assert.strictEqual(accessor.fieldCount(), 3);
  });

  test('getField', () => {
    const items = [new SpoiItem({ name: 'Potion', value: 50 })];
    const inv = new SpoiInventory({ items, equipped: null, gold: 500 });
    assert.deepStrictEqual(accessor.getField(inv, 0), items);
    assert.strictEqual(accessor.getField(inv, 1), null);
    assert.strictEqual(accessor.getField(inv, 2), 500);
  });

  test('setField', () => {
    const inv = new SpoiInventory();
    accessor.setField(inv, 0, ['a']);
    accessor.setField(inv, 1, 'sword');
    accessor.setField(inv, 2, 999);
    assert.deepStrictEqual(inv.items, ['a']);
    assert.strictEqual(inv.equipped, 'sword');
    assert.strictEqual(inv.gold, 999);
  });
});

suite('SpoiCharacterAccessor', () => {
  const accessor = SpoiAccessorRegistry.get('SpoiCharacter')!;

  test('fieldCount', () => {
    assert.strictEqual(accessor.fieldCount(), 5);
  });

  test('getField', () => {
    const inv = new SpoiInventory({ gold: 100 });
    const char = new SpoiCharacter({ name: 'Hero', hp: 80, inventory: inv, weapon: 'Axe', petLevel: 3 });
    assert.strictEqual(accessor.getField(char, 0), 'Hero');
    assert.strictEqual(accessor.getField(char, 1), 80);
    assert.strictEqual(accessor.getField(char, 2), inv);
    assert.strictEqual(accessor.getField(char, 3), 'Axe');
    assert.strictEqual(accessor.getField(char, 4), 3);
  });

  test('setField', () => {
    const char = new SpoiCharacter();
    accessor.setField(char, 0, 'Villain');
    accessor.setField(char, 1, 50);
    accessor.setField(char, 2, null);
    accessor.setField(char, 3, 'Bow');
    accessor.setField(char, 4, 7);
    assert.strictEqual(char.name, 'Villain');
    assert.strictEqual(char.hp, 50);
    assert.strictEqual(char.inventory, null);
    assert.strictEqual(char.weapon, 'Bow');
    assert.strictEqual(char.petLevel, 7);
  });
});

suite('SpoiWorldAccessor', () => {
  const accessor = SpoiAccessorRegistry.get('SpoiWorld')!;

  test('fieldCount', () => {
    assert.strictEqual(accessor.fieldCount(), 3);
  });

  test('getField', () => {
    const chars = [new SpoiCharacter({ name: 'Hero' })];
    const world = new SpoiWorld({ worldName: 'Realm', tick: 1000, characters: chars });
    assert.strictEqual(accessor.getField(world, 0), 'Realm');
    assert.strictEqual(accessor.getField(world, 1), 1000);
    assert.deepStrictEqual(accessor.getField(world, 2), chars);
  });

  test('setField', () => {
    const world = new SpoiWorld();
    accessor.setField(world, 0, 'Dreamland');
    accessor.setField(world, 1, 500);
    accessor.setField(world, 2, []);
    assert.strictEqual(world.worldName, 'Dreamland');
    assert.strictEqual(world.tick, 500);
    assert.deepStrictEqual(world.characters, []);
  });
});

// =============================== 4. SpoiAccessorRegistry 测试 ===============================

suite('SpoiAccessorRegistry', () => {
  test('包含所有 6 个类型', () => {
    assert.strictEqual(SpoiAccessorRegistry.size, 6);
    assert.ok(SpoiAccessorRegistry.has('SpoiTestPlayer'));
    assert.ok(SpoiAccessorRegistry.has('SpoiTestState'));
    assert.ok(SpoiAccessorRegistry.has('SpoiItem'));
    assert.ok(SpoiAccessorRegistry.has('SpoiInventory'));
    assert.ok(SpoiAccessorRegistry.has('SpoiCharacter'));
    assert.ok(SpoiAccessorRegistry.has('SpoiWorld'));
  });

  test('每个 entry 都是 SpoiAccessor 实例', () => {
    for (const [name, acc] of SpoiAccessorRegistry) {
      assert.ok(typeof acc.fieldCount === 'function', `${name}: fieldCount 应为函数`);
      assert.ok(typeof acc.getField === 'function', `${name}: getField 应为函数`);
      assert.ok(typeof acc.setField === 'function', `${name}: setField 应为函数`);
      assert.ok(acc.fieldCount() > 0, `${name}: fieldCount 应 > 0`);
    }
  });
});

// =============================== 5. Executor 集成测试（v2 访问器驱动） ===============================

suite('Executor 集成（v2 访问器驱动）', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  test('SET 写入字段', () => {
    const player = new SpoiTestPlayer({ name: 'Alice', hp: 100, level: 10, posX: 0 });
    const data = buildStream([setIntField([1], 200), execOp()]);
    executor.execute(player, data);
    assert.strictEqual(player.hp, 200);
  });

  test('SET 写入字符串字段', () => {
    const player = new SpoiTestPlayer({ name: 'Alice', hp: 100, level: 10, posX: 0 });
    const data = buildStream([setStrField([0], 'Bob'), execOp()]);
    executor.execute(player, data);
    assert.strictEqual(player.name, 'Bob');
  });

  test('SET 写入多个字段', () => {
    const player = new SpoiTestPlayer({ name: 'Alice', hp: 100, level: 10, posX: 0 });
    const data = buildStream([
      setIntField([1], 999),
      setIntField([2], 50),
      setStrField([0], 'Zelda'),
      execOp(),
    ]);
    executor.execute(player, data);
    assert.strictEqual(player.hp, 999);
    assert.strictEqual(player.level, 50);
    assert.strictEqual(player.name, 'Zelda');
  });

  test('PIPE 数组 → 结果类型 VECTOR', () => {
    const players = [
      new SpoiTestPlayer({ name: 'A', hp: 10 }),
      new SpoiTestPlayer({ name: 'B', hp: 20 }),
      new SpoiTestPlayer({ name: 'C', hp: 30 }),
    ];
    const data = buildStream([pipeOp(), execOp()]);
    const result = executor.execute(players, data);
    assert.strictEqual(result.resultType, RT_VECTOR);
    assert.strictEqual((result.value as any[]).length, 3);
  });

  test('PIPE 单个对象 → SINGLE', () => {
    const player = new SpoiTestPlayer({ name: 'Solo', hp: 50 });
    const data = buildStream([pipeOp(), execOp()]);
    const result = executor.execute(player, data);
    assert.strictEqual(result.resultType, RT_SINGLE);
    assert.strictEqual((result.value as SpoiTestPlayer).name, 'Solo');
  });

  test('PIPE → SELECT 字段', () => {
    const players = [
      new SpoiTestPlayer({ name: 'A', hp: 10, level: 1 }),
      new SpoiTestPlayer({ name: 'B', hp: 20, level: 2 }),
    ];
    const data = buildStream([pipeOp(), selectOp([0]), execOp()]);
    const result = executor.execute(players, data);
    assert.deepStrictEqual(result.value, ['A', 'B']);
  });

  test('导航嵌套结构体', () => {
    const inv = new SpoiInventory({ gold: 500 });
    const char = new SpoiCharacter({ name: 'Hero', hp: 80, inventory: inv, weapon: 'Sword', petLevel: 3 });
    // 导航到 character.inventory，然后访问 gold 字段
    const data = buildStream([pipeOp([2]), selectOp([2]), execOp()]);
    const result = executor.execute(char, data);
    assert.strictEqual(result.value, 500);
  });

  test('导航到 SpoiWorld 的 characters', () => {
    const chars = [
      new SpoiCharacter({ name: 'Hero', hp: 100 }),
      new SpoiCharacter({ name: 'Villain', hp: 50 }),
    ];
    const world = new SpoiWorld({ worldName: 'Realm', tick: 100, characters: chars });
    const data = buildStream([pipeOp([2]), selectOp([0]), execOp()]);
    const result = executor.execute(world, data);
    assert.deepStrictEqual(result.value, ['Hero', 'Villain']);
  });

  test('SET 后 PIPE 读取', () => {
    const player = new SpoiTestPlayer({ name: 'Alice', hp: 100, level: 10, posX: 0 });
    const data = buildStream([
      setIntField([1], 777),
      setStrField([0], 'Changed'),
      pipeOp(),
      execOp(),
    ]);
    const result = executor.execute(player, data);
    assert.strictEqual(player.hp, 777);
    assert.strictEqual(player.name, 'Changed');
    assert.strictEqual((result.value as SpoiTestPlayer).hp, 777);
  });

  test('写入 SpoiItem 并通过 PIPE 验证', () => {
    const item = new SpoiItem({ name: 'Dagger', value: 10 });
    const data = buildStream([
      setIntField([1], 999),
      setStrField([0], 'Excalibur'),
      pipeOp(),
      execOp(),
    ]);
    const result = executor.execute(item, data);
    assert.strictEqual(item.value, 999);
    assert.strictEqual(item.name, 'Excalibur');
    assert.strictEqual(result.resultType, RT_SINGLE);
  });

  test('写入 SpoiWorld 字段', () => {
    const world = new SpoiWorld({ worldName: 'Old', tick: 0, characters: [] });
    const data = buildStream([
      setStrField([0], 'NewWorld'),
      setIntField([1], 42),
      pipeOp(),
      execOp(),
    ]);
    const result = executor.execute(world, data);
    assert.strictEqual(world.worldName, 'NewWorld');
    assert.strictEqual(world.tick, 42);
    assert.strictEqual(result.resultType, RT_SINGLE);
  });

  test('访问器中不存在的类型抛出错误', () => {
    // 普通对象没有 constructor.name 匹配的访问器，导航应失败
    const plainObj = { x: 1, y: 2 };
    const data = buildStream([setIntField([0], 99), execOp()]);
    assert.throws(() => executor.execute(plainObj, data), /Cannot set field/);
  });
});

// =============================== 结果 ===============================

console.log(`\n${'='.repeat(50)}`);
console.log(`结果: ${passed} 通过, ${failed} 失败`);
console.log(`${'='.repeat(50)}`);

if (failed > 0) process.exit(1);