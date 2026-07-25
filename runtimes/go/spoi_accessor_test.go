// SPOI Accessor 测试
// 运行: go test -v -run "TestTypeId|TestDeserialize|TestAccessor|TestRegistry|TestAccessorIntegration"

package main

import (
	"encoding/binary"
	"math"
	"testing"
)

// ============================================================
// 测试用类型定义（与 spoi_accessor.go 中 accessor 引用的字段匹配）
// ============================================================

type SpoiTestPlayer struct {
	name  string
	hp    int32
	level int32
	posX  float64
}

type SpoiTestState struct {
	tick       int32
	currentMap string
	players    interface{}
}

type SpoiItem struct {
	name  string
	value int32
}

type SpoiInventory struct {
	items    interface{}
	equipped interface{}
	gold     int32
}

type SpoiCharacter struct {
	name      string
	hp        int32
	inventory interface{}
	weapon    interface{}
	petLevel  int32
}

type SpoiWorld struct {
	worldName  string
	tick       int32
	characters interface{}
}

// ============================================================
// 辅助函数：构造 DeserializeValue 的输入数据
// 格式: [type_id(u32 LE) + value_bytes]
// ============================================================

func makeDeserDataU8(v uint8) []byte {
	buf := make([]byte, 5)
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdU8)
	buf[4] = v
	return buf
}

func makeDeserDataU16(v uint16) []byte {
	buf := make([]byte, 6)
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdU16)
	binary.LittleEndian.PutUint16(buf[4:6], v)
	return buf
}

func makeDeserDataU32(v uint32) []byte {
	buf := make([]byte, 8)
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(buf[4:8], v)
	return buf
}

func makeDeserDataU64(v uint64) []byte {
	buf := make([]byte, 12)
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdU64)
	binary.LittleEndian.PutUint64(buf[4:12], v)
	return buf
}

func makeDeserDataI8(v int8) []byte {
	buf := make([]byte, 5)
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdI8)
	buf[4] = byte(v)
	return buf
}

func makeDeserDataI16(v int16) []byte {
	buf := make([]byte, 6)
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdI16)
	binary.LittleEndian.PutUint16(buf[4:6], uint16(v))
	return buf
}

func makeDeserDataI32(v int32) []byte {
	buf := make([]byte, 8)
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdI32)
	binary.LittleEndian.PutUint32(buf[4:8], uint32(v))
	return buf
}

func makeDeserDataI64(v int64) []byte {
	buf := make([]byte, 12)
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdI64)
	binary.LittleEndian.PutUint64(buf[4:12], uint64(v))
	return buf
}

func makeDeserDataF32(v float32) []byte {
	buf := make([]byte, 8)
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdF32)
	binary.LittleEndian.PutUint32(buf[4:8], math.Float32bits(v))
	return buf
}

func makeDeserDataF64(v float64) []byte {
	buf := make([]byte, 12)
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdF64)
	binary.LittleEndian.PutUint64(buf[4:12], math.Float64bits(v))
	return buf
}

func makeDeserDataString(v string) []byte {
	buf := make([]byte, 4+len(v))
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdString)
	copy(buf[4:], v)
	return buf
}

func makeDeserDataBool(v bool) []byte {
	buf := make([]byte, 5)
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdBool)
	if v {
		buf[4] = 1
	} else {
		buf[4] = 0
	}
	return buf
}

// ============================================================
// 1. TypeId 常量测试
// ============================================================

func TestTypeIdConstants(t *testing.T) {
	if TypeIdU8 != 26 {
		t.Errorf("TypeIdU8: expected 26, got %d", TypeIdU8)
	}
	if TypeIdU16 != 27 {
		t.Errorf("TypeIdU16: expected 27, got %d", TypeIdU16)
	}
	if TypeIdU32 != 28 {
		t.Errorf("TypeIdU32: expected 28, got %d", TypeIdU32)
	}
	if TypeIdU64 != 29 {
		t.Errorf("TypeIdU64: expected 29, got %d", TypeIdU64)
	}
	if TypeIdI8 != 30 {
		t.Errorf("TypeIdI8: expected 30, got %d", TypeIdI8)
	}
	if TypeIdI16 != 31 {
		t.Errorf("TypeIdI16: expected 31, got %d", TypeIdI16)
	}
	if TypeIdI32 != 32 {
		t.Errorf("TypeIdI32: expected 32, got %d", TypeIdI32)
	}
	if TypeIdI64 != 33 {
		t.Errorf("TypeIdI64: expected 33, got %d", TypeIdI64)
	}
	if TypeIdF32 != 34 {
		t.Errorf("TypeIdF32: expected 34, got %d", TypeIdF32)
	}
	if TypeIdF64 != 35 {
		t.Errorf("TypeIdF64: expected 35, got %d", TypeIdF64)
	}
	if TypeIdString != 9 {
		t.Errorf("TypeIdString: expected 9, got %d", TypeIdString)
	}
	if TypeIdBool != 40 {
		t.Errorf("TypeIdBool: expected 40, got %d", TypeIdBool)
	}
}

// ============================================================
// 2. DeserializeValue 测试
// ============================================================

func TestDeserializeValueU8(t *testing.T) {
	result := DeserializeValue(makeDeserDataU8(42))
	if result != uint8(42) {
		t.Errorf("U8: expected 42, got %v (type %T)", result, result)
	}
}

func TestDeserializeValueU8Boundary(t *testing.T) {
	// 边界值
	result := DeserializeValue(makeDeserDataU8(0))
	if result != uint8(0) {
		t.Errorf("U8 min: expected 0, got %v", result)
	}
	result = DeserializeValue(makeDeserDataU8(255))
	if result != uint8(255) {
		t.Errorf("U8 max: expected 255, got %v", result)
	}
}

func TestDeserializeValueU16(t *testing.T) {
	result := DeserializeValue(makeDeserDataU16(1000))
	if result != uint16(1000) {
		t.Errorf("U16: expected 1000, got %v (type %T)", result, result)
	}
}

func TestDeserializeValueU16Boundary(t *testing.T) {
	result := DeserializeValue(makeDeserDataU16(0))
	if result != uint16(0) {
		t.Errorf("U16 min: expected 0, got %v", result)
	}
	result = DeserializeValue(makeDeserDataU16(0xFFFF))
	if result != uint16(0xFFFF) {
		t.Errorf("U16 max: expected 0xFFFF, got %v", result)
	}
}

func TestDeserializeValueU32(t *testing.T) {
	result := DeserializeValue(makeDeserDataU32(123456))
	if result != uint32(123456) {
		t.Errorf("U32: expected 123456, got %v (type %T)", result, result)
	}
}

func TestDeserializeValueU32Boundary(t *testing.T) {
	result := DeserializeValue(makeDeserDataU32(0xFFFFFFFF))
	if result != uint32(0xFFFFFFFF) {
		t.Errorf("U32 max: expected 0xFFFFFFFF, got %v", result)
	}
}

func TestDeserializeValueU64(t *testing.T) {
	result := DeserializeValue(makeDeserDataU64(0x0807060504030201))
	if result != uint64(0x0807060504030201) {
		t.Errorf("U64: expected 0x0807060504030201, got %v (type %T)", result, result)
	}
}

func TestDeserializeValueU64Boundary(t *testing.T) {
	result := DeserializeValue(makeDeserDataU64(0xFFFFFFFFFFFFFFFF))
	if result != uint64(0xFFFFFFFFFFFFFFFF) {
		t.Errorf("U64 max: expected 0xFFFFFFFFFFFFFFFF, got %v", result)
	}
}

func TestDeserializeValueI8(t *testing.T) {
	result := DeserializeValue(makeDeserDataI8(-42))
	if result != int8(-42) {
		t.Errorf("I8: expected -42, got %v (type %T)", result, result)
	}
}

func TestDeserializeValueI8Boundary(t *testing.T) {
	result := DeserializeValue(makeDeserDataI8(-128))
	if result != int8(-128) {
		t.Errorf("I8 min: expected -128, got %v", result)
	}
	result = DeserializeValue(makeDeserDataI8(127))
	if result != int8(127) {
		t.Errorf("I8 max: expected 127, got %v", result)
	}
}

func TestDeserializeValueI16(t *testing.T) {
	result := DeserializeValue(makeDeserDataI16(-1000))
	if result != int16(-1000) {
		t.Errorf("I16: expected -1000, got %v (type %T)", result, result)
	}
}

func TestDeserializeValueI16Boundary(t *testing.T) {
	result := DeserializeValue(makeDeserDataI16(-32768))
	if result != int16(-32768) {
		t.Errorf("I16 min: expected -32768, got %v", result)
	}
	result = DeserializeValue(makeDeserDataI16(32767))
	if result != int16(32767) {
		t.Errorf("I16 max: expected 32767, got %v", result)
	}
}

func TestDeserializeValueI32(t *testing.T) {
	result := DeserializeValue(makeDeserDataI32(-123456))
	if result != int32(-123456) {
		t.Errorf("I32: expected -123456, got %v (type %T)", result, result)
	}
}

func TestDeserializeValueI32Boundary(t *testing.T) {
	result := DeserializeValue(makeDeserDataI32(-2147483648))
	if result != int32(-2147483648) {
		t.Errorf("I32 min: expected -2147483648, got %v", result)
	}
	result = DeserializeValue(makeDeserDataI32(2147483647))
	if result != int32(2147483647) {
		t.Errorf("I32 max: expected 2147483647, got %v", result)
	}
}

func TestDeserializeValueI64(t *testing.T) {
	result := DeserializeValue(makeDeserDataI64(-1234567890123))
	if result != int64(-1234567890123) {
		t.Errorf("I64: expected -1234567890123, got %v (type %T)", result, result)
	}
}

func TestDeserializeValueF32(t *testing.T) {
	result := DeserializeValue(makeDeserDataF32(3.14))
	if result != float32(3.14) {
		t.Errorf("F32: expected 3.14, got %v (type %T)", result, result)
	}
}

func TestDeserializeValueF32Negative(t *testing.T) {
	result := DeserializeValue(makeDeserDataF32(-1.5))
	if result != float32(-1.5) {
		t.Errorf("F32 negative: expected -1.5, got %v", result)
	}
}

func TestDeserializeValueF64(t *testing.T) {
	result := DeserializeValue(makeDeserDataF64(3.141592653589793))
	if result != 3.141592653589793 {
		t.Errorf("F64: expected 3.141592653589793, got %v (type %T)", result, result)
	}
}

func TestDeserializeValueF64Negative(t *testing.T) {
	result := DeserializeValue(makeDeserDataF64(-2.718281828))
	if result != -2.718281828 {
		t.Errorf("F64 negative: expected -2.718281828, got %v", result)
	}
}

func TestDeserializeValueString(t *testing.T) {
	result := DeserializeValue(makeDeserDataString("hello world"))
	if result != "hello world" {
		t.Errorf("String: expected 'hello world', got '%v' (type %T)", result, result)
	}
}

func TestDeserializeValueStringEmpty(t *testing.T) {
	result := DeserializeValue(makeDeserDataString(""))
	if result != "" {
		t.Errorf("String empty: expected '', got '%v'", result)
	}
}

func TestDeserializeValueStringUnicode(t *testing.T) {
	s := "你好世界 — 测试"
	result := DeserializeValue(makeDeserDataString(s))
	if result != s {
		t.Errorf("String unicode: expected '%s', got '%v'", s, result)
	}
}

func TestDeserializeValueBoolTrue(t *testing.T) {
	result := DeserializeValue(makeDeserDataBool(true))
	if result != true {
		t.Errorf("Bool true: expected true, got %v (type %T)", result, result)
	}
}

func TestDeserializeValueBoolFalse(t *testing.T) {
	result := DeserializeValue(makeDeserDataBool(false))
	if result != false {
		t.Errorf("Bool false: expected false, got %v (type %T)", result, result)
	}
}

func TestDeserializeValueEmptyData(t *testing.T) {
	result := DeserializeValue([]byte{})
	if result != nil {
		t.Errorf("Empty data: expected nil, got %v", result)
	}
}

func TestDeserializeValueShortData(t *testing.T) {
	// 少于 4 字节的数据
	result := DeserializeValue([]byte{0x01, 0x02})
	if result != nil {
		t.Errorf("Short data: expected nil, got %v", result)
	}
}

func TestDeserializeValueUnknownType(t *testing.T) {
	// 未知 type_id 返回原始字节
	buf := make([]byte, 7)
	binary.LittleEndian.PutUint32(buf[0:4], 999) // 未知 type_id
	buf[4] = 'A'
	buf[5] = 'B'
	buf[6] = 'C'
	result := DeserializeValue(buf)
	bytes, ok := result.([]byte)
	if !ok {
		t.Errorf("Unknown type: expected []byte, got %T", result)
	}
	if len(bytes) != 3 {
		t.Errorf("Unknown type: expected 3 bytes, got %d", len(bytes))
	}
	if bytes[0] != 'A' || bytes[1] != 'B' || bytes[2] != 'C' {
		t.Errorf("Unknown type: expected 'ABC', got %v", bytes)
	}
}

// ============================================================
// 3. Accessor 测试
// ============================================================

// ----- SpoiTestPlayerAccessor -----

func TestPlayerAccessorFieldCount(t *testing.T) {
	acc := SpoiTestPlayerAccessor{}
	if acc.FieldCount() != 4 {
		t.Errorf("PlayerAccessor FieldCount: expected 4, got %d", acc.FieldCount())
	}
}

func TestPlayerAccessorGetField(t *testing.T) {
	acc := SpoiTestPlayerAccessor{}
	p := &SpoiTestPlayer{name: "Hero", hp: 100, level: 5, posX: 12.5}

	if v := acc.GetField(p, 0); v != "Hero" {
		t.Errorf("GetField(0) name: expected 'Hero', got %v", v)
	}
	if v := acc.GetField(p, 1); v != int32(100) {
		t.Errorf("GetField(1) hp: expected 100, got %v", v)
	}
	if v := acc.GetField(p, 2); v != int32(5) {
		t.Errorf("GetField(2) level: expected 5, got %v", v)
	}
	if v := acc.GetField(p, 3); v != float64(12.5) {
		t.Errorf("GetField(3) posX: expected 12.5, got %v", v)
	}
}

func TestPlayerAccessorSetField(t *testing.T) {
	acc := SpoiTestPlayerAccessor{}
	p := &SpoiTestPlayer{}

	acc.SetField(p, 0, "Warrior")
	acc.SetField(p, 1, int32(200))
	acc.SetField(p, 2, int32(10))
	acc.SetField(p, 3, float64(25.0))

	if p.name != "Warrior" {
		t.Errorf("SetField(0) name: expected 'Warrior', got '%s'", p.name)
	}
	if p.hp != 200 {
		t.Errorf("SetField(1) hp: expected 200, got %d", p.hp)
	}
	if p.level != 10 {
		t.Errorf("SetField(2) level: expected 10, got %d", p.level)
	}
	if p.posX != 25.0 {
		t.Errorf("SetField(3) posX: expected 25.0, got %f", p.posX)
	}
}

func TestPlayerAccessorGetFieldInvalidIndex(t *testing.T) {
	acc := SpoiTestPlayerAccessor{}
	p := &SpoiTestPlayer{name: "Hero"}

	panicked := false
	func() {
		defer func() {
			if r := recover(); r != nil {
				panicked = true
			}
		}()
		acc.GetField(p, 99)
	}()
	if !panicked {
		t.Error("GetField with invalid index should panic")
	}
}

func TestPlayerAccessorSetFieldInvalidIndex(t *testing.T) {
	acc := SpoiTestPlayerAccessor{}
	p := &SpoiTestPlayer{}

	panicked := false
	func() {
		defer func() {
			if r := recover(); r != nil {
				panicked = true
			}
		}()
		acc.SetField(p, 99, "test")
	}()
	if !panicked {
		t.Error("SetField with invalid index should panic")
	}
}

// ----- SpoiTestStateAccessor -----

func TestStateAccessorFieldCount(t *testing.T) {
	acc := SpoiTestStateAccessor{}
	if acc.FieldCount() != 3 {
		t.Errorf("StateAccessor FieldCount: expected 3, got %d", acc.FieldCount())
	}
}

func TestStateAccessorGetField(t *testing.T) {
	acc := SpoiTestStateAccessor{}
	s := &SpoiTestState{tick: 42, currentMap: "level1", players: []int{1, 2, 3}}

	if v := acc.GetField(s, 0); v != int32(42) {
		t.Errorf("GetField(0) tick: expected 42, got %v", v)
	}
	if v := acc.GetField(s, 1); v != "level1" {
		t.Errorf("GetField(1) currentMap: expected 'level1', got %v", v)
	}
	players := acc.GetField(s, 2)
	if players == nil {
		t.Error("GetField(2) players: expected non-nil")
	}
}

func TestStateAccessorSetField(t *testing.T) {
	acc := SpoiTestStateAccessor{}
	s := &SpoiTestState{}

	acc.SetField(s, 0, int32(100))
	acc.SetField(s, 1, "level2")
	acc.SetField(s, 2, interface{}([]int{4, 5, 6}))

	if s.tick != 100 {
		t.Errorf("SetField(0) tick: expected 100, got %d", s.tick)
	}
	if s.currentMap != "level2" {
		t.Errorf("SetField(1) currentMap: expected 'level2', got '%s'", s.currentMap)
	}
}

func TestStateAccessorInvalidIndex(t *testing.T) {
	acc := SpoiTestStateAccessor{}
	s := &SpoiTestState{}

	panicked := false
	func() {
		defer func() {
			if r := recover(); r != nil {
				panicked = true
			}
		}()
		acc.GetField(s, 99)
	}()
	if !panicked {
		t.Error("StateAccessor GetField with invalid index should panic")
	}
}

// ----- SpoiItemAccessor -----

func TestItemAccessorFieldCount(t *testing.T) {
	acc := SpoiItemAccessor{}
	if acc.FieldCount() != 2 {
		t.Errorf("ItemAccessor FieldCount: expected 2, got %d", acc.FieldCount())
	}
}

func TestItemAccessorGetField(t *testing.T) {
	acc := SpoiItemAccessor{}
	item := &SpoiItem{name: "Sword", value: 500}

	if v := acc.GetField(item, 0); v != "Sword" {
		t.Errorf("GetField(0) name: expected 'Sword', got %v", v)
	}
	if v := acc.GetField(item, 1); v != int32(500) {
		t.Errorf("GetField(1) value: expected 500, got %v", v)
	}
}

func TestItemAccessorSetField(t *testing.T) {
	acc := SpoiItemAccessor{}
	item := &SpoiItem{}

	acc.SetField(item, 0, "Shield")
	acc.SetField(item, 1, int32(300))

	if item.name != "Shield" {
		t.Errorf("SetField(0) name: expected 'Shield', got '%s'", item.name)
	}
	if item.value != 300 {
		t.Errorf("SetField(1) value: expected 300, got %d", item.value)
	}
}

func TestItemAccessorInvalidIndex(t *testing.T) {
	acc := SpoiItemAccessor{}
	item := &SpoiItem{}

	panicked := false
	func() {
		defer func() {
			if r := recover(); r != nil {
				panicked = true
			}
		}()
		acc.GetField(item, 99)
	}()
	if !panicked {
		t.Error("ItemAccessor GetField with invalid index should panic")
	}
}

// ----- SpoiInventoryAccessor -----

func TestInventoryAccessorFieldCount(t *testing.T) {
	acc := SpoiInventoryAccessor{}
	if acc.FieldCount() != 3 {
		t.Errorf("InventoryAccessor FieldCount: expected 3, got %d", acc.FieldCount())
	}
}

func TestInventoryAccessorGetField(t *testing.T) {
	acc := SpoiInventoryAccessor{}
	inv := &SpoiInventory{items: []string{"a", "b"}, equipped: "sword", gold: 1000}

	if v := acc.GetField(inv, 0); v == nil {
		t.Error("GetField(0) items: expected non-nil")
	}
	if v := acc.GetField(inv, 1); v != "sword" {
		t.Errorf("GetField(1) equipped: expected 'sword', got %v", v)
	}
	if v := acc.GetField(inv, 2); v != int32(1000) {
		t.Errorf("GetField(2) gold: expected 1000, got %v", v)
	}
}

func TestInventoryAccessorSetField(t *testing.T) {
	acc := SpoiInventoryAccessor{}
	inv := &SpoiInventory{}

	acc.SetField(inv, 0, interface{}([]string{"x", "y"}))
	acc.SetField(inv, 1, interface{}("axe"))
	acc.SetField(inv, 2, int32(2000))

	if inv.gold != 2000 {
		t.Errorf("SetField(2) gold: expected 2000, got %d", inv.gold)
	}
}

func TestInventoryAccessorInvalidIndex(t *testing.T) {
	acc := SpoiInventoryAccessor{}
	inv := &SpoiInventory{}

	panicked := false
	func() {
		defer func() {
			if r := recover(); r != nil {
				panicked = true
			}
		}()
		acc.GetField(inv, 99)
	}()
	if !panicked {
		t.Error("InventoryAccessor GetField with invalid index should panic")
	}
}

// ----- SpoiCharacterAccessor -----

func TestCharacterAccessorFieldCount(t *testing.T) {
	acc := SpoiCharacterAccessor{}
	if acc.FieldCount() != 5 {
		t.Errorf("CharacterAccessor FieldCount: expected 5, got %d", acc.FieldCount())
	}
}

func TestCharacterAccessorGetField(t *testing.T) {
	acc := SpoiCharacterAccessor{}
	c := &SpoiCharacter{name: "Gandalf", hp: 500, petLevel: 3}

	if v := acc.GetField(c, 0); v != "Gandalf" {
		t.Errorf("GetField(0) name: expected 'Gandalf', got %v", v)
	}
	if v := acc.GetField(c, 1); v != int32(500) {
		t.Errorf("GetField(1) hp: expected 500, got %v", v)
	}
	if v := acc.GetField(c, 4); v != int32(3) {
		t.Errorf("GetField(4) petLevel: expected 3, got %v", v)
	}
}

func TestCharacterAccessorSetField(t *testing.T) {
	acc := SpoiCharacterAccessor{}
	c := &SpoiCharacter{}

	acc.SetField(c, 0, "Frodo")
	acc.SetField(c, 1, int32(100))
	acc.SetField(c, 2, interface{}("Backpack"))
	acc.SetField(c, 3, interface{}("Sting"))
	acc.SetField(c, 4, int32(5))

	if c.name != "Frodo" {
		t.Errorf("SetField(0) name: expected 'Frodo', got '%s'", c.name)
	}
	if c.hp != 100 {
		t.Errorf("SetField(1) hp: expected 100, got %d", c.hp)
	}
	if c.weapon != "Sting" {
		t.Errorf("SetField(3) weapon: expected 'Sting', got '%v'", c.weapon)
	}
	if c.petLevel != 5 {
		t.Errorf("SetField(4) petLevel: expected 5, got %d", c.petLevel)
	}
}

func TestCharacterAccessorInvalidIndex(t *testing.T) {
	acc := SpoiCharacterAccessor{}
	c := &SpoiCharacter{}

	panicked := false
	func() {
		defer func() {
			if r := recover(); r != nil {
				panicked = true
			}
		}()
		acc.GetField(c, 99)
	}()
	if !panicked {
		t.Error("CharacterAccessor GetField with invalid index should panic")
	}
}

// ----- SpoiWorldAccessor -----

func TestWorldAccessorFieldCount(t *testing.T) {
	acc := SpoiWorldAccessor{}
	if acc.FieldCount() != 3 {
		t.Errorf("WorldAccessor FieldCount: expected 3, got %d", acc.FieldCount())
	}
}

func TestWorldAccessorGetField(t *testing.T) {
	acc := SpoiWorldAccessor{}
	w := &SpoiWorld{worldName: "MiddleEarth", tick: 1000, characters: []string{"A", "B"}}

	if v := acc.GetField(w, 0); v != "MiddleEarth" {
		t.Errorf("GetField(0) worldName: expected 'MiddleEarth', got %v", v)
	}
	if v := acc.GetField(w, 1); v != int32(1000) {
		t.Errorf("GetField(1) tick: expected 1000, got %v", v)
	}
	if v := acc.GetField(w, 2); v == nil {
		t.Error("GetField(2) characters: expected non-nil")
	}
}

func TestWorldAccessorSetField(t *testing.T) {
	acc := SpoiWorldAccessor{}
	w := &SpoiWorld{}

	acc.SetField(w, 0, "NewWorld")
	acc.SetField(w, 1, int32(500))
	acc.SetField(w, 2, interface{}([]string{"X", "Y", "Z"}))

	if w.worldName != "NewWorld" {
		t.Errorf("SetField(0) worldName: expected 'NewWorld', got '%s'", w.worldName)
	}
	if w.tick != 500 {
		t.Errorf("SetField(1) tick: expected 500, got %d", w.tick)
	}
}

func TestWorldAccessorInvalidIndex(t *testing.T) {
	acc := SpoiWorldAccessor{}
	w := &SpoiWorld{}

	panicked := false
	func() {
		defer func() {
			if r := recover(); r != nil {
				panicked = true
			}
		}()
		acc.GetField(w, 99)
	}()
	if !panicked {
		t.Error("WorldAccessor GetField with invalid index should panic")
	}
}

// ============================================================
// 4. SpoiAccessorRegistry 测试
// ============================================================

func TestRegistryContainsAllTypes(t *testing.T) {
	expectedTypes := []string{
		"SpoiTestPlayer",
		"SpoiTestState",
		"SpoiItem",
		"SpoiInventory",
		"SpoiCharacter",
		"SpoiWorld",
	}

	if len(SpoiAccessorRegistry) != len(expectedTypes) {
		t.Errorf("Registry size: expected %d, got %d", len(expectedTypes), len(SpoiAccessorRegistry))
	}

	for _, typeName := range expectedTypes {
		acc, ok := SpoiAccessorRegistry[typeName]
		if !ok {
			t.Errorf("Registry missing type: %s", typeName)
			continue
		}
		if acc == nil {
			t.Errorf("Registry accessor for %s is nil", typeName)
		}
	}
}

func TestRegistryLookupByTypeName(t *testing.T) {
	// 验证可以通过类型名查找 accessor
	acc, ok := SpoiAccessorRegistry["SpoiTestPlayer"]
	if !ok {
		t.Fatal("SpoiTestPlayer not found in registry")
	}
	if acc.FieldCount() != 4 {
		t.Errorf("SpoiTestPlayer FieldCount: expected 4, got %d", acc.FieldCount())
	}

	acc, ok = SpoiAccessorRegistry["SpoiItem"]
	if !ok {
		t.Fatal("SpoiItem not found in registry")
	}
	if acc.FieldCount() != 2 {
		t.Errorf("SpoiItem FieldCount: expected 2, got %d", acc.FieldCount())
	}
}

func TestRegistryNonexistentType(t *testing.T) {
	_, ok := SpoiAccessorRegistry["NonExistentType"]
	if ok {
		t.Error("NonExistentType should not be in registry")
	}
}

// ============================================================
// 5. Executor 集成测试（使用 accessor 注册表）
// ============================================================

// 辅助函数：构造 SET 指令的操作数
func makeAccessorSetOperandString(value string) []byte {
	operand := make([]byte, 4+len(value))
	binary.LittleEndian.PutUint32(operand[0:4], TypeIdString)
	copy(operand[4:], value)
	return operand
}

func makeAccessorSetOperandI32(value int32) []byte {
	operand := make([]byte, 8)
	binary.LittleEndian.PutUint32(operand[0:4], TypeIdI32)
	binary.LittleEndian.PutUint32(operand[4:8], uint32(value))
	return operand
}

func makeAccessorSetOperandF64(value float64) []byte {
	operand := make([]byte, 12)
	binary.LittleEndian.PutUint32(operand[0:4], TypeIdF64)
	binary.LittleEndian.PutUint64(operand[4:12], math.Float64bits(value))
	return operand
}

func makeAccessorFilterInst(memberIdx uint32, cmpOp byte, value int32) SpoiInstruction {
	// 格式: memberIdx(u32) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
	valuePart := make([]byte, 8)
	binary.LittleEndian.PutUint32(valuePart[0:4], TypeIdI32)
	binary.LittleEndian.PutUint32(valuePart[4:8], uint32(value))
	operand := make([]byte, 5+1+len(valuePart))
	binary.LittleEndian.PutUint32(operand[0:4], memberIdx)
	operand[4] = cmpOp
	operand[5] = byte(len(valuePart))
	copy(operand[6:], valuePart)
	return makeInst(OP_FILTER, nil, operand)
}

func TestAccessorIntegrationSetPlayerName(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)
	player := &SpoiTestPlayer{name: "OldName", hp: 100, level: 5, posX: 1.0}

	instructions := []SpoiInstruction{
		{Op: OP_SET, Path: []int{0}, Operand: makeAccessorSetOperandString("NewName")},
	}
	data := buildSpoiStream(instructions)
	executor.Execute(player, data)

	if player.name != "NewName" {
		t.Errorf("SET name: expected 'NewName', got '%s'", player.name)
	}
}

func TestAccessorIntegrationSetPlayerHp(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)
	player := &SpoiTestPlayer{name: "Hero", hp: 100, level: 5, posX: 1.0}

	instructions := []SpoiInstruction{
		{Op: OP_SET, Path: []int{1}, Operand: makeAccessorSetOperandI32(200)},
	}
	data := buildSpoiStream(instructions)
	executor.Execute(player, data)

	if player.hp != 200 {
		t.Errorf("SET hp: expected 200, got %d", player.hp)
	}
}

func TestAccessorIntegrationSetMultipleFields(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)
	player := &SpoiTestPlayer{name: "X", hp: 1, level: 1, posX: 0.0}

	instructions := []SpoiInstruction{
		{Op: OP_SET, Path: []int{0}, Operand: makeAccessorSetOperandString("Warrior")},
		{Op: OP_SET, Path: []int{1}, Operand: makeAccessorSetOperandI32(99)},
		{Op: OP_SET, Path: []int{2}, Operand: makeAccessorSetOperandI32(50)},
		{Op: OP_SET, Path: []int{3}, Operand: makeAccessorSetOperandF64(12.5)},
	}
	data := buildSpoiStream(instructions)
	executor.Execute(player, data)

	if player.name != "Warrior" {
		t.Errorf("SET name: expected 'Warrior', got '%s'", player.name)
	}
	if player.hp != 99 {
		t.Errorf("SET hp: expected 99, got %d", player.hp)
	}
	if player.level != 50 {
		t.Errorf("SET level: expected 50, got %d", player.level)
	}
	if player.posX != 12.5 {
		t.Errorf("SET posX: expected 12.5, got %f", player.posX)
	}
}

func TestAccessorIntegrationSetItem(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)
	item := &SpoiItem{name: "Old", value: 0}

	instructions := []SpoiInstruction{
		{Op: OP_SET, Path: []int{0}, Operand: makeAccessorSetOperandString("Potion")},
		{Op: OP_SET, Path: []int{1}, Operand: makeAccessorSetOperandI32(100)},
	}
	data := buildSpoiStream(instructions)
	executor.Execute(item, data)

	if item.name != "Potion" {
		t.Errorf("SET item name: expected 'Potion', got '%s'", item.name)
	}
	if item.value != 100 {
		t.Errorf("SET item value: expected 100, got %d", item.value)
	}
}

func TestAccessorIntegrationSetWorld(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)
	world := &SpoiWorld{worldName: "Old", tick: 0}

	instructions := []SpoiInstruction{
		{Op: OP_SET, Path: []int{0}, Operand: makeAccessorSetOperandString("Earth")},
		{Op: OP_SET, Path: []int{1}, Operand: makeAccessorSetOperandI32(1000)},
	}
	data := buildSpoiStream(instructions)
	executor.Execute(world, data)

	if world.worldName != "Earth" {
		t.Errorf("SET worldName: expected 'Earth', got '%s'", world.worldName)
	}
	if world.tick != 1000 {
		t.Errorf("SET tick: expected 1000, got %d", world.tick)
	}
}

func TestAccessorIntegrationPipeAndSelect(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	players := []interface{}{
		&SpoiTestPlayer{name: "Alice", hp: 100, level: 10, posX: 1.0},
		&SpoiTestPlayer{name: "Bob", hp: 200, level: 20, posX: 2.0},
		&SpoiTestPlayer{name: "Carol", hp: 300, level: 30, posX: 3.0},
	}

	// PIPE → SELECT name (index 0) → EXEC
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		selectInst([]int{0}),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if len(values) != 3 {
		t.Fatalf("SELECT names: expected 3, got %d", len(values))
	}
	if values[0] != "Alice" {
		t.Errorf("SELECT[0]: expected 'Alice', got '%v'", values[0])
	}
	if values[1] != "Bob" {
		t.Errorf("SELECT[1]: expected 'Bob', got '%v'", values[1])
	}
	if values[2] != "Carol" {
		t.Errorf("SELECT[2]: expected 'Carol', got '%v'", values[2])
	}
}

func TestAccessorIntegrationSelectLevel(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	players := []interface{}{
		&SpoiTestPlayer{name: "Alice", hp: 100, level: 10, posX: 1.0},
		&SpoiTestPlayer{name: "Bob", hp: 200, level: 20, posX: 2.0},
	}

	// PIPE → SELECT level (index 2) → EXEC
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		selectInst([]int{2}),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if values[0] != int32(10) {
		t.Errorf("SELECT level[0]: expected 10, got %v", values[0])
	}
	if values[1] != int32(20) {
		t.Errorf("SELECT level[1]: expected 20, got %v", values[1])
	}
}

func TestAccessorIntegrationFilterByLevel(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	players := []interface{}{
		&SpoiTestPlayer{name: "Alice", hp: 100, level: 3, posX: 1.0},
		&SpoiTestPlayer{name: "Bob", hp: 200, level: 10, posX: 2.0},
		&SpoiTestPlayer{name: "Carol", hp: 300, level: 7, posX: 3.0},
		&SpoiTestPlayer{name: "Dave", hp: 400, level: 15, posX: 4.0},
	}

	// FILTER level > 5 (memberIdx=2), using SpoiTestPlayer accessor
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		makeAccessorFilterInst(2, CMP_GT, 5),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if len(values) != 3 {
		t.Errorf("FILTER level>5: expected 3, got %d", len(values))
	}
}

func TestAccessorIntegrationFilterByName(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	items := []interface{}{
		&SpoiItem{name: "Sword", value: 100},
		&SpoiItem{name: "Shield", value: 200},
		&SpoiItem{name: "Sword", value: 300},
	}

	// 使用 filterEq 测试 name == "Sword" (memberIdx=0)
	// 由于 filterEq 使用 TypeIdU32，这里需要构造字符串比较的 filter
	valuePart := make([]byte, 4+5) // TypeIdString + "Sword"
	binary.LittleEndian.PutUint32(valuePart[0:4], TypeIdString)
	copy(valuePart[4:], "Sword")
	operand := make([]byte, 5+1+len(valuePart))
	binary.LittleEndian.PutUint32(operand[0:4], 0) // memberIdx=0
	operand[4] = CMP_EQ
	operand[5] = byte(len(valuePart))
	copy(operand[6:], valuePart)

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		{Op: OP_FILTER, Path: nil, Operand: operand},
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(items, data)

	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("FILTER name=='Sword': expected 2, got %d", len(values))
	}
}

func TestAccessorIntegrationSortByHp(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	players := []interface{}{
		&SpoiTestPlayer{name: "Carol", hp: 300, level: 30, posX: 3.0},
		&SpoiTestPlayer{name: "Alice", hp: 100, level: 10, posX: 1.0},
		&SpoiTestPlayer{name: "Bob", hp: 200, level: 20, posX: 2.0},
	}

	// SORT by hp (index 1)
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		sortInst([]int{1}),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	first := values[0].(*SpoiTestPlayer)
	if first.name != "Alice" {
		t.Errorf("SORT by hp[0]: expected 'Alice', got '%s'", first.name)
	}
	second := values[1].(*SpoiTestPlayer)
	if second.name != "Bob" {
		t.Errorf("SORT by hp[1]: expected 'Bob', got '%s'", second.name)
	}
	third := values[2].(*SpoiTestPlayer)
	if third.name != "Carol" {
		t.Errorf("SORT by hp[2]: expected 'Carol', got '%s'", third.name)
	}
}

func TestAccessorIntegrationCount(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	players := []interface{}{
		&SpoiTestPlayer{name: "Alice", hp: 100, level: 10, posX: 1.0},
		&SpoiTestPlayer{name: "Bob", hp: 200, level: 20, posX: 2.0},
		&SpoiTestPlayer{name: "Carol", hp: 300, level: 30, posX: 3.0},
	}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		countInst(),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("COUNT: expected RESULT_SINGLE, got %v", result["resultType"])
	}
	if result["value"] != 3 {
		t.Errorf("COUNT: expected 3, got %v", result["value"])
	}
}

func TestAccessorIntegrationPipeline(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	players := []interface{}{
		&SpoiTestPlayer{name: "Alice", hp: 100, level: 3, posX: 1.0},
		&SpoiTestPlayer{name: "Bob", hp: 200, level: 10, posX: 2.0},
		&SpoiTestPlayer{name: "Carol", hp: 300, level: 7, posX: 3.0},
		&SpoiTestPlayer{name: "Dave", hp: 400, level: 15, posX: 4.0},
		&SpoiTestPlayer{name: "Eve", hp: 500, level: 12, posX: 5.0},
	}

	// PIPE → FILTER(level > 5) → SELECT(name) → TAKE(2) → EXEC
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		makeAccessorFilterInst(2, CMP_GT, 5),
		selectInst([]int{0}),
		takeInst(2),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Fatalf("Pipeline: expected 2 results, got %d", len(values))
	}
	// Bob (level 10) and Carol (level 7) should be first two after filter
	if values[0] != "Bob" {
		t.Errorf("Pipeline[0]: expected 'Bob', got '%v'", values[0])
	}
	if values[0] != "Bob" && values[1] != "Bob" {
		// at least one should be Bob
		t.Errorf("Pipeline: 'Bob' not found in results")
	}
}

func TestAccessorIntegrationCharacterSet(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)
	char := &SpoiCharacter{name: "Old", hp: 10, petLevel: 1}

	instructions := []SpoiInstruction{
		{Op: OP_SET, Path: []int{0}, Operand: makeAccessorSetOperandString("Mage")},
		{Op: OP_SET, Path: []int{1}, Operand: makeAccessorSetOperandI32(500)},
		{Op: OP_SET, Path: []int{4}, Operand: makeAccessorSetOperandI32(99)},
	}
	data := buildSpoiStream(instructions)
	executor.Execute(char, data)

	if char.name != "Mage" {
		t.Errorf("Character SET name: expected 'Mage', got '%s'", char.name)
	}
	if char.hp != 500 {
		t.Errorf("Character SET hp: expected 500, got %d", char.hp)
	}
	if char.petLevel != 99 {
		t.Errorf("Character SET petLevel: expected 99, got %d", char.petLevel)
	}
}

func TestAccessorIntegrationInventorySet(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)
	inv := &SpoiInventory{gold: 50}

	instructions := []SpoiInstruction{
		{Op: OP_SET, Path: []int{2}, Operand: makeAccessorSetOperandI32(9999)},
	}
	data := buildSpoiStream(instructions)
	executor.Execute(inv, data)

	if inv.gold != 9999 {
		t.Errorf("Inventory SET gold: expected 9999, got %d", inv.gold)
	}
}

func TestAccessorIntegrationStateSelect(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	state := &SpoiTestState{tick: 42, currentMap: "level1", players: []string{"A", "B"}}

	// PIPE → SELECT tick (index 0) → EXEC
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		selectInst([]int{0}),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(state, data)

	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("State SELECT tick: expected RESULT_SINGLE, got %v", result["resultType"])
	}
	if result["value"] != int32(42) {
		t.Errorf("State SELECT tick: expected 42, got %v", result["value"])
	}
}

func TestAccessorIntegrationWorldSelect(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	world := &SpoiWorld{worldName: "Mars", tick: 500}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		selectInst([]int{0}),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(world, data)

	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("World SELECT name: expected RESULT_SINGLE, got %v", result["resultType"])
	}
	if result["value"] != "Mars" {
		t.Errorf("World SELECT name: expected 'Mars', got '%v'", result["value"])
	}
}

func TestAccessorIntegrationTakeAndDrop(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	players := []interface{}{
		&SpoiTestPlayer{name: "Alice", hp: 100, level: 10, posX: 1.0},
		&SpoiTestPlayer{name: "Bob", hp: 200, level: 20, posX: 2.0},
		&SpoiTestPlayer{name: "Carol", hp: 300, level: 30, posX: 3.0},
		&SpoiTestPlayer{name: "Dave", hp: 400, level: 40, posX: 4.0},
	}

	// PIPE → DROP 1 → TAKE 2 → EXEC
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		dropInst(1),
		takeInst(2),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Fatalf("DROP+TAKE: expected 2, got %d", len(values))
	}
	if values[0].(*SpoiTestPlayer).name != "Bob" {
		t.Errorf("DROP+TAKE[0]: expected 'Bob', got '%s'", values[0].(*SpoiTestPlayer).name)
	}
	if values[1].(*SpoiTestPlayer).name != "Carol" {
		t.Errorf("DROP+TAKE[1]: expected 'Carol', got '%s'", values[1].(*SpoiTestPlayer).name)
	}
}

func TestAccessorIntegrationReverse(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	players := []interface{}{
		&SpoiTestPlayer{name: "Alice", hp: 100, level: 10, posX: 1.0},
		&SpoiTestPlayer{name: "Bob", hp: 200, level: 20, posX: 2.0},
		&SpoiTestPlayer{name: "Carol", hp: 300, level: 30, posX: 3.0},
	}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		reverseInst(),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if values[0].(*SpoiTestPlayer).name != "Carol" {
		t.Errorf("REVERSE[0]: expected 'Carol', got '%s'", values[0].(*SpoiTestPlayer).name)
	}
	if values[2].(*SpoiTestPlayer).name != "Alice" {
		t.Errorf("REVERSE[2]: expected 'Alice', got '%s'", values[2].(*SpoiTestPlayer).name)
	}
}

func TestAccessorIntegrationDistinct(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	values := []interface{}{int32(1), int32(2), int32(2), int32(3), int32(3), int32(3), int32(4)}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		distinctInst(),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(values, data)

	vals := result["value"].([]interface{})
	if len(vals) != 4 {
		t.Errorf("DISTINCT: expected 4, got %d", len(vals))
	}
}

func TestAccessorIntegrationAny(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	players := []interface{}{
		&SpoiTestPlayer{name: "Alice", hp: 100, level: 3, posX: 1.0},
		&SpoiTestPlayer{name: "Bob", hp: 200, level: 10, posX: 2.0},
		&SpoiTestPlayer{name: "Carol", hp: 300, level: 5, posX: 3.0},
	}

	// ANY level > 5 (memberIdx=2)
	valuePart := make([]byte, 8)
	binary.LittleEndian.PutUint32(valuePart[0:4], TypeIdI32)
	binary.LittleEndian.PutUint32(valuePart[4:8], 5)
	operand := make([]byte, 5+1+len(valuePart))
	binary.LittleEndian.PutUint32(operand[0:4], 2) // memberIdx=2
	operand[4] = CMP_GT
	operand[5] = byte(len(valuePart))
	copy(operand[6:], valuePart)

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		{Op: OP_ANY, Path: nil, Operand: operand},
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["value"] != true {
		t.Errorf("ANY level>5: expected true, got %v", result["value"])
	}
}

func TestAccessorIntegrationAll(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	players := []interface{}{
		&SpoiTestPlayer{name: "Alice", hp: 100, level: 10, posX: 1.0},
		&SpoiTestPlayer{name: "Bob", hp: 200, level: 20, posX: 2.0},
		&SpoiTestPlayer{name: "Carol", hp: 300, level: 30, posX: 3.0},
	}

	// ALL level > 5 (memberIdx=2)
	valuePart := make([]byte, 8)
	binary.LittleEndian.PutUint32(valuePart[0:4], TypeIdI32)
	binary.LittleEndian.PutUint32(valuePart[4:8], 5)
	operand := make([]byte, 5+1+len(valuePart))
	binary.LittleEndian.PutUint32(operand[0:4], 2)
	operand[4] = CMP_GT
	operand[5] = byte(len(valuePart))
	copy(operand[6:], valuePart)

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		{Op: OP_ALL, Path: nil, Operand: operand},
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["value"] != true {
		t.Errorf("ALL level>5: expected true, got %v", result["value"])
	}
}

func TestAccessorIntegrationFind(t *testing.T) {
	executor := NewSpoiExecutor(SpoiAccessorRegistry)

	players := []interface{}{
		&SpoiTestPlayer{name: "Alice", hp: 100, level: 3, posX: 1.0},
		&SpoiTestPlayer{name: "Bob", hp: 200, level: 10, posX: 2.0},
		&SpoiTestPlayer{name: "Carol", hp: 300, level: 15, posX: 3.0},
	}

	// FIND level == 10 (memberIdx=2)
	valuePart := make([]byte, 8)
	binary.LittleEndian.PutUint32(valuePart[0:4], TypeIdI32)
	binary.LittleEndian.PutUint32(valuePart[4:8], 10)
	operand := make([]byte, 5+1+len(valuePart))
	binary.LittleEndian.PutUint32(operand[0:4], 2)
	operand[4] = CMP_EQ
	operand[5] = byte(len(valuePart))
	copy(operand[6:], valuePart)

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		{Op: OP_FIND, Path: nil, Operand: operand},
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("FIND level==10: expected RESULT_SINGLE, got %v", result["resultType"])
	}
	found := result["value"].(*SpoiTestPlayer)
	if found.name != "Bob" {
		t.Errorf("FIND level==10: expected 'Bob', got '%s'", found.name)
	}
}