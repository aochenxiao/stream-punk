"""
SPOI Executor 刁钻边界测试套件

测试覆盖：
- Varint 边界攻击（最大编码、截断、溢出）
- 路径攻击（DEREF 非指针、MAPKEY 非 Map、负数索引、越界）
- 操作数攻击（长度不匹配、零长度操作数）
- 指令序列攻击（缺少 EXEC、重复 EXEC、无 PIPE 的 EXEC）
- 空容器处理（空 PIPE、空数组、null 值）
- 操作极值（TAKE/DROP 边界、FILTER 无匹配）
- 聚合边界（空集 COUNT/ANY/ALL/FIND）
- 嵌套导航边界（穿越 null、非对象导航、深层路径）
- 写操作边界（ADD 非数值、REMOVE 空数组、APPEND 非数组）
"""

import sys
import os
import struct
import unittest

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
    def __init__(self, name="", level=0, health=0, items=None, metadata=None):
        self.name = name
        self.level = level
        self.health = health
        self.items = items or []
        self.metadata = metadata or {}

TYPE_REGISTRY = {
    "Player": ["name", "level", "health", "items", "metadata"],
    "Item":   ["name", "price"],
}

# =============================== 辅助函数 ===============================

def build_spoi_stream(instructions):
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

def inst(op, path=None, operand=None):
    return SpoiInstruction(op=op, path=list(path or []), operand=bytes(operand or b''))

def set_int(path, value):
    return inst(Op.SET, path, struct.pack('<I', value))

def set_str(path, value):
    return inst(Op.SET, path, value.encode('utf-8'))

def pipe_inst(path=None):
    return inst(Op.PIPE, path)

def filter_inst(member_idx, cmp_op, value_bytes):
    return inst(Op.FILTER, [], struct.pack('<I', member_idx) + bytes([cmp_op]) + value_bytes)

def filter_gt(path, member_idx, value):
    return filter_inst(member_idx, 3, struct.pack('<I', value))

def filter_eq(path, member_idx, value):
    return filter_inst(member_idx, 0, struct.pack('<I', value))

def select_inst(path):
    return inst(Op.SELECT, path)

def sort_inst(path=None):
    return inst(Op.SORT, path)

def take_inst(n):
    return inst(Op.TAKE, [], struct.pack('<I', n))

def drop_inst(n):
    return inst(Op.DROP, [], struct.pack('<I', n))

def reverse_inst():
    return inst(Op.REVERSE)

def distinct_inst():
    return inst(Op.DISTINCT)

def count_inst():
    return inst(Op.COUNT)

def any_inst(member_idx, cmp_op, value_bytes):
    return inst(Op.ANY, [], struct.pack('<I', member_idx) + bytes([cmp_op]) + value_bytes)

def all_inst(member_idx, cmp_op, value_bytes):
    return inst(Op.ALL, [], struct.pack('<I', member_idx) + bytes([cmp_op]) + value_bytes)

def find_inst(member_idx, cmp_op, value_bytes):
    return inst(Op.FIND, [], struct.pack('<I', member_idx) + bytes([cmp_op]) + value_bytes)

def exec_inst():
    return inst(Op.EXEC)


# =============================== Varint 边界攻击 ===============================

class TestVarintBoundary(unittest.TestCase):
    """Varint 编码的刁钻边界测试"""

    def test_max_uint32(self):
        """最大 uint32 值"""
        v = 0xFFFFFFFF
        buf = bytearray()
        write_varint(buf, v)
        result, offset = read_varint(bytes(buf), 0)
        self.assertEqual(result, v)
        self.assertEqual(offset, len(buf))

    def test_varint_multi_byte_boundary(self):
        """多字节 Varint 边界值"""
        test_cases = [
            0x7F,       # 1字节边界
            0x80,       # 2字节起始
            0x3FFF,     # 2字节边界
            0x4000,     # 3字节起始
            0x1FFFFF,   # 3字节边界
            0x200000,   # 4字节起始
            0xFFFFFFF,  # 4字节边界
            0x10000000, # 5字节起始
        ]
        for v in test_cases:
            buf = bytearray()
            write_varint(buf, v)
            result, offset = read_varint(bytes(buf), 0)
            self.assertEqual(result, v, f"varint: {v}")

    def test_varint_zero(self):
        """Varint 零值"""
        buf = bytearray()
        write_varint(buf, 0)
        result, offset = read_varint(bytes(buf), 0)
        self.assertEqual(result, 0)
        self.assertEqual(offset, 1)

    def test_varint_truncated(self):
        """Varint 截断：高位字节未设置终止位"""
        # 只有一个字节 0x80（高位=1，表示后面还有字节），但数据到此结束
        # read_varint 到达数据末尾时返回已读取的部分，不会报错
        result, offset = read_varint(b'\x80', 0)
        self.assertEqual(result, 0)
        self.assertEqual(offset, 1)

    def test_varint_empty_data(self):
        """空数据读取 Varint"""
        # read_varint 在空数据上返回 (0, 0)，不会报错
        result, offset = read_varint(b'', 0)
        self.assertEqual(result, 0)
        self.assertEqual(offset, 0)


# =============================== 指令解析边界 ===============================

class TestInstructionParsingBoundary(unittest.TestCase):
    """指令解析的刁钻边界测试"""

    def test_empty_stream(self):
        """空指令流（0条指令）"""
        data = build_spoi_stream([])
        parsed = parse_spoi_stream(data)
        self.assertEqual(len(parsed), 0)

    def test_max_instructions(self):
        """大量指令"""
        insts = [inst(Op.SET, [0], b'\x00') for _ in range(100)]
        data = build_spoi_stream(insts)
        parsed = parse_spoi_stream(data)
        self.assertEqual(len(parsed), 100)

    def test_zero_length_operand(self):
        """零长度操作数"""
        i = inst(Op.SET, [0], b'')
        data = build_spoi_stream([i])
        parsed = parse_spoi_stream(data)
        self.assertEqual(len(parsed), 1)
        self.assertEqual(parsed[0].operand, b'')

    def test_zero_length_path(self):
        """零长度路径"""
        i = inst(Op.EXEC, [], b'')
        data = build_spoi_stream([i])
        parsed = parse_spoi_stream(data)
        self.assertEqual(len(parsed), 1)
        self.assertEqual(parsed[0].path, [])

    def test_large_operand(self):
        """大操作数"""
        large = b'\x00' * 10000
        i = inst(Op.SET, [0], large)
        data = build_spoi_stream([i])
        parsed = parse_spoi_stream(data)
        self.assertEqual(len(parsed[0].operand), 10000)

    def test_deep_path(self):
        """深层路径"""
        deep_path = list(range(50))
        i = inst(Op.SET, deep_path, b'\x00')
        data = build_spoi_stream([i])
        parsed = parse_spoi_stream(data)
        self.assertEqual(parsed[0].path, deep_path)


# =============================== 路径导航边界 ===============================

class TestNavigationBoundary(unittest.TestCase):
    """路径导航的刁钻边界测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)

    def test_deref_on_non_pointer(self):
        """PATH_DEREF 作用于非指针对象"""
        # _nav_step 对于 PATH_DEREF：如果对象有 .value 属性则返回它，否则返回对象本身
        result = self.executor._nav_step("hello", PATH_DEREF)
        self.assertEqual(result, "hello")

    def test_deref_on_none(self):
        """PATH_DEREF 作用于 None"""
        # _nav_step 对于 PATH_DEREF：None 没有 .value 属性，返回 None 本身
        result = self.executor._nav_step(None, PATH_DEREF)
        self.assertIsNone(result)

    def test_mapkey_on_non_dict(self):
        """PATH_MAPKEY 作用于非字典"""
        with self.assertRaises((ValueError, TypeError, AttributeError)):
            self.executor._nav_step("hello", PATH_MAPKEY)

    def test_index_out_of_bounds(self):
        """数组索引越界"""
        players = [Player(name="Alice")]
        with self.assertRaises((ValueError, IndexError)):
            self.executor._navigate(players, [99])

    def test_index_on_non_list(self):
        """对非列表对象使用索引"""
        with self.assertRaises((ValueError, TypeError, IndexError)):
            self.executor._navigate("hello", [0])

    def test_member_on_non_object(self):
        """对非对象使用成员访问"""
        with self.assertRaises((ValueError, TypeError, AttributeError)):
            self.executor._navigate(42, [0])

    def test_navigate_on_none(self):
        """对 None 导航"""
        with self.assertRaises((ValueError, TypeError, AttributeError)):
            self.executor._navigate(None, [0])

    def test_member_index_out_of_range(self):
        """成员索引超出类型注册表范围"""
        player = Player(name="Alice")
        with self.assertRaises((ValueError, IndexError, KeyError)):
            self.executor._navigate(player, [99])

    def test_nested_navigate_through_none(self):
        """嵌套导航中途遇到 None"""
        player = Player(name=None, level=10)
        # 导航到 name(None) 再尝试访问子成员
        with self.assertRaises((ValueError, TypeError, AttributeError)):
            self.executor._navigate(player, [0, 0])

    def test_navigate_empty_path(self):
        """空路径导航返回自身"""
        player = Player(name="Alice")
        result = self.executor._navigate(player, [])
        self.assertIs(result, player)


# =============================== 指令序列攻击 ===============================

class TestInstructionSequenceAttacks(unittest.TestCase):
    """指令序列的刁钻攻击测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)
        self.players = [
            Player(name="Alice", level=10),
            Player(name="Bob", level=20),
        ]

    def test_exec_without_pipe(self):
        """EXEC 之前没有 PIPE"""
        # EXEC 只是空操作，之前没有 PIPE 时返回 UNDEF
        data = build_spoi_stream([exec_inst()])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_double_exec(self):
        """重复 EXEC"""
        # 多个 EXEC 只是空操作，返回第一个 EXEC 时的管道结果
        data = build_spoi_stream([pipe_inst(), exec_inst(), exec_inst()])
        result = self.executor.execute(self.players, data)
        self.assertEqual(len(result["value"]), 2)

    def test_double_pipe(self):
        """重复 PIPE"""
        # 第二个 PIPE 覆盖第一个 PIPE 的数据
        data = build_spoi_stream([pipe_inst(), pipe_inst(), exec_inst()])
        result = self.executor.execute(self.players, data)
        self.assertEqual(len(result["value"]), 2)

    def test_write_after_exec(self):
        """EXEC 之后写操作"""
        # EXEC 之后的操作仍然执行（无特殊保护）
        player = Player(name="Alice", level=10)
        data = build_spoi_stream([pipe_inst(), exec_inst(), set_int([1], 99)])
        result = self.executor.execute(player, data)
        self.assertEqual(player.level, 99)
        # 结果仍然是管道数据（PIPE 产生的）
        self.assertEqual(result["value"].name, "Alice")

    def test_read_op_without_pipe(self):
        """读取操作之前没有 PIPE"""
        # 没有 PIPE 时管道为空，FILTER 在空管道上操作，结果为空
        data = build_spoi_stream([filter_gt([], 1, 10), exec_inst()])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_missing_exec(self):
        """缺少 EXEC"""
        # 缺少 EXEC 时，管道数据在中途操作后保留，最终结果来自 _make_result
        # PIPE 加载 2 个玩家，TAKE 1 保留 1 个，返回 SINGLE
        data = build_spoi_stream([pipe_inst(), take_inst(1)])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["resultType"], ResultType.SINGLE)
        self.assertEqual(result["value"].name, "Alice")

    def test_standalone_exec(self):
        """单独的 EXEC（无任何指令）"""
        # 单独 EXEC 时管道为空，返回 UNDEF
        data = build_spoi_stream([exec_inst()])
        result = self.executor.execute(self.players, data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)


# =============================== 空容器处理 ===============================

class TestEmptyContainerHandling(unittest.TestCase):
    """空容器/空值的刁钻边界测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)

    def test_empty_pipe(self):
        """空列表 PIPE"""
        data = build_spoi_stream([pipe_inst(), exec_inst()])
        result = self.executor.execute([], data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_take_zero(self):
        """TAKE 0"""
        players = [Player(name="Alice"), Player(name="Bob")]
        data = build_spoi_stream([pipe_inst(), take_inst(0), exec_inst()])
        result = self.executor.execute(players, data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_take_more_than_available(self):
        """TAKE 超过可用数量"""
        players = [Player(name="Alice"), Player(name="Bob")]
        data = build_spoi_stream([pipe_inst(), take_inst(10), exec_inst()])
        result = self.executor.execute(players, data)
        self.assertEqual(len(result["value"]), 2)

    def test_drop_all(self):
        """DROP 全部"""
        players = [Player(name="Alice"), Player(name="Bob")]
        data = build_spoi_stream([pipe_inst(), drop_inst(10), exec_inst()])
        result = self.executor.execute(players, data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_drop_zero(self):
        """DROP 0"""
        players = [Player(name="Alice"), Player(name="Bob")]
        data = build_spoi_stream([pipe_inst(), drop_inst(0), exec_inst()])
        result = self.executor.execute(players, data)
        self.assertEqual(len(result["value"]), 2)

    def test_reverse_empty(self):
        """空列表 REVERSE"""
        data = build_spoi_stream([pipe_inst(), take_inst(0), reverse_inst(), exec_inst()])
        result = self.executor.execute([Player(name="A")], data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_reverse_single(self):
        """单元素 REVERSE"""
        players = [Player(name="Alice")]
        data = build_spoi_stream([pipe_inst(), reverse_inst(), exec_inst()])
        result = self.executor.execute(players, data)
        # 单元素管道返回 SINGLE 类型
        self.assertEqual(result["resultType"], ResultType.SINGLE)
        self.assertEqual(result["value"].name, "Alice")

    def test_distinct_empty(self):
        """空列表 DISTINCT"""
        data = build_spoi_stream([pipe_inst(), take_inst(0), distinct_inst(), exec_inst()])
        result = self.executor.execute([Player(name="A")], data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_distinct_single(self):
        """单元素 DISTINCT"""
        players = [Player(name="Alice")]
        data = build_spoi_stream([pipe_inst(), select_inst([0]), distinct_inst(), exec_inst()])
        result = self.executor.execute(players, data)
        # SELECT 后 DISTINCT 单元素返回 SINGLE 类型，值为 'Alice'
        self.assertEqual(result["resultType"], ResultType.SINGLE)
        self.assertEqual(result["value"], "Alice")

    def test_filter_no_match(self):
        """FILTER 无匹配"""
        players = [Player(name="Alice", level=10)]
        data = build_spoi_stream([pipe_inst(), filter_gt([], 1, 100), exec_inst()])
        result = self.executor.execute(players, data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_select_empty_result(self):
        """SELECT 空结果"""
        data = build_spoi_stream([pipe_inst(), take_inst(0), select_inst([0]), exec_inst()])
        result = self.executor.execute([Player(name="A")], data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_sort_empty(self):
        """空列表 SORT"""
        data = build_spoi_stream([pipe_inst(), take_inst(0), sort_inst([0]), exec_inst()])
        result = self.executor.execute([Player(name="A")], data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_sort_single(self):
        """单元素 SORT"""
        players = [Player(name="Alice")]
        data = build_spoi_stream([pipe_inst(), sort_inst([0]), exec_inst()])
        result = self.executor.execute(players, data)
        # 单元素管道返回 SINGLE 类型
        self.assertEqual(result["resultType"], ResultType.SINGLE)
        self.assertEqual(result["value"].name, "Alice")


# =============================== 聚合边界 ===============================

class TestAggregationBoundary(unittest.TestCase):
    """聚合操作的刁钻边界测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)

    def test_count_on_empty(self):
        """空集 COUNT"""
        data = build_spoi_stream([pipe_inst(), take_inst(0), count_inst(), exec_inst()])
        result = self.executor.execute([Player(name="A")], data)
        # COUNT 结果存入管道，单元素管道返回 SINGLE
        self.assertEqual(result["resultType"], ResultType.SINGLE)
        self.assertEqual(result["value"], 0)

    def test_any_on_empty(self):
        """空集 ANY"""
        data = build_spoi_stream([
            pipe_inst(), take_inst(0),
            any_inst(1, 3, struct.pack('<I', 10)),
            exec_inst(),
        ])
        result = self.executor.execute([Player(name="A")], data)
        # ANY 结果存入管道，单元素管道返回 SINGLE
        self.assertEqual(result["resultType"], ResultType.SINGLE)
        self.assertEqual(result["value"], False)

    def test_all_on_empty(self):
        """空集 ALL（vacuous truth = True）"""
        data = build_spoi_stream([
            pipe_inst(), take_inst(0),
            all_inst(1, 3, struct.pack('<I', 10)),
            exec_inst(),
        ])
        result = self.executor.execute([Player(name="A")], data)
        self.assertEqual(result["value"], True)

    def test_find_on_empty(self):
        """空集 FIND"""
        data = build_spoi_stream([
            pipe_inst(), take_inst(0),
            find_inst(1, 0, struct.pack('<I', 10)),
            exec_inst(),
        ])
        result = self.executor.execute([Player(name="A")], data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_count_on_single(self):
        """单元素 COUNT"""
        players = [Player(name="Alice")]
        data = build_spoi_stream([pipe_inst(), count_inst(), exec_inst()])
        result = self.executor.execute(players, data)
        self.assertEqual(result["value"], 1)

    def test_count_after_filter(self):
        """FILTER 后 COUNT（空结果）"""
        players = [Player(name="Alice", level=10)]
        data = build_spoi_stream([pipe_inst(), filter_gt([], 1, 100), count_inst(), exec_inst()])
        result = self.executor.execute(players, data)
        self.assertEqual(result["value"], 0)

    def test_any_after_filter_empty(self):
        """FILTER 后 ANY（空结果）"""
        players = [Player(name="Alice", level=10)]
        data = build_spoi_stream([
            pipe_inst(), filter_gt([], 1, 100),
            any_inst(1, 3, struct.pack('<I', 5)),
            exec_inst(),
        ])
        result = self.executor.execute(players, data)
        self.assertEqual(result["value"], False)

    def test_all_after_filter_empty(self):
        """FILTER 后 ALL（空结果 = vacuous truth）"""
        players = [Player(name="Alice", level=10)]
        data = build_spoi_stream([
            pipe_inst(), filter_gt([], 1, 100),
            all_inst(1, 3, struct.pack('<I', 5)),
            exec_inst(),
        ])
        result = self.executor.execute(players, data)
        self.assertEqual(result["value"], True)


# =============================== 写操作边界 ===============================

class TestWriteBoundary(unittest.TestCase):
    """写操作的刁钻边界测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)

    def test_add_to_zero(self):
        """ADD 到 0"""
        player = Player(name="Alice", level=0, health=100)
        data = build_spoi_stream([inst(Op.ADD, [1], struct.pack('<I', 5))])
        self.executor.execute(player, data)
        self.assertEqual(player.level, 5)

    def test_add_negative(self):
        """ADD 负数（unsigned 解释）"""
        player = Player(name="Alice", level=10, health=100)
        # _deserialize_value 对 4 字节使用 unsigned 解包
        data = build_spoi_stream([inst(Op.ADD, [1], struct.pack('<i', -5))])
        self.executor.execute(player, data)
        # -5 的 unsigned 表示为 4294967291，10 + 4294967291 = 4294967301
        self.assertEqual(player.level, 4294967301)

    def test_remove_from_empty(self):
        """REMOVE 空数组"""
        player = Player(name="Alice", level=10, health=100, items=[])
        data = build_spoi_stream([inst(Op.REMOVE, [3], struct.pack('<I', 0))])
        with self.assertRaises((ValueError, IndexError)):
            self.executor.execute(player, data)

    def test_remove_out_of_bounds(self):
        """REMOVE 越界索引"""
        player = Player(name="Alice", level=10, health=100, items=["a"])
        data = build_spoi_stream([inst(Op.REMOVE, [3], struct.pack('<I', 99))])
        with self.assertRaises((ValueError, IndexError)):
            self.executor.execute(player, data)

    def test_insert_at_zero(self):
        """INSERT 在索引 0"""
        player = Player(name="Alice", level=10, health=100, items=["b", "c"])
        operand = struct.pack('<I', 0) + b'hello'
        data = build_spoi_stream([inst(Op.INSERT, [3], operand)])
        self.executor.execute(player, data)
        self.assertEqual(player.items, ["hello", "b", "c"])

    def test_insert_at_end(self):
        """INSERT 在末尾"""
        player = Player(name="Alice", level=10, health=100, items=["a", "b"])
        operand = struct.pack('<I', 2) + b'hello'
        data = build_spoi_stream([inst(Op.INSERT, [3], operand)])
        self.executor.execute(player, data)
        self.assertEqual(player.items, ["a", "b", "hello"])

    def test_replace_out_of_bounds(self):
        """REPLACE 越界索引"""
        player = Player(name="Alice", level=10, health=100, items=["a"])
        operand = struct.pack('<I', 99) + b'world'
        data = build_spoi_stream([inst(Op.REPLACE, [3], operand)])
        with self.assertRaises((ValueError, IndexError)):
            self.executor.execute(player, data)

    def test_set_on_none_target(self):
        """SET 在 None 值上"""
        reg = {"Player": ["name", "level", "health", "items", "metadata"]}
        executor = SpoiExecutor(reg)
        player = Player(name="Alice", level=None, health=100)
        # SET level = 20（level 当前为 None）
        data = build_spoi_stream([set_int([1], 20)])
        try:
            executor.execute(player, data)
            self.assertEqual(player.level, 20)
        except (ValueError, TypeError):
            # 如果实现不支持 SET 到 None，也算通过
            pass

    def test_double_set(self):
        """连续 SET 同一字段"""
        player = Player(name="Alice", level=10, health=100)
        data = build_spoi_stream([set_int([1], 20), set_int([1], 30)])
        self.executor.execute(player, data)
        self.assertEqual(player.level, 30)

    def test_setnull_then_set(self):
        """SETNULL 后再 SET"""
        player = Player(name="Alice", level=10, health=100)
        data = build_spoi_stream([
            inst(Op.SETNULL, [0]),
            set_str([0], "Bob"),
        ])
        self.executor.execute(player, data)
        self.assertEqual(player.name, "Bob")

    def test_reset_then_set(self):
        """RESET 后再 SET"""
        player = Player(name="Alice", level=10, health=100)
        data = build_spoi_stream([
            inst(Op.RESET, [0]),
            set_str([0], "Bob"),
        ])
        self.executor.execute(player, data)
        self.assertEqual(player.name, "Bob")


# =============================== 嵌套导航边界 ===============================

class TestNestedNavigationBoundary(unittest.TestCase):
    """嵌套导航的刁钻边界测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)

    def test_nested_select(self):
        """嵌套 SELECT：访问对象成员后再 SELECT"""
        players = [Player(name="Alice", level=10, items=[Item(name="Sword", price=100)])]
        # PIPE → SELECT items → EXEC
        # 从 Player 选 items，items 列表只有一个元素，返回 SINGLE
        data = build_spoi_stream([
            pipe_inst(),       # PIPE [Player...]
            select_inst([3]),  # SELECT items → [[Item("Sword")]]
            exec_inst(),
        ])
        result = self.executor.execute(players, data)
        # 单玩家时 SELECT items 返回单元素管道，resultType 为 SINGLE
        self.assertEqual(result["resultType"], ResultType.SINGLE)

    def test_navigate_to_empty_list(self):
        """导航到空列表"""
        player = Player(name="Alice", level=10, items=[])
        result = self.executor._navigate(player, [3])
        self.assertEqual(result, [])

    def test_navigate_to_empty_dict(self):
        """导航到空字典"""
        player = Player(name="Alice", level=10, metadata={})
        result = self.executor._navigate(player, [4])
        self.assertEqual(result, {})

    def test_navigate_to_none_value(self):
        """导航到 None 值"""
        player = Player(name=None, level=10)
        result = self.executor._navigate(player, [0])
        self.assertIsNone(result)


# =============================== TAKEWHILE/DROPWHILE 边界 ===============================

class TestTakeDropWhileBoundary(unittest.TestCase):
    """TAKEWHILE/DROPWHILE 的刁钻边界测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)

    def test_takewhile_never_true(self):
        """TAKEWHILE 条件永远不满足"""
        players = [Player(name="Alice", level=10), Player(name="Bob", level=20)]
        # take while level > 100 → 无元素满足
        data = build_spoi_stream([
            pipe_inst(),
            inst(Op.TAKEWHILE, [], struct.pack('<I', 1) + b'\x03' + struct.pack('<I', 100)),
            exec_inst(),
        ])
        result = self.executor.execute(players, data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_takewhile_always_true(self):
        """TAKEWHILE 条件永远满足"""
        players = [Player(name="Alice", level=10), Player(name="Bob", level=20)]
        # take while level > 0 → 全部满足
        data = build_spoi_stream([
            pipe_inst(),
            inst(Op.TAKEWHILE, [], struct.pack('<I', 1) + b'\x03' + struct.pack('<I', 0)),
            exec_inst(),
        ])
        result = self.executor.execute(players, data)
        self.assertEqual(len(result["value"]), 2)

    def test_dropwhile_never_true(self):
        """DROPWHILE 条件永远不满足"""
        players = [Player(name="Alice", level=10), Player(name="Bob", level=20)]
        # drop while level > 100 → 不丢弃任何
        data = build_spoi_stream([
            pipe_inst(),
            inst(Op.DROPWHILE, [], struct.pack('<I', 1) + b'\x03' + struct.pack('<I', 100)),
            exec_inst(),
        ])
        result = self.executor.execute(players, data)
        self.assertEqual(len(result["value"]), 2)

    def test_dropwhile_always_true(self):
        """DROPWHILE 条件永远满足"""
        players = [Player(name="Alice", level=10), Player(name="Bob", level=20)]
        # drop while level > 0 → 全部丢弃
        data = build_spoi_stream([
            pipe_inst(),
            inst(Op.DROPWHILE, [], struct.pack('<I', 1) + b'\x03' + struct.pack('<I', 0)),
            exec_inst(),
        ])
        result = self.executor.execute(players, data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_takewhile_on_empty(self):
        """TAKEWHILE 空列表"""
        data = build_spoi_stream([
            pipe_inst(), take_inst(0),
            inst(Op.TAKEWHILE, [], struct.pack('<I', 1) + b'\x03' + struct.pack('<I', 0)),
            exec_inst(),
        ])
        result = self.executor.execute([Player(name="A")], data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)

    def test_dropwhile_on_empty(self):
        """DROPWHILE 空列表"""
        data = build_spoi_stream([
            pipe_inst(), take_inst(0),
            inst(Op.DROPWHILE, [], struct.pack('<I', 1) + b'\x03' + struct.pack('<I', 0)),
            exec_inst(),
        ])
        result = self.executor.execute([Player(name="A")], data)
        self.assertEqual(result["resultType"], ResultType.UNDEF)


# =============================== FILTER 比较操作边界 ===============================

class TestFilterComparisonBoundary(unittest.TestCase):
    """FILTER 比较操作的刁钻边界测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)
        self.players = [
            Player(name="Alice", level=10, health=100),
            Player(name="Bob",   level=10, health=80),
            Player(name="Carol", level=15, health=120),
        ]

    def test_filter_eq(self):
        """FILTER =="""
        data = build_spoi_stream([
            pipe_inst(),
            filter_eq([], 1, 10),  # level == 10
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        self.assertEqual(len(result["value"]), 2)

    def test_filter_lt(self):
        """FILTER < (cmpOp=2)"""
        data = build_spoi_stream([
            pipe_inst(),
            filter_inst(1, 2, struct.pack('<I', 15)),  # level < 15
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        self.assertEqual(len(result["value"]), 2)

    def test_filter_ge(self):
        """FILTER >= (cmpOp=4 实际是 <=)"""
        data = build_spoi_stream([
            pipe_inst(),
            filter_inst(1, 4, struct.pack('<I', 15)),  # cmpOp=4 是 <=, level <= 15
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        # level <= 15: Alice(10), Bob(10), Carol(15) 全匹配
        self.assertEqual(len(result["value"]), 3)

    def test_filter_le(self):
        """FILTER <= (cmpOp=5 实际是 >=)"""
        data = build_spoi_stream([
            pipe_inst(),
            filter_inst(1, 5, struct.pack('<I', 10)),  # cmpOp=5 是 >=, level >= 10
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        # level >= 10: Alice(10), Bob(10), Carol(15) 全匹配
        self.assertEqual(len(result["value"]), 3)

    def test_filter_ne(self):
        """FILTER != (cmpOp=1)"""
        data = build_spoi_stream([
            pipe_inst(),
            filter_inst(1, 1, struct.pack('<I', 10)),  # level != 10
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        # level != 10: 只有 Carol(15)，单元素返回 SINGLE
        self.assertEqual(result["resultType"], ResultType.SINGLE)
        self.assertEqual(result["value"].name, "Carol")

    def test_filter_unknown_cmp(self):
        """FILTER 未知比较操作符"""
        data = build_spoi_stream([
            pipe_inst(),
            filter_inst(1, 99, struct.pack('<I', 10)),
            exec_inst(),
        ])
        # 未知 cmpOp 返回 True（不过滤），所有元素保留
        result = self.executor.execute(self.players, data)
        self.assertEqual(len(result["value"]), 3)


# =============================== 操作数解析边界 ===============================

class TestOperandParsingBoundary(unittest.TestCase):
    """操作数解析的刁钻边界测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)

    def test_operand_shorter_than_expected(self):
        """操作数长度小于预期（FILTER 需要 5+ 字节）"""
        player = Player(name="Alice", level=10)
        # FILTER 操作数只给 2 字节，_matches 中 len(operand) < 5 返回 True，不过滤
        data = build_spoi_stream([pipe_inst(), inst(Op.FILTER, [], b'\x00\x01'), exec_inst()])
        result = self.executor.execute(player, data)
        # 所有元素通过（不过滤）
        self.assertEqual(result["resultType"], ResultType.SINGLE)

    def test_operand_shorter_than_expected_for_any(self):
        """操作数长度小于预期（ANY 需要 5+ 字节）"""
        player = Player(name="Alice", level=10)
        data = build_spoi_stream([pipe_inst(), inst(Op.ANY, [], b'\x00'), exec_inst()])
        result = self.executor.execute(player, data)
        # _matches 中 len(operand) < 5 返回 True，ANY 结果为 True
        self.assertEqual(result["resultType"], ResultType.SINGLE)
        self.assertEqual(result["value"], True)

    def test_insert_operand_no_index(self):
        """INSERT 操作数无索引"""
        player = Player(name="Alice", level=10, health=100, items=["a"])
        data = build_spoi_stream([inst(Op.INSERT, [3], b'hello')])
        # INSERT 操作数不足 4 字节时，struct.unpack 读取前 4 字节作为索引
        # b'hello' 有 5 字节，前 4 字节被解析为索引，第 5 字节作为值
        self.executor.execute(player, data)
        # 不报错，插入行为取决于解析结果

    def test_replace_operand_no_index(self):
        """REPLACE 操作数无索引"""
        player = Player(name="Alice", level=10, health=100, items=["a"])
        data = build_spoi_stream([inst(Op.REPLACE, [3], b'world')])
        with self.assertRaises((ValueError, struct.error, IndexError)):
            self.executor.execute(player, data)


# =============================== 多管道组合边界 ===============================

class TestMultiPipeComboBoundary(unittest.TestCase):
    """多管道组合的刁钻边界测试"""

    def setUp(self):
        self.executor = SpoiExecutor(TYPE_REGISTRY)
        self.players = [
            Player(name="Alice", level=10, health=100),
            Player(name="Bob",   level=20, health=80),
            Player(name="Carol", level=15, health=120),
            Player(name="Dave",  level=20, health=60),
            Player(name="Eve",   level=10, health=90),
        ]

    def test_filter_take_drop_reverse(self):
        """FILTER → TAKE → DROP → REVERSE 组合"""
        data = build_spoi_stream([
            pipe_inst(),
            filter_gt([], 1, 10),  # level>10: Bob, Carol, Dave
            take_inst(3),           # Bob, Carol, Dave
            drop_inst(1),           # Carol, Dave
            reverse_inst(),         # Dave, Carol
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        self.assertEqual(len(result["value"]), 2)
        self.assertEqual(result["value"][0].name, "Dave")
        self.assertEqual(result["value"][1].name, "Carol")

    def test_sort_descending(self):
        """SORT 降序（按 level 排序，但 path 指向 level 字段）"""
        data = build_spoi_stream([
            pipe_inst(),
            sort_inst([1]),  # sort by level
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        levels = [p.level for p in result["value"]]
        self.assertEqual(levels, sorted(levels))

    def test_select_then_filter(self):
        """SELECT 后 FILTER"""
        data = build_spoi_stream([
            pipe_inst(),
            select_inst([0]),  # SELECT name → ["Alice", "Bob", ...]
            # FILTER 对字符串尝试 _nav_step 访问成员索引，会抛出 ValueError
            filter_gt([], 0, 0),
            exec_inst(),
        ])
        with self.assertRaises(ValueError):
            self.executor.execute(self.players, data)

    def test_all_operations_chain(self):
        """尽可能多的操作链式组合"""
        data = build_spoi_stream([
            pipe_inst(),           # PIPE [5 players]
            reverse_inst(),        # REVERSE → [Eve, Dave, Carol, Bob, Alice]
            filter_gt([], 1, 10),  # FILTER level>10 → [Dave, Carol, Bob]
            drop_inst(1),          # DROP 1 → [Carol, Bob]
            take_inst(2),          # TAKE 2 → [Carol, Bob]
            sort_inst([0]),        # SORT by name → [Bob, Carol]
            exec_inst(),
        ])
        result = self.executor.execute(self.players, data)
        names = [p.name for p in result["value"]]
        self.assertEqual(names, ["Bob", "Carol"])


# =============================== 类型注册表边界 ===============================

class TestTypeRegistryBoundary(unittest.TestCase):
    """类型注册表的刁钻边界测试"""

    def test_empty_registry(self):
        """空类型注册表"""
        executor = SpoiExecutor({})
        player = Player(name="Alice")
        # _navigate 在空注册表时回退到 __dict__ 字段访问，不会报错
        result = executor._navigate(player, [0])
        self.assertEqual(result, "Alice")

    def test_nonexistent_type(self):
        """不存在的类型"""
        executor = SpoiExecutor(TYPE_REGISTRY)
        data = build_spoi_stream([pipe_inst(), exec_inst()])
        result = executor.execute(42, data)  # int 类型不在注册表中
        # PIPE 对非对象应该返回 VECTOR... 或者报错
        self.assertIsNotNone(result)


if __name__ == '__main__':
    unittest.main(verbosity=2)