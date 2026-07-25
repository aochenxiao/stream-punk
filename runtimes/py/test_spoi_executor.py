"""
SPOI Executor Python 测试套件

测试覆盖：
- 指令解析（Varint、SpoiInstruction）
- 路径导航（成员访问、数组索引、指针解引用）
- 写操作（SET/ADD/APPEND/REMOVE/INSERT/REPLACE/RESET/SETNULL）
- 读操作（PIPE/FILTER/SELECT/SORT/TAKE/DROP/TAKEWHILE/DROPWHILE/DISTINCT）
- 聚合（COUNT/ANY/ALL/FIND）
- 容器操作（KEYS/VALUES/JOIN）
- 完整管道（多指令链式）
- 错误处理
"""

import sys
import os
import struct
import unittest

# 添加父目录到路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from spoi_executor import (
    SpoiExecutor, SpoiInstruction, parse_spoi_stream,
    read_varint, write_varint,
    Op, ResultType, PATH_DEREF, PATH_MAPKEY
)

# =============================== 测试数据类 ===============================

class Item:
    def __init__(self, name="", price=0):
        self.name = name
        self.price = price

class Player:
    def __init__(self, name="", level=0, health=0, items=None):
        self.name = name
        self.level = level
        self.health = health
        self.items = items or []

# 类型注册表
TYPE_REGISTRY = {
    "Player": ["name", "level", "health", "items"],
    "Item":   ["name", "price"],
}

# =============================== 辅助函数（新格式：type_id 前缀） ===============================

from spoi_accessor import TypeId

def build_operand_int(value):
    """构造 [type_id(u32 LE) + value_bytes] 格式的 int 操作数"""
    return struct.pack('<I', TypeId.I32) + struct.pack('<i', value)

def build_operand_str(value):
    """构造 [type_id(u32 LE) + value_bytes] 格式的字符串操作数"""
    return struct.pack('<I', TypeId.STRING) + value.encode('utf-8')

def build_spoi_stream(instructions):
    """构建 SPOI 指令流二进制数据"""
    buf = bytearray()
    write_varint(buf, len(instructions))
    for inst in instructions:
        buf.append(inst.op)
        write_varint(buf, len(inst.path))
        for seg in inst.path:
            write_varint(buf, seg)
        write_varint(buf, len(inst.operand))
        buf.extend(inst.operand)
    return bytes(buf)

def set_inst(path, value_bytes):
    """创建 SET 指令"""
    return SpoiInstruction(op=Op.SET, path=list(path), operand=bytes(value_bytes))

def set_int(path, value):
    """创建 SET 指令（int 值，新格式）"""
    return set_inst(path, build_operand_int(value))

def set_str(path, value):
    """创建 SET 指令（string 值，新格式）"""
    return set_inst(path, build_operand_str(value))

def pipe_inst(path=None):
    """创建 PIPE 指令"""
    return SpoiInstruction(op=Op.PIPE, path=list(path or []))

def filter_inst(member_idx, cmp_op, value_bytes):
    """创建 FILTER 指令（新格式：memberIdx + cmpOp + value_len(varint) + value_bytes）"""
    buf = bytearray()
    buf.extend(struct.pack('<I', member_idx))
    buf.append(cmp_op)
    write_varint(buf, len(value_bytes))
    buf.extend(value_bytes)
    return SpoiInstruction(op=Op.FILTER, path=[], operand=bytes(buf))

def filter_gt(path, member_idx, value):
    """创建 FILTER > 指令"""
    return filter_inst(member_idx, 3, build_operand_int(value))

def filter_eq(path, member_idx, value):
    """创建 FILTER == 指令"""
    return filter_inst(member_idx, 0, build_operand_int(value))

def select_inst(path):
    """创建 SELECT 指令"""
    return SpoiInstruction(op=Op.SELECT, path=list(path), operand=b'')

def sort_inst(path=None):
    """创建 SORT 指令（path 作为指令的 path 字段）"""
    return SpoiInstruction(op=Op.SORT, path=list(path or []), operand=b'')

def take_inst(n):
    """创建 TAKE 指令"""
    return SpoiInstruction(op=Op.TAKE, path=[], operand=struct.pack('<I', n))

def drop_inst(n):
    """创建 DROP 指令"""
    return SpoiInstruction(op=Op.DROP, path=[], operand=struct.pack('<I', n))

def reverse_inst():
    """创建 REVERSE 指令"""
    return SpoiInstruction(op=Op.REVERSE, path=[], operand=b'')

def distinct_inst():
    """创建 DISTINCT 指令"""
    return SpoiInstruction(op=Op.DISTINCT, path=[], operand=b'')

def count_inst():
    """创建 COUNT 指令"""
    return SpoiInstruction(op=Op.COUNT, path=[], operand=b'')

def any_inst(member_idx, cmp_op, value_bytes):
    """创建 ANY 指令（新格式）"""
    buf = bytearray()
    buf.extend(struct.pack('<I', member_idx))
    buf.append(cmp_op)
    write_varint(buf, len(value_bytes))
    buf.extend(value_bytes)
    return SpoiInstruction(op=Op.ANY, path=[], operand=bytes(buf))

def all_inst(member_idx, cmp_op, value_bytes):
    """创建 ALL 指令（新格式）"""
    buf = bytearray()
    buf.extend(struct.pack('<I', member_idx))
    buf.append(cmp_op)
    write_varint(buf, len(value_bytes))
    buf.extend(value_bytes)
    return SpoiInstruction(op=Op.ALL, path=[], operand=bytes(buf))

def find_inst(member_idx, cmp_op, value_bytes):
    """创建 FIND 指令（新格式）"""
    buf = bytearray()
    buf.extend(struct.pack('<I', member_idx))
    buf.append(cmp_op)
    write_varint(buf, len(value_bytes))
    buf.extend(value_bytes)
    return SpoiInstruction(op=Op.FIND, path=[], operand=bytes(buf))

def append_inst(path, value_bytes):
    """创建 APPEND 指令"""
    return SpoiInstruction(op=Op.APPEND, path=list(path), operand=bytes(value_bytes))

def remove_inst(path, idx):
    """创建 REMOVE 指令"""
    return SpoiInstruction(op=Op.REMOVE, path=list(path), operand=struct.pack('<I', idx))

def exec_inst():
    """创建 EXEC 指令"""
    return SpoiInstruction(op=Op.EXEC, path=[], operand=b'')


# =============================== 测试用例 ===============================

class TestVarint(unittest.TestCase):
    """Varint 编解码测试"""

    def test_small_values(self):
        for v in [0, 1, 127, 128, 255, 256, 1000, 10000, 100000, 0xFFFFFFFF]:
            buf = bytearray()
            write_varint(buf, v)
            result, offset = read_varint(bytes(buf), 0)
            self.assertEqual(v, result)
            self.assertEqual(offset, len(buf))

    def test_roundtrip(self):
        buf = bytearray()
        write_varint(buf, 42)
        write_varint(buf, 0xFFFF)
        write_varint(buf, 0x12345678)
        data = bytes(buf)
        v1, off = read_varint(data, 0)
        v2, off = read_varint(data, off)
        v3, off = read_varint(data, off)
        self.assertEqual(v1, 42)
        self.assertEqual(v2, 0xFFFF)
        self.assertEqual(v3, 0x12345678)


class TestInstructionParsing(unittest.TestCase):
    """指令解析测试"""

    def test_parse_single(self):
        inst = SpoiInstruction(op=Op.SET, path=[0, 1], operand=b'\x2A\x00\x00\x00')
        data = build_spoi_stream([inst])
        parsed = parse_spoi_stream(data)
        self.assertEqual(len(parsed), 1)
        self.assertEqual(parsed[0].op, Op.SET)
        self.assertEqual(parsed[0].path, [0, 1])
        self.assertEqual(parsed[0].operand, b'\x2A\x00\x00\x00')

    def test_parse_multiple(self):
        insts = [
            SpoiInstruction(op=Op.PIPE, path=[], operand=b''),
            SpoiInstruction(op=Op.FILTER, path=[], operand=b'\x00\x00\x00\x00\x03\x0A\x00\x00\x00'),
            SpoiInstruction(op=Op.TAKE, path=[], operand=b'\x03\x00\x00\x00'),
            SpoiInstruction(op=Op.EXEC, path=[], operand=b''),
        ]
        data = build_spoi_stream(insts)
        parsed = parse_spoi_stream(data)
        self.assertEqual(len(parsed), 4)
        self.assertEqual(parsed[0].op, Op.PIPE)
        self.assertEqual(parsed[1].op, Op.FILTER)
        self.assertEqual(parsed[2].op, Op.TAKE)
        self.assertEqual(parsed[3].op, Op.EXEC)


class TestNavigation(unittest.TestCase):
    """路径导航测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)

    def test_member_access(self):
        player = Player(name="Alice", level=10, health=100)
        # path [0] = name
        result = self.executor._navigate(player, [0])
        self.assertEqual(result, "Alice")
        # path [1] = level
        result = self.executor._navigate(player, [1])
        self.assertEqual(result, 10)
        # path [2] = health
        result = self.executor._navigate(player, [2])
        self.assertEqual(result, 100)

    def test_array_index(self):
        players = [
            Player(name="Alice", level=10),
            Player(name="Bob", level=20),
        ]
        result = self.executor._navigate(players, [0])
        self.assertEqual(result.name, "Alice")
        result = self.executor._navigate(players, [1])
        self.assertEqual(result.name, "Bob")

    def test_nested_navigation(self):
        players = [Player(name="Alice", level=10, health=100)]
        # path [0, 0] = players[0].name
        result = self.executor._navigate(players, [0, 0])
        self.assertEqual(result, "Alice")

    def test_deref_pointer(self):
        class SpRef:
            def __init__(self, value):
                self.value = value
        ref = SpRef("hello")
        result = self.executor._nav_step(ref, PATH_DEREF)
        self.assertEqual(result, "hello")


class TestWriteOperations(unittest.TestCase):
    """写操作测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)

    def test_set_field(self):
        player = Player(name="Alice", level=10, health=100)
        data = build_spoi_stream([set_int([1], 20)])  # SET level = 20
        self.executor.execute(player, data)
        self.assertEqual(player.level, 20)

    def test_set_string(self):
        player = Player(name="Alice", level=10, health=100)
        data = build_spoi_stream([set_str([0], "Bob")])  # SET name = "Bob"
        self.executor.execute(player, data)
        self.assertEqual(player.name, "Bob")

    def test_add(self):
        player = Player(name="Alice", level=10, health=100)
        inst = SpoiInstruction(op=Op.ADD, path=[1], operand=build_operand_int(5))
        data = build_spoi_stream([inst])
        self.executor.execute(player, data)
        self.assertEqual(player.level, 15)

    def test_append(self):
        player = Player(name="Alice", level=10, health=100, items=[])
        inst = append_inst([3], build_operand_str('hello'))
        data = build_spoi_stream([inst])
        self.executor.execute(player, data)
        self.assertEqual(len(player.items), 1)
        self.assertEqual(player.items[0], 'hello')

    def test_remove(self):
        player = Player(name="Alice", level=10, health=100, items=["a", "b", "c"])
        data = build_spoi_stream([remove_inst([3], 1)])  # REMOVE items[1]
        self.executor.execute(player, data)
        self.assertEqual(player.items, ["a", "c"])

    def test_insert(self):
        player = Player(name="Alice", level=10, health=100, items=["a", "c"])
        operand = struct.pack('<I', 1) + build_operand_str('hello')
        inst = SpoiInstruction(op=Op.INSERT, path=[3], operand=operand)
        data = build_spoi_stream([inst])
        self.executor.execute(player, data)
        self.assertEqual(player.items, ["a", "hello", "c"])

    def test_replace(self):
        player = Player(name="Alice", level=10, health=100, items=["a", "b", "c"])
        operand = struct.pack('<I', 1) + build_operand_str('world')
        inst = SpoiInstruction(op=Op.REPLACE, path=[3], operand=operand)
        data = build_spoi_stream([inst])
        self.executor.execute(player, data)
        self.assertEqual(player.items, ["a", "world", "c"])

    def test_reset(self):
        player = Player(name="Alice", level=10, health=100)
        data = build_spoi_stream([SpoiInstruction(op=Op.RESET, path=[0], operand=b'')])
        self.executor.execute(player, data)
        self.assertIsNone(player.name)

    def test_setnull(self):
        player = Player(name="Alice", level=10, health=100)
        data = build_spoi_stream([SpoiInstruction(op=Op.SETNULL, path=[0], operand=b'')])
        self.executor.execute(player, data)
        self.assertIsNone(player.name)


class TestReadOperations(unittest.TestCase):
    """读操作测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)
        self.players = [
            Player(name="Alice", level=10, health=100),
            Player(name="Bob",   level=20, health=80),
            Player(name="Carol", level=15, health=120),
            Player(name="Dave",  level=20, health=60),
            Player(name="Eve",   level=10, health=90),
        ]

    def test_pipe(self):
        data = build_spoi_stream([pipe_inst(), exec_inst()])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["resultType"], ResultType.VECTOR)
        self.assertEqual(len(result["value"]), 5)

    def test_pipe_single(self):
        data = build_spoi_stream([pipe_inst(), exec_inst()])
        result = self.executor.execute(self.players[0], data)
        self.assertEqual(result["resultType"], ResultType.SINGLE)

    def test_filter_gt(self):
        # 过滤 level > 15
        data = build_spoi_stream([
            pipe_inst(),
            filter_gt([], 1, 15),  # memberIdx=1(level), cmpOp=3(gt), value=15
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        self.assertEqual(len(result["value"]), 2)  # Bob(20), Dave(20)
        names = [p.name for p in result["value"]]
        self.assertIn("Bob", names)
        self.assertIn("Dave", names)

    def test_select(self):
        data = build_spoi_stream([
            pipe_inst(),
            select_inst([0]),  # SELECT name
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["value"], ["Alice", "Bob", "Carol", "Dave", "Eve"])

    def test_take(self):
        data = build_spoi_stream([
            pipe_inst(),
            take_inst(3),
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        self.assertEqual(len(result["value"]), 3)

    def test_drop(self):
        data = build_spoi_stream([
            pipe_inst(),
            drop_inst(2),
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        self.assertEqual(len(result["value"]), 3)  # Carol, Dave, Eve

    def test_reverse(self):
        data = build_spoi_stream([
            pipe_inst(),
            reverse_inst(),
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["value"][0].name, "Eve")
        self.assertEqual(result["value"][-1].name, "Alice")

    def test_distinct(self):
        data = build_spoi_stream([
            pipe_inst(),
            select_inst([1]),  # SELECT level → [10, 20, 15, 20, 10]
            distinct_inst(),
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        self.assertEqual(sorted(result["value"]), [10, 15, 20])

    def test_takewhile(self):
        val_bytes = build_operand_int(10)
        buf = bytearray()
        buf.extend(struct.pack('<I', 1))  # memberIdx=1(level)
        buf.append(3)  # cmpOp=gt
        write_varint(buf, len(val_bytes))
        buf.extend(val_bytes)
        data = build_spoi_stream([
            pipe_inst(),
            SpoiInstruction(op=Op.TAKEWHILE, path=[], operand=bytes(buf)),
            # take while level > 10
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        # Alice level=10 → 不满足 >10, 立即停止 → 结果为空 → UNDEF
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_dropwhile(self):
        val_bytes = build_operand_int(10)
        buf = bytearray()
        buf.extend(struct.pack('<I', 1))  # memberIdx=1(level)
        buf.append(3)  # cmpOp=gt
        write_varint(buf, len(val_bytes))
        buf.extend(val_bytes)
        data = build_spoi_stream([
            pipe_inst(),
            SpoiInstruction(op=Op.DROPWHILE, path=[], operand=bytes(buf)),
            # drop while level > 10 — Alice level=10 不满足, 开始保留
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        self.assertEqual(len(result["value"]), 5)  # 全部保留


class TestAggregation(unittest.TestCase):
    """聚合操作测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)
        self.players = [
            Player(name="Alice", level=10, health=100),
            Player(name="Bob",   level=20, health=80),
            Player(name="Carol", level=15, health=120),
        ]

    def test_count(self):
        data = build_spoi_stream([pipe_inst(), count_inst(), exec_inst()])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["value"], 3)  # 单值不包裹在列表中

    def test_any_true(self):
        # any level > 15
        data = build_spoi_stream([pipe_inst(), any_inst(1, 3, build_operand_int(15)), exec_inst()])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["value"], True)

    def test_any_false(self):
        # any level > 100
        data = build_spoi_stream([pipe_inst(), any_inst(1, 3, build_operand_int(100)), exec_inst()])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["value"], False)

    def test_all_true(self):
        # all level >= 10
        data = build_spoi_stream([pipe_inst(), all_inst(1, 5, build_operand_int(10)), exec_inst()])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["value"], True)

    def test_all_false(self):
        # all level > 15
        data = build_spoi_stream([pipe_inst(), all_inst(1, 3, build_operand_int(15)), exec_inst()])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["value"], False)

    def test_find(self):
        # find level == 20
        data = build_spoi_stream([pipe_inst(), find_inst(1, 0, build_operand_int(20)), exec_inst()])
        result = self.executor.execute(self.players, data)
        # 单值返回，result["value"] 就是找到的 Player 对象
        self.assertEqual(result["value"].name, "Bob")

    def test_find_not_found(self):
        # find level == 999
        data = build_spoi_stream([pipe_inst(), find_inst(1, 0, build_operand_int(999)), exec_inst()])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)


class TestFullPipeline(unittest.TestCase):
    """完整管道测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)
        self.players = [
            Player(name="Alice", level=10, health=100),
            Player(name="Bob",   level=20, health=80),
            Player(name="Carol", level=15, health=120),
            Player(name="Dave",  level=20, health=60),
            Player(name="Eve",   level=10, health=90),
            Player(name="Frank", level=25, health=70),
            Player(name="Grace", level=30, health=50),
        ]

    def test_filter_take_sort(self):
        """过滤 level > 10 → 取前 3 → 按 name 排序"""
        data = build_spoi_stream([
            pipe_inst(),
            filter_gt([], 1, 10),  # level > 10
            take_inst(3),
            sort_inst([0]),  # sort by name (index 0)
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        self.assertEqual(len(result["value"]), 3)
        # 过滤后: Bob(20), Carol(15), Dave(20), Frank(25), Grace(30)
        # 取前 3: Bob, Carol, Dave
        # 按 name 排序: Bob, Carol, Dave
        names = [p.name for p in result["value"]]
        self.assertEqual(names, ["Bob", "Carol", "Dave"])

    def test_filter_select_count(self):
        """过滤 level > 15 → 选择 name → 计数"""
        data = build_spoi_stream([
            pipe_inst(),
            filter_gt([], 1, 15),  # level > 15
            select_inst([0]),       # SELECT name
            count_inst(),
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["value"], 4)  # Bob, Dave, Frank, Grace → count=4

    def test_write_then_read(self):
        """先写再读"""
        player = Player(name="Alice", level=10, health=100, items=[])
        data = build_spoi_stream([
            set_int([1], 99),       # SET level = 99
            set_str([0], "Zelda"),  # SET name = "Zelda"
            pipe_inst([]),           # PIPE root
            exec_inst(),
        ])
        result = self.executor.execute(player, data)
        self.assertEqual(player.level, 99)
        self.assertEqual(player.name, "Zelda")
        # 单值返回，result["value"] 就是 Player 对象
        self.assertEqual(result["value"].name, "Zelda")


class TestErrorHandling(unittest.TestCase):
    """错误处理测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)

    def test_unknown_opcode(self):
        inst = SpoiInstruction(op=0xFF, path=[], operand=b'')
        data = build_spoi_stream([inst])
        with self.assertRaises(ValueError):
            self.executor.execute(Player(), data)

    def test_invalid_path(self):
        player = Player(name="Alice", level=10, health=100)
        # path index 99 超出范围
        data = build_spoi_stream([set_int([99], 42)])
        with self.assertRaises((ValueError, IndexError, KeyError)):
            self.executor.execute(player, data)


if __name__ == '__main__':
    unittest.main(verbosity=2)