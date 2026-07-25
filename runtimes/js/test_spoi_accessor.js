/**
 * SPOI Accessor JavaScript 测试套件
 *
 * 测试 spoi_js_accessor.js 和 spoi_executor.js 的集成
 * 运行: node test_spoi_accessor.js
 */

const {
  TypeId, SpoiAccessor, deserializeValue, SpoiAccessorRegistry,
  SpoiTestPlayerAccessor, SpoiTestStateAccessor, SpoiItemAccessor,
  SpoiInventoryAccessor, SpoiCharacterAccessor, SpoiWorldAccessor,
} = require('./spoi_js_accessor.js');
const { SpoiExecutor, Op, ResultType } = require('./spoi_executor.js');

// =============================== 测试数据类 ===============================

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

// =============================== 测试框架 ===============================

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    passed++;
    console.log(`  \u2713 ${name}`);
  } catch (e) {
    failed++;
    console.log(`  \u2717 ${name}`);
    console.log(`    ${e.message}`);
  }
}

function suite(name, fn) {
  console.log(`\n${name}`);
  fn();
}

// =============================== 1. TypeId 常量测试 ===============================

suite('TypeId 常量', () => {
  test('U8 = 26', () => console.assert(TypeId.U8 === 26, `Expected 26, got ${TypeId.U8}`));
  test('U16 = 27', () => console.assert(TypeId.U16 === 27, `Expected 27, got ${TypeId.U16}`));
  test('U32 = 28', () => console.assert(TypeId.U32 === 28, `Expected 28, got ${TypeId.U32}`));
  test('U64 = 29', () => console.assert(TypeId.U64 === 29, `Expected 29, got ${TypeId.U64}`));
  test('I8 = 30', () => console.assert(TypeId.I8 === 30, `Expected 30, got ${TypeId.I8}`));
  test('I16 = 31', () => console.assert(TypeId.I16 === 31, `Expected 31, got ${TypeId.I16}`));
  test('I32 = 32', () => console.assert(TypeId.I32 === 32, `Expected 32, got ${TypeId.I32}`));
  test('I64 = 33', () => console.assert(TypeId.I64 === 33, `Expected 33, got ${TypeId.I64}`));
  test('F32 = 34', () => console.assert(TypeId.F32 === 34, `Expected 34, got ${TypeId.F32}`));
  test('F64 = 35', () => console.assert(TypeId.F64 === 35, `Expected 35, got ${TypeId.F64}`));
  test('STRING = 9', () => console.assert(TypeId.STRING === 9, `Expected 9, got ${TypeId.STRING}`));
  test('BOOL = 40', () => console.assert(TypeId.BOOL === 40, `Expected 40, got ${TypeId.BOOL}`));
});

// =============================== 2. deserializeValue 测试 ===============================

suite('deserializeValue', () => {
  test('U8', () => {
    const data = buildValueBytes(TypeId.U8, new Uint8Array([42]));
    console.assert(deserializeValue(data) === 42, `Expected 42, got ${deserializeValue(data)}`);
  });

  test('U16', () => {
    const valBuf = new ArrayBuffer(2);
    new DataView(valBuf).setUint16(0, 12345, true);
    const data = buildValueBytes(TypeId.U16, new Uint8Array(valBuf));
    console.assert(deserializeValue(data) === 12345, `Expected 12345, got ${deserializeValue(data)}`);
  });

  test('U32', () => {
    const valBuf = new ArrayBuffer(4);
    new DataView(valBuf).setUint32(0, 3000000000, true);
    const data = buildValueBytes(TypeId.U32, new Uint8Array(valBuf));
    console.assert(deserializeValue(data) === 3000000000, `Expected 3000000000, got ${deserializeValue(data)}`);
  });

  test('U64', () => {
    const valBuf = new ArrayBuffer(8);
    new DataView(valBuf).setBigUint64(0, 9007199254740991n, true);
    const data = buildValueBytes(TypeId.U64, new Uint8Array(valBuf));
    console.assert(deserializeValue(data) === 9007199254740991n, `Expected 9007199254740991n, got ${deserializeValue(data)}`);
  });

  test('I8', () => {
    const valBuf = new ArrayBuffer(1);
    new DataView(valBuf).setInt8(0, -42);
    const data = buildValueBytes(TypeId.I8, new Uint8Array(valBuf));
    console.assert(deserializeValue(data) === -42, `Expected -42, got ${deserializeValue(data)}`);
  });

  test('I16', () => {
    const valBuf = new ArrayBuffer(2);
    new DataView(valBuf).setInt16(0, -12345, true);
    const data = buildValueBytes(TypeId.I16, new Uint8Array(valBuf));
    console.assert(deserializeValue(data) === -12345, `Expected -12345, got ${deserializeValue(data)}`);
  });

  test('I32', () => {
    const valBuf = new ArrayBuffer(4);
    new DataView(valBuf).setInt32(0, -100000, true);
    const data = buildValueBytes(TypeId.I32, new Uint8Array(valBuf));
    console.assert(deserializeValue(data) === -100000, `Expected -100000, got ${deserializeValue(data)}`);
  });

  test('I64', () => {
    const valBuf = new ArrayBuffer(8);
    new DataView(valBuf).setBigInt64(0, -9007199254740991n, true);
    const data = buildValueBytes(TypeId.I64, new Uint8Array(valBuf));
    console.assert(deserializeValue(data) === -9007199254740991n, `Expected -9007199254740991n, got ${deserializeValue(data)}`);
  });

  test('F32', () => {
    const valBuf = new ArrayBuffer(4);
    new DataView(valBuf).setFloat32(0, 3.14, true);
    const data = buildValueBytes(TypeId.F32, new Uint8Array(valBuf));
    console.assert(Math.abs(deserializeValue(data) - 3.14) < 0.001, `Expected ~3.14, got ${deserializeValue(data)}`);
  });

  test('F64', () => {
    const valBuf = new ArrayBuffer(8);
    new DataView(valBuf).setFloat64(0, 2.718281828, true);
    const data = buildValueBytes(TypeId.F64, new Uint8Array(valBuf));
    console.assert(Math.abs(deserializeValue(data) - 2.718281828) < 0.0000001, `Expected ~2.718281828, got ${deserializeValue(data)}`);
  });

  test('String', () => {
    const strBytes = new TextEncoder().encode('hello world');
    const data = buildValueBytes(TypeId.STRING, strBytes);
    console.assert(deserializeValue(data) === 'hello world', `Expected "hello world", got "${deserializeValue(data)}"`);
  });

  test('Bool true', () => {
    const data = buildValueBytes(TypeId.BOOL, new Uint8Array([1]));
    console.assert(deserializeValue(data) === true, `Expected true, got ${deserializeValue(data)}`);
  });

  test('Bool false', () => {
    const data = buildValueBytes(TypeId.BOOL, new Uint8Array([0]));
    console.assert(deserializeValue(data) === false, `Expected false, got ${deserializeValue(data)}`);
  });

  test('empty data', () => {
    console.assert(deserializeValue(null) === null, `Expected null for null input`);
    console.assert(deserializeValue(new Uint8Array(0)) === null, `Expected null for empty input`);
    console.assert(deserializeValue(new Uint8Array(2)) === null, `Expected null for < 4 bytes`);
  });

  test('U8 with empty value bytes', () => {
    const data = buildValueBytes(TypeId.U8, new Uint8Array(0));
    console.assert(deserializeValue(data) === 0, `Expected 0 for empty U8 value, got ${deserializeValue(data)}`);
  });

  test('U32 with empty value bytes', () => {
    const data = buildValueBytes(TypeId.U32, new Uint8Array(0));
    console.assert(deserializeValue(data) === 0, `Expected 0 for empty U32 value, got ${deserializeValue(data)}`);
  });

  test('String empty', () => {
    const data = buildValueBytes(TypeId.STRING, new TextEncoder().encode(''));
    console.assert(deserializeValue(data) === '', `Expected "", got "${deserializeValue(data)}"`);
  });
});

// =============================== 3. Accessor 测试 ===============================

suite('SpoiTestPlayerAccessor', () => {
  const accessor = new SpoiTestPlayerAccessor();
  const obj = new SpoiTestPlayer();

  test('fieldCount = 4', () => console.assert(accessor.fieldCount() === 4, `Expected 4, got ${accessor.fieldCount()}`));

  test('getField default values', () => {
    console.assert(accessor.getField(obj, 0) === '', `name should be ""`);
    console.assert(accessor.getField(obj, 1) === 0, `hp should be 0`);
    console.assert(accessor.getField(obj, 2) === 0, `level should be 0`);
    console.assert(accessor.getField(obj, 3) === 0, `posX should be 0`);
  });

  test('setField and getField', () => {
    accessor.setField(obj, 0, 'Hero');
    accessor.setField(obj, 1, 100);
    accessor.setField(obj, 2, 5);
    accessor.setField(obj, 3, 12.5);
    console.assert(accessor.getField(obj, 0) === 'Hero', `name should be "Hero"`);
    console.assert(accessor.getField(obj, 1) === 100, `hp should be 100`);
    console.assert(accessor.getField(obj, 2) === 5, `level should be 5`);
    console.assert(accessor.getField(obj, 3) === 12.5, `posX should be 12.5`);
  });

  test('object properties reflect setField', () => {
    console.assert(obj.name === 'Hero', `obj.name should be "Hero"`);
    console.assert(obj.hp === 100, `obj.hp should be 100`);
    console.assert(obj.level === 5, `obj.level should be 5`);
    console.assert(obj.posX === 12.5, `obj.posX should be 12.5`);
  });

  test('getField invalid index throws', () => {
    let threw = false;
    try { accessor.getField(obj, 99); } catch (e) { threw = true; }
    console.assert(threw, 'Should throw for invalid getField index');
  });

  test('setField invalid index throws', () => {
    let threw = false;
    try { accessor.setField(obj, 99, 1); } catch (e) { threw = true; }
    console.assert(threw, 'Should throw for invalid setField index');
  });
});

suite('SpoiTestStateAccessor', () => {
  const accessor = new SpoiTestStateAccessor();
  const obj = new SpoiTestState();

  test('fieldCount = 3', () => console.assert(accessor.fieldCount() === 3, `Expected 3, got ${accessor.fieldCount()}`));

  test('getField default values', () => {
    console.assert(accessor.getField(obj, 0) === 0, 'tick should be 0');
    console.assert(accessor.getField(obj, 1) === '', 'currentMap should be ""');
    console.assert(accessor.getField(obj, 2) === null, 'players should be null');
  });

  test('setField and getField', () => {
    accessor.setField(obj, 0, 42);
    accessor.setField(obj, 1, 'Overworld');
    accessor.setField(obj, 2, [new SpoiTestPlayer()]);
    console.assert(accessor.getField(obj, 0) === 42, 'tick should be 42');
    console.assert(accessor.getField(obj, 1) === 'Overworld', 'currentMap should be "Overworld"');
    console.assert(Array.isArray(accessor.getField(obj, 2)), 'players should be array');
    console.assert(accessor.getField(obj, 2).length === 1, 'players should have 1 element');
  });
});

suite('SpoiItemAccessor', () => {
  const accessor = new SpoiItemAccessor();
  const obj = new SpoiItem();

  test('fieldCount = 2', () => console.assert(accessor.fieldCount() === 2, `Expected 2, got ${accessor.fieldCount()}`));

  test('setField and getField', () => {
    accessor.setField(obj, 0, 'Sword');
    accessor.setField(obj, 1, 150);
    console.assert(accessor.getField(obj, 0) === 'Sword', 'name should be "Sword"');
    console.assert(accessor.getField(obj, 1) === 150, 'value should be 150');
    console.assert(obj.name === 'Sword', 'obj.name should be "Sword"');
    console.assert(obj.value === 150, 'obj.value should be 150');
  });
});

suite('SpoiInventoryAccessor', () => {
  const accessor = new SpoiInventoryAccessor();
  const obj = new SpoiInventory();

  test('fieldCount = 3', () => console.assert(accessor.fieldCount() === 3, `Expected 3, got ${accessor.fieldCount()}`));

  test('setField and getField', () => {
    const items = [new SpoiItem(), new SpoiItem()];
    accessor.setField(obj, 0, items);
    accessor.setField(obj, 1, items[0]);
    accessor.setField(obj, 2, 999);
    console.assert(accessor.getField(obj, 0) === items, 'items should be the same array');
    console.assert(accessor.getField(obj, 1) === items[0], 'equipped should be the first item');
    console.assert(accessor.getField(obj, 2) === 999, 'gold should be 999');
  });
});

suite('SpoiCharacterAccessor', () => {
  const accessor = new SpoiCharacterAccessor();
  const obj = new SpoiCharacter();

  test('fieldCount = 5', () => console.assert(accessor.fieldCount() === 5, `Expected 5, got ${accessor.fieldCount()}`));

  test('setField and getField', () => {
    const inv = new SpoiInventory();
    accessor.setField(obj, 0, 'Knight');
    accessor.setField(obj, 1, 200);
    accessor.setField(obj, 2, inv);
    accessor.setField(obj, 3, 'Excalibur');
    accessor.setField(obj, 4, 3);
    console.assert(accessor.getField(obj, 0) === 'Knight', 'name should be "Knight"');
    console.assert(accessor.getField(obj, 1) === 200, 'hp should be 200');
    console.assert(accessor.getField(obj, 2) === inv, 'inventory should be the same object');
    console.assert(accessor.getField(obj, 3) === 'Excalibur', 'weapon should be "Excalibur"');
    console.assert(accessor.getField(obj, 4) === 3, 'petLevel should be 3');
  });
});

suite('SpoiWorldAccessor', () => {
  const accessor = new SpoiWorldAccessor();
  const obj = new SpoiWorld();

  test('fieldCount = 3', () => console.assert(accessor.fieldCount() === 3, `Expected 3, got ${accessor.fieldCount()}`));

  test('setField and getField', () => {
    const chars = [new SpoiCharacter()];
    accessor.setField(obj, 0, 'Fantasia');
    accessor.setField(obj, 1, 1000);
    accessor.setField(obj, 2, chars);
    console.assert(accessor.getField(obj, 0) === 'Fantasia', 'worldName should be "Fantasia"');
    console.assert(accessor.getField(obj, 1) === 1000, 'tick should be 1000');
    console.assert(accessor.getField(obj, 2) === chars, 'characters should be the same array');
  });
});

// =============================== 4. SpoiAccessorRegistry 测试 ===============================

suite('SpoiAccessorRegistry', () => {
  test('is a Map', () => console.assert(SpoiAccessorRegistry instanceof Map, 'Should be a Map'));

  test('contains all 6 types', () => {
    console.assert(SpoiAccessorRegistry.has('SpoiTestPlayer'), 'Should have SpoiTestPlayer');
    console.assert(SpoiAccessorRegistry.has('SpoiTestState'), 'Should have SpoiTestState');
    console.assert(SpoiAccessorRegistry.has('SpoiItem'), 'Should have SpoiItem');
    console.assert(SpoiAccessorRegistry.has('SpoiInventory'), 'Should have SpoiInventory');
    console.assert(SpoiAccessorRegistry.has('SpoiCharacter'), 'Should have SpoiCharacter');
    console.assert(SpoiAccessorRegistry.has('SpoiWorld'), 'Should have SpoiWorld');
    console.assert(SpoiAccessorRegistry.size === 6, `Expected 6 entries, got ${SpoiAccessorRegistry.size}`);
  });

  test('each entry is a SpoiAccessor instance', () => {
    for (const [name, accessor] of SpoiAccessorRegistry) {
      console.assert(accessor instanceof SpoiAccessor, `${name} should be SpoiAccessor instance`);
    }
  });

  test('each accessor has correct fieldCount', () => {
    console.assert(SpoiAccessorRegistry.get('SpoiTestPlayer').fieldCount() === 4);
    console.assert(SpoiAccessorRegistry.get('SpoiTestState').fieldCount() === 3);
    console.assert(SpoiAccessorRegistry.get('SpoiItem').fieldCount() === 2);
    console.assert(SpoiAccessorRegistry.get('SpoiInventory').fieldCount() === 3);
    console.assert(SpoiAccessorRegistry.get('SpoiCharacter').fieldCount() === 5);
    console.assert(SpoiAccessorRegistry.get('SpoiWorld').fieldCount() === 3);
  });
});

// =============================== 5. Executor 集成测试 ===============================

suite('Executor + Accessor 集成', () => {
  const executor = new SpoiExecutor(SpoiAccessorRegistry);

  function writeVarint(buf, v) {
    while (v >= 0x80) { buf.push((v & 0x7F) | 0x80); v >>>= 7; }
    buf.push(v & 0x7F);
  }

  function buildStream(instructions) {
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

  function inst(op, path, operand) {
    return { op, path, operand };
  }

  test('pipe and exec on SpoiTestPlayer', () => {
    const player = new SpoiTestPlayer();
    player.name = 'Alice';
    player.hp = 100;
    player.level = 10;
    const data = buildStream([inst(Op.PIPE, [], new Uint8Array()), inst(Op.EXEC, [], new Uint8Array())]);
    const result = executor.execute(player, data);
    console.assert(result.resultType === ResultType.SINGLE, `Expected SINGLE, got ${result.resultType}`);
    console.assert(result.value === player, 'Value should be the player object');
  });

  test('set field on SpoiTestPlayer via accessor', () => {
    const player = new SpoiTestPlayer();
    // SET hp = 75 (path [1] = hp field, value = I32 75)
    const valBuf = new ArrayBuffer(4);
    new DataView(valBuf).setInt32(0, 75, true);
    const operand = buildValueBytes(TypeId.I32, new Uint8Array(valBuf));
    const data = buildStream([inst(Op.SET, [1], operand)]);
    executor.execute(player, data);
    console.assert(player.hp === 75, `hp should be 75, got ${player.hp}`);
  });

  test('set string field on SpoiCharacter via accessor', () => {
    const char = new SpoiCharacter();
    const strBytes = new TextEncoder().encode('Wizard');
    const operand = buildValueBytes(TypeId.STRING, strBytes);
    const data = buildStream([inst(Op.SET, [0], operand)]);
    executor.execute(char, data);
    console.assert(char.name === 'Wizard', `name should be "Wizard", got "${char.name}"`);
  });

  test('navigate and set nested field via accessor', () => {
    const world = new SpoiWorld();
    const char = new SpoiCharacter();
    char.name = 'Hero';
    char.hp = 100;
    world.characters = [char];

    // SET world.characters[0].hp = 50 (path [2, 0, 1])
    const valBuf = new ArrayBuffer(4);
    new DataView(valBuf).setInt32(0, 50, true);
    const operand = buildValueBytes(TypeId.I32, new Uint8Array(valBuf));
    const data = buildStream([inst(Op.SET, [2, 0, 1], operand)]);
    executor.execute(world, data);
    console.assert(world.characters[0].hp === 50, `nested hp should be 50, got ${world.characters[0].hp}`);
  });

  test('pipe array of SpoiItem and select name', () => {
    const items = [new SpoiItem(), new SpoiItem()];
    items[0].name = 'Sword';
    items[0].value = 100;
    items[1].name = 'Shield';
    items[1].value = 200;

    const data = buildStream([
      inst(Op.PIPE, [], new Uint8Array()),
      inst(Op.SELECT, [0], new Uint8Array()),
      inst(Op.EXEC, [], new Uint8Array()),
    ]);
    const result = executor.execute(items, data);
    console.assert(result.resultType === ResultType.VECTOR, `Expected VECTOR, got ${result.resultType}`);
    console.assert(result.value.length === 2, `Expected 2 items, got ${result.value.length}`);
    console.assert(result.value[0] === 'Sword', `Expected "Sword", got "${result.value[0]}"`);
    console.assert(result.value[1] === 'Shield', `Expected "Shield", got "${result.value[1]}"`);
  });

  test('set field on SpoiInventory via accessor', () => {
    const inv = new SpoiInventory();
    const valBuf = new ArrayBuffer(4);
    new DataView(valBuf).setInt32(0, 500, true);
    const operand = buildValueBytes(TypeId.I32, new Uint8Array(valBuf));
    // gold = field index 2
    const data = buildStream([inst(Op.SET, [2], operand)]);
    executor.execute(inv, data);
    console.assert(inv.gold === 500, `gold should be 500, got ${inv.gold}`);
  });

  test('set null on SpoiCharacter via accessor', () => {
    const char = new SpoiCharacter();
    char.weapon = 'Excalibur';
    // weapon = field index 3
    const data = buildStream([inst(Op.SETNULL, [3], new Uint8Array())]);
    executor.execute(char, data);
    console.assert(char.weapon === null, `weapon should be null, got ${char.weapon}`);
  });

  test('reset field on SpoiTestState via accessor', () => {
    const state = new SpoiTestState();
    state.currentMap = 'Dungeon';
    // currentMap = field index 1
    const data = buildStream([inst(Op.RESET, [1], new Uint8Array())]);
    executor.execute(state, data);
    console.assert(state.currentMap === null, `currentMap should be null, got "${state.currentMap}"`);
  });
});

// =============================== 结果 ===============================

console.log(`\n${'='.repeat(50)}`);
console.log(`\u7ed3\u679c: ${passed} \u901a\u8fc7, ${failed} \u5931\u8d25`);
console.log(`${'='.repeat(50)}`);

if (failed > 0) process.exit(1);