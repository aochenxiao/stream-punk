/*
    spoi_ts_generator.cpp — TypeScript SPOI 代码生成器

    生成 TypeScript 的 SPOI 查询/更新 builder，包含：
    - 类型常量（成员索引）
    - SpoiQuery 流式 API
    - SpoiUpdate 写操作 API
    - SPOI 二进制序列化
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

std::string generateTypeScriptSpoi(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "// ============================================================\n";
    ss << "// SPOI — StreamPunk Operation Instruction\n";
    ss << "// TypeScript 查询/更新 Builder（自动生成）\n";
    ss << "// ============================================================\n\n";

    // ── 操作码常量 ──
    ss << "// ============================================================\n";
    ss << "// 操作码\n";
    ss << "// ============================================================\n\n";
    ss << "export const Op = {\n";
    ss << "  SET:       0x04,\n";
    ss << "  ADD:       0x05,\n";
    ss << "  APPEND:    0x06,\n";
    ss << "  REMOVE:    0x07,\n";
    ss << "  INSERT:    0x08,\n";
    ss << "  REPLACE:   0x09,\n";
    ss << "  RESET:     0x0A,\n";
    ss << "  SETNULL:   0x0B,\n";
    ss << "  FILTER:    0x0C,\n";
    ss << "  SELECT:    0x0D,\n";
    ss << "  SORT:      0x0E,\n";
    ss << "  REVERSE:   0x0F,\n";
    ss << "  TAKE:      0x10,\n";
    ss << "  DROP:      0x11,\n";
    ss << "  TAKEWHILE: 0x12,\n";
    ss << "  DROPWHILE: 0x13,\n";
    ss << "  DISTINCT:  0x14,\n";
    ss << "  COUNT:     0x15,\n";
    ss << "  ANY:       0x16,\n";
    ss << "  ALL:       0x17,\n";
    ss << "  FIND:      0x18,\n";
    ss << "  KEYS:      0x19,\n";
    ss << "  VALUES:    0x1A,\n";
    ss << "  JOIN:      0x1B,\n";
    ss << "  ENUMERATE: 0x1C,\n";
    ss << "  CHUNK:     0x1D,\n";
    ss << "  SLIDE:     0x1E,\n";
    ss << "  STRIDE:    0x1F,\n";
    ss << "  ADJACENT:  0x20,\n";
    ss << "  EXEC:      0x21,\n";
    ss << "} as const;\n\n";

    ss << "export const Cmp = {\n";
    ss << "  EQ: 0, NE: 1, LT: 2, GT: 3, LE: 4, GE: 5,\n";
    ss << "} as const;\n\n";
    ss << "export const PATH_DEREF = 0xFFFF;\n\n";

    // ── 类型成员常量 ──
    ss << "// ============================================================\n";
    ss << "// 类型成员索引常量\n";
    ss << "// ============================================================\n\n";
    for (auto& t : types) {
        ss << "// " << t.className << "\n";
        ss << "export const " << t.className << " = {\n";
        for (auto& f : t.fields) {
            ss << "  " << f.name << ": " << f.index << ",\n";
        }
        ss << "} as const;\n\n";
    }

    // ── Varint 编码 ──
    ss << "// ============================================================\n";
    ss << "// Varint 编码\n";
    ss << "// ============================================================\n\n";
    ss << "function writeVarint(buf: number[], v: number): void {\n";
    ss << "  while (v >= 0x80) {\n";
    ss << "    buf.push((v & 0x7F) | 0x80);\n";
    ss << "    v >>>= 7;\n";
    ss << "  }\n";
    ss << "  buf.push(v & 0x7F);\n";
    ss << "}\n\n";
    ss << "function writeU32(buf: number[], v: number): void {\n";
    ss << "  buf.push(v & 0xFF, (v >>> 8) & 0xFF, (v >>> 16) & 0xFF, (v >>> 24) & 0xFF);\n";
    ss << "}\n\n";

    // ── SpoiInstruction ──
    ss << "// ============================================================\n";
    ss << "// SpoiInstruction\n";
    ss << "// ============================================================\n\n";
    ss << "export class SpoiInstruction {\n";
    ss << "  constructor(\n";
    ss << "    public op: number,\n";
    ss << "    public path: number[],\n";
    ss << "    public operand: Uint8Array = new Uint8Array(0)\n";
    ss << "  ) {}\n\n";
    ss << "  serialize(): Uint8Array {\n";
    ss << "    const buf: number[] = [];\n";
    ss << "    buf.push(this.op);\n";
    ss << "    writeVarint(buf, this.path.length);\n";
    ss << "    for (const seg of this.path) writeU32(buf, seg);\n";
    ss << "    writeVarint(buf, this.operand.length);\n";
    ss << "    for (const b of this.operand) buf.push(b);\n";
    ss << "    return new Uint8Array(buf);\n";
    ss << "  }\n";
    ss << "}\n\n";

    // ── SpoiStream ──
    ss << "// ============================================================\n";
    ss << "// SpoiStream\n";
    ss << "// ============================================================\n\n";
    ss << "export class SpoiStream {\n";
    ss << "  instructions: SpoiInstruction[] = [];\n\n";
    ss << "  build(): Uint8Array {\n";
    ss << "    const buf: number[] = [];\n";
    ss << "    writeVarint(buf, this.instructions.length);\n";
    ss << "    for (const inst of this.instructions) {\n";
    ss << "      for (const b of inst.serialize()) buf.push(b);\n";
    ss << "    }\n";
    ss << "    return new Uint8Array(buf);\n";
    ss << "  }\n\n";
    ss << "  buildHex(): string {\n";
    ss << "    return Array.from(this.build()).map(b => b.toString(16).padStart(2, '0')).join('');\n";
    ss << "  }\n";
    ss << "}\n\n";

    // ── SpoiUpdate ──
    ss << "// ============================================================\n";
    ss << "// SpoiUpdate — 写操作 Builder\n";
    ss << "// ============================================================\n\n";
    ss << "export class SpoiUpdate {\n";
    ss << "  private _stream = new SpoiStream();\n\n";
    ss << "  set(path: number[], value: Uint8Array): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.SET, path, value));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  setI32(path: number[], value: number): this {\n";
    ss << "    const buf = new Uint8Array(4);\n";
    ss << "    new DataView(buf.buffer).setInt32(0, value, true);\n";
    ss << "    return this.set(path, buf);\n";
    ss << "  }\n\n";
    ss << "  setU32(path: number[], value: number): this {\n";
    ss << "    const buf = new Uint8Array(4);\n";
    ss << "    new DataView(buf.buffer).setUint32(0, value, true);\n";
    ss << "    return this.set(path, buf);\n";
    ss << "  }\n\n";
    ss << "  setF64(path: number[], value: number): this {\n";
    ss << "    const buf = new Uint8Array(8);\n";
    ss << "    new DataView(buf.buffer).setFloat64(0, value, true);\n";
    ss << "    return this.set(path, buf);\n";
    ss << "  }\n\n";
    ss << "  setStr(path: number[], value: string): this {\n";
    ss << "    const enc = new TextEncoder().encode(value);\n";
    ss << "    const buf: number[] = [];\n";
    ss << "    writeVarint(buf, enc.length);\n";
    ss << "    for (const b of enc) buf.push(b);\n";
    ss << "    return this.set(path, new Uint8Array(buf));\n";
    ss << "  }\n\n";
    ss << "  setBool(path: number[], value: boolean): this {\n";
    ss << "    return this.set(path, new Uint8Array([value ? 1 : 0]));\n";
    ss << "  }\n\n";
    ss << "  addI32(path: number[], delta: number): this {\n";
    ss << "    const buf = new Uint8Array(4);\n";
    ss << "    new DataView(buf.buffer).setInt32(0, delta, true);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.ADD, path, buf));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  add(path: number[], value: Uint8Array): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.ADD, path, value));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  append(path: number[], value: Uint8Array): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.APPEND, path, value));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  remove(path: number[], value: Uint8Array): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.REMOVE, path, value));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  insert(path: number[], value: Uint8Array): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.INSERT, path, value));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  replace(path: number[], value: Uint8Array): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.REPLACE, path, value));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  reset(path: number[]): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.RESET, path));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  setnull(path: number[]): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.SETNULL, path));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  build(): Uint8Array { return this._stream.build(); }\n";
    ss << "  buildHex(): string { return this._stream.buildHex(); }\n";
    ss << "}\n\n";

    // ── SpoiQuery ──
    ss << "// ============================================================\n";
    ss << "// SpoiQuery — 查询 Builder\n";
    ss << "// ============================================================\n\n";
    ss << "export class SpoiQuery {\n";
    ss << "  private _stream = new SpoiStream();\n\n";
    ss << "  nav(field: number): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.FILTER, [field]));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  filter(field: number, cmpOp: number, value: Uint8Array): this {\n";
    ss << "    const buf: number[] = [];\n";
    ss << "    writeU32(buf, field);\n";
    ss << "    buf.push(cmpOp);\n";
    ss << "    writeVarint(buf, value.length);\n";
    ss << "    for (const b of value) buf.push(b);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.FILTER, [], new Uint8Array(buf)));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  filterI32(field: number, cmpOp: number, value: number): this {\n";
    ss << "    const buf = new Uint8Array(4);\n";
    ss << "    new DataView(buf.buffer).setInt32(0, value, true);\n";
    ss << "    return this.filter(field, cmpOp, buf);\n";
    ss << "  }\n\n";
    ss << "  filterStr(field: number, cmpOp: number, value: string): this {\n";
    ss << "    const enc = new TextEncoder().encode(value);\n";
    ss << "    const buf: number[] = [];\n";
    ss << "    writeVarint(buf, enc.length);\n";
    ss << "    for (const b of enc) buf.push(b);\n";
    ss << "    return this.filter(field, cmpOp, new Uint8Array(buf));\n";
    ss << "  }\n\n";
    ss << "  select(...fields: number[]): this {\n";
    ss << "    const buf: number[] = [];\n";
    ss << "    writeU32(buf, fields.length);\n";
    ss << "    for (const f of fields) writeU32(buf, f);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.SELECT, [], new Uint8Array(buf)));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  sort(field: number, ascending: boolean = true): this {\n";
    ss << "    const buf: number[] = [];\n";
    ss << "    writeU32(buf, field);\n";
    ss << "    buf.push(ascending ? 1 : 0);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.SORT, [], new Uint8Array(buf)));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  reverse(): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.REVERSE, []));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  take(count: number): this {\n";
    ss << "    const buf = new Uint8Array(4);\n";
    ss << "    new DataView(buf.buffer).setUint32(0, count, true);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.TAKE, [], buf));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  drop(count: number): this {\n";
    ss << "    const buf = new Uint8Array(4);\n";
    ss << "    new DataView(buf.buffer).setUint32(0, count, true);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.DROP, [], buf));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  distinct(): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.DISTINCT, []));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  count(): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.COUNT, []));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  keys(): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.KEYS, []));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  values(): this {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.VALUES, []));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  join(field: number): this {\n";
    ss << "    const buf = new Uint8Array(4);\n";
    ss << "    new DataView(buf.buffer).setUint32(0, field, true);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.JOIN, [], buf));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  enumerate(start: number = 0): this {\n";
    ss << "    const buf = new Uint8Array(4);\n";
    ss << "    new DataView(buf.buffer).setUint32(0, start, true);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.ENUMERATE, [], buf));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  chunk(size: number): this {\n";
    ss << "    const buf = new Uint8Array(4);\n";
    ss << "    new DataView(buf.buffer).setUint32(0, size, true);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.CHUNK, [], buf));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  stride(step: number): this {\n";
    ss << "    const buf = new Uint8Array(4);\n";
    ss << "    new DataView(buf.buffer).setUint32(0, step, true);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.STRIDE, [], buf));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  takewhile(field: number, cmpOp: number, value: Uint8Array): this {\n";
    ss << "    const buf: number[] = [];\n";
    ss << "    writeU32(buf, field);\n";
    ss << "    buf.push(cmpOp);\n";
    ss << "    writeVarint(buf, value.length);\n";
    ss << "    for (const b of value) buf.push(b);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.TAKEWHILE, [], new Uint8Array(buf)));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  dropwhile(field: number, cmpOp: number, value: Uint8Array): this {\n";
    ss << "    const buf: number[] = [];\n";
    ss << "    writeU32(buf, field);\n";
    ss << "    buf.push(cmpOp);\n";
    ss << "    writeVarint(buf, value.length);\n";
    ss << "    for (const b of value) buf.push(b);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.DROPWHILE, [], new Uint8Array(buf)));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  any(field: number, cmpOp: number, value: Uint8Array): this {\n";
    ss << "    const buf: number[] = [];\n";
    ss << "    writeU32(buf, field);\n";
    ss << "    buf.push(cmpOp);\n";
    ss << "    writeVarint(buf, value.length);\n";
    ss << "    for (const b of value) buf.push(b);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.ANY, [], new Uint8Array(buf)));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  all(field: number, cmpOp: number, value: Uint8Array): this {\n";
    ss << "    const buf: number[] = [];\n";
    ss << "    writeU32(buf, field);\n";
    ss << "    buf.push(cmpOp);\n";
    ss << "    writeVarint(buf, value.length);\n";
    ss << "    for (const b of value) buf.push(b);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.ALL, [], new Uint8Array(buf)));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  find(field: number, cmpOp: number, value: Uint8Array): this {\n";
    ss << "    const buf: number[] = [];\n";
    ss << "    writeU32(buf, field);\n";
    ss << "    buf.push(cmpOp);\n";
    ss << "    writeVarint(buf, value.length);\n";
    ss << "    for (const b of value) buf.push(b);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.FIND, [], new Uint8Array(buf)));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  slide(size: number): this {\n";
    ss << "    const buf = new Uint8Array(4);\n";
    ss << "    new DataView(buf.buffer).setUint32(0, size, true);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.SLIDE, [], buf));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  adjacent(n: number): this {\n";
    ss << "    const buf = new Uint8Array(4);\n";
    ss << "    new DataView(buf.buffer).setUint32(0, n, true);\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.ADJACENT, [], buf));\n";
    ss << "    return this;\n";
    ss << "  }\n\n";
    ss << "  build(): Uint8Array {\n";
    ss << "    this._stream.instructions.push(new SpoiInstruction(Op.EXEC, []));\n";
    ss << "    return this._stream.build();\n";
    ss << "  }\n";
    ss << "}\n\n";

    return ss.str();
}

int generate_spoi_ts(const std::string& output_path, const std::string& meta_path) {
    try {
        auto meta = sp_meta::readMetaFile(meta_path);
        auto types = extractSpoiTypes(meta);
        auto code = generateTypeScriptSpoi(types);

        std::ofstream out(output_path);
        if (!out.is_open()) throw std::runtime_error("Cannot open: " + output_path);
        out << code;
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "SPOI TS generator error: " << e.what() << "\n";
        return 1;
    }
}