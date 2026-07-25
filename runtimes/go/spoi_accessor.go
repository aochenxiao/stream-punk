// ============================================================
// SPOI Accessor — Go 类型特化访问器（自动生成）
// 由 sp-gen spoi-go-accessor 从 C++ 元数据生成
// 替代反射机制，直接通过字段索引访问/设置值
// ============================================================

package main

import (
	"encoding/binary"
	"math"
)

// ============================================================
// 基本类型 ID（与 C++ E_type 枚举值一致）
// ============================================================

const (
	TypeIdU8     uint32 = 26
	TypeIdU16    uint32 = 27
	TypeIdU32    uint32 = 28
	TypeIdU64    uint32 = 29
	TypeIdI8     uint32 = 30
	TypeIdI16    uint32 = 31
	TypeIdI32    uint32 = 32
	TypeIdI64    uint32 = 33
	TypeIdF32    uint32 = 34
	TypeIdF64    uint32 = 35
	TypeIdString uint32 = 9
	TypeIdBool   uint32 = 40
)

// ============================================================
// SpoiAccessor — 类型特化访问器接口
// ============================================================

type SpoiAccessor interface {
	FieldCount() int
	GetField(obj any, idx int) any
	SetField(obj any, idx int, val any)
}

// ============================================================
// DeserializeValue — 通用值反序列化（基于 type_id 前缀）
// 格式: [type_id(u32 LE) + value_bytes]
// ============================================================

func DeserializeValue(data []byte) any {
	if len(data) < 4 {
		return nil
	}
	typeId := binary.LittleEndian.Uint32(data[:4])
	valueBytes := data[4:]
	switch typeId {
	case TypeIdU8:
		return valueBytes[0]
	case TypeIdU16:
		return binary.LittleEndian.Uint16(valueBytes)
	case TypeIdU32:
		return binary.LittleEndian.Uint32(valueBytes)
	case TypeIdU64:
		return binary.LittleEndian.Uint64(valueBytes)
	case TypeIdI8:
		return int8(valueBytes[0])
	case TypeIdI16:
		return int16(binary.LittleEndian.Uint16(valueBytes))
	case TypeIdI32:
		return int32(binary.LittleEndian.Uint32(valueBytes))
	case TypeIdI64:
		return int64(binary.LittleEndian.Uint64(valueBytes))
	case TypeIdF32:
		return math.Float32frombits(binary.LittleEndian.Uint32(valueBytes))
	case TypeIdF64:
		return math.Float64frombits(binary.LittleEndian.Uint64(valueBytes))
	case TypeIdString:
		return string(valueBytes)
	case TypeIdBool:
		return valueBytes[0] != 0
	default:
		return valueBytes
	}
}

// ============================================================
// SpoiTestPlayerAccessor
// ============================================================

type SpoiTestPlayerAccessor struct{}

func (a SpoiTestPlayerAccessor) FieldCount() int {
	return 4
}

func (a SpoiTestPlayerAccessor) GetField(obj any, idx int) any {
	o := obj.(*SpoiTestPlayer)
	switch idx {
	case 0:
		return o.name
	case 1:
		return o.hp
	case 2:
		return o.level
	case 3:
		return o.posX
	default:
		panic("invalid field index for SpoiTestPlayer")
	}
}

func (a SpoiTestPlayerAccessor) SetField(obj any, idx int, val any) {
	o := obj.(*SpoiTestPlayer)
	switch idx {
	case 0:
		o.name = val.(string)
	case 1:
		o.hp = val.(int32)
	case 2:
		o.level = val.(int32)
	case 3:
		switch v := val.(type) {
		case float64:
			o.posX = v
		case float32:
			o.posX = float64(v)
		default:
			panic("invalid type for posX")
		}
	default:
		panic("invalid field index for SpoiTestPlayer")
	}
}

// ============================================================
// SpoiTestStateAccessor
// ============================================================

type SpoiTestStateAccessor struct{}

func (a SpoiTestStateAccessor) FieldCount() int {
	return 3
}

func (a SpoiTestStateAccessor) GetField(obj any, idx int) any {
	o := obj.(*SpoiTestState)
	switch idx {
	case 0:
		return o.tick
	case 1:
		return o.currentMap
	case 2:
		return o.players
	default:
		panic("invalid field index for SpoiTestState")
	}
}

func (a SpoiTestStateAccessor) SetField(obj any, idx int, val any) {
	o := obj.(*SpoiTestState)
	switch idx {
	case 0:
		o.tick = val.(int32)
	case 1:
		o.currentMap = val.(string)
	case 2:
		o.players = val.(interface{})
	default:
		panic("invalid field index for SpoiTestState")
	}
}

// ============================================================
// SpoiItemAccessor
// ============================================================

type SpoiItemAccessor struct{}

func (a SpoiItemAccessor) FieldCount() int {
	return 2
}

func (a SpoiItemAccessor) GetField(obj any, idx int) any {
	o := obj.(*SpoiItem)
	switch idx {
	case 0:
		return o.name
	case 1:
		return o.value
	default:
		panic("invalid field index for SpoiItem")
	}
}

func (a SpoiItemAccessor) SetField(obj any, idx int, val any) {
	o := obj.(*SpoiItem)
	switch idx {
	case 0:
		o.name = val.(string)
	case 1:
		o.value = val.(int32)
	default:
		panic("invalid field index for SpoiItem")
	}
}

// ============================================================
// SpoiInventoryAccessor
// ============================================================

type SpoiInventoryAccessor struct{}

func (a SpoiInventoryAccessor) FieldCount() int {
	return 3
}

func (a SpoiInventoryAccessor) GetField(obj any, idx int) any {
	o := obj.(*SpoiInventory)
	switch idx {
	case 0:
		return o.items
	case 1:
		return o.equipped
	case 2:
		return o.gold
	default:
		panic("invalid field index for SpoiInventory")
	}
}

func (a SpoiInventoryAccessor) SetField(obj any, idx int, val any) {
	o := obj.(*SpoiInventory)
	switch idx {
	case 0:
		o.items = val.(interface{})
	case 1:
		o.equipped = val.(interface{})
	case 2:
		o.gold = val.(int32)
	default:
		panic("invalid field index for SpoiInventory")
	}
}

// ============================================================
// SpoiCharacterAccessor
// ============================================================

type SpoiCharacterAccessor struct{}

func (a SpoiCharacterAccessor) FieldCount() int {
	return 5
}

func (a SpoiCharacterAccessor) GetField(obj any, idx int) any {
	o := obj.(*SpoiCharacter)
	switch idx {
	case 0:
		return o.name
	case 1:
		return o.hp
	case 2:
		return o.inventory
	case 3:
		return o.weapon
	case 4:
		return o.petLevel
	default:
		panic("invalid field index for SpoiCharacter")
	}
}

func (a SpoiCharacterAccessor) SetField(obj any, idx int, val any) {
	o := obj.(*SpoiCharacter)
	switch idx {
	case 0:
		o.name = val.(string)
	case 1:
		o.hp = val.(int32)
	case 2:
		o.inventory = val.(interface{})
	case 3:
		o.weapon = val.(interface{})
	case 4:
		o.petLevel = val.(int32)
	default:
		panic("invalid field index for SpoiCharacter")
	}
}

// ============================================================
// SpoiWorldAccessor
// ============================================================

type SpoiWorldAccessor struct{}

func (a SpoiWorldAccessor) FieldCount() int {
	return 3
}

func (a SpoiWorldAccessor) GetField(obj any, idx int) any {
	o := obj.(*SpoiWorld)
	switch idx {
	case 0:
		return o.worldName
	case 1:
		return o.tick
	case 2:
		return o.characters
	default:
		panic("invalid field index for SpoiWorld")
	}
}

func (a SpoiWorldAccessor) SetField(obj any, idx int, val any) {
	o := obj.(*SpoiWorld)
	switch idx {
	case 0:
		o.worldName = val.(string)
	case 1:
		o.tick = val.(int32)
	case 2:
		o.characters = val.(interface{})
	default:
		panic("invalid field index for SpoiWorld")
	}
}

// ============================================================
// SpoiAccessorRegistry — 静态类型注册表
// 替代运行时 map[string][]string
// ============================================================

var SpoiAccessorRegistry = map[string]SpoiAccessor{
	"SpoiTestPlayer": SpoiTestPlayerAccessor{},
	"SpoiTestState": SpoiTestStateAccessor{},
	"SpoiItem": SpoiItemAccessor{},
	"SpoiInventory": SpoiInventoryAccessor{},
	"SpoiCharacter": SpoiCharacterAccessor{},
	"SpoiWorld": SpoiWorldAccessor{},
}

