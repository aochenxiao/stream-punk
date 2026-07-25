/**
 * StreamPunk SPOI Executor — JavaScript Runtime（v2: 访问器驱动，零反射）
 *
 * SPOI = StreamPunk Operation Instruction
 * 执行 SPOI 指令流，对 JavaScript 对象进行查询/更新操作。
 *
 * 与 v1 的区别：
 *   - 使用 SpoiAccessor 接口替代 constructor.name 查找属性名
 *   - 使用 DeserializeValue（基于 type_id 前缀）替代字节长度启发式
 *   - 导航和字段设置通过访问器的 switch 跳转表，O(1) 且无反射开销
 *
 * 用法（CommonJS）：
 *   const { SpoiExecutor } = require('./spoi_executor');
 *   const accessors = require('./spoi_js_accessor');
 *   const executor = new SpoiExecutor(accessors.SpoiAccessorRegistry);
 *   const result = executor.execute(rootObj, instructionBytes);
 *
 * 用法（ES Module）：
 *   import { SpoiExecutor } from './spoi_executor.js';
 *   import { SpoiAccessorRegistry } from './spoi_js_accessor.js';
 *   const executor = new SpoiExecutor(SpoiAccessorRegistry);
 *   const result = executor.execute(rootObj, instructionBytes);
 */

'use strict';

const { TypeId, deserializeValue } = require('./spoi_js_accessor');

// =============================== 操作码常量 ===============================

const Op = {
  // 导航
  NAV:        0x00,
  IDX:        0x01,
  DEREF:      0x02,
  UNWRAP:     0x03,
  // 写操作
  SET:        0x04,
  ADD:        0x05,
  APPEND:     0x06,
  REMOVE:     0x07,
  INSERT:     0x08,
  REPLACE:    0x09,
  RESET:      0x0A,
  SETNULL:    0x0B,
  // 读操作
  FILTER:     0x0C,
  SELECT:     0x0D,
  SORT:       0x0E,
  REVERSE:    0x0F,
  TAKE:       0x10,
  DROP:       0x11,
  TAKEWHILE:  0x12,
  DROPWHILE:  0x13,
  DISTINCT:   0x14,
  // 聚合
  COUNT:      0x15,
  ANY:        0x16,
  ALL:        0x17,
  FIND:       0x18,
  // 容器
  KEYS:       0x19,
  VALUES:     0x1A,
  JOIN:       0x1B,
  // 控制
  EXEC:       0x21,
  PIPE:       0x22,
};

// 路径特殊标记
const PATH_DEREF  = 0xFFFF;
const PATH_MAPKEY = 0xFFFE;

// 结果类型
const ResultType = {
  UNDEF:    0,
  SINGLE:   1,
  VECTOR:   2,
  COUNT:    3,
  BOOL:     4,
  OPTIONAL: 5,
  ERROR:    6,
};

// =============================== Varint 编解码 ===============================

function readVarint(data, offset) {
  let result = 0;
  let shift = 0;
  while (offset < data.length) {
    const b = data[offset++];
    result |= (b & 0x7F) << shift;
    if (!(b & 0x80)) return [result >>> 0, offset];
    shift += 7;
  }
  return [result >>> 0, offset];
}

function writeVarint(buf, v) {
  while (v >= 0x80) {
    buf.push((v & 0x7F) | 0x80);
    v >>>= 7;
  }
  buf.push(v & 0x7F);
}

// =============================== SPOI 指令解析 ===============================

function parseSpoiStream(data) {
  let offset = 0;
  const [count, off1] = readVarint(data, offset);
  offset = off1;
  const instructions = [];

  for (let i = 0; i < count; i++) {
    const op = data[offset++];

    const [pathLen, off2] = readVarint(data, offset);
    offset = off2;
    const path = [];
    for (let j = 0; j < pathLen; j++) {
      const [seg, off3] = readVarint(data, offset);
      offset = off3;
      path.push(seg);
    }

    const [operandLen, off4] = readVarint(data, offset);
    offset = off4;
    const operand = data.slice(offset, offset + operandLen);
    offset += operandLen;

    instructions.push({ op, path, operand });
  }

  return instructions;
}

// =============================== SPOI 执行器（v2: 访问器驱动） ===============================

/**
 * @param {Map<string, SpoiAccessor>} accessors - 类型注册表，格式: Map<"TypeName", SpoiAccessor>
 */
class SpoiExecutor {
  constructor(accessors) {
    /** @type {Map<string, SpoiAccessor>} */
    this.accessors = accessors;
    /** @type {unknown[]} */
    this.pipeData = [];
  }

  /**
   * 执行 SPOI 指令流
   * @param {unknown} root - 根对象
   * @param {Uint8Array} instructionBytes - SPOI 指令流二进制数据
   * @returns {{resultType: number, value?: unknown, data?: Uint8Array}}
   */
  execute(root, instructionBytes) {
    const instructions = parseSpoiStream(instructionBytes);
    this.pipeData = [];

    for (const inst of instructions) {
      this.dispatch(inst, root);
    }

    return this.makeResult();
  }

  makeResult() {
    if (this.pipeData.length === 0) {
      return { resultType: ResultType.UNDEF, data: new Uint8Array() };
    }
    if (this.pipeData.length === 1) {
      return { resultType: ResultType.SINGLE, value: this.pipeData[0] };
    }
    return { resultType: ResultType.VECTOR, value: this.pipeData };
  }

  // =============================== 调度 ===============================

  dispatch(inst, root) {
    const { op, path, operand } = inst;

    if (op === Op.SET) {
      this.opSet(root, path, operand);
    } else if (op === Op.ADD) {
      this.opAdd(root, path, operand);
    } else if (op === Op.APPEND) {
      this.opAppend(root, path, operand);
    } else if (op === Op.REMOVE) {
      this.opRemove(root, path, operand);
    } else if (op === Op.INSERT) {
      this.opInsert(root, path, operand);
    } else if (op === Op.REPLACE) {
      this.opReplace(root, path, operand);
    } else if (op === Op.RESET) {
      this.opReset(root, path);
    } else if (op === Op.SETNULL) {
      this.opSetNull(root, path);
    } else if (op === Op.FILTER) {
      this.opFilter(root, path, operand);
    } else if (op === Op.SELECT) {
      this.opSelect(root, path);
    } else if (op === Op.SORT) {
      this.opSort(path);
    } else if (op === Op.REVERSE) {
      this.opReverse();
    } else if (op === Op.TAKE) {
      this.opTake(operand);
    } else if (op === Op.DROP) {
      this.opDrop(operand);
    } else if (op === Op.TAKEWHILE) {
      this.opTakeWhile(root, path, operand);
    } else if (op === Op.DROPWHILE) {
      this.opDropWhile(root, path, operand);
    } else if (op === Op.DISTINCT) {
      this.opDistinct();
    } else if (op === Op.COUNT) {
      this.opCount();
    } else if (op === Op.ANY) {
      this.opAny(root, path, operand);
    } else if (op === Op.ALL) {
      this.opAll(root, path, operand);
    } else if (op === Op.FIND) {
      this.opFind(root, path, operand);
    } else if (op === Op.KEYS) {
      this.opKeys();
    } else if (op === Op.VALUES) {
      this.opValues();
    } else if (op === Op.JOIN) {
      this.opJoin();
    } else if (op === Op.EXEC) {
      // 执行结束，结果已在 pipeData 中
    } else if (op === Op.PIPE) {
      this.opPipe(root, path);
    } else {
      throw new Error(`Unknown SPOI opcode: 0x${op.toString(16).padStart(2, '0')}`);
    }
  }

  // =============================== 导航（访问器驱动） ===============================

  navigate(obj, path) {
    let current = obj;
    for (const seg of path) {
      current = this.navStep(current, seg);
    }
    return current;
  }

  navStep(obj, seg) {
    // null 处理
    if (obj == null) {
      throw new Error('Cannot navigate on null');
    }

    // 指针解引用
    if (seg === PATH_DEREF) {
      // 基本类型返回自身
      if (typeof obj === 'string' || typeof obj === 'number' || typeof obj === 'boolean' || typeof obj === 'bigint') {
        return obj;
      }
      const result = this.accessorNavigate(obj, 0);
      if (result !== undefined) {
        return result;
      }
      // 回退：尝试 obj.value
      if (obj.value !== undefined) {
        return obj.value;
      }
      return obj;
    }

    // 容器索引访问
    if (Array.isArray(obj)) {
      return obj[seg];
    }
    if (obj instanceof Map) {
      return Array.from(obj.values())[seg];
    }

    // 基本类型：seg == 0 时返回自身
    if (typeof obj === 'string' || typeof obj === 'number' || typeof obj === 'boolean' || typeof obj === 'bigint') {
      if (seg === 0) {
        return obj;
      }
      throw new Error(`Cannot navigate segment ${seg} on ${typeof obj}`);
    }

    // 结构体成员访问 — 使用访问器（兼容新旧两种格式）
    const result = this.accessorNavigate(obj, seg);
    if (result !== undefined) {
      return result;
    }

    throw new Error(`Cannot navigate segment ${seg} on ${typeof obj}`);
  }

  navSet(obj, path, value) {
    if (path.length === 0) {
      return;
    }
    if (path.length === 1) {
      this.setField(obj, path[0], value);
      return;
    }

    let target = obj;
    for (let i = 0; i < path.length - 1; i++) {
      target = this.navStep(target, path[i]);
    }
    this.setField(target, path[path.length - 1], value);
  }

  setField(obj, seg, value) {
    if (Array.isArray(obj)) {
      obj[seg] = value;
      return;
    }
    if (obj instanceof Map) {
      const keys = Array.from(obj.keys());
      obj.set(keys[seg], value);
      return;
    }

    // 结构体 — 使用访问器（兼容新旧两种格式）
    if (this.accessorSet(obj, seg, value)) {
      return;
    }

    throw new Error(`Cannot set field ${seg} on ${typeof obj}`);
  }

  /** 获取对象对应的注册表条目（兼容新旧两种格式） */
  getAccessor(obj) {
    if (!obj || !this.accessors) {
      return null;
    }
    const typeName = obj.constructor?.name;
    if (!typeName) {
      return null;
    }
    // 新式：Map 格式
    if (this.accessors instanceof Map) {
      return this.accessors.get(typeName) || null;
    }
    // 旧式：普通对象（字段名数组）
    return this.accessors[typeName] || null;
  }

  /** 通过注册表获取字段值（兼容新旧两种格式） */
  accessorNavigate(obj, seg) {
    const entry = this.getAccessor(obj);
    if (!entry) return undefined;
    // 新式：SpoiAccessor 对象
    if (typeof entry.getField === 'function') {
      return entry.getField(obj, seg);
    }
    // 旧式：字段名数组
    if (Array.isArray(entry)) {
      return obj[entry[seg]];
    }
    return undefined;
  }

  /** 通过注册表设置字段值（兼容新旧两种格式） */
  accessorSet(obj, seg, value) {
    const entry = this.getAccessor(obj);
    if (!entry) return false;
    // 新式：SpoiAccessor 对象
    if (typeof entry.setField === 'function') {
      entry.setField(obj, seg, value);
      return true;
    }
    // 旧式：字段名数组
    if (Array.isArray(entry)) {
      obj[entry[seg]] = value;
      return true;
    }
    return false;
  }

  // =============================== 写操作 ===============================

  opSet(root, path, operand) {
    const value = deserializeValue(operand);
    this.navSet(root, path, value);
  }

  opAdd(root, path, operand) {
    const delta = deserializeValue(operand);
    const target = this.navigate(root, path);
    const result = this.addValues(target, delta);
    this.navSet(root, path, result);
  }

  addValues(a, b) {
    if (typeof a === 'number' && typeof b === 'number') {
      return a + b;
    }
    if (typeof a === 'bigint' && typeof b === 'bigint') {
      return a + b;
    }
    if (typeof a === 'string' || typeof b === 'string') {
      return String(a) + String(b);
    }
    throw new Error(`Cannot add ${typeof a} and ${typeof b}`);
  }

  opAppend(root, path, operand) {
    const value = deserializeValue(operand);
    const target = this.navigate(root, path);
    if (Array.isArray(target)) {
      target.push(value);
    } else {
      throw new Error(`Cannot append to ${typeof target}`);
    }
  }

  opRemove(root, path, operand) {
    const target = this.navigate(root, path);
    const idx = new DataView(operand.buffer, operand.byteOffset, 4).getUint32(0, true);
    if (Array.isArray(target)) {
      target.splice(idx, 1);
    } else {
      throw new Error(`Cannot remove from ${typeof target}`);
    }
  }

  opInsert(root, path, operand) {
    const idx = new DataView(operand.buffer, operand.byteOffset, 4).getUint32(0, true);
    const value = deserializeValue(operand.slice(4));
    const target = this.navigate(root, path);
    if (Array.isArray(target)) {
      target.splice(idx, 0, value);
    } else {
      throw new Error(`Cannot insert into ${typeof target}`);
    }
  }

  opReplace(root, path, operand) {
    const idx = new DataView(operand.buffer, operand.byteOffset, 4).getUint32(0, true);
    const value = deserializeValue(operand.slice(4));
    const target = this.navigate(root, path);
    if (Array.isArray(target)) {
      target[idx] = value;
    } else {
      throw new Error(`Cannot replace in ${typeof target}`);
    }
  }

  opReset(root, path) {
    this.navSet(root, path, null);
  }

  opSetNull(root, path) {
    this.navSet(root, path, null);
  }

  // =============================== 读操作 ===============================

  opPipe(root, path) {
    let data;
    if (path.length > 0) {
      data = this.navigate(root, path);
    } else {
      data = root;
    }

    if (Array.isArray(data)) {
      this.pipeData = [...data];
    } else if (data instanceof Map) {
      this.pipeData = Array.from(data.values());
    } else {
      this.pipeData = [data];
    }
  }

  /** 检查对象是否匹配比较表达式（v2: 访问器驱动） */
  matches(obj, path, operand) {
    // operand 格式: memberIdx(u32) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
    if (operand.length < 9) {
      return true;
    }
    const view = new DataView(operand.buffer, operand.byteOffset, operand.length);
    const memberIdx = view.getUint32(0, true);
    const cmpOp = view.getUint8(4);
    // 跳过 value_len（varint 编码）
    const [_, valueOffset] = readVarint(operand, 5);
    const valueBytes = operand.slice(valueOffset);

    // 先按路径导航到目标对象，再访问成员字段
    let target = obj;
    if (path.length > 0) {
      target = this.navigate(obj, path);
    }
    // 对于基本类型，memberIdx=0 时直接比较值本身
    let fieldValue;
    if (memberIdx === 0 && (typeof target === 'number' || typeof target === 'string' || typeof target === 'boolean' || typeof target === 'bigint')) {
      fieldValue = target;
    } else {
      fieldValue = this.navStep(target, memberIdx);
    }
    const expected = deserializeValue(valueBytes);

    return this.compareValues(fieldValue, cmpOp, expected);
  }

  compareValues(fieldValue, cmpOp, expected) {
    if (fieldValue == null && expected == null) {
      return cmpOp === 0; // eq
    }
    if (fieldValue == null || expected == null) {
      return cmpOp === 1; // ne
    }

    let cmp;
    if (typeof fieldValue === 'string' && typeof expected === 'string') {
      cmp = fieldValue < expected ? -1 : fieldValue > expected ? 1 : 0;
    } else if (typeof fieldValue === 'number' && typeof expected === 'number') {
      cmp = fieldValue < expected ? -1 : fieldValue > expected ? 1 : 0;
    } else if (typeof fieldValue === 'bigint' && typeof expected === 'bigint') {
      cmp = fieldValue < expected ? -1 : fieldValue > expected ? 1 : 0;
    } else {
      cmp = String(fieldValue).localeCompare(String(expected));
    }

    switch (cmpOp) {
      case 0: return cmp === 0;  // eq
      case 1: return cmp !== 0;  // ne
      case 2: return cmp < 0;   // lt
      case 3: return cmp > 0;   // gt
      case 4: return cmp <= 0;  // le
      case 5: return cmp >= 0;  // ge
      default: return true;
    }
  }

  opFilter(root, path, operand) {
    this.pipeData = this.pipeData.filter(obj => this.matches(obj, path, operand));
  }

  opSelect(root, path) {
    if (path.length > 0) {
      this.pipeData = this.pipeData.map(obj => this.navigate(obj, path));
    }
  }

  opSort(path) {
    if (path.length > 0) {
      this.pipeData.sort((a, b) => {
        const va = this.navigate(a, path);
        const vb = this.navigate(b, path);
        return String(va).localeCompare(String(vb));
      });
    } else {
      this.pipeData.sort((a, b) => String(a).localeCompare(String(b)));
    }
  }

  opReverse() {
    this.pipeData.reverse();
  }

  opTake(operand) {
    const n = new DataView(operand.buffer, operand.byteOffset, 4).getUint32(0, true);
    this.pipeData = this.pipeData.slice(0, n);
  }

  opDrop(operand) {
    const n = new DataView(operand.buffer, operand.byteOffset, 4).getUint32(0, true);
    this.pipeData = this.pipeData.slice(n);
  }

  opTakeWhile(root, path, operand) {
    const result = [];
    for (const obj of this.pipeData) {
      if (this.matches(obj, path, operand)) {
        result.push(obj);
      } else {
        break;
      }
    }
    this.pipeData = result;
  }

  opDropWhile(root, path, operand) {
    let idx = this.pipeData.length;
    for (let i = 0; i < this.pipeData.length; i++) {
      if (!this.matches(this.pipeData[i], path, operand)) {
        idx = i;
        break;
      }
    }
    this.pipeData = this.pipeData.slice(idx);
  }

  opDistinct() {
    const seen = new Set();
    const result = [];
    for (const obj of this.pipeData) {
      const key = (typeof obj === 'object' && obj !== null) ? JSON.stringify(obj) : obj;
      if (!seen.has(key)) {
        seen.add(key);
        result.push(obj);
      }
    }
    this.pipeData = result;
  }

  // =============================== 聚合 ===============================

  opCount() {
    this.pipeData = [this.pipeData.length];
  }

  opAny(root, path, operand) {
    let any = false;
    for (const obj of this.pipeData) {
      if (this.matches(obj, path, operand)) {
        any = true;
        break;
      }
    }
    this.pipeData = [any];
  }

  opAll(root, path, operand) {
    let all = true;
    for (const obj of this.pipeData) {
      if (!this.matches(obj, path, operand)) {
        all = false;
        break;
      }
    }
    this.pipeData = [all];
  }

  opFind(root, path, operand) {
    for (const obj of this.pipeData) {
      if (this.matches(obj, path, operand)) {
        this.pipeData = [obj];
        return;
      }
    }
    this.pipeData = [];
  }

  // =============================== 容器操作 ===============================

  opKeys() {
    const result = [];
    for (const obj of this.pipeData) {
      if (obj instanceof Map) {
        result.push(...obj.keys());
      }
    }
    this.pipeData = result;
  }

  opValues() {
    const result = [];
    for (const obj of this.pipeData) {
      if (obj instanceof Map) {
        result.push(...obj.values());
      }
    }
    this.pipeData = result;
  }

  opJoin() {
    const result = [];
    for (const obj of this.pipeData) {
      if (Array.isArray(obj)) {
        result.push(...obj);
      } else {
        result.push(obj);
      }
    }
    this.pipeData = result;
  }
}

// =============================== 导出 ===============================

// 同时支持 CommonJS 和 ES Module
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { SpoiExecutor, Op, ResultType, PATH_DEREF, PATH_MAPKEY, TypeId };
}