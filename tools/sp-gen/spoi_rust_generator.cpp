/*
    spoi_rust_generator.cpp — Rust SPOI 代码生成器

    生成 Rust 的 SPOI 查询/更新 builder，包含：
    - 类型常量（成员索引）
    - SpoiQuery 流式 API
    - SpoiUpdate 写操作 API
    - SPOI 二进制序列化
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

std::string generateRustSpoi(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "// ============================================================\n";
    ss << "// SPOI — StreamPunk Operation Instruction\n";
    ss << "// Rust 查询/更新 Builder（自动生成）\n";
    ss << "// ============================================================\n\n";

    ss << "use std::io::Write;\n\n";

    // ── 操作码常量 ──
    ss << "// 操作码\n";
    ss << "pub mod op {\n";
    ss << "    pub const SET:       u8 = 0x04;\n";
    ss << "    pub const ADD:       u8 = 0x05;\n";
    ss << "    pub const APPEND:    u8 = 0x06;\n";
    ss << "    pub const REMOVE:    u8 = 0x07;\n";
    ss << "    pub const INSERT:    u8 = 0x08;\n";
    ss << "    pub const REPLACE:   u8 = 0x09;\n";
    ss << "    pub const RESET:     u8 = 0x0A;\n";
    ss << "    pub const SETNULL:   u8 = 0x0B;\n";
    ss << "    pub const FILTER:    u8 = 0x0C;\n";
    ss << "    pub const SELECT:    u8 = 0x0D;\n";
    ss << "    pub const SORT:      u8 = 0x0E;\n";
    ss << "    pub const REVERSE:   u8 = 0x0F;\n";
    ss << "    pub const TAKE:      u8 = 0x10;\n";
    ss << "    pub const DROP:      u8 = 0x11;\n";
    ss << "    pub const TAKEWHILE: u8 = 0x12;\n";
    ss << "    pub const DROPWHILE: u8 = 0x13;\n";
    ss << "    pub const DISTINCT:  u8 = 0x14;\n";
    ss << "    pub const COUNT:     u8 = 0x15;\n";
    ss << "    pub const ANY:       u8 = 0x16;\n";
    ss << "    pub const ALL:       u8 = 0x17;\n";
    ss << "    pub const FIND:      u8 = 0x18;\n";
    ss << "    pub const KEYS:      u8 = 0x19;\n";
    ss << "    pub const VALUES:    u8 = 0x1A;\n";
    ss << "    pub const JOIN:      u8 = 0x1B;\n";
    ss << "    pub const ENUMERATE: u8 = 0x1C;\n";
    ss << "    pub const CHUNK:     u8 = 0x1D;\n";
    ss << "    pub const SLIDE:     u8 = 0x1E;\n";
    ss << "    pub const STRIDE:    u8 = 0x1F;\n";
    ss << "    pub const ADJACENT:  u8 = 0x20;\n";
    ss << "    pub const EXEC:      u8 = 0x21;\n";
    ss << "}\n\n";

    ss << "// 比较运算符\n";
    ss << "pub mod cmp {\n";
    ss << "    pub const EQ: u8 = 0;\n";
    ss << "    pub const NE: u8 = 1;\n";
    ss << "    pub const LT: u8 = 2;\n";
    ss << "    pub const GT: u8 = 3;\n";
    ss << "    pub const LE: u8 = 4;\n";
    ss << "    pub const GE: u8 = 5;\n";
    ss << "}\n\n";

    ss << "pub const PATH_DEREF: u32 = 0xFFFF;\n\n";

    // ── 类型成员常量 ──
    ss << "// 类型成员索引常量\n";
    for (auto& t : types) {
        ss << "pub mod " << t.className << " {\n";
        for (auto& f : t.fields) {
            ss << "    pub const " << f.name << ": u32 = " << f.index << ";\n";
        }
        ss << "}\n\n";
    }

    // ── Varint 编码 ──
    ss << "// Varint 编码\n";
    ss << "fn write_varint(buf: &mut Vec<u8>, mut v: u32) {\n";
    ss << "    while v >= 0x80 {\n";
    ss << "        buf.push((v as u8 & 0x7F) | 0x80);\n";
    ss << "        v >>= 7;\n";
    ss << "    }\n";
    ss << "    buf.push(v as u8 & 0x7F);\n";
    ss << "}\n\n";
    ss << "fn write_u32(buf: &mut Vec<u8>, v: u32) {\n";
    ss << "    buf.extend_from_slice(&v.to_le_bytes());\n";
    ss << "}\n\n";

    // ── SpoiInstruction ──
    ss << "// SpoiInstruction\n";
    ss << "#[derive(Clone)]\n";
    ss << "pub struct SpoiInstruction {\n";
    ss << "    pub op: u8,\n";
    ss << "    pub path: Vec<u32>,\n";
    ss << "    pub operand: Vec<u8>,\n";
    ss << "}\n\n";
    ss << "impl SpoiInstruction {\n";
    ss << "    pub fn new(op: u8, path: Vec<u32>, operand: Vec<u8>) -> Self {\n";
    ss << "        Self { op, path, operand }\n";
    ss << "    }\n\n";
    ss << "    pub fn serialize(&self) -> Vec<u8> {\n";
    ss << "        let mut buf = Vec::new();\n";
    ss << "        buf.push(self.op);\n";
    ss << "        write_varint(&mut buf, self.path.len() as u32);\n";
    ss << "        for &seg in &self.path { write_u32(&mut buf, seg); }\n";
    ss << "        write_varint(&mut buf, self.operand.len() as u32);\n";
    ss << "        buf.extend_from_slice(&self.operand);\n";
    ss << "        buf\n";
    ss << "    }\n";
    ss << "}\n\n";

    // ── SpoiStream ──
    ss << "// SpoiStream\n";
    ss << "pub struct SpoiStream {\n";
    ss << "    pub instructions: Vec<SpoiInstruction>,\n";
    ss << "}\n\n";
    ss << "impl SpoiStream {\n";
    ss << "    pub fn new() -> Self { Self { instructions: Vec::new() } }\n\n";
    ss << "    pub fn build(&self) -> Vec<u8> {\n";
    ss << "        let mut buf = Vec::new();\n";
    ss << "        write_varint(&mut buf, self.instructions.len() as u32);\n";
    ss << "        for inst in &self.instructions {\n";
    ss << "            buf.extend_from_slice(&inst.serialize());\n";
    ss << "        }\n";
    ss << "        buf\n";
    ss << "    }\n\n";
    ss << "    pub fn build_hex(&self) -> String {\n";
    ss << "        self.build().iter().map(|b| format!(\"{:02x}\", b)).collect()\n";
    ss << "    }\n";
    ss << "}\n\n";

    // ── SpoiUpdate ──
    ss << "// SpoiUpdate — 写操作 Builder\n";
    ss << "pub struct SpoiUpdate {\n";
    ss << "    stream: SpoiStream,\n";
    ss << "}\n\n";
    ss << "impl SpoiUpdate {\n";
    ss << "    pub fn new() -> Self { Self { stream: SpoiStream::new() } }\n\n";
    ss << "    pub fn set(mut self, path: Vec<u32>, value: Vec<u8>) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::SET, path, value));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn set_i32(self, path: Vec<u32>, value: i32) -> Self {\n";
    ss << "        self.set(path, value.to_le_bytes().to_vec())\n";
    ss << "    }\n\n";
    ss << "    pub fn set_u32(self, path: Vec<u32>, value: u32) -> Self {\n";
    ss << "        self.set(path, value.to_le_bytes().to_vec())\n";
    ss << "    }\n\n";
    ss << "    pub fn set_f64(self, path: Vec<u32>, value: f64) -> Self {\n";
    ss << "        self.set(path, value.to_le_bytes().to_vec())\n";
    ss << "    }\n\n";
    ss << "    pub fn set_str(self, path: Vec<u32>, value: &str) -> Self {\n";
    ss << "        let mut buf = Vec::new();\n";
    ss << "        write_varint(&mut buf, value.len() as u32);\n";
    ss << "        buf.extend_from_slice(value.as_bytes());\n";
    ss << "        self.set(path, buf)\n";
    ss << "    }\n\n";
    ss << "    pub fn set_bool(self, path: Vec<u32>, value: bool) -> Self {\n";
    ss << "        self.set(path, vec![value as u8])\n";
    ss << "    }\n\n";
    ss << "    pub fn add_i32(mut self, path: Vec<u32>, delta: i32) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::ADD, path, delta.to_le_bytes().to_vec()));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn add(mut self, path: Vec<u32>, value: Vec<u8>) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::ADD, path, value));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn append(mut self, path: Vec<u32>, value: Vec<u8>) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::APPEND, path, value));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn remove(mut self, path: Vec<u32>, value: Vec<u8>) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::REMOVE, path, value));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn insert(mut self, path: Vec<u32>, value: Vec<u8>) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::INSERT, path, value));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn replace(mut self, path: Vec<u32>, value: Vec<u8>) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::REPLACE, path, value));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn reset(mut self, path: Vec<u32>) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::RESET, path, vec![]));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn setnull(mut self, path: Vec<u32>) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::SETNULL, path, vec![]));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn build(self) -> Vec<u8> { self.stream.build() }\n";
    ss << "    pub fn build_hex(self) -> String { self.stream.build_hex() }\n";
    ss << "}\n\n";

    // ── SpoiQuery ──
    ss << "// SpoiQuery — 查询 Builder\n";
    ss << "pub struct SpoiQuery {\n";
    ss << "    stream: SpoiStream,\n";
    ss << "}\n\n";
    ss << "impl SpoiQuery {\n";
    ss << "    pub fn new() -> Self { Self { stream: SpoiStream::new() } }\n\n";
    ss << "    pub fn nav(mut self, field: u32) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::FILTER, vec![field], vec![]));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn filter(mut self, field: u32, cmp_op: u8, value: Vec<u8>) -> Self {\n";
    ss << "        let mut buf = Vec::new();\n";
    ss << "        write_u32(&mut buf, field);\n";
    ss << "        buf.push(cmp_op);\n";
    ss << "        write_varint(&mut buf, value.len() as u32);\n";
    ss << "        buf.extend_from_slice(&value);\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::FILTER, vec![], buf));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn filter_i32(self, field: u32, cmp_op: u8, value: i32) -> Self {\n";
    ss << "        self.filter(field, cmp_op, value.to_le_bytes().to_vec())\n";
    ss << "    }\n\n";
    ss << "    pub fn filter_str(self, field: u32, cmp_op: u8, value: &str) -> Self {\n";
    ss << "        let mut buf = Vec::new();\n";
    ss << "        write_varint(&mut buf, value.len() as u32);\n";
    ss << "        buf.extend_from_slice(value.as_bytes());\n";
    ss << "        self.filter(field, cmp_op, buf)\n";
    ss << "    }\n\n";
    ss << "    pub fn select(mut self, fields: &[u32]) -> Self {\n";
    ss << "        let mut buf = Vec::new();\n";
    ss << "        write_u32(&mut buf, fields.len() as u32);\n";
    ss << "        for &f in fields { write_u32(&mut buf, f); }\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::SELECT, vec![], buf));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn sort(mut self, field: u32, ascending: bool) -> Self {\n";
    ss << "        let mut buf = Vec::new();\n";
    ss << "        write_u32(&mut buf, field);\n";
    ss << "        buf.push(if ascending { 1 } else { 0 });\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::SORT, vec![], buf));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn reverse(mut self) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::REVERSE, vec![], vec![]));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn take(mut self, count: u32) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::TAKE, vec![], count.to_le_bytes().to_vec()));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn drop(mut self, count: u32) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::DROP, vec![], count.to_le_bytes().to_vec()));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn distinct(mut self) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::DISTINCT, vec![], vec![]));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn count(mut self) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::COUNT, vec![], vec![]));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn keys(mut self) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::KEYS, vec![], vec![]));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn values(mut self) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::VALUES, vec![], vec![]));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn join(mut self, field: u32) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::JOIN, vec![], field.to_le_bytes().to_vec()));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn enumerate(mut self, start: u32) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::ENUMERATE, vec![], start.to_le_bytes().to_vec()));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn chunk(mut self, size: u32) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::CHUNK, vec![], size.to_le_bytes().to_vec()));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn stride(mut self, step: u32) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::STRIDE, vec![], step.to_le_bytes().to_vec()));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn takewhile(mut self, field: u32, cmp_op: u8, value: Vec<u8>) -> Self {\n";
    ss << "        let mut buf = Vec::new();\n";
    ss << "        write_u32(&mut buf, field);\n";
    ss << "        buf.push(cmp_op);\n";
    ss << "        write_varint(&mut buf, value.len() as u32);\n";
    ss << "        buf.extend_from_slice(&value);\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::TAKEWHILE, vec![], buf));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn dropwhile(mut self, field: u32, cmp_op: u8, value: Vec<u8>) -> Self {\n";
    ss << "        let mut buf = Vec::new();\n";
    ss << "        write_u32(&mut buf, field);\n";
    ss << "        buf.push(cmp_op);\n";
    ss << "        write_varint(&mut buf, value.len() as u32);\n";
    ss << "        buf.extend_from_slice(&value);\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::DROPWHILE, vec![], buf));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn any(mut self, field: u32, cmp_op: u8, value: Vec<u8>) -> Self {\n";
    ss << "        let mut buf = Vec::new();\n";
    ss << "        write_u32(&mut buf, field);\n";
    ss << "        buf.push(cmp_op);\n";
    ss << "        write_varint(&mut buf, value.len() as u32);\n";
    ss << "        buf.extend_from_slice(&value);\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::ANY, vec![], buf));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn all(mut self, field: u32, cmp_op: u8, value: Vec<u8>) -> Self {\n";
    ss << "        let mut buf = Vec::new();\n";
    ss << "        write_u32(&mut buf, field);\n";
    ss << "        buf.push(cmp_op);\n";
    ss << "        write_varint(&mut buf, value.len() as u32);\n";
    ss << "        buf.extend_from_slice(&value);\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::ALL, vec![], buf));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn find(mut self, field: u32, cmp_op: u8, value: Vec<u8>) -> Self {\n";
    ss << "        let mut buf = Vec::new();\n";
    ss << "        write_u32(&mut buf, field);\n";
    ss << "        buf.push(cmp_op);\n";
    ss << "        write_varint(&mut buf, value.len() as u32);\n";
    ss << "        buf.extend_from_slice(&value);\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::FIND, vec![], buf));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn slide(mut self, size: u32) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::SLIDE, vec![], size.to_le_bytes().to_vec()));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn adjacent(mut self, n: u32) -> Self {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::ADJACENT, vec![], n.to_le_bytes().to_vec()));\n";
    ss << "        self\n";
    ss << "    }\n\n";
    ss << "    pub fn build(mut self) -> Vec<u8> {\n";
    ss << "        self.stream.instructions.push(SpoiInstruction::new(op::EXEC, vec![], vec![]));\n";
    ss << "        self.stream.build()\n";
    ss << "    }\n";
    ss << "}\n\n";

    return ss.str();
}

int generate_spoi_rust(const std::string& output_path, const std::string& meta_path) {
    try {
        auto meta = sp_meta::readMetaFile(meta_path);
        auto types = extractSpoiTypes(meta);
        auto code = generateRustSpoi(types);

        std::ofstream out(output_path);
        if (!out.is_open()) throw std::runtime_error("Cannot open: " + output_path);
        out << code;
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "SPOI Rust generator error: " << e.what() << "\n";
        return 1;
    }
}