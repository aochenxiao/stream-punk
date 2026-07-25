/**
 * Stream-Punk JavaScript 序列化/反序列化测试套件
 * 运行: node test_stream_punk.js
 */
const { I, O, SpRef, SpArray, SpVariant } = require('./stream-punk.js');

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

function assertEqual(actual, expected, msg) {
    if (actual !== expected) {
        throw new Error(msg || `expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
    }
}

function assertDeepEqual(actual, expected, msg) {
    if (JSON.stringify(actual) !== JSON.stringify(expected)) {
        throw new Error(msg || `expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
    }
}

function assertTrue(v, msg) {
    if (!v) throw new Error(msg || `expected true, got ${v}`);
}

function assertFalse(v, msg) {
    if (v) throw new Error(msg || `expected false, got ${v}`);
}

function assertNull(v, msg) {
    if (v !== null) throw new Error(msg || `expected null, got ${v}`);
}

function assertThrows(fn, msg) {
    try {
        fn();
        throw new Error(msg || 'expected error but none thrown');
    } catch (e) {
        if (e.message === (msg || 'expected error but none thrown')) throw e;
        // expected
    }
}

// ======================== 基本类型测试 ========================

console.log('\n基本类型');
test('u8 roundtrip', () => {
    for (const v of [0, 1, 127, 128, 255]) {
        const o = new O(); o.write_u8(v);
        const i = new I(o.to_array_buffer());
        assertEqual(i.read_u8(), v, `u8: ${v}`);
    }
});

test('u16 roundtrip', () => {
    for (const v of [0, 1, 256, 1000, 0xFFFF]) {
        const o = new O(); o.write_u16(v);
        const i = new I(o.to_array_buffer());
        assertEqual(i.read_u16(), v, `u16: ${v}`);
    }
});

test('u32 roundtrip', () => {
    for (const v of [0, 1, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF]) {
        const o = new O(); o.write_u32(v);
        const i = new I(o.to_array_buffer());
        assertEqual(i.read_u32(), v, `u32: ${v}`);
    }
});

test('u64 roundtrip', () => {
    for (const v of [0n, 1n, 0x100000000n, 0x7FFFFFFFFFFFFFFFn, 0xFFFFFFFFFFFFFFFFn]) {
        const o = new O(); o.write_u64(v);
        const i = new I(o.to_array_buffer());
        assertEqual(i.read_u64(), v, `u64: ${v}`);
    }
});

test('i8 roundtrip', () => {
    for (const v of [-128, -1, 0, 1, 127]) {
        const o = new O(); o.write_i8(v);
        const i = new I(o.to_array_buffer());
        assertEqual(i.read_i8(), v, `i8: ${v}`);
    }
});

test('i16 roundtrip', () => {
    for (const v of [-32768, -1, 0, 1, 32767]) {
        const o = new O(); o.write_i16(v);
        const i = new I(o.to_array_buffer());
        assertEqual(i.read_i16(), v, `i16: ${v}`);
    }
});

test('i32 roundtrip', () => {
    for (const v of [-2147483648, -1, 0, 1, 2147483647]) {
        const o = new O(); o.write_i32(v);
        const i = new I(o.to_array_buffer());
        assertEqual(i.read_i32(), v, `i32: ${v}`);
    }
});

test('i64 roundtrip', () => {
    for (const v of [-9223372036854775808n, -1n, 0n, 1n, 9223372036854775807n]) {
        const o = new O(); o.write_i64(v);
        const i = new I(o.to_array_buffer());
        assertEqual(i.read_i64(), v, `i64: ${v}`);
    }
});

test('f32 roundtrip', () => {
    for (const v of [0.0, -1.0, 3.14]) {
        const o = new O(); o.write_f32(v);
        const i = new I(o.to_array_buffer());
        const result = i.read_f32();
        assertTrue(Math.abs(result - v) < 0.0001, `f32: expected ${v}, got ${result}`);
    }
    // Infinity
    const o2 = new O(); o2.write_f32(Infinity);
    const i2 = new I(o2.to_array_buffer());
    assertTrue(i2.read_f32() === Infinity);
    // -Infinity
    const o3 = new O(); o3.write_f32(-Infinity);
    const i3 = new I(o3.to_array_buffer());
    assertTrue(i3.read_f32() === -Infinity);
});

test('f32 NaN', () => {
    const o = new O(); o.write_f32(NaN);
    const i = new I(o.to_array_buffer());
    assertTrue(isNaN(i.read_f32()));
});

test('f64 roundtrip', () => {
    for (const v of [0.0, -1.0, 3.141592653589793]) {
        const o = new O(); o.write_f64(v);
        const i = new I(o.to_array_buffer());
        const result = i.read_f64();
        assertTrue(Math.abs(result - v) < 0.0000000001, `f64: expected ${v}, got ${result}`);
    }
    const o2 = new O(); o2.write_f64(Infinity);
    assertTrue(new I(o2.to_array_buffer()).read_f64() === Infinity);
    const o3 = new O(); o3.write_f64(-Infinity);
    assertTrue(new I(o3.to_array_buffer()).read_f64() === -Infinity);
});

test('f64 NaN', () => {
    const o = new O(); o.write_f64(NaN);
    const i = new I(o.to_array_buffer());
    assertTrue(isNaN(i.read_f64()));
});

test('bl true', () => {
    const o = new O(); o.write_bl(true);
    const i = new I(o.to_array_buffer());
    assertTrue(i.read_bl());
});

test('bl false', () => {
    const o = new O(); o.write_bl(false);
    const i = new I(o.to_array_buffer());
    assertFalse(i.read_bl());
});

test('ch roundtrip', () => {
    for (const v of ['A', 'z', '0', '\n']) {
        const o = new O(); o.write_ch8(v);
        const i = new I(o.to_array_buffer());
        assertEqual(i.read_ch8(), v);
    }
});

test('ch16 roundtrip', () => {
    for (const v of ['A', '\u4e2d']) {
        const o = new O(); o.write_ch16(v);
        const i = new I(o.to_array_buffer());
        assertEqual(i.read_ch16(), v);
    }
});

// ======================== 字符串测试 ========================

console.log('\n字符串');
test('write read empty', () => {
    const o = new O(); o.write_string("");
    const i = new I(o.to_array_buffer());
    assertEqual(i.read_string(), "");
});

test('write read ascii', () => {
    const o = new O(); o.write_string("Hello World");
    const i = new I(o.to_array_buffer());
    assertEqual(i.read_string(), "Hello World");
});

test('write read unicode', () => {
    const s = "你好世界 🌍 — 测试";
    const o = new O(); o.write_string(s);
    const i = new I(o.to_array_buffer());
    assertEqual(i.read_string(), s);
});

test('write read long', () => {
    const s = "A".repeat(10000);
    const o = new O(); o.write_string(s);
    const i = new I(o.to_array_buffer());
    assertEqual(i.read_string(), s);
});

test('u8string', () => {
    const o = new O(); o.write_u8string("Hello UTF-8");
    const i = new I(o.to_array_buffer());
    assertEqual(i.read_u8string(), "Hello UTF-8");
});

test('u16string', () => {
    const s = "你好世界";
    const o = new O(); o.write_u16string(s);
    const i = new I(o.to_array_buffer());
    assertEqual(i.read_u16string(), s);
});

test('u16string empty', () => {
    const o = new O(); o.write_u16string("");
    const i = new I(o.to_array_buffer());
    assertEqual(i.read_u16string(), "");
});

test('u32string roundtrip', () => {
    const data = new Uint8Array([1, 0, 0, 0, 2, 0, 0, 0]);
    const o = new O(); o.write_u32string(data);
    const i = new I(o.to_array_buffer());
    const result = i.read_u32string();
    assertEqual(result.length, data.length);
    for (let j = 0; j < data.length; j++) {
        assertEqual(result[j], data[j]);
    }
});

test('std_string', () => {
    const o = new O(); o.write_string("std::string");
    const i = new I(o.to_array_buffer());
    assertEqual(i.read_std_string(), "std::string");
});

// ======================== 容器测试 ========================

console.log('\n容器');
test('array empty', () => {
    const o = new O(); o.write_Array([], v => o.write_u32(v));
    const i = new I(o.to_array_buffer());
    assertDeepEqual(i.read_Array(() => i.read_u32()), []);
});

test('array u32', () => {
    const data = [1, 2, 3, 4, 5];
    const o = new O(); o.write_Array(data, v => o.write_u32(v));
    const i = new I(o.to_array_buffer());
    assertDeepEqual(i.read_Array(() => i.read_u32()), data);
});

test('vector', () => {
    const data = [10, 20, 30];
    const o = new O(); o.write_vector(data, v => o.write_u32(v));
    const i = new I(o.to_array_buffer());
    assertDeepEqual(i.read_vector(() => i.read_u32()), data);
});

test('array strings', () => {
    const data = ["Alice", "Bob", "Carol"];
    const o = new O(); o.write_Array(data, v => o.write_string(v));
    const i = new I(o.to_array_buffer());
    assertDeepEqual(i.read_Array(() => i.read_string()), data);
});

test('set', () => {
    const data = new Set([1, 2, 3]);
    const o = new O(); o.write_set(data, v => o.write_u32(v));
    const i = new I(o.to_array_buffer());
    const result = i.read_set(() => i.read_u32());
    assertEqual(result.size, 3);
    assertTrue(result.has(1) && result.has(2) && result.has(3));
});

test('set empty', () => {
    const o = new O(); o.write_set(new Set(), v => o.write_u32(v));
    const i = new I(o.to_array_buffer());
    assertEqual(i.read_set(() => i.read_u32()).size, 0);
});

test('map', () => {
    const data = new Map([["key1", 100], ["key2", 200]]);
    const o = new O(); o.write_map(data, k => o.write_string(k), v => o.write_u32(v));
    const i = new I(o.to_array_buffer());
    const result = i.read_map(() => i.read_string(), () => i.read_u32());
    assertEqual(result.get("key1"), 100);
    assertEqual(result.get("key2"), 200);
});

test('map empty', () => {
    const o = new O(); o.write_map(new Map(), k => o.write_string(k), v => o.write_u32(v));
    const i = new I(o.to_array_buffer());
    assertEqual(i.read_map(() => i.read_string(), () => i.read_u32()).size, 0);
});

test('nested array', () => {
    const data = [[1, 2], [3, 4, 5], []];
    const o = new O();
    o.write_Array(data, inner => o.write_Array(inner, v => o.write_u32(v)));
    const i = new I(o.to_array_buffer());
    const result = i.read_Array(() => i.read_Array(() => i.read_u32()));
    assertDeepEqual(result, data);
});

// ======================== Optional 测试 ========================

console.log('\nOptional');
test('optional present', () => {
    const o = new O(); o.write_optional(42, v => o.write_u32(v));
    const i = new I(o.to_array_buffer());
    assertEqual(i.read_optional(() => i.read_u32()), 42);
});

test('optional null', () => {
    const o = new O(); o.write_optional(null, v => o.write_u32(v));
    const i = new I(o.to_array_buffer());
    assertNull(i.read_optional(() => i.read_u32()));
});

test('optional zero', () => {
    const o = new O(); o.write_optional(0, v => o.write_u32(v));
    const i = new I(o.to_array_buffer());
    assertEqual(i.read_optional(() => i.read_u32()), 0);
});

// ======================== Bitset 测试 ========================

console.log('\nBitset');
test('bitset empty', () => {
    const o = new O(); o.write_bitset([]);
    const i = new I(o.to_array_buffer());
    assertDeepEqual(i.read_bitset(), []);
});

test('bitset all false', () => {
    const bits = [false, false, false];
    const o = new O(); o.write_bitset(bits);
    const i = new I(o.to_array_buffer());
    assertDeepEqual(i.read_bitset(), bits);
});

test('bitset all true', () => {
    const bits = [true, true, true];
    const o = new O(); o.write_bitset(bits);
    const i = new I(o.to_array_buffer());
    assertDeepEqual(i.read_bitset(), bits);
});

test('bitset mixed', () => {
    const bits = [true, false, true, false, false, true, false, true, true];
    const o = new O(); o.write_bitset(bits);
    const i = new I(o.to_array_buffer());
    assertDeepEqual(i.read_bitset(), bits);
});

test('bitset large', () => {
    const bits = Array.from({ length: 100 }, (_, i) => i % 3 === 0);
    const o = new O(); o.write_bitset(bits);
    const i = new I(o.to_array_buffer());
    assertDeepEqual(i.read_bitset(), bits);
});

// ======================== 多字段往返测试 ========================

console.log('\n多字段往返');
test('mixed types', () => {
    const o = new O();
    o.write_u8(42);
    o.write_i32(-100);
    o.write_f64(3.14);
    o.write_bl(true);
    o.write_string("hello");
    o.write_Array([1, 2, 3], v => o.write_u32(v));

    const i = new I(o.to_array_buffer());
    assertEqual(i.read_u8(), 42);
    assertEqual(i.read_i32(), -100);
    assertTrue(Math.abs(i.read_f64() - 3.14) < 0.0000000001);
    assertTrue(i.read_bl());
    assertEqual(i.read_string(), "hello");
    assertDeepEqual(i.read_Array(() => i.read_u32()), [1, 2, 3]);
});

test('offset reader', () => {
    const o = new O();
    o.write_u32(0); // padding
    o.write_u32(42);
    o.write_u32(0); // padding

    const data = o.to_array_buffer();
    const i = new I(data, 4);
    assertEqual(i.read_u32(), 42);
    assertEqual(i.offset, 8);
});

test('has more data', () => {
    const o = new O(); o.write_u32(42);
    const i = new I(o.to_array_buffer());
    assertTrue(i.hasMoreData());
    i.read_u32();
    assertFalse(i.hasMoreData());
});

// ======================== 辅助类型测试 ========================

console.log('\n辅助类型');
test('SpArray create and access', () => {
    const arr = new SpArray(3, "init");
    assertEqual(arr.size, 3);
    assertEqual(arr.at(0), "init");
    assertEqual(arr.at(1), "init");
    assertEqual(arr.at(2), "init");
});

test('SpArray set and get', () => {
    const arr = new SpArray(3, 0);
    arr.set(0, 10);
    arr.set(1, 20);
    assertEqual(arr.at(0), 10);
    assertEqual(arr.at(1), 20);
});

test('SpArray out of bounds', () => {
    const arr = new SpArray(2, 0);
    assertThrows(() => arr.at(2));
    assertThrows(() => arr.set(2, 42));
});

test('SpRef create', () => {
    const ref = new SpRef("hello", 0x1000n);
    assertEqual(ref.value, "hello");
    assertEqual(ref.address, 0x1000n);
});

test('SpRef null', () => {
    const ref = new SpRef(null, 0n);
    assertNull(ref.value);
    assertEqual(ref.address, 0n);
});

test('SpVariant create', () => {
    const v = new SpVariant(42);
    assertEqual(v.value, 42);
});

test('SpVariant set', () => {
    const v = new SpVariant();
    v.set("hello");
    assertEqual(v.value, "hello");
});

// ======================== 边界条件测试 ========================

console.log('\n边界条件');
test('zero length array', () => {
    const o = new O(); o.write_Array([], v => o.write_u8(v));
    const i = new I(o.to_array_buffer());
    assertDeepEqual(i.read_Array(() => i.read_u8()), []);
});

test('empty string vs null', () => {
    const o1 = new O(); o1.write_optional("", v => o1.write_string(v));
    const i1 = new I(o1.to_array_buffer());
    assertEqual(i1.read_optional(() => i1.read_string()), "");

    const o2 = new O(); o2.write_optional(null, v => o2.write_string(v));
    const i2 = new I(o2.to_array_buffer());
    assertNull(i2.read_optional(() => i2.read_string()));
});

test('max u32 array', () => {
    const data = [0xFFFFFFFF, 0, 0xFFFFFFFF];
    const o = new O(); o.write_Array(data, v => o.write_u32(v));
    const i = new I(o.to_array_buffer());
    assertDeepEqual(i.read_Array(() => i.read_u32()), data);
});

test('negative in array', () => {
    const data = [-1, 0, 1, -100, 100];
    const o = new O(); o.write_Array(data, v => o.write_i32(v));
    const i = new I(o.to_array_buffer());
    assertDeepEqual(i.read_Array(() => i.read_i32()), data);
});

// ======================== 结果 ========================

console.log(`\n${'='.repeat(50)}`);
console.log(`结果: ${passed} 通过, ${failed} 失败`);
console.log('='.repeat(50));

if (failed > 0) process.exit(1);