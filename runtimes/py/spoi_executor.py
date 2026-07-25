"""
StreamPunk SPOI Executor — Python Runtime（v2: 访问器驱动，零反射）

SPOI = StreamPunk Operation Instruction
执行 SPOI 指令流，对 Python 对象进行查询/更新操作。

与 v1 的区别：
  - 使用 SpoiAccessor 接口替代 getattr/setattr 反射
  - 使用 deserialize_value（基于 type_id 前缀）替代字节长度启发式
  - 导航和字段设置通过访问器的 if/elif 跳转表，O(1) 且无反射开销

用法：
    from spoi_executor import SpoiExecutor
    from spoi_accessor import SpoiAccessorRegistry  # sp-gen 生成

    executor = SpoiExecutor(SpoiAccessorRegistry)

    # 执行 SPOI 指令流
    result = executor.execute(root_obj, instruction_bytes)
"""

import struct
from typing import Any, Dict, List, Tuple

from spoi_accessor import TypeId, deserialize_value

# =============================== 操作码常量 ===============================

class Op:
    # 导航
    NAV        = 0x00
    IDX        = 0x01
    DEREF      = 0x02
    UNWRAP     = 0x03
    # 写操作
    SET        = 0x04
    ADD        = 0x05
    APPEND     = 0x06
    REMOVE     = 0x07
    INSERT     = 0x08
    REPLACE    = 0x09
    RESET      = 0x0A
    SETNULL    = 0x0B
    # 读操作
    FILTER     = 0x0C
    SELECT     = 0x0D
    SORT       = 0x0E
    REVERSE    = 0x0F
    TAKE       = 0x10
    DROP       = 0x11
    TAKEWHILE  = 0x12
    DROPWHILE  = 0x13
    DISTINCT   = 0x14
    # 聚合
    COUNT      = 0x15
    ANY        = 0x16
    ALL        = 0x17
    FIND       = 0x18
    # 容器
    KEYS       = 0x19
    VALUES     = 0x1A
    JOIN       = 0x1B
    # 控制
    EXEC       = 0x21
    PIPE       = 0x22

# 路径特殊标记
PATH_DEREF  = 0xFFFF
PATH_MAPKEY = 0xFFFE

# 结果类型
class ResultType:
    UNDEF    = 0
    SINGLE   = 1
    VECTOR   = 2
    COUNT    = 3
    BOOL     = 4
    OPTIONAL = 5
    ERROR    = 6

# =============================== Varint 编解码 ===============================

def read_varint(data: bytes, offset: int) -> Tuple[int, int]:
    """读取 varint，返回 (value, new_offset)"""
    result = 0
    shift = 0
    while offset < len(data):
        b = data[offset]
        offset += 1
        result |= (b & 0x7F) << shift
        if not (b & 0x80):
            return result, offset
        shift += 7
    return result, offset

def write_varint(buf: bytearray, v: int) -> None:
    """写入 varint 到 bytearray"""
    while v >= 0x80:
        buf.append((v & 0x7F) | 0x80)
        v >>= 7
    buf.append(v & 0x7F)

# =============================== SPOI 指令解析 ===============================

class SpoiInstruction:
    """SPOI 指令"""
    __slots__ = ('op', 'path', 'operand')

    def __init__(self, op: int = 0, path: List[int] = None, operand: bytes = None):
        self.op = op
        self.path = path or []
        self.operand = operand or b''

    def __repr__(self):
        return f"SpoiInstruction(op=0x{self.op:02X}, path={self.path}, operand_len={len(self.operand)})"


def parse_spoi_stream(data: bytes) -> List[SpoiInstruction]:
    """解析 SPOI 指令流"""
    offset = 0
    count, offset = read_varint(data, offset)
    instructions = []

    for _ in range(count):
        # op
        op = data[offset]
        offset += 1

        # path
        path_len, offset = read_varint(data, offset)
        path = []
        for _ in range(path_len):
            seg, offset = read_varint(data, offset)
            path.append(seg)

        # operand
        operand_len, offset = read_varint(data, offset)
        operand = data[offset:offset + operand_len]
        offset += operand_len

        instructions.append(SpoiInstruction(op, path, operand))

    return instructions

# =============================== SPOI 执行器（v2: 访问器驱动） ===============================

class SpoiExecutor:
    """SPOI 指令执行器（v2: 访问器驱动，零反射）"""

    def __init__(self, accessors: Dict[str, Any]):
        self._accessors = accessors  # type_name -> SpoiAccessor
        self._pipe_data: List[Any] = []  # 管道数据缓冲区

    def execute(self, root: Any, instruction_bytes: bytes) -> Dict[str, Any]:
        """
        执行 SPOI 指令流。
        返回 {"resultType": int, "data": bytes} 或 {"resultType": int, "value": Any}
        """
        instructions = parse_spoi_stream(instruction_bytes)
        self._pipe_data = []

        for inst in instructions:
            self._dispatch(inst, root)

        return self._make_result()

    def _make_result(self) -> Dict[str, Any]:
        """构建结果"""
        if not self._pipe_data:
            return {"resultType": ResultType.UNDEF, "data": b''}

        data = self._pipe_data
        if len(data) == 1:
            return {"resultType": ResultType.SINGLE, "value": data[0]}
        return {"resultType": ResultType.VECTOR, "value": data}

    def _dispatch(self, inst: SpoiInstruction, root: Any) -> None:
        """按操作码分发"""
        op = inst.op

        # 写操作
        if op == Op.SET:
            self._op_set(root, inst.path, inst.operand)
        elif op == Op.ADD:
            self._op_add(root, inst.path, inst.operand)
        elif op == Op.APPEND:
            self._op_append(root, inst.path, inst.operand)
        elif op == Op.REMOVE:
            self._op_remove(root, inst.path, inst.operand)
        elif op == Op.INSERT:
            self._op_insert(root, inst.path, inst.operand)
        elif op == Op.REPLACE:
            self._op_replace(root, inst.path, inst.operand)
        elif op == Op.RESET:
            self._op_reset(root, inst.path)
        elif op == Op.SETNULL:
            self._op_setnull(root, inst.path)
        # 读操作
        elif op == Op.FILTER:
            self._op_filter(root, inst.path, inst.operand)
        elif op == Op.SELECT:
            self._op_select(root, inst.path)
        elif op == Op.SORT:
            self._op_sort(inst.path)
        elif op == Op.REVERSE:
            self._op_reverse()
        elif op == Op.TAKE:
            self._op_take(inst.operand)
        elif op == Op.DROP:
            self._op_drop(inst.operand)
        elif op == Op.TAKEWHILE:
            self._op_takewhile(root, inst.path, inst.operand)
        elif op == Op.DROPWHILE:
            self._op_dropwhile(root, inst.path, inst.operand)
        elif op == Op.DISTINCT:
            self._op_distinct()
        # 聚合
        elif op == Op.COUNT:
            self._op_count()
        elif op == Op.ANY:
            self._op_any(root, inst.path, inst.operand)
        elif op == Op.ALL:
            self._op_all(root, inst.path, inst.operand)
        elif op == Op.FIND:
            self._op_find(root, inst.path, inst.operand)
        # 容器
        elif op == Op.KEYS:
            self._op_keys()
        elif op == Op.VALUES:
            self._op_values()
        elif op == Op.JOIN:
            self._op_join()
        # 控制
        elif op == Op.EXEC:
            pass  # 执行结束，结果已在 _pipe_data 中
        elif op == Op.PIPE:
            self._op_pipe(root, inst.path)
        else:
            raise ValueError(f"Unknown SPOI opcode: 0x{op:02X}")

    # =============================== 导航（访问器驱动） ===============================

    def _navigate(self, obj: Any, path: List[int]) -> Any:
        """沿路径导航到目标字段"""
        current = obj
        for seg in path:
            current = self._nav_step(current, seg)
        return current

    def _nav_step(self, obj: Any, seg: int) -> Any:
        """单步导航（v2: 访问器驱动，兼容旧反射式注册表）"""
        # null 处理
        if obj is None:
            raise RuntimeError("Cannot navigate on None")

        # 指针解引用
        if seg == PATH_DEREF:
            # 基本类型返回自身
            if isinstance(obj, (str, int, float, bool)):
                return obj
            result = self._accessor_navigate(obj, 0)
            if result is not None:
                return result
            # 回退：尝试 getattr('value') 或 __dict__
            if hasattr(obj, 'value'):
                return obj.value
            return obj

        # 容器索引访问
        if isinstance(obj, (list, tuple)):
            return obj[seg]
        if isinstance(obj, dict):
            return list(obj.values())[seg]

        # 基本类型：seg == 0 时返回自身
        if isinstance(obj, (str, int, float, bool)):
            if seg == 0:
                return obj
            raise RuntimeError(f"Cannot navigate segment {seg} on {type(obj).__name__}")

        # 结构体成员访问 — 使用访问器（兼容旧反射式注册表）
        result = self._accessor_navigate(obj, seg)
        if result is not None:
            return result

        raise RuntimeError(f"Cannot navigate segment {seg} on {type(obj).__name__}")

    def _nav_set(self, obj: Any, path: List[int], value: Any) -> None:
        """沿路径设置值（到最后一个段之前，然后设置最后一个段）"""
        if not path:
            return
        if len(path) == 1:
            self._set_field(obj, path[0], value)
            return

        # 导航到倒数第二个段
        target = obj
        for seg in path[:-1]:
            target = self._nav_step(target, seg)
        self._set_field(target, path[-1], value)

    def _set_field(self, obj: Any, seg: int, value: Any) -> None:
        """设置字段值（v2: 访问器驱动，兼容旧反射式注册表）"""
        if isinstance(obj, list):
            obj[seg] = value
            return
        if isinstance(obj, dict):
            keys = list(obj.keys())
            obj[keys[seg]] = value
            return

        # 结构体 — 使用访问器（兼容旧反射式注册表）
        if self._accessor_set(obj, seg, value):
            return

        raise RuntimeError(f"Cannot set field {seg} on {type(obj).__name__}")

    def _get_accessor(self, obj: Any) -> Any:
        """获取对象对应的注册表条目（可能是 SpoiAccessor 或旧式字段名列表）"""
        if obj is None or self._accessors is None:
            return None
        type_name = type(obj).__name__
        return self._accessors.get(type_name)

    def _accessor_navigate(self, obj: Any, seg: int) -> Any:
        """通过注册表导航到字段（兼容新旧两种注册表格式）"""
        entry = self._get_accessor(obj)
        if entry is None:
            return None
        # 新式：SpoiAccessor 对象
        if hasattr(entry, 'get_field'):
            return entry.get_field(obj, seg)
        # 旧式：字段名列表
        if isinstance(entry, list):
            field_name = entry[seg]
            return getattr(obj, field_name)
        return None

    def _accessor_set(self, obj: Any, seg: int, value: Any) -> bool:
        """通过注册表设置字段值（兼容新旧两种注册表格式）"""
        entry = self._get_accessor(obj)
        if entry is None:
            return False
        # 新式：SpoiAccessor 对象
        if hasattr(entry, 'set_field'):
            entry.set_field(obj, seg, value)
            return True
        # 旧式：字段名列表
        if isinstance(entry, list):
            field_name = entry[seg]
            setattr(obj, field_name, value)
            return True
        return False

    # =============================== 写操作 ===============================

    def _op_set(self, root: Any, path: List[int], operand: bytes) -> None:
        value = deserialize_value(operand)
        self._nav_set(root, path, value)

    def _op_add(self, root: Any, path: List[int], operand: bytes) -> None:
        delta = deserialize_value(operand)
        target = self._navigate(root, path)
        result = self._add_values(target, delta)
        self._nav_set(root, path, result)

    def _add_values(self, a: Any, b: Any) -> Any:
        """值加法，支持数值和字符串"""
        if isinstance(a, (int, float)) and isinstance(b, (int, float)):
            return a + b
        if isinstance(a, str) or isinstance(b, str):
            return str(a) + str(b)
        raise RuntimeError(f"Cannot add {type(a).__name__} and {type(b).__name__}")

    def _op_append(self, root: Any, path: List[int], operand: bytes) -> None:
        value = deserialize_value(operand)
        target = self._navigate(root, path)
        if isinstance(target, list):
            target.append(value)
        elif hasattr(target, 'append'):
            target.append(value)
        else:
            raise RuntimeError(f"Cannot append to {type(target).__name__}")

    def _op_remove(self, root: Any, path: List[int], operand: bytes) -> None:
        target = self._navigate(root, path)
        if isinstance(target, list):
            idx = struct.unpack('<I', operand[:4])[0]
            target.pop(idx)
        else:
            raise RuntimeError(f"Cannot remove from {type(target).__name__}")

    def _op_insert(self, root: Any, path: List[int], operand: bytes) -> None:
        # operand: [u32 index] + [serialized value]
        idx = struct.unpack('<I', operand[:4])[0]
        value = deserialize_value(operand[4:])
        target = self._navigate(root, path)
        if isinstance(target, list):
            target.insert(idx, value)
        else:
            raise RuntimeError(f"Cannot insert into {type(target).__name__}")

    def _op_replace(self, root: Any, path: List[int], operand: bytes) -> None:
        # operand: [u32 index] + [serialized value]
        idx = struct.unpack('<I', operand[:4])[0]
        value = deserialize_value(operand[4:])
        target = self._navigate(root, path)
        if isinstance(target, list):
            target[idx] = value
        else:
            raise RuntimeError(f"Cannot replace in {type(target).__name__}")

    def _op_reset(self, root: Any, path: List[int]) -> None:
        self._nav_set(root, path, None)

    def _op_setnull(self, root: Any, path: List[int]) -> None:
        self._nav_set(root, path, None)

    # =============================== 读操作 ===============================

    def _op_pipe(self, root: Any, path: List[int]) -> None:
        """管道入口：将路径指向的数据加载到管道缓冲区"""
        if path:
            data = self._navigate(root, path)
        else:
            data = root

        if isinstance(data, (list, tuple)):
            self._pipe_data = list(data)
        elif isinstance(data, dict):
            self._pipe_data = list(data.values())
        else:
            self._pipe_data = [data]

    def _matches(self, obj: Any, path: List[int], operand: bytes) -> bool:
        """检查对象是否匹配比较表达式（v2: 访问器驱动）"""
        # operand 格式: memberIdx(u32) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
        if len(operand) < 9:
            return True

        member_idx = struct.unpack('<I', operand[:4])[0]
        cmp_op = operand[4]
        # 跳过 value_len（varint 编码）
        value_offset = 5
        _, value_offset = read_varint(operand, value_offset)
        value_bytes = operand[value_offset:]

        # 先按路径导航到目标对象，再访问成员字段
        target = obj
        if path:
            target = self._navigate(obj, path)

        # 对于基本类型，member_idx=0 时直接比较值本身
        if member_idx == 0 and isinstance(target, (int, float, str, bool)):
            field_value = target
        else:
            field_value = self._nav_step(target, member_idx)

        expected = deserialize_value(value_bytes)

        return self._compare_values(field_value, cmp_op, expected)

    def _compare_values(self, field_value: Any, cmp_op: int, expected: Any) -> bool:
        """比较两个值"""
        if field_value is None and expected is None:
            return cmp_op == 0  # eq
        if field_value is None or expected is None:
            return cmp_op == 1  # ne

        # 尝试数值比较
        try:
            if cmp_op == 0:  # eq
                return field_value == expected
            elif cmp_op == 1:  # ne
                return field_value != expected
            elif cmp_op == 2:  # lt
                return field_value < expected
            elif cmp_op == 3:  # gt
                return field_value > expected
            elif cmp_op == 4:  # le
                return field_value <= expected
            elif cmp_op == 5:  # ge
                return field_value >= expected
            return True
        except TypeError:
            # 降级到字符串比较
            s1 = str(field_value)
            s2 = str(expected)
            if cmp_op == 0:
                return s1 == s2
            elif cmp_op == 1:
                return s1 != s2
            elif cmp_op == 2:
                return s1 < s2
            elif cmp_op == 3:
                return s1 > s2
            elif cmp_op == 4:
                return s1 <= s2
            elif cmp_op == 5:
                return s1 >= s2
            return True

    def _op_filter(self, _root: Any, path: List[int], operand: bytes) -> None:
        self._pipe_data = [obj for obj in self._pipe_data if self._matches(obj, path, operand)]

    def _op_select(self, _root: Any, path: List[int]) -> None:
        if path:
            self._pipe_data = [self._navigate(obj, path) for obj in self._pipe_data]

    def _op_sort(self, path: List[int]) -> None:
        """按路径排序"""
        if path:
            self._pipe_data.sort(key=lambda obj: str(self._navigate(obj, path)))
        else:
            self._pipe_data.sort(key=str)

    def _op_reverse(self) -> None:
        self._pipe_data.reverse()

    def _op_take(self, operand: bytes) -> None:
        n = struct.unpack('<I', operand[:4])[0] if len(operand) >= 4 else 0
        self._pipe_data = self._pipe_data[:n]

    def _op_drop(self, operand: bytes) -> None:
        n = struct.unpack('<I', operand[:4])[0] if len(operand) >= 4 else 0
        self._pipe_data = self._pipe_data[n:]

    def _op_takewhile(self, _root: Any, path: List[int], operand: bytes) -> None:
        result = []
        for obj in self._pipe_data:
            if self._matches(obj, path, operand):
                result.append(obj)
            else:
                break
        self._pipe_data = result

    def _op_dropwhile(self, _root: Any, path: List[int], operand: bytes) -> None:
        idx = 0
        for i, obj in enumerate(self._pipe_data):
            if not self._matches(obj, path, operand):
                idx = i
                break
        else:
            idx = len(self._pipe_data)
        self._pipe_data = self._pipe_data[idx:]

    def _op_distinct(self) -> None:
        seen = set()
        result = []
        for obj in self._pipe_data:
            key = str(obj) if not isinstance(obj, (int, float, str, bool)) else obj
            if key not in seen:
                seen.add(key)
                result.append(obj)
        self._pipe_data = result

    # =============================== 聚合 ===============================

    def _op_count(self) -> None:
        self._pipe_data = [len(self._pipe_data)]

    def _op_any(self, _root: Any, path: List[int], operand: bytes) -> None:
        self._pipe_data = [any(self._matches(obj, path, operand) for obj in self._pipe_data)]

    def _op_all(self, _root: Any, path: List[int], operand: bytes) -> None:
        self._pipe_data = [all(self._matches(obj, path, operand) for obj in self._pipe_data)]

    def _op_find(self, _root: Any, path: List[int], operand: bytes) -> None:
        for obj in self._pipe_data:
            if self._matches(obj, path, operand):
                self._pipe_data = [obj]
                return
        self._pipe_data = []

    # =============================== 容器操作 ===============================

    def _op_keys(self) -> None:
        result = []
        for obj in self._pipe_data:
            if isinstance(obj, dict):
                result.extend(obj.keys())
        self._pipe_data = result

    def _op_values(self) -> None:
        result = []
        for obj in self._pipe_data:
            if isinstance(obj, dict):
                result.extend(obj.values())
        self._pipe_data = result

    def _op_join(self) -> None:
        """展平嵌套列表"""
        result = []
        for obj in self._pipe_data:
            if isinstance(obj, (list, tuple)):
                result.extend(obj)
            else:
                result.append(obj)
        self._pipe_data = result

    