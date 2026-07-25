"use strict";
/**
 * SPOI Executor TypeScript 测试套件
 *
 * 测试覆盖：Varint、指令解析、导航、写操作、读操作、聚合、管道、错误处理
 * 零外部依赖，使用 node 内置 assert 模块
 *
 * 运行：npx ts-node test_spoi_executor.ts
 * 或：tsc test_spoi_executor.ts --target ES2020 && node test_spoi_executor.js
 */
Object.defineProperty(exports, "__esModule", { value: true });
const spoi_executor_1 = require("./spoi_executor");
const assert = require('assert');
// =============================== 测试数据类 ===============================
class Item {
    constructor(init) {
        this.name = '';
        this.price = 0;
        Object.assign(this, init);
    }
}
class Player {
    constructor(init) {
        this.name = '';
        this.level = 0;
        this.health = 0;
        this.items = [];
        Object.assign(this, init);
    }
}
// 类型注册表
const TYPE_REGISTRY = {
    "Player": ["name", "level", "health", "items"],
    "Item": ["name", "price"],
};
// =============================== 辅助函数 ===============================
function writeVarint(buf, v) {
    while (v >= 0x80) {
        buf.push((v & 0x7F) | 0x80);
        v >>>= 7;
    }
    buf.push(v & 0x7F);
}
function buildStream(instructions) {
    const buf = [];
    writeVarint(buf, instructions.length);
    for (const inst of instructions) {
        buf.push(inst.op);
        writeVarint(buf, inst.path.length);
        for (const seg of inst.path)
            writeVarint(buf, seg);
        writeVarint(buf, inst.operand.length);
        for (const b of inst.operand)
            buf.push(b);
    }
    return new Uint8Array(buf);
}
function i32(n) {
    const buf = new ArrayBuffer(4);
    new DataView(buf).setInt32(0, n, true);
    return new Uint8Array(buf);
}
function u32(n) {
    const buf = new ArrayBuffer(4);
    new DataView(buf).setUint32(0, n, true);
    return new Uint8Array(buf);
}
function str(s) {
    return new TextEncoder().encode(s);
}
function inst(op, path, operand) {
    return { op, path, operand };
}
function setInt(path, value) {
    return inst(4 /* Op.SET */, path, u32(value));
}
function setStr(path, value) {
    return inst(4 /* Op.SET */, path, str(value));
}
function pipe(path) {
    return inst(34 /* Op.PIPE */, path || [], new Uint8Array());
}
function filterGt(memberIdx, value) {
    const op = new Uint8Array(5 + 4);
    op.set(u32(memberIdx), 0);
    op[4] = 3; // e_gt
    op.set(u32(value), 5);
    return inst(12 /* Op.FILTER */, [], op);
}
function select(path) {
    return inst(13 /* Op.SELECT */, path, new Uint8Array());
}
function sort(path) {
    return inst(14 /* Op.SORT */, path || [], new Uint8Array());
}
function take(n) {
    return inst(16 /* Op.TAKE */, [], u32(n));
}
function drop(n) {
    return inst(17 /* Op.DROP */, [], u32(n));
}
function reverse() {
    return inst(15 /* Op.REVERSE */, [], new Uint8Array());
}
function distinct() {
    return inst(20 /* Op.DISTINCT */, [], new Uint8Array());
}
function count() {
    return inst(21 /* Op.COUNT */, [], new Uint8Array());
}
function exec() {
    return inst(33 /* Op.EXEC */, [], new Uint8Array());
}
// =============================== 测试用例 ===============================
let passed = 0;
let failed = 0;
function test(name, fn) {
    try {
        fn();
        passed++;
        console.log(`  ✓ ${name}`);
    }
    catch (e) {
        failed++;
        console.log(`  ✗ ${name}`);
        console.log(`    ${e.message}`);
    }
}
function suite(name, fn) {
    console.log(`\n${name}`);
    fn();
}
// =============================== Varint ===============================
suite('Varint 编解码', () => {
    // 测试内联 readVarint
    function testVarint(v) {
        const buf = [];
        writeVarint(buf, v);
        // 直接用 JS 的 readVarint
        let result = 0, shift = 0, offset = 0;
        const data = new Uint8Array(buf);
        while (offset < data.length) {
            const b = data[offset++];
            result |= (b & 0x7F) << shift;
            if (!(b & 0x80))
                break;
            shift += 7;
        }
        assert.strictEqual(result >>> 0, v >>> 0);
    }
    test('small values', () => {
        testVarint(0);
        testVarint(1);
        testVarint(127);
        testVarint(128);
        testVarint(256);
        testVarint(1000);
        testVarint(0xFFFFFFFF);
    });
});
// =============================== 指令解析 ===============================
suite('指令解析', () => {
    const executor = new spoi_executor_1.SpoiExecutor(TYPE_REGISTRY);
    test('parse single instruction', () => {
        const data = buildStream([setInt([1], 42)]); // SET level = 42
        const result = executor.execute(new Player({ name: "test", level: 0 }), data);
    });
    test('parse multiple instructions', () => {
        const players = [
            new Player({ name: "A", level: 10 }),
            new Player({ name: "B", level: 20 }),
        ];
        const data = buildStream([pipe(), take(1), exec()]);
        const result = executor.execute(players, data);
        assert.strictEqual(result.resultType, 1 /* ResultType.SINGLE */);
    });
});
// =============================== 导航 ===============================
suite('路径导航', () => {
    const executor = new spoi_executor_1.SpoiExecutor(TYPE_REGISTRY);
    test('member access', () => {
        const player = new Player({ name: "Alice", level: 10, health: 100 });
        const result = executor.navigate(player, [0]);
        assert.strictEqual(result, "Alice");
        const result2 = executor.navigate(player, [1]);
        assert.strictEqual(result2, 10);
        const result3 = executor.navigate(player, [2]);
        assert.strictEqual(result3, 100);
    });
    test('array index', () => {
        const players = [
            new Player({ name: "Alice", level: 10 }),
            new Player({ name: "Bob", level: 20 }),
        ];
        const result = executor.navigate(players, [0]);
        assert.strictEqual(result.name, "Alice");
        const result2 = executor.navigate(players, [1]);
        assert.strictEqual(result2.name, "Bob");
    });
    test('nested navigation', () => {
        const players = [new Player({ name: "Alice", level: 10 })];
        const result = executor.navigate(players, [0, 0]);
        assert.strictEqual(result, "Alice");
    });
    test('deref pointer', () => {
        const ref = { value: "hello" };
        const result = executor.navStep(ref, spoi_executor_1.PATH_DEREF);
        assert.strictEqual(result, "hello");
    });
});
// =============================== 写操作 ===============================
suite('写操作', () => {
    const executor = new spoi_executor_1.SpoiExecutor(TYPE_REGISTRY);
    test('set field', () => {
        const player = new Player({ name: "Alice", level: 10, health: 100 });
        const data = buildStream([setInt([1], 20)]);
        executor.execute(player, data);
        assert.strictEqual(player.level, 20);
    });
    test('set string', () => {
        const player = new Player({ name: "Alice", level: 10, health: 100 });
        const data = buildStream([setStr([0], "Bob")]);
        executor.execute(player, data);
        assert.strictEqual(player.name, "Bob");
    });
    test('add', () => {
        const player = new Player({ name: "Alice", level: 10, health: 100 });
        const data = buildStream([inst(5 /* Op.ADD */, [1], u32(5))]);
        executor.execute(player, data);
        assert.strictEqual(player.level, 15);
    });
    test('append', () => {
        const player = new Player({ name: "Alice", level: 10, health: 100, items: [] });
        const data = buildStream([inst(6 /* Op.APPEND */, [3], str("hello"))]);
        executor.execute(player, data);
        assert.strictEqual(player.items.length, 1);
        assert.strictEqual(player.items[0], "hello");
    });
    test('remove', () => {
        const player = new Player({ name: "Alice", level: 10, health: 100, items: ["a", "b", "c"] });
        const data = buildStream([inst(7 /* Op.REMOVE */, [3], u32(1))]);
        executor.execute(player, data);
        assert.deepStrictEqual(player.items, ["a", "c"]);
    });
    test('insert', () => {
        const player = new Player({ name: "Alice", level: 10, health: 100, items: ["a", "c"] });
        const operand = new Uint8Array(4 + 5);
        operand.set(u32(1), 0);
        operand.set(str("hello"), 4);
        const data = buildStream([inst(8 /* Op.INSERT */, [3], operand)]);
        executor.execute(player, data);
        assert.deepStrictEqual(player.items, ["a", "hello", "c"]);
    });
    test('replace', () => {
        const player = new Player({ name: "Alice", level: 10, health: 100, items: ["a", "b", "c"] });
        const operand = new Uint8Array(4 + 5);
        operand.set(u32(1), 0);
        operand.set(str("world"), 4);
        const data = buildStream([inst(9 /* Op.REPLACE */, [3], operand)]);
        executor.execute(player, data);
        assert.deepStrictEqual(player.items, ["a", "world", "c"]);
    });
    test('reset', () => {
        const player = new Player({ name: "Alice", level: 10, health: 100 });
        const data = buildStream([inst(10 /* Op.RESET */, [0], new Uint8Array())]);
        executor.execute(player, data);
        assert.strictEqual(player.name, null);
    });
    test('set null', () => {
        const player = new Player({ name: "Alice", level: 10, health: 100 });
        const data = buildStream([inst(11 /* Op.SETNULL */, [0], new Uint8Array())]);
        executor.execute(player, data);
        assert.strictEqual(player.name, null);
    });
});
// =============================== 读操作 ===============================
suite('读操作', () => {
    const executor = new spoi_executor_1.SpoiExecutor(TYPE_REGISTRY);
    const players = [
        new Player({ name: "Alice", level: 10, health: 100 }),
        new Player({ name: "Bob", level: 20, health: 80 }),
        new Player({ name: "Carol", level: 15, health: 120 }),
        new Player({ name: "Dave", level: 20, health: 60 }),
        new Player({ name: "Eve", level: 10, health: 90 }),
    ];
    test('pipe', () => {
        const data = buildStream([pipe(), exec()]);
        const result = executor.execute(players, data);
        assert.strictEqual(result.resultType, 2 /* ResultType.VECTOR */);
        assert.strictEqual(result.value.length, 5);
    });
    test('pipe single', () => {
        const data = buildStream([pipe(), exec()]);
        const result = executor.execute(players[0], data);
        assert.strictEqual(result.resultType, 1 /* ResultType.SINGLE */);
    });
    test('filter gt', () => {
        const data = buildStream([pipe(), filterGt(1, 15), exec()]);
        const result = executor.execute(players, data);
        const values = result.value;
        assert.strictEqual(values.length, 2); // Bob(20), Dave(20)
    });
    test('select', () => {
        const data = buildStream([pipe(), select([0]), exec()]);
        const result = executor.execute(players, data);
        assert.deepStrictEqual(result.value, ["Alice", "Bob", "Carol", "Dave", "Eve"]);
    });
    test('take', () => {
        const data = buildStream([pipe(), take(3), exec()]);
        const result = executor.execute(players, data);
        assert.strictEqual(result.value.length, 3);
    });
    test('drop', () => {
        const data = buildStream([pipe(), drop(2), exec()]);
        const result = executor.execute(players, data);
        assert.strictEqual(result.value.length, 3);
    });
    test('reverse', () => {
        const data = buildStream([pipe(), reverse(), exec()]);
        const result = executor.execute(players, data);
        const values = result.value;
        assert.strictEqual(values[0].name, "Eve");
        assert.strictEqual(values[values.length - 1].name, "Alice");
    });
    test('distinct', () => {
        const data = buildStream([pipe(), select([1]), distinct(), exec()]);
        const result = executor.execute(players, data);
        const values = result.value;
        values.sort((a, b) => a - b);
        assert.deepStrictEqual(values, [10, 15, 20]);
    });
});
// =============================== 聚合 ===============================
suite('聚合', () => {
    const executor = new spoi_executor_1.SpoiExecutor(TYPE_REGISTRY);
    const players = [
        new Player({ name: "Alice", level: 10, health: 100 }),
        new Player({ name: "Bob", level: 20, health: 80 }),
        new Player({ name: "Carol", level: 15, health: 120 }),
    ];
    test('count', () => {
        const data = buildStream([pipe(), count(), exec()]);
        const result = executor.execute(players, data);
        assert.strictEqual(result.value, 3);
    });
    test('any true', () => {
        const op = new Uint8Array(5 + 4);
        op.set(u32(1), 0);
        op[4] = 3;
        op.set(u32(15), 5); // level > 15
        const data = buildStream([pipe(), inst(22 /* Op.ANY */, [], op), exec()]);
        const result = executor.execute(players, data);
        assert.strictEqual(result.value, true);
    });
    test('any false', () => {
        const op = new Uint8Array(5 + 4);
        op.set(u32(1), 0);
        op[4] = 3;
        op.set(u32(100), 5); // level > 100
        const data = buildStream([pipe(), inst(22 /* Op.ANY */, [], op), exec()]);
        const result = executor.execute(players, data);
        assert.strictEqual(result.value, false);
    });
    test('all true', () => {
        const op = new Uint8Array(5 + 4);
        op.set(u32(1), 0);
        op[4] = 5;
        op.set(u32(10), 5); // level >= 10
        const data = buildStream([pipe(), inst(23 /* Op.ALL */, [], op), exec()]);
        const result = executor.execute(players, data);
        assert.strictEqual(result.value, true);
    });
    test('all false', () => {
        const op = new Uint8Array(5 + 4);
        op.set(u32(1), 0);
        op[4] = 3;
        op.set(u32(15), 5); // level > 15
        const data = buildStream([pipe(), inst(23 /* Op.ALL */, [], op), exec()]);
        const result = executor.execute(players, data);
        assert.strictEqual(result.value, false);
    });
    test('find', () => {
        const op = new Uint8Array(5 + 4);
        op.set(u32(1), 0);
        op[4] = 0;
        op.set(u32(20), 5); // level == 20
        const data = buildStream([pipe(), inst(24 /* Op.FIND */, [], op), exec()]);
        const result = executor.execute(players, data);
        assert.strictEqual(result.value.name, "Bob");
    });
    test('find not found', () => {
        const op = new Uint8Array(5 + 4);
        op.set(u32(1), 0);
        op[4] = 0;
        op.set(u32(999), 5); // level == 999
        const data = buildStream([pipe(), inst(24 /* Op.FIND */, [], op), exec()]);
        const result = executor.execute(players, data);
        assert.strictEqual(result.resultType, 0 /* ResultType.UNDEF */);
    });
});
// =============================== 完整管道 ===============================
suite('完整管道', () => {
    const executor = new spoi_executor_1.SpoiExecutor(TYPE_REGISTRY);
    const players = [
        new Player({ name: "Alice", level: 10, health: 100 }),
        new Player({ name: "Bob", level: 20, health: 80 }),
        new Player({ name: "Carol", level: 15, health: 120 }),
        new Player({ name: "Dave", level: 20, health: 60 }),
        new Player({ name: "Eve", level: 10, health: 90 }),
        new Player({ name: "Frank", level: 25, health: 70 }),
        new Player({ name: "Grace", level: 30, health: 50 }),
    ];
    test('filter → take → sort', () => {
        const data = buildStream([pipe(), filterGt(1, 10), take(3), sort([0]), exec()]);
        const result = executor.execute(players, data);
        const values = result.value;
        assert.strictEqual(values.length, 3);
        const names = values.map(p => p.name);
        assert.deepStrictEqual(names, ["Bob", "Carol", "Dave"]);
    });
    test('filter → select → count', () => {
        const data = buildStream([pipe(), filterGt(1, 15), select([0]), count(), exec()]);
        const result = executor.execute(players, data);
        assert.strictEqual(result.value, 4); // Bob, Dave, Frank, Grace
    });
    test('write then read', () => {
        const player = new Player({ name: "Alice", level: 10, health: 100, items: [] });
        const data = buildStream([
            setInt([1], 99),
            setStr([0], "Zelda"),
            pipe([]),
            exec(),
        ]);
        const result = executor.execute(player, data);
        assert.strictEqual(player.level, 99);
        assert.strictEqual(player.name, "Zelda");
        assert.strictEqual(result.value.name, "Zelda");
    });
});
// =============================== 错误处理 ===============================
suite('错误处理', () => {
    const executor = new spoi_executor_1.SpoiExecutor(TYPE_REGISTRY);
    test('unknown opcode', () => {
        const data = buildStream([inst(0xFF, [], new Uint8Array())]);
        assert.throws(() => executor.execute(new Player(), data), /Unknown SPOI opcode/);
    });
    test('invalid path', () => {
        const player = new Player({ name: "Alice", level: 10, health: 100 });
        const data = buildStream([setInt([0, 99], 42)]);
        assert.throws(() => executor.execute(player, data));
    });
});
// =============================== 结果 ===============================
console.log(`\n${'='.repeat(50)}`);
console.log(`结果: ${passed} 通过, ${failed} 失败`);
console.log(`${'='.repeat(50)}`);
if (failed > 0)
    process.exit(1);
