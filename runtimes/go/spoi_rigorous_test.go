// ============================================================
// SPOI Accessor 刁钻测试 — Go 版
//
// 测试覆盖：
//   1. 数值边界：各类型的最大值/最小值/零值/NaN/Inf
//   2. 字符串边界：空串、Unicode/emoji、null字节、长串、特殊字符
//   3. 反序列化异常：截断数据、无效type_id、空数据
//   4. Accessor 越界/类型不匹配：负索引、超大索引、错误类型
//   5. Executor 组合操作：多层FILTER、空管道、边界组合
//   6. 跨类型 Executor
//   7. Registry 边界
//
// 运行: go test -v -run "TestRigorous" .
// ============================================================

package main

import (
	"encoding/binary"
	"math"
	"testing"
)

// ============================================================
// 辅助函数
// ============================================================

func makeTypedValueRig(typeId uint32, valueBytes []byte) []byte {
	buf := make([]byte, 4+len(valueBytes))
	binary.LittleEndian.PutUint32(buf[0:4], typeId)
	copy(buf[4:], valueBytes)
	return buf
}

func buildSpoiStreamRig(instructions []SpoiInstruction) []byte {
	var buf []byte
	writeVarint(&buf, len(instructions))
	for _, inst := range instructions {
		buf = append(buf, inst.Op)
		writeVarint(&buf, len(inst.Path))
		for _, seg := range inst.Path {
			writeVarint(&buf, seg)
		}
		writeVarint(&buf, len(inst.Operand))
		buf = append(buf, inst.Operand...)
	}
	return buf
}

func makeInstRig(op byte, path []int, operand []byte) SpoiInstruction {
	if path == nil {
		path = []int{}
	}
	if operand == nil {
		operand = []byte{}
	}
	return SpoiInstruction{Op: op, Path: path, Operand: operand}
}

func setInstRig(path []int, typeId uint32, valueBytes []byte) SpoiInstruction {
	return makeInstRig(OP_SET, path, makeTypedValueRig(typeId, valueBytes))
}

func pipeInstRig(path []int) SpoiInstruction {
	return makeInstRig(OP_PIPE, path, nil)
}

func execInstRig() SpoiInstruction {
	return makeInstRig(OP_EXEC, nil, nil)
}

func filterInstRig(path []int, memberIdx uint32, cmpOp byte, typeId uint32, valueBytes []byte) SpoiInstruction {
	typedValue := makeTypedValueRig(typeId, valueBytes)
	var varintBuf []byte
	writeVarint(&varintBuf, len(typedValue))
	operand := make([]byte, 5+len(varintBuf)+len(typedValue))
	binary.LittleEndian.PutUint32(operand[0:4], memberIdx)
	operand[4] = cmpOp
	copy(operand[5:], varintBuf)
	copy(operand[5+len(varintBuf):], typedValue)
	return makeInstRig(OP_FILTER, path, operand)
}

func selectInstRig(path []int) SpoiInstruction {
	return makeInstRig(OP_SELECT, path, nil)
}

func sortInstRig(path []int) SpoiInstruction {
	return makeInstRig(OP_SORT, path, nil)
}

func takeInstRig(n uint32) SpoiInstruction {
	operand := make([]byte, 4)
	binary.LittleEndian.PutUint32(operand, n)
	return makeInstRig(OP_TAKE, nil, operand)
}

func dropInstRig(n uint32) SpoiInstruction {
	operand := make([]byte, 4)
	binary.LittleEndian.PutUint32(operand, n)
	return makeInstRig(OP_DROP, nil, operand)
}

func reverseInstRig() SpoiInstruction {
	return makeInstRig(OP_REVERSE, nil, nil)
}

func distinctInstRig() SpoiInstruction {
	return makeInstRig(OP_DISTINCT, nil, nil)
}

func countInstRig() SpoiInstruction {
	return makeInstRig(OP_COUNT, nil, nil)
}

func i32LE(v int32) []byte {
	buf := make([]byte, 4)
	binary.LittleEndian.PutUint32(buf, uint32(v))
	return buf
}

func f32LE(v float32) []byte {
	buf := make([]byte, 4)
	binary.LittleEndian.PutUint32(buf, math.Float32bits(v))
	return buf
}

func f64LE(v float64) []byte {
	buf := make([]byte, 8)
	binary.LittleEndian.PutUint64(buf, math.Float64bits(v))
	return buf
}

// ============================================================
// 1. 数值边界测试
// ============================================================

func TestRigorous_NumericBoundaries(t *testing.T) {
	t.Run("U8=0", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdU8, []byte{0}))
		if v != uint8(0) {
			t.Errorf("expected 0, got %v (%T)", v, v)
		}
	})
	t.Run("U8=255", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdU8, []byte{255}))
		if v != uint8(255) {
			t.Errorf("expected 255, got %v", v)
		}
	})
	t.Run("U8=128", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdU8, []byte{128}))
		if v != uint8(128) {
			t.Errorf("expected 128, got %v", v)
		}
	})

	t.Run("U16=0", func(t *testing.T) {
		buf := make([]byte, 2)
		binary.LittleEndian.PutUint16(buf, 0)
		v := DeserializeValue(makeTypedValueRig(TypeIdU16, buf))
		if v != uint16(0) {
			t.Errorf("expected 0, got %v", v)
		}
	})
	t.Run("U16=65535", func(t *testing.T) {
		buf := make([]byte, 2)
		binary.LittleEndian.PutUint16(buf, 65535)
		v := DeserializeValue(makeTypedValueRig(TypeIdU16, buf))
		if v != uint16(65535) {
			t.Errorf("expected 65535, got %v", v)
		}
	})

	t.Run("U32=0", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdU32, i32LE(0)))
		if v != uint32(0) {
			t.Errorf("expected 0, got %v", v)
		}
	})
	t.Run("U32=4294967295", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdU32, i32LE(-1)))
		if v != uint32(4294967295) {
			t.Errorf("expected 4294967295, got %v", v)
		}
	})

	t.Run("U64=0", func(t *testing.T) {
		buf := make([]byte, 8)
		v := DeserializeValue(makeTypedValueRig(TypeIdU64, buf))
		if v != uint64(0) {
			t.Errorf("expected 0, got %v", v)
		}
	})
	t.Run("U64=max", func(t *testing.T) {
		buf := make([]byte, 8)
		binary.LittleEndian.PutUint64(buf, 0xFFFFFFFFFFFFFFFF)
		v := DeserializeValue(makeTypedValueRig(TypeIdU64, buf))
		if v != uint64(0xFFFFFFFFFFFFFFFF) {
			t.Errorf("expected max, got %v", v)
		}
	})

	t.Run("I8=-128", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdI8, []byte{128}))
		if v != int8(-128) {
			t.Errorf("expected -128, got %v", v)
		}
	})
	t.Run("I8=127", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdI8, []byte{127}))
		if v != int8(127) {
			t.Errorf("expected 127, got %v", v)
		}
	})
	t.Run("I8=0", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdI8, []byte{0}))
		if v != int8(0) {
			t.Errorf("expected 0, got %v", v)
		}
	})

	t.Run("I16=-32768", func(t *testing.T) {
		buf := make([]byte, 2)
		binary.LittleEndian.PutUint16(buf, 0x8000)
		v := DeserializeValue(makeTypedValueRig(TypeIdI16, buf))
		if v != int16(-32768) {
			t.Errorf("expected -32768, got %v", v)
		}
	})
	t.Run("I16=32767", func(t *testing.T) {
		buf := make([]byte, 2)
		binary.LittleEndian.PutUint16(buf, 32767)
		v := DeserializeValue(makeTypedValueRig(TypeIdI16, buf))
		if v != int16(32767) {
			t.Errorf("expected 32767, got %v", v)
		}
	})

	t.Run("I32=min", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdI32, i32LE(-2147483648)))
		if v != int32(-2147483648) {
			t.Errorf("expected min, got %v", v)
		}
	})
	t.Run("I32=max", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdI32, i32LE(2147483647)))
		if v != int32(2147483647) {
			t.Errorf("expected max, got %v", v)
		}
	})
	t.Run("I32=0", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdI32, i32LE(0)))
		if v != int32(0) {
			t.Errorf("expected 0, got %v", v)
		}
	})

	t.Run("I64=min", func(t *testing.T) {
		buf := make([]byte, 8)
		binary.LittleEndian.PutUint64(buf, 0x8000000000000000)
		v := DeserializeValue(makeTypedValueRig(TypeIdI64, buf))
		if v != int64(-9223372036854775808) {
			t.Errorf("expected min, got %v", v)
		}
	})
	t.Run("I64=max", func(t *testing.T) {
		buf := make([]byte, 8)
		binary.LittleEndian.PutUint64(buf, 0x7FFFFFFFFFFFFFFF)
		v := DeserializeValue(makeTypedValueRig(TypeIdI64, buf))
		if v != int64(9223372036854775807) {
			t.Errorf("expected max, got %v", v)
		}
	})

	t.Run("F32=0.0", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdF32, f32LE(0.0)))
		if v != float32(0.0) {
			t.Errorf("expected 0.0, got %v", v)
		}
	})
	t.Run("F32=NaN", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdF32, f32LE(float32(math.NaN()))))
		if !math.IsNaN(float64(v.(float32))) {
			t.Errorf("expected NaN, got %v", v)
		}
	})
	t.Run("F32=+Inf", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdF32, f32LE(float32(math.Inf(1)))))
		if !math.IsInf(float64(v.(float32)), 1) {
			t.Errorf("expected +Inf, got %v", v)
		}
	})
	t.Run("F32=-Inf", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdF32, f32LE(float32(math.Inf(-1)))))
		if !math.IsInf(float64(v.(float32)), -1) {
			t.Errorf("expected -Inf, got %v", v)
		}
	})

	t.Run("F64=0.0", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdF64, f64LE(0.0)))
		if v != float64(0.0) {
			t.Errorf("expected 0.0, got %v", v)
		}
	})
	t.Run("F64=NaN", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdF64, f64LE(math.NaN())))
		if !math.IsNaN(v.(float64)) {
			t.Errorf("expected NaN, got %v", v)
		}
	})
	t.Run("F64=+Inf", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdF64, f64LE(math.Inf(1))))
		if !math.IsInf(v.(float64), 1) {
			t.Errorf("expected +Inf, got %v", v)
		}
	})
	t.Run("F64=-Inf", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdF64, f64LE(math.Inf(-1))))
		if !math.IsInf(v.(float64), -1) {
			t.Errorf("expected -Inf, got %v", v)
		}
	})

	t.Run("Bool=1", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdBool, []byte{1}))
		if v != true {
			t.Errorf("expected true, got %v", v)
		}
	})
	t.Run("Bool=0", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdBool, []byte{0}))
		if v != false {
			t.Errorf("expected false, got %v", v)
		}
	})
	t.Run("Bool=42", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdBool, []byte{42}))
		if v != true {
			t.Errorf("expected true (non-zero), got %v", v)
		}
	})
	t.Run("Bool=255", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdBool, []byte{255}))
		if v != true {
			t.Errorf("expected true (non-zero), got %v", v)
		}
	})
}

// ============================================================
// 2. 字符串边界测试
// ============================================================

func TestRigorous_StringBoundaries(t *testing.T) {
	t.Run("empty", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdString, []byte{}))
		if v != "" {
			t.Errorf("expected empty string, got '%v'", v)
		}
	})
	t.Run("Unicode/emoji", func(t *testing.T) {
		s := "你好世界🌍🎉"
		v := DeserializeValue(makeTypedValueRig(TypeIdString, []byte(s)))
		if v != s {
			t.Errorf("expected '%s', got '%v'", s, v)
		}
	})
	t.Run("null byte", func(t *testing.T) {
		s := "hello\x00world"
		v := DeserializeValue(makeTypedValueRig(TypeIdString, []byte(s)))
		if v != s {
			t.Errorf("expected '%s', got '%v'", s, v)
		}
	})
	t.Run("special chars", func(t *testing.T) {
		s := "\n\t\r\b\f"
		v := DeserializeValue(makeTypedValueRig(TypeIdString, []byte(s)))
		if v != s {
			t.Errorf("expected special chars, got '%v'", v)
		}
	})
	t.Run("long str", func(t *testing.T) {
		s := make([]byte, 1000)
		for i := range s {
			s[i] = 'x'
		}
		v := DeserializeValue(makeTypedValueRig(TypeIdString, s))
		if v != string(s) {
			t.Errorf("long string mismatch")
		}
	})
	t.Run("CJK", func(t *testing.T) {
		s := "日本語テスト"
		v := DeserializeValue(makeTypedValueRig(TypeIdString, []byte(s)))
		if v != s {
			t.Errorf("expected '%s', got '%v'", s, v)
		}
	})
	t.Run("spaces", func(t *testing.T) {
		s := "     "
		v := DeserializeValue(makeTypedValueRig(TypeIdString, []byte(s)))
		if v != s {
			t.Errorf("expected spaces, got '%v'", v)
		}
	})
}

// ============================================================
// 3. 反序列化异常/边界测试
// ============================================================

func TestRigorous_DeserializeEdgeCases(t *testing.T) {
	t.Run("truncated empty", func(t *testing.T) {
		v := DeserializeValue([]byte{})
		if v != nil {
			t.Errorf("expected nil, got %v", v)
		}
	})
	t.Run("truncated 1 byte", func(t *testing.T) {
		v := DeserializeValue([]byte{0x01})
		if v != nil {
			t.Errorf("expected nil, got %v", v)
		}
	})
	t.Run("truncated 3 bytes", func(t *testing.T) {
		v := DeserializeValue([]byte{0x01, 0x02, 0x03})
		if v != nil {
			t.Errorf("expected nil, got %v", v)
		}
	})

	t.Run("U8 no value", func(t *testing.T) {
		// valueBytes 为空时 valueBytes[0] 会 panic
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		DeserializeValue(makeTypedValueRig(TypeIdU8, []byte{}))
	})
	t.Run("BOOL no value", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		DeserializeValue(makeTypedValueRig(TypeIdBool, []byte{}))
	})
	t.Run("STRING no value", func(t *testing.T) {
		v := DeserializeValue(makeTypedValueRig(TypeIdString, []byte{}))
		if v != "" {
			t.Errorf("expected empty string, got '%v'", v)
		}
	})
	t.Run("U16 claim 1 byte", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		DeserializeValue(makeTypedValueRig(TypeIdU16, []byte{0xAB}))
	})
	t.Run("U32 claim 2 bytes", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		DeserializeValue(makeTypedValueRig(TypeIdU32, []byte{0x01, 0x02}))
	})
	t.Run("U64 claim 3 bytes", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		DeserializeValue(makeTypedValueRig(TypeIdU64, []byte{0x01, 0x02, 0x03}))
	})

	t.Run("type_id=0", func(t *testing.T) {
		buf := make([]byte, 7)
		binary.LittleEndian.PutUint32(buf[0:4], 0)
		buf[4] = 'A'
		buf[5] = 'B'
		buf[6] = 'C'
		v := DeserializeValue(buf)
		bytes, ok := v.([]byte)
		if !ok {
			t.Errorf("expected []byte for unknown type, got %T", v)
		}
		if len(bytes) != 3 || bytes[0] != 'A' || bytes[1] != 'B' || bytes[2] != 'C' {
			t.Errorf("expected ABC, got %v", bytes)
		}
	})
	t.Run("type_id=999", func(t *testing.T) {
		buf := make([]byte, 6)
		binary.LittleEndian.PutUint32(buf[0:4], 999)
		buf[4] = 'X'
		buf[5] = 'Y'
		v := DeserializeValue(buf)
		bytes, ok := v.([]byte)
		if !ok || len(bytes) != 2 {
			t.Errorf("expected raw bytes, got %v", v)
		}
	})
}

// ============================================================
// 4. Accessor 越界/类型不匹配测试
// ============================================================

func TestRigorous_AccessorEdgeCases(t *testing.T) {
	acc := SpoiTestPlayerAccessor{}
	obj := &SpoiTestPlayer{name: "Test", hp: 100, level: 50, posX: 12.5}

	t.Run("get_field -1", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		acc.GetField(obj, -1)
	})
	t.Run("get_field -100", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		acc.GetField(obj, -100)
	})
	t.Run("get_field =fc", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		acc.GetField(obj, 4)
	})
	t.Run("get_field >fc", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		acc.GetField(obj, 10)
	})
	t.Run("get_field huge", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		acc.GetField(obj, 999999)
	})
	t.Run("set_field -1", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		acc.SetField(obj, -1, 0)
	})
	t.Run("set_field huge", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		acc.SetField(obj, 999999, 0)
	})

	// Go 类型断言：SetField 中会做类型断言，错误类型会 panic
	t.Run("set str for int", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		acc.SetField(obj, 1, "not a number")
	})
	t.Run("set int for str", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		acc.SetField(obj, 0, 42)
	})

	// 不同类型对象
	item := &SpoiItem{}
	t.Run("get on wrong type", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		acc.GetField(item, 0)
	})

	// State accessor
	sa := SpoiTestStateAccessor{}
	s := &SpoiTestState{}
	t.Run("state get neg", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		sa.GetField(s, -1)
	})
	t.Run("state get fc", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		sa.GetField(s, 3)
	})

	// Character accessor
	ca := SpoiCharacterAccessor{}
	c := &SpoiCharacter{name: "A", hp: 1, petLevel: 0}
	t.Run("char fc=5", func(t *testing.T) {
		if ca.FieldCount() != 5 {
			t.Errorf("expected 5, got %d", ca.FieldCount())
		}
	})
	t.Run("char get 4", func(t *testing.T) {
		if v := ca.GetField(c, 4); v != int32(0) {
			t.Errorf("expected 0, got %v", v)
		}
	})
	t.Run("char set 4", func(t *testing.T) {
		ca.SetField(c, 4, int32(99))
	})
	t.Run("char verify", func(t *testing.T) {
		if c.petLevel != 99 {
			t.Errorf("expected 99, got %d", c.petLevel)
		}
	})
	t.Run("char get 5 oob", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Error("expected panic but none occurred")
			}
		}()
		ca.GetField(c, 5)
	})

	// World accessor
	wa := SpoiWorldAccessor{}
	t.Run("world fc=3", func(t *testing.T) {
		if wa.FieldCount() != 3 {
			t.Errorf("expected 3, got %d", wa.FieldCount())
		}
	})
}

// ============================================================
// 5. Executor 组合操作/边界测试
// ============================================================

func TestRigorous_ExecutorEdgeCases(t *testing.T) {
	ex := NewSpoiExecutor(SpoiAccessorRegistry)

	// SET + PIPE + EXEC
	t.Run("SET+PIPE hp=999", func(t *testing.T) {
		p := &SpoiTestPlayer{name: "Hero", hp: 100, level: 5, posX: 0.0}
		data := buildSpoiStreamRig([]SpoiInstruction{
			setInstRig([]int{1}, TypeIdI32, i32LE(999)),
			pipeInstRig([]int{}),
			execInstRig(),
		})
		r := ex.Execute(p, data)
		if p.hp != 999 {
			t.Errorf("hp should be 999, got %d", p.hp)
		}
		if r["resultType"] != RESULT_SINGLE {
			t.Errorf("expected SINGLE result, got %v", r["resultType"])
		}
	})

	// 多层 SET
	t.Run("multi SET", func(t *testing.T) {
		p := &SpoiTestPlayer{name: "X", hp: 0, level: 0, posX: 0.0}
		data := buildSpoiStreamRig([]SpoiInstruction{
			setInstRig([]int{0}, TypeIdString, []byte("Warrior")),
			setInstRig([]int{1}, TypeIdI32, i32LE(500)),
			setInstRig([]int{2}, TypeIdI32, i32LE(99)),
			setInstRig([]int{3}, TypeIdF32, f32LE(3.14)),
			pipeInstRig([]int{}),
			execInstRig(),
		})
		ex.Execute(p, data)
		if p.name != "Warrior" {
			t.Errorf("name should be 'Warrior', got '%s'", p.name)
		}
		if p.hp != 500 {
			t.Errorf("hp should be 500, got %d", p.hp)
		}
		if p.level != 99 {
			t.Errorf("level should be 99, got %d", p.level)
		}
		if math.Abs(p.posX-3.14) > 0.001 {
			t.Errorf("posX should be ~3.14, got %f", p.posX)
		}
	})

	// 创建测试数据
	p1 := &SpoiTestPlayer{name: "Alice", hp: 100, level: 1, posX: 0.0}
	p2 := &SpoiTestPlayer{name: "Bob", hp: 200, level: 5, posX: 0.0}
	p3 := &SpoiTestPlayer{name: "Carol", hp: 300, level: 10, posX: 0.0}
	st := &SpoiTestState{tick: 0, currentMap: "Test", players: []interface{}{p1, p2, p3}}

	// FILTER: hp > 150
	t.Run("FILTER hp>150", func(t *testing.T) {
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			filterInstRig([]int{}, 1, CMP_GT, TypeIdI32, i32LE(150)),
			execInstRig(),
		})
		r := ex.Execute(st, data)
		val := r["value"].([]interface{})
		if len(val) != 2 {
			t.Errorf("expected 2, got %d", len(val))
		}
	})

	// 双层 FILTER
	t.Run("double FILTER", func(t *testing.T) {
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			filterInstRig([]int{}, 1, CMP_GT, TypeIdI32, i32LE(150)),
			filterInstRig([]int{}, 2, CMP_LT, TypeIdI32, i32LE(10)),
			execInstRig(),
		})
		r := ex.Execute(st, data)
		if r["resultType"] != RESULT_SINGLE {
			t.Errorf("expected SINGLE, got %v", r["resultType"])
		}
		player := r["value"].(*SpoiTestPlayer)
		if player.name != "Bob" {
			t.Errorf("expected Bob, got %s", player.name)
		}
	})

	// FILTER 空结果
	t.Run("FILTER empty", func(t *testing.T) {
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			filterInstRig([]int{}, 1, CMP_GT, TypeIdI32, i32LE(999)),
			execInstRig(),
		})
		r := ex.Execute(st, data)
		if r["resultType"] != RESULT_UNDEF {
			t.Errorf("expected UNDEF, got %v", r["resultType"])
		}
	})

	// COUNT
	t.Run("COUNT 3", func(t *testing.T) {
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			countInstRig(),
			execInstRig(),
		})
		r := ex.Execute(st, data)
		if r["value"] != 3 {
			t.Errorf("expected 3, got %v", r["value"])
		}
	})

	// COUNT 空管道
	t.Run("COUNT empty", func(t *testing.T) {
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			filterInstRig([]int{}, 1, CMP_GT, TypeIdI32, i32LE(999)),
			countInstRig(),
			execInstRig(),
		})
		r := ex.Execute(st, data)
		if r["value"] != 0 {
			t.Errorf("expected 0, got %v", r["value"])
		}
	})

	// SORT
	t.Run("SORT by name", func(t *testing.T) {
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			sortInstRig([]int{0}),
			execInstRig(),
		})
		r := ex.Execute(st, data)
		val := r["value"].([]interface{})
		if len(val) != 3 {
			t.Fatalf("expected 3, got %d", len(val))
		}
		if val[0].(*SpoiTestPlayer).name != "Alice" {
			t.Errorf("first should be Alice, got %s", val[0].(*SpoiTestPlayer).name)
		}
		if val[2].(*SpoiTestPlayer).name != "Carol" {
			t.Errorf("last should be Carol, got %s", val[2].(*SpoiTestPlayer).name)
		}
	})

	// TAKE
	t.Run("TAKE 2", func(t *testing.T) {
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			takeInstRig(2),
			execInstRig(),
		})
		r := ex.Execute(st, data)
		val := r["value"].([]interface{})
		if len(val) != 2 {
			t.Errorf("expected 2, got %d", len(val))
		}
	})
	t.Run("TAKE 100", func(t *testing.T) {
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			takeInstRig(100),
			execInstRig(),
		})
		r := ex.Execute(st, data)
		val := r["value"].([]interface{})
		if len(val) != 3 {
			t.Errorf("expected 3, got %d", len(val))
		}
	})
	t.Run("TAKE 0", func(t *testing.T) {
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			takeInstRig(0),
			execInstRig(),
		})
		r := ex.Execute(st, data)
		if r["resultType"] != RESULT_UNDEF {
			t.Errorf("expected UNDEF, got %v", r["resultType"])
		}
	})

	// DROP
	t.Run("DROP 1", func(t *testing.T) {
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			dropInstRig(1),
			execInstRig(),
		})
		r := ex.Execute(st, data)
		val := r["value"].([]interface{})
		if len(val) != 2 {
			t.Errorf("expected 2, got %d", len(val))
		}
	})
	t.Run("DROP 100", func(t *testing.T) {
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			dropInstRig(100),
			execInstRig(),
		})
		r := ex.Execute(st, data)
		if r["resultType"] != RESULT_UNDEF {
			t.Errorf("expected UNDEF, got %v", r["resultType"])
		}
	})

	// SELECT
	t.Run("SELECT names", func(t *testing.T) {
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			selectInstRig([]int{0}),
			execInstRig(),
		})
		r := ex.Execute(st, data)
		val := r["value"].([]interface{})
		if len(val) != 3 {
			t.Errorf("expected 3, got %d", len(val))
		}
		if val[0] != "Alice" || val[1] != "Bob" || val[2] != "Carol" {
			t.Errorf("expected [Alice Bob Carol], got %v", val)
		}
	})

	// REVERSE
	t.Run("REVERSE", func(t *testing.T) {
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			reverseInstRig(),
			execInstRig(),
		})
		r := ex.Execute(st, data)
		val := r["value"].([]interface{})
		if val[0].(*SpoiTestPlayer).name != "Carol" {
			t.Errorf("first should be Carol, got %s", val[0].(*SpoiTestPlayer).name)
		}
	})

	// DISTINCT
	t.Run("DISTINCT", func(t *testing.T) {
		st2 := &SpoiTestState{tick: 0, currentMap: "", players: []interface{}{p1, p1, p2, p2, p3}}
		data := buildSpoiStreamRig([]SpoiInstruction{
			pipeInstRig([]int{2}),
			distinctInstRig(),
			execInstRig(),
		})
		r := ex.Execute(st2, data)
		val := r["value"].([]interface{})
		if len(val) != 3 {
			t.Errorf("expected 3 after distinct, got %d", len(val))
		}
	})
}

// ============================================================
// 6. 跨类型 Executor 测试
// ============================================================

func TestRigorous_CrossTypeExecutor(t *testing.T) {
	ex := NewSpoiExecutor(SpoiAccessorRegistry)

	t.Run("Item SET", func(t *testing.T) {
		item := &SpoiItem{name: "", value: 0}
		data := buildSpoiStreamRig([]SpoiInstruction{
			setInstRig([]int{0}, TypeIdString, []byte("Potion")),
			setInstRig([]int{1}, TypeIdI32, i32LE(50)),
			pipeInstRig([]int{}),
			execInstRig(),
		})
		ex.Execute(item, data)
		if item.name != "Potion" {
			t.Errorf("expected Potion, got %s", item.name)
		}
		if item.value != 50 {
			t.Errorf("expected 50, got %d", item.value)
		}
	})

	t.Run("Inventory SET gold", func(t *testing.T) {
		inv := &SpoiInventory{gold: 0}
		data := buildSpoiStreamRig([]SpoiInstruction{
			setInstRig([]int{2}, TypeIdI32, i32LE(1000)),
			pipeInstRig([]int{}),
			execInstRig(),
		})
		ex.Execute(inv, data)
		if inv.gold != 1000 {
			t.Errorf("expected 1000, got %d", inv.gold)
		}
	})

	t.Run("Character SET", func(t *testing.T) {
		char := &SpoiCharacter{name: "", hp: 0, petLevel: 0}
		data := buildSpoiStreamRig([]SpoiInstruction{
			setInstRig([]int{0}, TypeIdString, []byte("Hero")),
			setInstRig([]int{1}, TypeIdI32, i32LE(2000)),
			setInstRig([]int{4}, TypeIdI32, i32LE(5)),
			pipeInstRig([]int{}),
			execInstRig(),
		})
		ex.Execute(char, data)
		if char.name != "Hero" {
			t.Errorf("expected Hero, got %s", char.name)
		}
		if char.hp != 2000 {
			t.Errorf("expected 2000, got %d", char.hp)
		}
		if char.petLevel != 5 {
			t.Errorf("expected 5, got %d", char.petLevel)
		}
	})

	t.Run("World SET", func(t *testing.T) {
		world := &SpoiWorld{worldName: "", tick: 0}
		data := buildSpoiStreamRig([]SpoiInstruction{
			setInstRig([]int{0}, TypeIdString, []byte("Azeroth")),
			setInstRig([]int{1}, TypeIdI32, i32LE(9999)),
			pipeInstRig([]int{}),
			execInstRig(),
		})
		ex.Execute(world, data)
		if world.worldName != "Azeroth" {
			t.Errorf("expected Azeroth, got %s", world.worldName)
		}
		if world.tick != 9999 {
			t.Errorf("expected 9999, got %d", world.tick)
		}
	})
}

// ============================================================
// 7. Registry 边界测试
// ============================================================

func TestRigorous_RegistryEdgeCases(t *testing.T) {
	t.Run("size=6", func(t *testing.T) {
		if len(SpoiAccessorRegistry) != 6 {
			t.Errorf("expected 6, got %d", len(SpoiAccessorRegistry))
		}
	})
	t.Run("contains SpoiTestPlayer", func(t *testing.T) {
		if _, ok := SpoiAccessorRegistry["SpoiTestPlayer"]; !ok {
			t.Error("missing SpoiTestPlayer")
		}
	})
	t.Run("contains SpoiTestState", func(t *testing.T) {
		if _, ok := SpoiAccessorRegistry["SpoiTestState"]; !ok {
			t.Error("missing SpoiTestState")
		}
	})
	t.Run("contains SpoiItem", func(t *testing.T) {
		if _, ok := SpoiAccessorRegistry["SpoiItem"]; !ok {
			t.Error("missing SpoiItem")
		}
	})
	t.Run("missing key", func(t *testing.T) {
		if _, ok := SpoiAccessorRegistry["NonExistent"]; ok {
			t.Error("should not find NonExistent")
		}
	})
	t.Run("all fc>0", func(t *testing.T) {
		for name, acc := range SpoiAccessorRegistry {
			if acc.FieldCount() <= 0 {
				t.Errorf("%s fieldCount should be > 0, got %d", name, acc.FieldCount())
			}
		}
	})
}