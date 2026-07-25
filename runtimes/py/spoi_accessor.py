# ============================================================
# SPOI Accessor — Python 类型特化访问器（自动生成）
# 由 sp-gen spoi-python-accessor 从 C++ 元数据生成
# 替代反射机制（getattr/setattr），直接通过字段索引访问/设置值
# ============================================================

import struct
from typing import Any, Dict

# ============================================================
# 基本类型 ID（与 C++ E_type 枚举值一致）
# ============================================================

class TypeId:
    U8: int     = 26
    U16: int    = 27
    U32: int    = 28
    U64: int    = 29
    I8: int     = 30
    I16: int    = 31
    I32: int    = 32
    I64: int    = 33
    F32: int    = 34
    F64: int    = 35
    STRING: int = 9
    BOOL: int   = 40
    CUSTOM: int = 0

# ============================================================
# SpoiAccessor — 类型特化访问器基类
# ============================================================

class SpoiAccessor:
    """类型特化访问器基类，子类通过 if/elif 跳转表实现 O(1) 字段访问"""

    def field_count(self) -> int:
        """返回字段数量"""
        raise NotImplementedError

    def get_field(self, obj: Any, idx: int) -> Any:
        """通过索引读取字段值"""
        raise NotImplementedError

    def set_field(self, obj: Any, idx: int, val: Any) -> None:
        """通过索引设置字段值"""
        raise NotImplementedError

# ============================================================
# DeserializeValue — 通用值反序列化（基于 type_id 前缀）
# 格式: [type_id(u32 LE) + value_bytes]
# ============================================================

def deserialize_value(data: bytes) -> Any:
    """从 type_id 前缀格式的二进制数据反序列化值"""
    if len(data) < 4:
        return None
    type_id = struct.unpack('<I', data[:4])[0]
    value_bytes = data[4:]
    if type_id == TypeId.U8:
        if len(value_bytes) >= 1:
            return value_bytes[0]
        return 0
    if type_id == TypeId.U16:
        if len(value_bytes) >= 2:
            return struct.unpack('<H', value_bytes)[0]
        return 0
    if type_id == TypeId.U32:
        if len(value_bytes) >= 4:
            return struct.unpack('<I', value_bytes)[0]
        return 0
    if type_id == TypeId.U64:
        if len(value_bytes) >= 8:
            return struct.unpack('<Q', value_bytes)[0]
        return 0
    if type_id == TypeId.I8:
        if len(value_bytes) >= 1:
            return struct.unpack('<b', value_bytes)[0]
        return 0
    if type_id == TypeId.I16:
        if len(value_bytes) >= 2:
            return struct.unpack('<h', value_bytes)[0]
        return 0
    if type_id == TypeId.I32:
        if len(value_bytes) >= 4:
            return struct.unpack('<i', value_bytes)[0]
        return 0
    if type_id == TypeId.I64:
        if len(value_bytes) >= 8:
            return struct.unpack('<q', value_bytes)[0]
        return 0
    if type_id == TypeId.F32:
        if len(value_bytes) >= 4:
            return struct.unpack('<f', value_bytes)[0]
        return 0.0
    if type_id == TypeId.F64:
        if len(value_bytes) >= 8:
            return struct.unpack('<d', value_bytes)[0]
        return 0.0
    if type_id == TypeId.STRING:
        return value_bytes.decode('utf-8')
    if type_id == TypeId.BOOL:
        if len(value_bytes) >= 1:
            return value_bytes[0] != 0
        return False
    return value_bytes

# ============================================================
# SpoiTestPlayerAccessor
# ============================================================

class SpoiTestPlayerAccessor(SpoiAccessor):
    """SpoiTestPlayer 类型特化访问器"""

    def field_count(self) -> int:
        return 4

    def get_field(self, obj: Any, idx: int) -> Any:
        if idx == 0:
            return obj.name
        elif idx == 1:
            return obj.hp
        elif idx == 2:
            return obj.level
        elif idx == 3:
            return obj.posX
        else:
            raise ValueError(f"invalid field index for SpoiTestPlayer: {idx}")

    def set_field(self, obj: Any, idx: int, val: Any) -> None:
        if idx == 0:
            obj.name = val
        elif idx == 1:
            obj.hp = val
        elif idx == 2:
            obj.level = val
        elif idx == 3:
            obj.posX = val
        else:
            raise ValueError(f"invalid field index for SpoiTestPlayer: {idx}")

# ============================================================
# SpoiTestStateAccessor
# ============================================================

class SpoiTestStateAccessor(SpoiAccessor):
    """SpoiTestState 类型特化访问器"""

    def field_count(self) -> int:
        return 3

    def get_field(self, obj: Any, idx: int) -> Any:
        if idx == 0:
            return obj.tick
        elif idx == 1:
            return obj.currentMap
        elif idx == 2:
            return obj.players
        else:
            raise ValueError(f"invalid field index for SpoiTestState: {idx}")

    def set_field(self, obj: Any, idx: int, val: Any) -> None:
        if idx == 0:
            obj.tick = val
        elif idx == 1:
            obj.currentMap = val
        elif idx == 2:
            obj.players = val
        else:
            raise ValueError(f"invalid field index for SpoiTestState: {idx}")

# ============================================================
# SpoiItemAccessor
# ============================================================

class SpoiItemAccessor(SpoiAccessor):
    """SpoiItem 类型特化访问器"""

    def field_count(self) -> int:
        return 2

    def get_field(self, obj: Any, idx: int) -> Any:
        if idx == 0:
            return obj.name
        elif idx == 1:
            return obj.value
        else:
            raise ValueError(f"invalid field index for SpoiItem: {idx}")

    def set_field(self, obj: Any, idx: int, val: Any) -> None:
        if idx == 0:
            obj.name = val
        elif idx == 1:
            obj.value = val
        else:
            raise ValueError(f"invalid field index for SpoiItem: {idx}")

# ============================================================
# SpoiInventoryAccessor
# ============================================================

class SpoiInventoryAccessor(SpoiAccessor):
    """SpoiInventory 类型特化访问器"""

    def field_count(self) -> int:
        return 3

    def get_field(self, obj: Any, idx: int) -> Any:
        if idx == 0:
            return obj.items
        elif idx == 1:
            return obj.equipped
        elif idx == 2:
            return obj.gold
        else:
            raise ValueError(f"invalid field index for SpoiInventory: {idx}")

    def set_field(self, obj: Any, idx: int, val: Any) -> None:
        if idx == 0:
            obj.items = val
        elif idx == 1:
            obj.equipped = val
        elif idx == 2:
            obj.gold = val
        else:
            raise ValueError(f"invalid field index for SpoiInventory: {idx}")

# ============================================================
# SpoiCharacterAccessor
# ============================================================

class SpoiCharacterAccessor(SpoiAccessor):
    """SpoiCharacter 类型特化访问器"""

    def field_count(self) -> int:
        return 5

    def get_field(self, obj: Any, idx: int) -> Any:
        if idx == 0:
            return obj.name
        elif idx == 1:
            return obj.hp
        elif idx == 2:
            return obj.inventory
        elif idx == 3:
            return obj.weapon
        elif idx == 4:
            return obj.petLevel
        else:
            raise ValueError(f"invalid field index for SpoiCharacter: {idx}")

    def set_field(self, obj: Any, idx: int, val: Any) -> None:
        if idx == 0:
            obj.name = val
        elif idx == 1:
            obj.hp = val
        elif idx == 2:
            obj.inventory = val
        elif idx == 3:
            obj.weapon = val
        elif idx == 4:
            obj.petLevel = val
        else:
            raise ValueError(f"invalid field index for SpoiCharacter: {idx}")

# ============================================================
# SpoiWorldAccessor
# ============================================================

class SpoiWorldAccessor(SpoiAccessor):
    """SpoiWorld 类型特化访问器"""

    def field_count(self) -> int:
        return 3

    def get_field(self, obj: Any, idx: int) -> Any:
        if idx == 0:
            return obj.worldName
        elif idx == 1:
            return obj.tick
        elif idx == 2:
            return obj.characters
        else:
            raise ValueError(f"invalid field index for SpoiWorld: {idx}")

    def set_field(self, obj: Any, idx: int, val: Any) -> None:
        if idx == 0:
            obj.worldName = val
        elif idx == 1:
            obj.tick = val
        elif idx == 2:
            obj.characters = val
        else:
            raise ValueError(f"invalid field index for SpoiWorld: {idx}")

# ============================================================
# SpoiAccessorRegistry — 静态类型注册表
# 替代运行时 Dict[str, List[str]]（反射版本）
# ============================================================

SpoiAccessorRegistry: Dict[str, SpoiAccessor] = {
    "SpoiTestPlayer": SpoiTestPlayerAccessor(),
    "SpoiTestState": SpoiTestStateAccessor(),
    "SpoiItem": SpoiItemAccessor(),
    "SpoiInventory": SpoiInventoryAccessor(),
    "SpoiCharacter": SpoiCharacterAccessor(),
    "SpoiWorld": SpoiWorldAccessor(),
}
