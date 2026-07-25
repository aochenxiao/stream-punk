# ============================================================
# SPOI — StreamPunk Operation Instruction
# Python 查询/更新 Builder（自动生成）
# ============================================================

import struct
from typing import List, Optional, Any, Union
from io import BytesIO

# ============================================================
# 操作码
# ============================================================

class Op:
    SET       = 0x04
    ADD       = 0x05
    APPEND    = 0x06
    REMOVE    = 0x07
    INSERT    = 0x08
    REPLACE   = 0x09
    RESET     = 0x0A
    SETNULL   = 0x0B
    FILTER    = 0x0C
    SELECT    = 0x0D
    SORT      = 0x0E
    REVERSE   = 0x0F
    TAKE      = 0x10
    DROP      = 0x11
    TAKEWHILE = 0x12
    DROPWHILE = 0x13
    DISTINCT  = 0x14
    COUNT     = 0x15
    ANY       = 0x16
    ALL       = 0x17
    FIND      = 0x18
    KEYS      = 0x19
    VALUES    = 0x1A
    JOIN      = 0x1B
    ENUMERATE = 0x1C
    CHUNK     = 0x1D
    SLIDE     = 0x1E
    STRIDE    = 0x1F
    ADJACENT  = 0x20
    EXEC      = 0x21

# ============================================================
# 比较运算符
# ============================================================

class Cmp:
    EQ = 0
    NE = 1
    LT = 2
    GT = 3
    LE = 4
    GE = 5

# ============================================================
# 类型成员索引常量
# ============================================================

# SpoiTestPlayer
SpoiTestPlayer_name = 0
SpoiTestPlayer_hp = 1
SpoiTestPlayer_level = 2
SpoiTestPlayer_posX = 3

# SpoiTestState
SpoiTestState_tick = 0
SpoiTestState_currentMap = 1
SpoiTestState_players = 2

# SpoiItem
SpoiItem_name = 0
SpoiItem_value = 1

# SpoiInventory
SpoiInventory_items = 0
SpoiInventory_equipped = 1
SpoiInventory_gold = 2

# SpoiCharacter
SpoiCharacter_name = 0
SpoiCharacter_hp = 1
SpoiCharacter_inventory = 2
SpoiCharacter_weapon = 3
SpoiCharacter_petLevel = 4

# SpoiWorld
SpoiWorld_worldName = 0
SpoiWorld_tick = 1
SpoiWorld_characters = 2

# ============================================================
# Varint 编码
# ============================================================

def _write_varint(buf: bytearray, v: int) -> None:
    while v >= 0x80:
        buf.append((v & 0x7F) | 0x80)
        v >>= 7
    buf.append(v & 0x7F)

def _write_u32(buf: bytearray, v: int) -> None:
    buf.extend(struct.pack('<I', v & 0xFFFFFFFF))

# ============================================================
# SpoiInstruction 序列化
# ============================================================

class SpoiInstruction:
    def __init__(self, op: int, path: List[int], operand: bytes = b''):
        self.op = op
        self.path = path
        self.operand = operand

    def serialize(self) -> bytes:
        buf = bytearray()
        buf.append(self.op)
        _write_varint(buf, len(self.path))
        for seg in self.path:
            _write_u32(buf, seg)
        _write_varint(buf, len(self.operand))
        buf.extend(self.operand)
        return bytes(buf)

# ============================================================
# SpoiStream（指令流）
# ============================================================

class SpoiStream:
    def __init__(self):
        self.instructions: List[SpoiInstruction] = []

    def build(self) -> bytes:
        buf = bytearray()
        _write_varint(buf, len(self.instructions))
        for inst in self.instructions:
            buf.extend(inst.serialize())
        return bytes(buf)

    def build_hex(self) -> str:
        return self.build().hex()

# ============================================================
# SpoiUpdate — 写操作 Builder
# ============================================================

class SpoiUpdate:
    def __init__(self):
        self._stream = SpoiStream()

    def set(self, path: List[int], value_bytes: bytes) -> 'SpoiUpdate':
        self._stream.instructions.append(SpoiInstruction(Op.SET, path, value_bytes))
        return self

    def set_i32(self, path: List[int], value: int) -> 'SpoiUpdate':
        return self.set(path, struct.pack('<i', value))

    def set_u32(self, path: List[int], value: int) -> 'SpoiUpdate':
        return self.set(path, struct.pack('<I', value))

    def set_f64(self, path: List[int], value: float) -> 'SpoiUpdate':
        return self.set(path, struct.pack('<d', value))

    def set_str(self, path: List[int], value: str) -> 'SpoiUpdate':
        data = value.encode('utf-8')
        buf = bytearray()
        _write_varint(buf, len(data))
        buf.extend(data)
        return self.set(path, bytes(buf))

    def set_bool(self, path: List[int], value: bool) -> 'SpoiUpdate':
        return self.set(path, b'\x01' if value else b'\x00')

    def add_i32(self, path: List[int], delta: int) -> 'SpoiUpdate':
        self._stream.instructions.append(SpoiInstruction(Op.ADD, path, struct.pack('<i', delta)))
        return self

    def add_f64(self, path: List[int], delta: float) -> 'SpoiUpdate':
        self._stream.instructions.append(SpoiInstruction(Op.ADD, path, struct.pack('<d', delta)))
        return self

    def add(self, path: List[int], value_bytes: bytes) -> 'SpoiUpdate':
        self._stream.instructions.append(SpoiInstruction(Op.ADD, path, value_bytes))
        return self

    def append(self, path: List[int], value_bytes: bytes) -> 'SpoiUpdate':
        self._stream.instructions.append(SpoiInstruction(Op.APPEND, path, value_bytes))
        return self

    def remove(self, path: List[int], value_bytes: bytes) -> 'SpoiUpdate':
        self._stream.instructions.append(SpoiInstruction(Op.REMOVE, path, value_bytes))
        return self

    def insert(self, path: List[int], value_bytes: bytes) -> 'SpoiUpdate':
        self._stream.instructions.append(SpoiInstruction(Op.INSERT, path, value_bytes))
        return self

    def replace(self, path: List[int], value_bytes: bytes) -> 'SpoiUpdate':
        self._stream.instructions.append(SpoiInstruction(Op.REPLACE, path, value_bytes))
        return self

    def reset(self, path: List[int]) -> 'SpoiUpdate':
        self._stream.instructions.append(SpoiInstruction(Op.RESET, path))
        return self

    def setnull(self, path: List[int]) -> 'SpoiUpdate':
        self._stream.instructions.append(SpoiInstruction(Op.SETNULL, path))
        return self

    def build(self) -> SpoiStream:
        return self._stream

    def build_hex(self) -> str:
        return self._stream.build_hex()

# ============================================================
# SpoiQuery — 查询 Builder
# ============================================================

class SpoiQuery:
    def __init__(self, root_type: str):
        self._stream = SpoiStream()
        self._root_type = root_type

    def nav(self, field: int) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.FILTER, [field]))
        return self

    def filter(self, field: int, cmp_op: int, value_bytes: bytes) -> 'SpoiQuery':
        buf = bytearray()
        _write_u32(buf, field)
        buf.append(cmp_op)
        _write_varint(buf, len(value_bytes))
        buf.extend(value_bytes)
        self._stream.instructions.append(SpoiInstruction(Op.FILTER, [], bytes(buf)))
        return self

    def filter_i32(self, field: int, cmp_op: int, value: int) -> 'SpoiQuery':
        return self.filter(field, cmp_op, struct.pack('<i', value))

    def filter_f64(self, field: int, cmp_op: int, value: float) -> 'SpoiQuery':
        return self.filter(field, cmp_op, struct.pack('<d', value))

    def filter_str(self, field: int, cmp_op: int, value: str) -> 'SpoiQuery':
        data = value.encode('utf-8')
        buf = bytearray()
        _write_varint(buf, len(data))
        buf.extend(data)
        return self.filter(field, cmp_op, bytes(buf))

    def filter_bool(self, field: int, cmp_op: int, value: bool) -> 'SpoiQuery':
        return self.filter(field, cmp_op, b'\x01' if value else b'\x00')

    def select(self, *fields: int) -> 'SpoiQuery':
        buf = bytearray()
        _write_u32(buf, len(fields))
        for f in fields:
            _write_u32(buf, f)
        self._stream.instructions.append(SpoiInstruction(Op.SELECT, [], bytes(buf)))
        return self

    def sort(self, field: int, ascending: bool = True) -> 'SpoiQuery':
        buf = bytearray()
        _write_u32(buf, field)
        buf.append(1 if ascending else 0)
        self._stream.instructions.append(SpoiInstruction(Op.SORT, [], bytes(buf)))
        return self

    def reverse(self) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.REVERSE, []))
        return self

    def take(self, count: int) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.TAKE, [], struct.pack('<I', count)))
        return self

    def drop(self, count: int) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.DROP, [], struct.pack('<I', count)))
        return self

    def distinct(self) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.DISTINCT, []))
        return self

    def count(self) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.COUNT, []))
        return self

    def takewhile(self, field: int, cmp_op: int, value_bytes: bytes) -> 'SpoiQuery':
        buf = bytearray()
        _write_u32(buf, field)
        buf.append(cmp_op)
        _write_varint(buf, len(value_bytes))
        buf.extend(value_bytes)
        self._stream.instructions.append(SpoiInstruction(Op.TAKEWHILE, [], bytes(buf)))
        return self

    def dropwhile(self, field: int, cmp_op: int, value_bytes: bytes) -> 'SpoiQuery':
        buf = bytearray()
        _write_u32(buf, field)
        buf.append(cmp_op)
        _write_varint(buf, len(value_bytes))
        buf.extend(value_bytes)
        self._stream.instructions.append(SpoiInstruction(Op.DROPWHILE, [], bytes(buf)))
        return self

    def any(self, field: int, cmp_op: int, value_bytes: bytes) -> 'SpoiQuery':
        buf = bytearray()
        _write_u32(buf, field)
        buf.append(cmp_op)
        _write_varint(buf, len(value_bytes))
        buf.extend(value_bytes)
        self._stream.instructions.append(SpoiInstruction(Op.ANY, [], bytes(buf)))
        return self

    def all(self, field: int, cmp_op: int, value_bytes: bytes) -> 'SpoiQuery':
        buf = bytearray()
        _write_u32(buf, field)
        buf.append(cmp_op)
        _write_varint(buf, len(value_bytes))
        buf.extend(value_bytes)
        self._stream.instructions.append(SpoiInstruction(Op.ALL, [], bytes(buf)))
        return self

    def find(self, field: int, cmp_op: int, value_bytes: bytes) -> 'SpoiQuery':
        buf = bytearray()
        _write_u32(buf, field)
        buf.append(cmp_op)
        _write_varint(buf, len(value_bytes))
        buf.extend(value_bytes)
        self._stream.instructions.append(SpoiInstruction(Op.FIND, [], bytes(buf)))
        return self

    def keys(self) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.KEYS, []))
        return self

    def values(self) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.VALUES, []))
        return self

    def join(self, field: int) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.JOIN, [], struct.pack('<I', field)))
        return self

    def enumerate(self, start: int = 0) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.ENUMERATE, [], struct.pack('<I', start)))
        return self

    def chunk(self, size: int) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.CHUNK, [], struct.pack('<I', size)))
        return self

    def slide(self, size: int) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.SLIDE, [], struct.pack('<I', size)))
        return self

    def stride(self, step: int) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.STRIDE, [], struct.pack('<I', step)))
        return self

    def adjacent(self, n: int) -> 'SpoiQuery':
        self._stream.instructions.append(SpoiInstruction(Op.ADJACENT, [], struct.pack('<I', n)))
        return self

    def build(self) -> SpoiStream:
        self._stream.instructions.append(SpoiInstruction(Op.EXEC, []))
        return self._stream

    def build_hex(self) -> str:
        self.build()
        return self._stream.build_hex()

