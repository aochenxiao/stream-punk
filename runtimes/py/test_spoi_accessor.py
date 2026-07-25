"""
SPOI Accessor 单元测试

测试覆盖：
- TypeId 常量值
- deserialize_value 函数
- 6 个 Accessor 类（field_count / get_field / set_field / 无效索引）
- SpoiAccessorRegistry 注册表
- Executor 集成测试（使用 SpoiAccessorRegistry）
"""

import sys
import os
import struct

# 添加父目录到路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from spoi_accessor import (
    TypeId, deserialize_value,
    SpoiAccessor,
    SpoiTestPlayerAccessor, SpoiTestStateAccessor,
    SpoiItemAccessor, SpoiInventoryAccessor,
    SpoiCharacterAccessor, SpoiWorldAccessor,
    SpoiAccessorRegistry,
)
from spoi_executor import SpoiExecutor, SpoiInstruction, Op, write_varint


# ============================================================
# 测试数据类（与 accessor 中引用的类型名匹配）
# ============================================================

class SpoiTestPlayer:
    def __init__(self):
        self.name = ""
        self.hp = 0
        self.level = 0
        self.posX = 0.0

class SpoiTestState:
    def __init__(self):
        self.tick = 0
        self.currentMap = ""
        self.players = None

class SpoiItem:
    def __init__(self):
        self.name = ""
        self.value = 0

class SpoiInventory:
    def __init__(self):
        self.items = None
        self.equipped = None
        self.gold = 0

class SpoiCharacter:
    def __init__(self):
        self.name = ""
        self.hp = 0
        self.inventory = None
        self.weapon = None
        self.petLevel = 0

class SpoiWorld:
    def __init__(self):
        self.worldName = ""
        self.tick = 0
        self.characters = None


# ============================================================
# 测试 1: TypeId 常量
# ============================================================

def test_type_id_constants():
    assert TypeId.U8 == 26, f"U8 expected 26, got {TypeId.U8}"
    assert TypeId.U16 == 27, f"U16 expected 27, got {TypeId.U16}"
    assert TypeId.U32 == 28, f"U32 expected 28, got {TypeId.U32}"
    assert TypeId.U64 == 29, f"U64 expected 29, got {TypeId.U64}"
    assert TypeId.I8 == 30, f"I8 expected 30, got {TypeId.I8}"
    assert TypeId.I16 == 31, f"I16 expected 31, got {TypeId.I16}"
    assert TypeId.I32 == 32, f"I32 expected 32, got {TypeId.I32}"
    assert TypeId.I64 == 33, f"I64 expected 33, got {TypeId.I64}"
    assert TypeId.F32 == 34, f"F32 expected 34, got {TypeId.F32}"
    assert TypeId.F64 == 35, f"F64 expected 35, got {TypeId.F64}"
    assert TypeId.STRING == 9, f"STRING expected 9, got {TypeId.STRING}"
    assert TypeId.BOOL == 40, f"BOOL expected 40, got {TypeId.BOOL}"
    print("  PASS test_type_id_constants")


# ============================================================
# 测试 2: deserialize_value 函数
# ============================================================

def test_deserialize_value():
    # U8
    data = struct.pack('<I', TypeId.U8) + bytes([255])
    assert deserialize_value(data) == 255

    # U16
    data = struct.pack('<I', TypeId.U16) + struct.pack('<H', 65535)
    assert deserialize_value(data) == 65535

    # U32
    data = struct.pack('<I', TypeId.U32) + struct.pack('<I', 4000000000)
    assert deserialize_value(data) == 4000000000

    # U64
    data = struct.pack('<I', TypeId.U64) + struct.pack('<Q', 12345678901234567890)
    assert deserialize_value(data) == 12345678901234567890

    # I8
    data = struct.pack('<I', TypeId.I8) + struct.pack('<b', -128)
    assert deserialize_value(data) == -128

    # I16
    data = struct.pack('<I', TypeId.I16) + struct.pack('<h', -32768)
    assert deserialize_value(data) == -32768

    # I32
    data = struct.pack('<I', TypeId.I32) + struct.pack('<i', -2147483648)
    assert deserialize_value(data) == -2147483648

    # I64
    data = struct.pack('<I', TypeId.I64) + struct.pack('<q', -1234567890123456789)
    assert deserialize_value(data) == -1234567890123456789

    # F32
    data = struct.pack('<I', TypeId.F32) + struct.pack('<f', 3.14)
    result = deserialize_value(data)
    assert abs(result - 3.14) < 0.001, f"F32 expected ~3.14, got {result}"

    # F64
    data = struct.pack('<I', TypeId.F64) + struct.pack('<d', 2.718281828)
    result = deserialize_value(data)
    assert abs(result - 2.718281828) < 0.0000001, f"F64 expected ~2.718281828, got {result}"

    # String
    data = struct.pack('<I', TypeId.STRING) + "hello world".encode('utf-8')
    assert deserialize_value(data) == "hello world"

    # Bool (True)
    data = struct.pack('<I', TypeId.BOOL) + bytes([1])
    assert deserialize_value(data) == True

    # Bool (False)
    data = struct.pack('<I', TypeId.BOOL) + bytes([0])
    assert deserialize_value(data) == False

    # 边界条件：空数据返回 None
    assert deserialize_value(b'') is None
    assert deserialize_value(b'\x01') is None
    assert deserialize_value(b'\x01\x02\x03') is None

    print("  PASS test_deserialize_value")


# ============================================================
# 测试 3: SpoiTestPlayerAccessor
# ============================================================

def test_accessor_player():
    acc = SpoiTestPlayerAccessor()
    obj = SpoiTestPlayer()
    obj.name = "Alice"
    obj.hp = 100
    obj.level = 5
    obj.posX = 10.5

    assert acc.field_count() == 4

    assert acc.get_field(obj, 0) == "Alice"
    assert acc.get_field(obj, 1) == 100
    assert acc.get_field(obj, 2) == 5
    assert acc.get_field(obj, 3) == 10.5

    acc.set_field(obj, 0, "Bob")
    acc.set_field(obj, 1, 200)
    acc.set_field(obj, 2, 10)
    acc.set_field(obj, 3, 20.0)

    assert obj.name == "Bob"
    assert obj.hp == 200
    assert obj.level == 10
    assert obj.posX == 20.0

    try:
        acc.get_field(obj, 4)
        assert False, "should have raised ValueError"
    except ValueError:
        pass

    try:
        acc.set_field(obj, 4, 0)
        assert False, "should have raised ValueError"
    except ValueError:
        pass

    print("  PASS test_accessor_player")


# ============================================================
# 测试 4: SpoiTestStateAccessor
# ============================================================

def test_accessor_state():
    acc = SpoiTestStateAccessor()
    obj = SpoiTestState()
    obj.tick = 42
    obj.currentMap = "Dungeon"
    obj.players = [SpoiTestPlayer()]

    assert acc.field_count() == 3

    assert acc.get_field(obj, 0) == 42
    assert acc.get_field(obj, 1) == "Dungeon"
    assert isinstance(acc.get_field(obj, 2), list)

    acc.set_field(obj, 0, 100)
    acc.set_field(obj, 1, "Forest")
    acc.set_field(obj, 2, None)

    assert obj.tick == 100
    assert obj.currentMap == "Forest"
    assert obj.players is None

    try:
        acc.get_field(obj, 3)
        assert False, "should have raised ValueError"
    except ValueError:
        pass

    try:
        acc.set_field(obj, -1, 0)
        assert False, "should have raised ValueError"
    except ValueError:
        pass

    print("  PASS test_accessor_state")


# ============================================================
# 测试 5: SpoiItemAccessor
# ============================================================

def test_accessor_item():
    acc = SpoiItemAccessor()
    obj = SpoiItem()
    obj.name = "Sword"
    obj.value = 150

    assert acc.field_count() == 2

    assert acc.get_field(obj, 0) == "Sword"
    assert acc.get_field(obj, 1) == 150

    acc.set_field(obj, 0, "Shield")
    acc.set_field(obj, 1, 200)

    assert obj.name == "Shield"
    assert obj.value == 200

    try:
        acc.get_field(obj, 2)
        assert False, "should have raised ValueError"
    except ValueError:
        pass

    print("  PASS test_accessor_item")


# ============================================================
# 测试 6: SpoiInventoryAccessor
# ============================================================

def test_accessor_inventory():
    acc = SpoiInventoryAccessor()
    item1 = SpoiItem()
    item1.name = "Potion"
    obj = SpoiInventory()
    obj.items = [item1]
    obj.equipped = None
    obj.gold = 500

    assert acc.field_count() == 3

    assert acc.get_field(obj, 0) == [item1]
    assert acc.get_field(obj, 1) is None
    assert acc.get_field(obj, 2) == 500

    item2 = SpoiItem()
    item2.name = "Elixir"
    acc.set_field(obj, 0, [item2])
    acc.set_field(obj, 1, item1)
    acc.set_field(obj, 2, 999)

    assert obj.items == [item2]
    assert obj.equipped == item1
    assert obj.gold == 999

    try:
        acc.get_field(obj, 3)
        assert False, "should have raised ValueError"
    except ValueError:
        pass

    print("  PASS test_accessor_inventory")


# ============================================================
# 测试 7: SpoiCharacterAccessor
# ============================================================

def test_accessor_character():
    acc = SpoiCharacterAccessor()
    obj = SpoiCharacter()
    obj.name = "Hero"
    obj.hp = 1000
    obj.inventory = None
    obj.weapon = "Excalibur"
    obj.petLevel = 3

    assert acc.field_count() == 5

    assert acc.get_field(obj, 0) == "Hero"
    assert acc.get_field(obj, 1) == 1000
    assert acc.get_field(obj, 2) is None
    assert acc.get_field(obj, 3) == "Excalibur"
    assert acc.get_field(obj, 4) == 3

    acc.set_field(obj, 0, "Villain")
    acc.set_field(obj, 1, 500)
    inv = SpoiInventory()
    inv.gold = 100
    acc.set_field(obj, 2, inv)
    acc.set_field(obj, 3, "Dark Sword")
    acc.set_field(obj, 4, 99)

    assert obj.name == "Villain"
    assert obj.hp == 500
    assert obj.inventory == inv
    assert obj.weapon == "Dark Sword"
    assert obj.petLevel == 99

    try:
        acc.get_field(obj, 5)
        assert False, "should have raised ValueError"
    except ValueError:
        pass

    try:
        acc.set_field(obj, 5, 0)
        assert False, "should have raised ValueError"
    except ValueError:
        pass

    print("  PASS test_accessor_character")


# ============================================================
# 测试 8: SpoiWorldAccessor
# ============================================================

def test_accessor_world():
    acc = SpoiWorldAccessor()
    obj = SpoiWorld()
    obj.worldName = "Azeroth"
    obj.tick = 1000
    obj.characters = None

    assert acc.field_count() == 3

    assert acc.get_field(obj, 0) == "Azeroth"
    assert acc.get_field(obj, 1) == 1000
    assert acc.get_field(obj, 2) is None

    char = SpoiCharacter()
    char.name = "NPC"
    acc.set_field(obj, 0, "Middle Earth")
    acc.set_field(obj, 1, 2000)
    acc.set_field(obj, 2, [char])

    assert obj.worldName == "Middle Earth"
    assert obj.tick == 2000
    assert obj.characters == [char]

    try:
        acc.get_field(obj, 3)
        assert False, "should have raised ValueError"
    except ValueError:
        pass

    print("  PASS test_accessor_world")


# ============================================================
# 测试 9: SpoiAccessorRegistry
# ============================================================

def test_accessor_registry():
    assert len(SpoiAccessorRegistry) == 6

    assert "SpoiTestPlayer" in SpoiAccessorRegistry
    assert "SpoiTestState" in SpoiAccessorRegistry
    assert "SpoiItem" in SpoiAccessorRegistry
    assert "SpoiInventory" in SpoiAccessorRegistry
    assert "SpoiCharacter" in SpoiAccessorRegistry
    assert "SpoiWorld" in SpoiAccessorRegistry

    assert isinstance(SpoiAccessorRegistry["SpoiTestPlayer"], SpoiTestPlayerAccessor)
    assert isinstance(SpoiAccessorRegistry["SpoiTestState"], SpoiTestStateAccessor)
    assert isinstance(SpoiAccessorRegistry["SpoiItem"], SpoiItemAccessor)
    assert isinstance(SpoiAccessorRegistry["SpoiInventory"], SpoiInventoryAccessor)
    assert isinstance(SpoiAccessorRegistry["SpoiCharacter"], SpoiCharacterAccessor)
    assert isinstance(SpoiAccessorRegistry["SpoiWorld"], SpoiWorldAccessor)

    print("  PASS test_accessor_registry")


# ============================================================
# 测试 10: Executor 集成测试（使用 SpoiAccessorRegistry）
# ============================================================

def build_set_operand_int(value):
    """构造 [type_id(u32 LE) + value_bytes] 格式的 operand（int 值）"""
    return struct.pack('<I', TypeId.I32) + struct.pack('<i', value)

def build_set_operand_str(value):
    """构造 [type_id(u32 LE) + value_bytes] 格式的 operand（字符串值）"""
    return struct.pack('<I', TypeId.STRING) + value.encode('utf-8')

def build_set_operand_f32(value):
    """构造 [type_id(u32 LE) + value_bytes] 格式的 operand（float 值）"""
    return struct.pack('<I', TypeId.F32) + struct.pack('<f', value)

def build_set_inst(path, operand):
    """创建 SET 指令"""
    return SpoiInstruction(op=Op.SET, path=list(path), operand=bytes(operand))

def build_pipe_inst(path=None):
    """创建 PIPE 指令"""
    return SpoiInstruction(op=Op.PIPE, path=list(path or []), operand=b'')

def build_exec_inst():
    """创建 EXEC 指令"""
    return SpoiInstruction(op=Op.EXEC, path=[], operand=b'')

def build_spoi(instructions):
    """构建 SPOI 指令流"""
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


def test_executor_integration():
    executor = SpoiExecutor(SpoiAccessorRegistry)

    # --- 测试 SET int 字段 ---
    player = SpoiTestPlayer()
    player.name = "Alice"
    player.hp = 100
    player.level = 5
    player.posX = 0.0

    # SET hp (field idx=1) = 200
    data = build_spoi([build_set_inst([1], build_set_operand_int(200))])
    executor.execute(player, data)
    assert player.hp == 200, f"hp expected 200, got {player.hp}"

    # SET level (field idx=2) = 99
    data = build_spoi([build_set_inst([2], build_set_operand_int(99))])
    executor.execute(player, data)
    assert player.level == 99, f"level expected 99, got {player.level}"

    # --- 测试 SET string 字段 ---
    # SET name (field idx=0) = "Bob"
    data = build_spoi([build_set_inst([0], build_set_operand_str("Bob"))])
    executor.execute(player, data)
    assert player.name == "Bob", f"name expected 'Bob', got '{player.name}'"

    # --- 测试 SET float 字段 ---
    # SET posX (field idx=3) = 3.14
    data = build_spoi([build_set_inst([3], build_set_operand_f32(3.14))])
    executor.execute(player, data)
    assert abs(player.posX - 3.14) < 0.001, f"posX expected ~3.14, got {player.posX}"

    # --- 测试 SET 后读取（PIPE） ---
    player2 = SpoiTestPlayer()
    player2.name = "TestHero"
    player2.hp = 500
    player2.level = 10
    player2.posX = 100.0

    data = build_spoi([
        build_set_inst([1], build_set_operand_int(999)),
        build_pipe_inst([]),
        build_exec_inst(),
    ])
    result = executor.execute(player2, data)
    assert player2.hp == 999
    assert result["value"].hp == 999

    # --- 测试 State 类型 ---
    state = SpoiTestState()
    state.tick = 0
    state.currentMap = ""

    # SET tick (field idx=0) = 42
    data = build_spoi([build_set_inst([0], build_set_operand_int(42))])
    executor.execute(state, data)
    assert state.tick == 42

    # SET currentMap (field idx=1) = "Overworld"
    data = build_spoi([build_set_inst([1], build_set_operand_str("Overworld"))])
    executor.execute(state, data)
    assert state.currentMap == "Overworld"

    # --- 测试 Item 类型 ---
    item = SpoiItem()
    item.name = ""
    item.value = 0

    data = build_spoi([
        build_set_inst([0], build_set_operand_str("Potion")),
        build_set_inst([1], build_set_operand_int(50)),
    ])
    executor.execute(item, data)
    assert item.name == "Potion"
    assert item.value == 50

    # --- 测试 Inventory 类型 ---
    inv = SpoiInventory()
    inv.items = None
    inv.equipped = None
    inv.gold = 0

    data = build_spoi([build_set_inst([2], build_set_operand_int(1000))])
    executor.execute(inv, data)
    assert inv.gold == 1000

    # --- 测试 Character 类型 ---
    char = SpoiCharacter()
    char.name = ""
    char.hp = 0
    char.inventory = None
    char.weapon = None
    char.petLevel = 0

    data = build_spoi([
        build_set_inst([0], build_set_operand_str("Hero")),
        build_set_inst([1], build_set_operand_int(2000)),
        build_set_inst([4], build_set_operand_int(5)),
    ])
    executor.execute(char, data)
    assert char.name == "Hero"
    assert char.hp == 2000
    assert char.petLevel == 5

    # --- 测试 World 类型 ---
    world = SpoiWorld()
    world.worldName = ""
    world.tick = 0
    world.characters = None

    data = build_spoi([
        build_set_inst([0], build_set_operand_str("Azeroth")),
        build_set_inst([1], build_set_operand_int(9999)),
    ])
    executor.execute(world, data)
    assert world.worldName == "Azeroth"
    assert world.tick == 9999

    print("  PASS test_executor_integration")


# ============================================================
# 测试入口
# ============================================================

if __name__ == '__main__':
    print("Running SPOI Accessor tests...\n")
    test_type_id_constants()
    test_deserialize_value()
    test_accessor_player()
    test_accessor_state()
    test_accessor_item()
    test_accessor_inventory()
    test_accessor_character()
    test_accessor_world()
    test_accessor_registry()
    test_executor_integration()
    print("\nAll tests passed!")