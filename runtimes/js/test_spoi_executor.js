/**
 * SPOI Executor JavaScript 测试套件
 *
 * 运行: node test_spoi_executor.js
 */

const { SpoiExecutor, Op, ResultType, PATH_DEREF } = require('./spoi_executor.js');
const { TypeId } = require('./spoi_js_accessor.js');
const assert = require('assert');

// =============================== 测试数据类 ===============================

class Player {
  constructor(init) {
    this.name = '';
    this.level = 0;
    this.health = 0;
    this.items = [];
    if (init) Object.assign(this, init);
  }
}

const TYPE_REGISTRY = {
  "Player": ["name", "level", "health", "items"],
  "Item":   ["name", "price"],
};

// =============================== 辅助函数（新格式：type_id 前缀） ===============================

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

function u32(n) {
  const buf = new ArrayBuffer(4);
  new DataView(buf).setUint32(0, n, true);
  return new Uint8Array(buf);
}

function i32(n) {
  const buf = new ArrayBuffer(4);
  new DataView(buf).setInt32(0, n, true);
  return new Uint8Array(buf);
}

function str(s) {
  return new TextEncoder().encode(s);
}

function inst(op, path, operand) {
  return { op, path, operand };
}

function buildOperandInt(value) {
  const result = new Uint8Array(4 + 4);
  result.set(u32(TypeId.I32), 0);
  result.set(i32(value), 4);
  return result;
}

function buildOperandStr(value) {
  const encoded = str(value);
  const result = new Uint8Array(4 + encoded.length);
  result.set(u32(TypeId.STRING), 0);
  result.set(encoded, 4);
  return result;
}

function setInt(path, value) { return inst(Op.SET, path, buildOperandInt(value)); }
function setStr(path, value) { return inst(Op.SET, path, buildOperandStr(value)); }
function pipe(path) { return inst(Op.PIPE, path || [], new Uint8Array()); }
function filterGt(memberIdx, value) {
  const valBytes = buildOperandInt(value);
  const buf = [];
  const memberBytes = u32(memberIdx);
  for (const b of memberBytes) buf.push(b);
  buf.push(3); // cmpOp: gt
  writeVarint(buf, valBytes.length);
  for (const b of valBytes) buf.push(b);
  return inst(Op.FILTER, [], new Uint8Array(buf));
}
function select(path) { return inst(Op.SELECT, path, new Uint8Array()); }
function sort(path) { return inst(Op.SORT, path || [], new Uint8Array()); }
function take(n) { return inst(Op.TAKE, [], u32(n)); }
function drop(n) { return inst(Op.DROP, [], u32(n)); }
function reverse() { return inst(Op.REVERSE, [], new Uint8Array()); }
function distinct() { return inst(Op.DISTINCT, [], new Uint8Array()); }
function count() { return inst(Op.COUNT, [], new Uint8Array()); }
function exec() { return inst(Op.EXEC, [], new Uint8Array()); }

// =============================== 测试用例 ===============================

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    passed++;
    console.log(`  ✓ ${name}`);
  } catch (e) {
    failed++;
    console.log(`  ✗ ${name}`);
    console.log(`    ${e.message}`);
  }
}

function suite(name, fn) {
  console.log(`\n${name}`);
  fn();
}

// =============================== 指令解析 ===============================

suite('指令解析', () => {
  const executor = new SpoiExecutor(TYPE_REGISTRY);

  test('parse single instruction', () => {
    const data = buildStream([setInt([1], 42)]);  // SET level = 42
    executor.execute(new Player({ name: "test", level: 0 }), data);
  });

  test('parse multiple instructions', () => {
    const players = [
      new Player({ name: "A", level: 10 }),
      new Player({ name: "B", level: 20 }),
    ];
    const data = buildStream([pipe(), take(1), exec()]);
    const result = executor.execute(players, data);
    assert.strictEqual(result.resultType, ResultType.SINGLE);
  });
});

// =============================== 导航 ===============================

suite('路径导航', () => {
  const executor = new SpoiExecutor(TYPE_REGISTRY);

  test('member access', () => {
    const player = new Player({ name: "Alice", level: 10, health: 100 });
    assert.strictEqual(executor.navigate(player, [0]), "Alice");
    assert.strictEqual(executor.navigate(player, [1]), 10);
    assert.strictEqual(executor.navigate(player, [2]), 100);
  });

  test('array index', () => {
    const players = [
      new Player({ name: "Alice", level: 10 }),
      new Player({ name: "Bob", level: 20 }),
    ];
    assert.strictEqual(executor.navigate(players, [0]).name, "Alice");
    assert.strictEqual(executor.navigate(players, [1]).name, "Bob");
  });

  test('nested navigation', () => {
    const players = [new Player({ name: "Alice", level: 10 })];
    assert.strictEqual(executor.navigate(players, [0, 0]), "Alice");
  });

  test('deref pointer', () => {
    const ref = { value: "hello" };
    assert.strictEqual(executor.navStep(ref, PATH_DEREF), "hello");
  });
});

// =============================== 写操作 ===============================

suite('写操作', () => {
  const executor = new SpoiExecutor(TYPE_REGISTRY);

  test('set field', () => {
    const player = new Player({ name: "Alice", level: 10, health: 100 });
    executor.execute(player, buildStream([setInt([1], 20)]));
    assert.strictEqual(player.level, 20);
  });

  test('set string', () => {
    const player = new Player({ name: "Alice", level: 10, health: 100 });
    executor.execute(player, buildStream([setStr([0], "Bob")]));
    assert.strictEqual(player.name, "Bob");
  });

  test('add', () => {
    const player = new Player({ name: "Alice", level: 10, health: 100 });
    executor.execute(player, buildStream([inst(Op.ADD, [1], buildOperandInt(5))]));
    assert.strictEqual(player.level, 15);
  });

  test('append', () => {
    const player = new Player({ name: "Alice", level: 10, health: 100, items: [] });
    executor.execute(player, buildStream([inst(Op.APPEND, [3], buildOperandStr("hello"))]));
    assert.strictEqual(player.items.length, 1);
    assert.strictEqual(player.items[0], "hello");
  });

  test('remove', () => {
    const player = new Player({ name: "Alice", level: 10, health: 100, items: ["a", "b", "c"] });
    executor.execute(player, buildStream([inst(Op.REMOVE, [3], u32(1))]));
    assert.deepStrictEqual(player.items, ["a", "c"]);
  });

  test('insert', () => {
    const player = new Player({ name: "Alice", level: 10, health: 100, items: ["a", "c"] });
    const valBytes = buildOperandStr("hello");
    const operand = new Uint8Array(4 + valBytes.length);
    operand.set(u32(1), 0); operand.set(valBytes, 4);
    executor.execute(player, buildStream([inst(Op.INSERT, [3], operand)]));
    assert.deepStrictEqual(player.items, ["a", "hello", "c"]);
  });

  test('replace', () => {
    const player = new Player({ name: "Alice", level: 10, health: 100, items: ["a", "b", "c"] });
    const valBytes = buildOperandStr("world");
    const operand = new Uint8Array(4 + valBytes.length);
    operand.set(u32(1), 0); operand.set(valBytes, 4);
    executor.execute(player, buildStream([inst(Op.REPLACE, [3], operand)]));
    assert.deepStrictEqual(player.items, ["a", "world", "c"]);
  });

  test('reset', () => {
    const player = new Player({ name: "Alice", level: 10, health: 100 });
    executor.execute(player, buildStream([inst(Op.RESET, [0], new Uint8Array())]));
    assert.strictEqual(player.name, null);
  });

  test('set null', () => {
    const player = new Player({ name: "Alice", level: 10, health: 100 });
    executor.execute(player, buildStream([inst(Op.SETNULL, [0], new Uint8Array())]));
    assert.strictEqual(player.name, null);
  });
});

// =============================== 读操作 ===============================

suite('读操作', () => {
  const executor = new SpoiExecutor(TYPE_REGISTRY);
  const players = [
    new Player({ name: "Alice", level: 10, health: 100 }),
    new Player({ name: "Bob",   level: 20, health: 80 }),
    new Player({ name: "Carol", level: 15, health: 120 }),
    new Player({ name: "Dave",  level: 20, health: 60 }),
    new Player({ name: "Eve",   level: 10, health: 90 }),
  ];

  test('pipe', () => {
    const result = executor.execute(players, buildStream([pipe(), exec()]));
    assert.strictEqual(result.resultType, ResultType.VECTOR);
    assert.strictEqual(result.value.length, 5);
  });

  test('pipe single', () => {
    const result = executor.execute(players[0], buildStream([pipe(), exec()]));
    assert.strictEqual(result.resultType, ResultType.SINGLE);
  });

  test('filter gt', () => {
    const result = executor.execute(players, buildStream([pipe(), filterGt(1, 15), exec()]));
    assert.strictEqual(result.value.length, 2); // Bob(20), Dave(20)
  });

  test('select', () => {
    const result = executor.execute(players, buildStream([pipe(), select([0]), exec()]));
    assert.deepStrictEqual(result.value, ["Alice", "Bob", "Carol", "Dave", "Eve"]);
  });

  test('take', () => {
    const result = executor.execute(players, buildStream([pipe(), take(3), exec()]));
    assert.strictEqual(result.value.length, 3);
  });

  test('drop', () => {
    const result = executor.execute(players, buildStream([pipe(), drop(2), exec()]));
    assert.strictEqual(result.value.length, 3);
  });

  test('reverse', () => {
    const result = executor.execute(players, buildStream([pipe(), reverse(), exec()]));
    assert.strictEqual(result.value[0].name, "Eve");
    assert.strictEqual(result.value[result.value.length - 1].name, "Alice");
  });

  test('distinct', () => {
    const result = executor.execute(players, buildStream([pipe(), select([1]), distinct(), exec()]));
    result.value.sort((a, b) => a - b);
    assert.deepStrictEqual(result.value, [10, 15, 20]);
  });
});

// =============================== 聚合 ===============================

suite('聚合', () => {
  const executor = new SpoiExecutor(TYPE_REGISTRY);
  const players = [
    new Player({ name: "Alice", level: 10, health: 100 }),
    new Player({ name: "Bob",   level: 20, health: 80 }),
    new Player({ name: "Carol", level: 15, health: 120 }),
  ];

  test('count', () => {
    const result = executor.execute(players, buildStream([pipe(), count(), exec()]));
    assert.strictEqual(result.value, 3);
  });

  test('any true', () => {
    const op = new Uint8Array(5 + 1 + 8);
    op.set(u32(1), 0); op[4] = 3; op[5] = 8; op.set(buildOperandInt(15), 6);
    const result = executor.execute(players, buildStream([pipe(), inst(Op.ANY, [], op), exec()]));
    assert.strictEqual(result.value, true);
  });

  test('any false', () => {
    const op = new Uint8Array(5 + 1 + 8);
    op.set(u32(1), 0); op[4] = 3; op[5] = 8; op.set(buildOperandInt(100), 6);
    const result = executor.execute(players, buildStream([pipe(), inst(Op.ANY, [], op), exec()]));
    assert.strictEqual(result.value, false);
  });

  test('all true', () => {
    const op = new Uint8Array(5 + 1 + 8);
    op.set(u32(1), 0); op[4] = 5; op[5] = 8; op.set(buildOperandInt(10), 6);
    const result = executor.execute(players, buildStream([pipe(), inst(Op.ALL, [], op), exec()]));
    assert.strictEqual(result.value, true);
  });

  test('all false', () => {
    const op = new Uint8Array(5 + 1 + 8);
    op.set(u32(1), 0); op[4] = 3; op[5] = 8; op.set(buildOperandInt(15), 6);
    const result = executor.execute(players, buildStream([pipe(), inst(Op.ALL, [], op), exec()]));
    assert.strictEqual(result.value, false);
  });

  test('find', () => {
    const op = new Uint8Array(5 + 1 + 8);
    op.set(u32(1), 0); op[4] = 0; op[5] = 8; op.set(buildOperandInt(20), 6);
    const result = executor.execute(players, buildStream([pipe(), inst(Op.FIND, [], op), exec()]));
    assert.strictEqual(result.value.name, "Bob");
  });

  test('find not found', () => {
    const op = new Uint8Array(5 + 1 + 8);
    op.set(u32(1), 0); op[4] = 0; op[5] = 8; op.set(buildOperandInt(999), 6);
    const result = executor.execute(players, buildStream([pipe(), inst(Op.FIND, [], op), exec()]));
    assert.strictEqual(result.resultType, ResultType.UNDEF);
  });
});

// =============================== 完整管道 ===============================

suite('完整管道', () => {
  const executor = new SpoiExecutor(TYPE_REGISTRY);
  const players = [
    new Player({ name: "Alice", level: 10, health: 100 }),
    new Player({ name: "Bob",   level: 20, health: 80 }),
    new Player({ name: "Carol", level: 15, health: 120 }),
    new Player({ name: "Dave",  level: 20, health: 60 }),
    new Player({ name: "Eve",   level: 10, health: 90 }),
    new Player({ name: "Frank", level: 25, health: 70 }),
    new Player({ name: "Grace", level: 30, health: 50 }),
  ];

  test('filter → take → sort', () => {
    const result = executor.execute(players, buildStream([pipe(), filterGt(1, 10), take(3), sort([0]), exec()]));
    assert.strictEqual(result.value.length, 3);
    const names = result.value.map(p => p.name);
    assert.deepStrictEqual(names, ["Bob", "Carol", "Dave"]);
  });

  test('filter → select → count', () => {
    const result = executor.execute(players, buildStream([pipe(), filterGt(1, 15), select([0]), count(), exec()]));
    assert.strictEqual(result.value, 4);
  });

  test('write then read', () => {
    const player = new Player({ name: "Alice", level: 10, health: 100, items: [] });
    const result = executor.execute(player, buildStream([
      setInt([1], 99), setStr([0], "Zelda"), pipe([]), exec(),
    ]));
    assert.strictEqual(player.level, 99);
    assert.strictEqual(player.name, "Zelda");
    assert.strictEqual(result.value.name, "Zelda");
  });
});

// =============================== 错误处理 ===============================

suite('错误处理', () => {
  const executor = new SpoiExecutor(TYPE_REGISTRY);

  test('unknown opcode', () => {
    assert.throws(() => executor.execute(new Player(), buildStream([inst(0xFF, [], new Uint8Array())])), /Unknown SPOI opcode/);
  });

  test('invalid path', () => {
    const player = new Player({ name: "Alice", level: 10, health: 100 });
    // 导航到 name 字段(字符串)后再尝试索引 99，会失败
    const data = buildStream([setInt([0, 99], 42)]);
    assert.throws(() => executor.execute(player, data));
  });
});

// =============================== 结果 ===============================

console.log(`\n${'='.repeat(50)}`);
console.log(`结果: ${passed} 通过, ${failed} 失败`);
console.log(`${'='.repeat(50)}`);

if (failed > 0) process.exit(1);