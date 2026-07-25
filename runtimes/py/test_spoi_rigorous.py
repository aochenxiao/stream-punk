"""
SPOI Accessor 刁钻测试 — Python 版

测试覆盖：
  1. 数值边界：各类型的最大值/最小值/零值/NaN/Inf
  2. 字符串边界：空串、Unicode/emoji、null字节、长串、特殊字符
  3. 反序列化异常：截断数据、无效type_id、空数据、类型不匹配
  4. Accessor 越界/类型不匹配：负索引、超大索引、setField传入错误类型
  5. Executor 组合操作：多层FILTER、空管道、边界组合
"""

import sys, os, struct, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from spoi_accessor import (
    TypeId, deserialize_value,
    SpoiTestPlayerAccessor, SpoiTestStateAccessor,
    SpoiItemAccessor, SpoiInventoryAccessor,
    SpoiCharacterAccessor, SpoiWorldAccessor,
    SpoiAccessorRegistry,
)
from spoi_executor import SpoiExecutor, SpoiInstruction, Op, read_varint, write_varint

# ============================================================
# 测试数据类
# ============================================================
class SpoiTestPlayer:
    def __init__(self): self.name = ""; self.hp = 0; self.level = 0; self.posX = 0.0
class SpoiTestState:
    def __init__(self): self.tick = 0; self.currentMap = ""; self.players = None
class SpoiItem:
    def __init__(self): self.name = ""; self.value = 0
class SpoiInventory:
    def __init__(self): self.items = None; self.equipped = None; self.gold = 0
class SpoiCharacter:
    def __init__(self): self.name = ""; self.hp = 0; self.inventory = None; self.weapon = None; self.petLevel = 0
class SpoiWorld:
    def __init__(self): self.worldName = ""; self.tick = 0; self.characters = None

# ============================================================
# 辅助函数
# ============================================================
def make_typed_value(type_id, value_bytes):
    return struct.pack('<I', type_id) + value_bytes

def build_set_inst(path, type_id, value_bytes):
    return SpoiInstruction(op=Op.SET, path=list(path), operand=make_typed_value(type_id, value_bytes))

def build_pipe_inst(path=None):
    return SpoiInstruction(op=Op.PIPE, path=list(path or []), operand=b'')

def build_exec_inst():
    return SpoiInstruction(op=Op.EXEC, path=[], operand=b'')

def build_filter_inst(path, member_idx, cmp_op, type_id, value_bytes):
    typed_val = struct.pack('<I', type_id) + value_bytes
    vlen_buf = bytearray()
    write_varint(vlen_buf, len(typed_val))
    operand = struct.pack('<I', member_idx) + bytes([cmp_op]) + bytes(vlen_buf) + typed_val
    return SpoiInstruction(op=Op.FILTER, path=list(path), operand=operand)

def build_spoi(instructions):
    buf = bytearray()
    write_varint(buf, len(instructions))
    for inst in instructions:
        buf.append(inst.op)
        write_varint(buf, len(inst.path))
        for seg in inst.path: write_varint(buf, seg)
        write_varint(buf, len(inst.operand))
        buf.extend(inst.operand)
    return bytes(buf)

# ============================================================
# 测试框架
# ============================================================
passed = 0
failed = 0
failures = []

def check(cond, msg=""):
    if not cond: raise AssertionError(msg)

def test(name, fn):
    global passed, failed
    try:
        fn()
        passed += 1
    except Exception as e:
        failed += 1
        failures.append(f"  FAIL {name}: {e}")

def _assert_raises(exc_type, fn):
    try:
        fn()
        raise AssertionError(f"Expected {exc_type.__name__} but no exception raised")
    except exc_type: pass

def _assert_no_raise(fn):
    try: fn()
    except Exception as e: raise AssertionError(f"Expected no exception but got {type(e).__name__}: {e}")

# ============================================================
# 1. 数值边界
# ============================================================
def test_numeric_boundaries():
    test("U8=0",    lambda: check(deserialize_value(make_typed_value(TypeId.U8, bytes([0]))) == 0))
    test("U8=255",  lambda: check(deserialize_value(make_typed_value(TypeId.U8, bytes([255]))) == 255))
    test("U8=128",  lambda: check(deserialize_value(make_typed_value(TypeId.U8, bytes([128]))) == 128))
    test("U16=0",   lambda: check(deserialize_value(make_typed_value(TypeId.U16, struct.pack('<H', 0))) == 0))
    test("U16=65535", lambda: check(deserialize_value(make_typed_value(TypeId.U16, struct.pack('<H', 65535))) == 65535))
    test("U16=32768", lambda: check(deserialize_value(make_typed_value(TypeId.U16, struct.pack('<H', 32768))) == 32768))
    test("U32=0",   lambda: check(deserialize_value(make_typed_value(TypeId.U32, struct.pack('<I', 0))) == 0))
    test("U32=4294967295", lambda: check(deserialize_value(make_typed_value(TypeId.U32, struct.pack('<I', 4294967295))) == 4294967295))
    test("U32=2147483648", lambda: check(deserialize_value(make_typed_value(TypeId.U32, struct.pack('<I', 2147483648))) == 2147483648))
    test("U64=0",   lambda: check(deserialize_value(make_typed_value(TypeId.U64, struct.pack('<Q', 0))) == 0))
    test("U64=max", lambda: check(deserialize_value(make_typed_value(TypeId.U64, struct.pack('<Q', 18446744073709551615))) == 18446744073709551615))
    test("U64=mid", lambda: check(deserialize_value(make_typed_value(TypeId.U64, struct.pack('<Q', 9223372036854775808))) == 9223372036854775808))
    test("I8=-128", lambda: check(deserialize_value(make_typed_value(TypeId.I8, struct.pack('<b', -128))) == -128))
    test("I8=127",  lambda: check(deserialize_value(make_typed_value(TypeId.I8, struct.pack('<b', 127))) == 127))
    test("I8=0",    lambda: check(deserialize_value(make_typed_value(TypeId.I8, struct.pack('<b', 0))) == 0))
    test("I8=-1",   lambda: check(deserialize_value(make_typed_value(TypeId.I8, struct.pack('<b', -1))) == -1))
    test("I16=-32768", lambda: check(deserialize_value(make_typed_value(TypeId.I16, struct.pack('<h', -32768))) == -32768))
    test("I16=32767",  lambda: check(deserialize_value(make_typed_value(TypeId.I16, struct.pack('<h', 32767))) == 32767))
    test("I16=0",    lambda: check(deserialize_value(make_typed_value(TypeId.I16, struct.pack('<h', 0))) == 0))
    test("I16=-1",   lambda: check(deserialize_value(make_typed_value(TypeId.I16, struct.pack('<h', -1))) == -1))
    test("I32=min",  lambda: check(deserialize_value(make_typed_value(TypeId.I32, struct.pack('<i', -2147483648))) == -2147483648))
    test("I32=max",  lambda: check(deserialize_value(make_typed_value(TypeId.I32, struct.pack('<i', 2147483647))) == 2147483647))
    test("I32=0",    lambda: check(deserialize_value(make_typed_value(TypeId.I32, struct.pack('<i', 0))) == 0))
    test("I32=-1",   lambda: check(deserialize_value(make_typed_value(TypeId.I32, struct.pack('<i', -1))) == -1))
    test("I64=min",  lambda: check(deserialize_value(make_typed_value(TypeId.I64, struct.pack('<q', -9223372036854775808))) == -9223372036854775808))
    test("I64=max",  lambda: check(deserialize_value(make_typed_value(TypeId.I64, struct.pack('<q', 9223372036854775807))) == 9223372036854775807))
    test("I64=0",    lambda: check(deserialize_value(make_typed_value(TypeId.I64, struct.pack('<q', 0))) == 0))
    test("I64=-1",   lambda: check(deserialize_value(make_typed_value(TypeId.I64, struct.pack('<q', -1))) == -1))
    test("F32=0.0",  lambda: check(deserialize_value(make_typed_value(TypeId.F32, struct.pack('<f', 0.0))) == 0.0))
    test("F32=-0.0", lambda: check(math.copysign(1.0, deserialize_value(make_typed_value(TypeId.F32, struct.pack('<f', -0.0)))) == -1.0))
    test("F32=NaN",  lambda: check(math.isnan(deserialize_value(make_typed_value(TypeId.F32, struct.pack('<f', float('nan')))))))
    test("F32=Inf",  lambda: check(math.isinf(deserialize_value(make_typed_value(TypeId.F32, struct.pack('<f', float('inf'))))) and deserialize_value(make_typed_value(TypeId.F32, struct.pack('<f', float('inf')))) > 0))
    test("F32=-Inf", lambda: check(math.isinf(deserialize_value(make_typed_value(TypeId.F32, struct.pack('<f', float('-inf'))))) and deserialize_value(make_typed_value(TypeId.F32, struct.pack('<f', float('-inf')))) < 0))
    test("F64=0.0",  lambda: check(deserialize_value(make_typed_value(TypeId.F64, struct.pack('<d', 0.0))) == 0.0))
    test("F64=-0.0", lambda: check(math.copysign(1.0, deserialize_value(make_typed_value(TypeId.F64, struct.pack('<d', -0.0)))) == -1.0))
    test("F64=NaN",  lambda: check(math.isnan(deserialize_value(make_typed_value(TypeId.F64, struct.pack('<d', float('nan')))))))
    test("F64=Inf",  lambda: check(math.isinf(deserialize_value(make_typed_value(TypeId.F64, struct.pack('<d', float('inf'))))) and deserialize_value(make_typed_value(TypeId.F64, struct.pack('<d', float('inf')))) > 0))
    test("F64=-Inf", lambda: check(math.isinf(deserialize_value(make_typed_value(TypeId.F64, struct.pack('<d', float('-inf'))))) and deserialize_value(make_typed_value(TypeId.F64, struct.pack('<d', float('-inf')))) < 0))
    test("Bool=1",   lambda: check(deserialize_value(make_typed_value(TypeId.BOOL, bytes([1]))) == True))
    test("Bool=0",   lambda: check(deserialize_value(make_typed_value(TypeId.BOOL, bytes([0]))) == False))
    test("Bool=42",  lambda: check(deserialize_value(make_typed_value(TypeId.BOOL, bytes([42]))) == True))
    test("Bool=255", lambda: check(deserialize_value(make_typed_value(TypeId.BOOL, bytes([255]))) == True))
    test("Bool=-1",  lambda: check(deserialize_value(make_typed_value(TypeId.BOOL, struct.pack('<b', -1))) == True))

# ============================================================
# 2. 字符串边界
# ============================================================
def test_string_boundaries():
    test("empty", lambda: check(deserialize_value(make_typed_value(TypeId.STRING, b"")) == ""))
    test("Unicode/emoji", lambda: check(deserialize_value(make_typed_value(TypeId.STRING, "你好世界🌍🎉".encode('utf-8'))) == "你好世界🌍🎉"))
    test("null byte", lambda: check(deserialize_value(make_typed_value(TypeId.STRING, b"hello\x00world")) == "hello\x00world"))
    test("special chars", lambda: check(deserialize_value(make_typed_value(TypeId.STRING, b"\n\t\r\b\f")) == "\n\t\r\b\f"))
    test("long str", lambda: check(deserialize_value(make_typed_value(TypeId.STRING, b"x" * 1000)) == "x" * 1000))
    test("non-ASCII", lambda: check(deserialize_value(make_typed_value(TypeId.STRING, "café".encode('utf-8'))) == "café"))
    test("CJK", lambda: check(deserialize_value(make_typed_value(TypeId.STRING, "日本語テスト".encode('utf-8'))) == "日本語テスト"))
    test("spaces", lambda: check(deserialize_value(make_typed_value(TypeId.STRING, b"     ")) == "     "))

# ============================================================
# 3. 反序列化异常/边界
# ============================================================
def test_deserialize_edge_cases():
    test("truncated empty", lambda: check(deserialize_value(b"") is None))
    test("truncated 1 byte", lambda: check(deserialize_value(b"\x01") is None))
    test("truncated 3 bytes", lambda: check(deserialize_value(b"\x01\x02\x03") is None))
    # Python struct.unpack 要求精确字节长度，截断数据会抛异常
    test("U32 claim 2 bytes", lambda: _assert_raises(struct.error, lambda: deserialize_value(struct.pack('<I', TypeId.U32) + b"\x01\x02")))
    test("U64 claim 3 bytes", lambda: _assert_raises(struct.error, lambda: deserialize_value(struct.pack('<I', TypeId.U64) + b"\x01\x02\x03")))
    test("U16 claim 1 byte", lambda: _assert_raises(struct.error, lambda: deserialize_value(struct.pack('<I', TypeId.U16) + b"\xAB")))
    test("type_id=0", lambda: check(isinstance(deserialize_value(struct.pack('<I', 0) + b"\xAA\xBB"), bytes)))
    test("type_id=999", lambda: check(isinstance(deserialize_value(struct.pack('<I', 999) + b"\xCC\xDD"), bytes)))
    test("type_id=999 empty", lambda: check(isinstance(deserialize_value(struct.pack('<I', 999) + b""), bytes)))
    # U8/BOOL: value_bytes 为空时 value_bytes[0] 会 IndexError
    test("U8 no value", lambda: _assert_raises(IndexError, lambda: deserialize_value(struct.pack('<I', TypeId.U8))))
    test("BOOL no value", lambda: _assert_raises(IndexError, lambda: deserialize_value(struct.pack('<I', TypeId.BOOL))))
    test("STRING no value", lambda: check(deserialize_value(struct.pack('<I', TypeId.STRING)) == ""))

# ============================================================
# 4. Accessor 越界/类型不匹配
# ============================================================
def test_accessor_edge_cases():
    acc = SpoiTestPlayerAccessor()
    obj = SpoiTestPlayer()
    obj.name = "Test"; obj.hp = 100; obj.level = 50; obj.posX = 12.5

    test("get_field -1", lambda: _assert_raises(ValueError, lambda: acc.get_field(obj, -1)))
    test("get_field -100", lambda: _assert_raises(ValueError, lambda: acc.get_field(obj, -100)))
    test("set_field -1", lambda: _assert_raises(ValueError, lambda: acc.set_field(obj, -1, 0)))
    test("set_field -100", lambda: _assert_raises(ValueError, lambda: acc.set_field(obj, -100, 0)))
    test("get_field =fc", lambda: _assert_raises(ValueError, lambda: acc.get_field(obj, 4)))
    test("get_field >fc", lambda: _assert_raises(ValueError, lambda: acc.get_field(obj, 10)))
    test("get_field huge", lambda: _assert_raises(ValueError, lambda: acc.get_field(obj, 999999)))
    test("set_field =fc", lambda: _assert_raises(ValueError, lambda: acc.set_field(obj, 4, 0)))
    test("set_field >fc", lambda: _assert_raises(ValueError, lambda: acc.set_field(obj, 10, 0)))
    test("set_field huge", lambda: _assert_raises(ValueError, lambda: acc.set_field(obj, 999999, 0)))

    # Python 不检查类型，赋什么就是什么
    test("set str for int", lambda: _assert_no_raise(lambda: acc.set_field(obj, 1, "not a number")))
    test("set int for str", lambda: _assert_no_raise(lambda: acc.set_field(obj, 0, 42)))
    test("verify str for int", lambda: check(obj.hp == "not a number"))
    test("verify int for str", lambda: check(obj.name == 42))

    # 不同类型对象
    item = SpoiItem()
    test("get on wrong type", lambda: _assert_raises(Exception, lambda: acc.get_field(item, 0)))
    test("set on wrong type", lambda: _assert_raises(Exception, lambda: acc.set_field(item, 0, 0)))

    # State accessor
    sa = SpoiTestStateAccessor()
    s = SpoiTestState(); s.tick = 0; s.currentMap = ""; s.players = None
    test("state get neg", lambda: _assert_raises(ValueError, lambda: sa.get_field(s, -1)))
    test("state set neg", lambda: _assert_raises(ValueError, lambda: sa.set_field(s, -1, 0)))
    test("state get fc", lambda: _assert_raises(ValueError, lambda: sa.get_field(s, 3)))
    test("state set fc", lambda: _assert_raises(ValueError, lambda: sa.set_field(s, 3, 0)))

    # Character
    ca = SpoiCharacterAccessor()
    c = SpoiCharacter(); c.name = "A"; c.hp = 1; c.inventory = None; c.weapon = None; c.petLevel = 0
    test("char fc=5", lambda: check(ca.field_count() == 5))
    test("char get 4", lambda: check(ca.get_field(c, 4) == 0))
    test("char set 4", lambda: _assert_no_raise(lambda: ca.set_field(c, 4, 99)))
    test("char verify", lambda: check(c.petLevel == 99))
    test("char get 5 oob", lambda: _assert_raises(ValueError, lambda: ca.get_field(c, 5)))

    # World
    wa = SpoiWorldAccessor()
    w = SpoiWorld(); w.worldName = "X"; w.tick = 0; w.characters = None
    test("world fc=3", lambda: check(wa.field_count() == 3))
    test("world get 2", lambda: check(wa.get_field(w, 2) is None))
    test("world set 2", lambda: _assert_no_raise(lambda: wa.set_field(w, 2, "test")))
    test("world verify", lambda: check(w.characters == "test"))

# ============================================================
# 5. Executor 组合操作/边界
# ============================================================
def test_executor_edge_cases():
    ex = SpoiExecutor(SpoiAccessorRegistry)

    # SET + PIPE + EXEC
    p = SpoiTestPlayer(); p.name = "Hero"; p.hp = 100; p.level = 5; p.posX = 0.0
    data = build_spoi([build_set_inst([1], TypeId.I32, struct.pack('<i', 999)), build_pipe_inst([]), build_exec_inst()])
    r = ex.execute(p, data)
    test("SET+PIPE hp=999", lambda: check(p.hp == 999))
    test("SET+PIPE result", lambda: check(r["value"].hp == 999))

    # 多层 SET
    p2 = SpoiTestPlayer(); p2.name = "X"; p2.hp = 0; p2.level = 0; p2.posX = 0.0
    data = build_spoi([
        build_set_inst([0], TypeId.STRING, b"Warrior"),
        build_set_inst([1], TypeId.I32, struct.pack('<i', 500)),
        build_set_inst([2], TypeId.I32, struct.pack('<i', 99)),
        build_set_inst([3], TypeId.F32, struct.pack('<f', 3.14)),
        build_pipe_inst([]), build_exec_inst(),
    ])
    ex.execute(p2, data)
    test("multi SET name", lambda: check(p2.name == "Warrior"))
    test("multi SET hp", lambda: check(p2.hp == 500))
    test("multi SET level", lambda: check(p2.level == 99))
    test("multi SET posX", lambda: check(abs(p2.posX - 3.14) < 0.001))

    # 创建测试数据
    p1 = SpoiTestPlayer(); p1.name = "Alice"; p1.hp = 100; p1.level = 1; p1.posX = 0.0
    p2 = SpoiTestPlayer(); p2.name = "Bob"; p2.hp = 200; p2.level = 5; p2.posX = 0.0
    p3 = SpoiTestPlayer(); p3.name = "Carol"; p3.hp = 300; p3.level = 10; p3.posX = 0.0
    st = SpoiTestState(); st.tick = 0; st.currentMap = "Test"; st.players = [p1, p2, p3]

    # FILTER: hp > 150
    data = build_spoi([build_pipe_inst([2]), build_filter_inst([], 1, 3, TypeId.I32, struct.pack('<i', 150)), build_exec_inst()])
    r = ex.execute(st, data)
    test("FILTER hp>150 count", lambda: check(len(r["value"]) == 2))

    # 双层 FILTER
    data = build_spoi([
        build_pipe_inst([2]),
        build_filter_inst([], 1, 3, TypeId.I32, struct.pack('<i', 150)),
        build_filter_inst([], 2, 2, TypeId.I32, struct.pack('<i', 10)),
        build_exec_inst(),
    ])
    r = ex.execute(st, data)
    # 单结果时 resultType=SINGLE, value 是单个对象
    test("double FILTER name", lambda: check(r["value"].name == "Bob"))

    # FILTER: 空结果 → resultType=UNDEF, 无 "value" 键
    data = build_spoi([build_pipe_inst([2]), build_filter_inst([], 1, 4, TypeId.I32, struct.pack('<i', 0)), build_exec_inst()])
    r = ex.execute(st, data)
    test("FILTER empty", lambda: check(r["resultType"] == 0 and "value" not in r))

    # COUNT
    data = build_spoi([build_pipe_inst([2]), SpoiInstruction(op=Op.COUNT, path=[], operand=b''), build_exec_inst()])
    test("COUNT 3", lambda: check(ex.execute(st, data)["value"] == 3))

    # COUNT 空管道
    data = build_spoi([SpoiInstruction(op=Op.COUNT, path=[], operand=b''), build_exec_inst()])
    test("COUNT empty", lambda: check(ex.execute(st, data)["value"] == 0))

    # SORT
    data = build_spoi([build_pipe_inst([2]), SpoiInstruction(op=Op.SORT, path=[0], operand=b''), build_exec_inst()])
    r = ex.execute(st, data)
    test("SORT Alice first", lambda: check(r["value"][0].name == "Alice"))
    test("SORT Carol last", lambda: check(r["value"][2].name == "Carol"))

    # SORT 空列表
    data = build_spoi([build_pipe_inst([2]), build_filter_inst([], 1, 4, TypeId.I32, struct.pack('<i', 0)), SpoiInstruction(op=Op.SORT, path=[0], operand=b''), build_exec_inst()])
    test("SORT empty", lambda: check(ex.execute(st, data)["resultType"] == 0))

    # TAKE
    data = build_spoi([build_pipe_inst([2]), SpoiInstruction(op=Op.TAKE, path=[], operand=struct.pack('<I', 2)), build_exec_inst()])
    test("TAKE 2", lambda: check(len(ex.execute(st, data)["value"]) == 2))
    data = build_spoi([build_pipe_inst([2]), SpoiInstruction(op=Op.TAKE, path=[], operand=struct.pack('<I', 100)), build_exec_inst()])
    test("TAKE 100", lambda: check(len(ex.execute(st, data)["value"]) == 3))
    data = build_spoi([build_pipe_inst([2]), SpoiInstruction(op=Op.TAKE, path=[], operand=struct.pack('<I', 0)), build_exec_inst()])
    test("TAKE 0", lambda: check(ex.execute(st, data)["resultType"] == 0))

    # DROP
    data = build_spoi([build_pipe_inst([2]), SpoiInstruction(op=Op.DROP, path=[], operand=struct.pack('<I', 1)), build_exec_inst()])
    test("DROP 1", lambda: check(len(ex.execute(st, data)["value"]) == 2))
    data = build_spoi([build_pipe_inst([2]), SpoiInstruction(op=Op.DROP, path=[], operand=struct.pack('<I', 100)), build_exec_inst()])
    test("DROP 100", lambda: check(ex.execute(st, data)["resultType"] == 0))

    # SELECT
    data = build_spoi([build_pipe_inst([2]), SpoiInstruction(op=Op.SELECT, path=[0], operand=b''), build_exec_inst()])
    r = ex.execute(st, data)
    test("SELECT count", lambda: check(len(r["value"]) == 3))
    test("SELECT values", lambda: check(r["value"] == ["Alice", "Bob", "Carol"]))

    # SELECT 空管道
    data = build_spoi([build_pipe_inst([2]), build_filter_inst([], 1, 4, TypeId.I32, struct.pack('<i', 0)), SpoiInstruction(op=Op.SELECT, path=[0], operand=b''), build_exec_inst()])
    test("SELECT empty", lambda: check(ex.execute(st, data)["resultType"] == 0))

    # REVERSE
    data = build_spoi([build_pipe_inst([2]), SpoiInstruction(op=Op.REVERSE, path=[], operand=b''), build_exec_inst()])
    test("REVERSE Carol", lambda: check(ex.execute(st, data)["value"][0].name == "Carol"))
    # REVERSE 空
    data = build_spoi([build_pipe_inst([2]), build_filter_inst([], 1, 4, TypeId.I32, struct.pack('<i', 0)), SpoiInstruction(op=Op.REVERSE, path=[], operand=b''), build_exec_inst()])
    test("REVERSE empty", lambda: check(ex.execute(st, data)["resultType"] == 0))

    # DISTINCT
    st2 = SpoiTestState(); st2.tick = 0; st2.currentMap = ""; st2.players = [p1, p1, p2, p2, p3]
    data = build_spoi([build_pipe_inst([2]), SpoiInstruction(op=Op.DISTINCT, path=[], operand=b''), build_exec_inst()])
    test("DISTINCT 3", lambda: check(len(ex.execute(st2, data)["value"]) == 3))
    data = build_spoi([build_pipe_inst([2]), build_filter_inst([], 1, 4, TypeId.I32, struct.pack('<i', 0)), SpoiInstruction(op=Op.DISTINCT, path=[], operand=b''), build_exec_inst()])
    test("DISTINCT empty", lambda: check(ex.execute(st2, data)["resultType"] == 0))

# ============================================================
# 6. 跨类型 Executor
# ============================================================
def test_executor_cross_type():
    ex = SpoiExecutor(SpoiAccessorRegistry)

    # Item
    item = SpoiItem(); item.name = ""; item.value = 0
    data = build_spoi([build_set_inst([0], TypeId.STRING, b"Potion"), build_set_inst([1], TypeId.I32, struct.pack('<i', 50)), build_pipe_inst([]), build_exec_inst()])
    ex.execute(item, data)
    test("Item name", lambda: check(item.name == "Potion"))
    test("Item value", lambda: check(item.value == 50))

    # Inventory
    inv = SpoiInventory(); inv.items = None; inv.equipped = None; inv.gold = 0
    data = build_spoi([build_set_inst([2], TypeId.I32, struct.pack('<i', 1000)), build_pipe_inst([]), build_exec_inst()])
    ex.execute(inv, data)
    test("Inv gold", lambda: check(inv.gold == 1000))

    # Character
    char = SpoiCharacter(); char.name = ""; char.hp = 0; char.inventory = None; char.weapon = None; char.petLevel = 0
    data = build_spoi([build_set_inst([0], TypeId.STRING, b"Hero"), build_set_inst([1], TypeId.I32, struct.pack('<i', 2000)), build_set_inst([4], TypeId.I32, struct.pack('<i', 5)), build_pipe_inst([]), build_exec_inst()])
    ex.execute(char, data)
    test("Char name", lambda: check(char.name == "Hero"))
    test("Char hp", lambda: check(char.hp == 2000))
    test("Char petLevel", lambda: check(char.petLevel == 5))

    # World
    world = SpoiWorld(); world.worldName = ""; world.tick = 0; world.characters = None
    data = build_spoi([build_set_inst([0], TypeId.STRING, b"Azeroth"), build_set_inst([1], TypeId.I32, struct.pack('<i', 9999)), build_pipe_inst([]), build_exec_inst()])
    ex.execute(world, data)
    test("World name", lambda: check(world.worldName == "Azeroth"))
    test("World tick", lambda: check(world.tick == 9999))

# ============================================================
# 7. Registry 边界
# ============================================================
def test_registry_edge_cases():
    test("size=6", lambda: check(len(SpoiAccessorRegistry) == 6))
    test("missing key", lambda: check(SpoiAccessorRegistry.get("NonExistentType") is None))
    for name, acc in SpoiAccessorRegistry.items():
        test(f"registry {name} fc>0", lambda acc=acc: check(acc.field_count() > 0))

# ============================================================
# 测试入口
# ============================================================
if __name__ == '__main__':
    print("=" * 60)
    print("  SPOI Accessor 刁钻测试 — Python 版")
    print("=" * 60)

    suites = [
        ("1. 数值边界", test_numeric_boundaries),
        ("2. 字符串边界", test_string_boundaries),
        ("3. 反序列化异常/边界", test_deserialize_edge_cases),
        ("4. Accessor 越界/类型不匹配", test_accessor_edge_cases),
        ("5. Executor 组合操作/边界", test_executor_edge_cases),
        ("6. 跨类型 Executor", test_executor_cross_type),
        ("7. Registry 边界", test_registry_edge_cases),
    ]

    for name, fn in suites:
        p_before, f_before = passed, failed
        fn()
        total = (passed - p_before) + (failed - f_before)
        print(f"\n{'='*60}")
        print(f"  {name}  [{passed - p_before}/{total} passed]")
        print(f"{'='*60}")

    print(f"\n{'='*60}")
    print(f"  Total: {passed + failed} tests | Passed: {passed} | Failed: {failed}")
    print(f"{'='*60}")
    if failures:
        print("\nFailures:")
        for f in failures: print(f)
    sys.exit(0 if failed == 0 else 1)