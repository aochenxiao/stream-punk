// ============================================================
// SPOI — StreamPunk Operation Instruction
// Rust 查询/更新 Builder（自动生成）
// ============================================================

use std::io::Write;

// 操作码
pub mod op {
    pub const SET:       u8 = 0x04;
    pub const ADD:       u8 = 0x05;
    pub const APPEND:    u8 = 0x06;
    pub const REMOVE:    u8 = 0x07;
    pub const INSERT:    u8 = 0x08;
    pub const REPLACE:   u8 = 0x09;
    pub const RESET:     u8 = 0x0A;
    pub const SETNULL:   u8 = 0x0B;
    pub const FILTER:    u8 = 0x0C;
    pub const SELECT:    u8 = 0x0D;
    pub const SORT:      u8 = 0x0E;
    pub const REVERSE:   u8 = 0x0F;
    pub const TAKE:      u8 = 0x10;
    pub const DROP:      u8 = 0x11;
    pub const TAKEWHILE: u8 = 0x12;
    pub const DROPWHILE: u8 = 0x13;
    pub const DISTINCT:  u8 = 0x14;
    pub const COUNT:     u8 = 0x15;
    pub const ANY:       u8 = 0x16;
    pub const ALL:       u8 = 0x17;
    pub const FIND:      u8 = 0x18;
    pub const KEYS:      u8 = 0x19;
    pub const VALUES:    u8 = 0x1A;
    pub const JOIN:      u8 = 0x1B;
    pub const ENUMERATE: u8 = 0x1C;
    pub const CHUNK:     u8 = 0x1D;
    pub const SLIDE:     u8 = 0x1E;
    pub const STRIDE:    u8 = 0x1F;
    pub const ADJACENT:  u8 = 0x20;
    pub const EXEC:      u8 = 0x21;
}

// 比较运算符
pub mod cmp {
    pub const EQ: u8 = 0;
    pub const NE: u8 = 1;
    pub const LT: u8 = 2;
    pub const GT: u8 = 3;
    pub const LE: u8 = 4;
    pub const GE: u8 = 5;
}

pub const PATH_DEREF: u32 = 0xFFFF;

// 类型成员索引常量
pub mod SpoiTestPlayer {
    pub const name: u32 = 0;
    pub const hp: u32 = 1;
    pub const level: u32 = 2;
    pub const posX: u32 = 3;
}

pub mod SpoiTestState {
    pub const tick: u32 = 0;
    pub const currentMap: u32 = 1;
    pub const players: u32 = 2;
}

pub mod SpoiItem {
    pub const name: u32 = 0;
    pub const value: u32 = 1;
}

pub mod SpoiInventory {
    pub const items: u32 = 0;
    pub const equipped: u32 = 1;
    pub const gold: u32 = 2;
}

pub mod SpoiCharacter {
    pub const name: u32 = 0;
    pub const hp: u32 = 1;
    pub const inventory: u32 = 2;
    pub const weapon: u32 = 3;
    pub const petLevel: u32 = 4;
}

pub mod SpoiWorld {
    pub const worldName: u32 = 0;
    pub const tick: u32 = 1;
    pub const characters: u32 = 2;
}

// Varint 编码
fn write_varint(buf: &mut Vec<u8>, mut v: u32) {
    while v >= 0x80 {
        buf.push((v as u8 & 0x7F) | 0x80);
        v >>= 7;
    }
    buf.push(v as u8 & 0x7F);
}

fn write_u32(buf: &mut Vec<u8>, v: u32) {
    buf.extend_from_slice(&v.to_le_bytes());
}

// SpoiInstruction
#[derive(Clone)]
pub struct SpoiInstruction {
    pub op: u8,
    pub path: Vec<u32>,
    pub operand: Vec<u8>,
}

impl SpoiInstruction {
    pub fn new(op: u8, path: Vec<u32>, operand: Vec<u8>) -> Self {
        Self { op, path, operand }
    }

    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::new();
        buf.push(self.op);
        write_varint(&mut buf, self.path.len() as u32);
        for &seg in &self.path { write_u32(&mut buf, seg); }
        write_varint(&mut buf, self.operand.len() as u32);
        buf.extend_from_slice(&self.operand);
        buf
    }
}

// SpoiStream
pub struct SpoiStream {
    pub instructions: Vec<SpoiInstruction>,
}

impl SpoiStream {
    pub fn new() -> Self { Self { instructions: Vec::new() } }

    pub fn build(&self) -> Vec<u8> {
        let mut buf = Vec::new();
        write_varint(&mut buf, self.instructions.len() as u32);
        for inst in &self.instructions {
            buf.extend_from_slice(&inst.serialize());
        }
        buf
    }

    pub fn build_hex(&self) -> String {
        self.build().iter().map(|b| format!("{:02x}", b)).collect()
    }
}

// SpoiUpdate — 写操作 Builder
pub struct SpoiUpdate {
    stream: SpoiStream,
}

impl SpoiUpdate {
    pub fn new() -> Self { Self { stream: SpoiStream::new() } }

    pub fn set(mut self, path: Vec<u32>, value: Vec<u8>) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::SET, path, value));
        self
    }

    pub fn set_i32(self, path: Vec<u32>, value: i32) -> Self {
        self.set(path, value.to_le_bytes().to_vec())
    }

    pub fn set_u32(self, path: Vec<u32>, value: u32) -> Self {
        self.set(path, value.to_le_bytes().to_vec())
    }

    pub fn set_f64(self, path: Vec<u32>, value: f64) -> Self {
        self.set(path, value.to_le_bytes().to_vec())
    }

    pub fn set_str(self, path: Vec<u32>, value: &str) -> Self {
        let mut buf = Vec::new();
        write_varint(&mut buf, value.len() as u32);
        buf.extend_from_slice(value.as_bytes());
        self.set(path, buf)
    }

    pub fn set_bool(self, path: Vec<u32>, value: bool) -> Self {
        self.set(path, vec![value as u8])
    }

    pub fn add_i32(mut self, path: Vec<u32>, delta: i32) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::ADD, path, delta.to_le_bytes().to_vec()));
        self
    }

    pub fn add(mut self, path: Vec<u32>, value: Vec<u8>) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::ADD, path, value));
        self
    }

    pub fn append(mut self, path: Vec<u32>, value: Vec<u8>) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::APPEND, path, value));
        self
    }

    pub fn remove(mut self, path: Vec<u32>, value: Vec<u8>) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::REMOVE, path, value));
        self
    }

    pub fn insert(mut self, path: Vec<u32>, value: Vec<u8>) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::INSERT, path, value));
        self
    }

    pub fn replace(mut self, path: Vec<u32>, value: Vec<u8>) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::REPLACE, path, value));
        self
    }

    pub fn reset(mut self, path: Vec<u32>) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::RESET, path, vec![]));
        self
    }

    pub fn setnull(mut self, path: Vec<u32>) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::SETNULL, path, vec![]));
        self
    }

    pub fn build(self) -> Vec<u8> { self.stream.build() }
    pub fn build_hex(self) -> String { self.stream.build_hex() }
}

// SpoiQuery — 查询 Builder
pub struct SpoiQuery {
    stream: SpoiStream,
}

impl SpoiQuery {
    pub fn new() -> Self { Self { stream: SpoiStream::new() } }

    pub fn nav(mut self, field: u32) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::FILTER, vec![field], vec![]));
        self
    }

    pub fn filter(mut self, field: u32, cmp_op: u8, value: Vec<u8>) -> Self {
        let mut buf = Vec::new();
        write_u32(&mut buf, field);
        buf.push(cmp_op);
        write_varint(&mut buf, value.len() as u32);
        buf.extend_from_slice(&value);
        self.stream.instructions.push(SpoiInstruction::new(op::FILTER, vec![], buf));
        self
    }

    pub fn filter_i32(self, field: u32, cmp_op: u8, value: i32) -> Self {
        self.filter(field, cmp_op, value.to_le_bytes().to_vec())
    }

    pub fn filter_str(self, field: u32, cmp_op: u8, value: &str) -> Self {
        let mut buf = Vec::new();
        write_varint(&mut buf, value.len() as u32);
        buf.extend_from_slice(value.as_bytes());
        self.filter(field, cmp_op, buf)
    }

    pub fn select(mut self, fields: &[u32]) -> Self {
        let mut buf = Vec::new();
        write_u32(&mut buf, fields.len() as u32);
        for &f in fields { write_u32(&mut buf, f); }
        self.stream.instructions.push(SpoiInstruction::new(op::SELECT, vec![], buf));
        self
    }

    pub fn sort(mut self, field: u32, ascending: bool) -> Self {
        let mut buf = Vec::new();
        write_u32(&mut buf, field);
        buf.push(if ascending { 1 } else { 0 });
        self.stream.instructions.push(SpoiInstruction::new(op::SORT, vec![], buf));
        self
    }

    pub fn reverse(mut self) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::REVERSE, vec![], vec![]));
        self
    }

    pub fn take(mut self, count: u32) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::TAKE, vec![], count.to_le_bytes().to_vec()));
        self
    }

    pub fn drop(mut self, count: u32) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::DROP, vec![], count.to_le_bytes().to_vec()));
        self
    }

    pub fn distinct(mut self) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::DISTINCT, vec![], vec![]));
        self
    }

    pub fn count(mut self) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::COUNT, vec![], vec![]));
        self
    }

    pub fn keys(mut self) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::KEYS, vec![], vec![]));
        self
    }

    pub fn values(mut self) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::VALUES, vec![], vec![]));
        self
    }

    pub fn join(mut self, field: u32) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::JOIN, vec![], field.to_le_bytes().to_vec()));
        self
    }

    pub fn enumerate(mut self, start: u32) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::ENUMERATE, vec![], start.to_le_bytes().to_vec()));
        self
    }

    pub fn chunk(mut self, size: u32) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::CHUNK, vec![], size.to_le_bytes().to_vec()));
        self
    }

    pub fn stride(mut self, step: u32) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::STRIDE, vec![], step.to_le_bytes().to_vec()));
        self
    }

    pub fn takewhile(mut self, field: u32, cmp_op: u8, value: Vec<u8>) -> Self {
        let mut buf = Vec::new();
        write_u32(&mut buf, field);
        buf.push(cmp_op);
        write_varint(&mut buf, value.len() as u32);
        buf.extend_from_slice(&value);
        self.stream.instructions.push(SpoiInstruction::new(op::TAKEWHILE, vec![], buf));
        self
    }

    pub fn dropwhile(mut self, field: u32, cmp_op: u8, value: Vec<u8>) -> Self {
        let mut buf = Vec::new();
        write_u32(&mut buf, field);
        buf.push(cmp_op);
        write_varint(&mut buf, value.len() as u32);
        buf.extend_from_slice(&value);
        self.stream.instructions.push(SpoiInstruction::new(op::DROPWHILE, vec![], buf));
        self
    }

    pub fn any(mut self, field: u32, cmp_op: u8, value: Vec<u8>) -> Self {
        let mut buf = Vec::new();
        write_u32(&mut buf, field);
        buf.push(cmp_op);
        write_varint(&mut buf, value.len() as u32);
        buf.extend_from_slice(&value);
        self.stream.instructions.push(SpoiInstruction::new(op::ANY, vec![], buf));
        self
    }

    pub fn all(mut self, field: u32, cmp_op: u8, value: Vec<u8>) -> Self {
        let mut buf = Vec::new();
        write_u32(&mut buf, field);
        buf.push(cmp_op);
        write_varint(&mut buf, value.len() as u32);
        buf.extend_from_slice(&value);
        self.stream.instructions.push(SpoiInstruction::new(op::ALL, vec![], buf));
        self
    }

    pub fn find(mut self, field: u32, cmp_op: u8, value: Vec<u8>) -> Self {
        let mut buf = Vec::new();
        write_u32(&mut buf, field);
        buf.push(cmp_op);
        write_varint(&mut buf, value.len() as u32);
        buf.extend_from_slice(&value);
        self.stream.instructions.push(SpoiInstruction::new(op::FIND, vec![], buf));
        self
    }

    pub fn slide(mut self, size: u32) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::SLIDE, vec![], size.to_le_bytes().to_vec()));
        self
    }

    pub fn adjacent(mut self, n: u32) -> Self {
        self.stream.instructions.push(SpoiInstruction::new(op::ADJACENT, vec![], n.to_le_bytes().to_vec()));
        self
    }

    pub fn build(mut self) -> Vec<u8> {
        self.stream.instructions.push(SpoiInstruction::new(op::EXEC, vec![], vec![]));
        self.stream.build()
    }
}

