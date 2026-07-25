// Stream-Punk Go 序列化/反序列化测试
// 运行: go test -v

package main

import (
	"testing"
)

// ======================== 基本类型测试 ========================

func TestU8Roundtrip(t *testing.T) {
	for _, v := range []uint8{0, 1, 127, 128, 255} {
		o := NewO()
		o.WriteU8(v)
		i := NewI(o.Bytes())
		if r := i.ReadU8(); r != v {
			t.Errorf("u8: expected %d, got %d", v, r)
		}
	}
}

func TestU16Roundtrip(t *testing.T) {
	for _, v := range []uint16{0, 1, 256, 1000, 0xFFFF} {
		o := NewO()
		o.WriteU16(v)
		i := NewI(o.Bytes())
		if r := i.ReadU16(); r != v {
			t.Errorf("u16: expected %d, got %d", v, r)
		}
	}
}

func TestU32Roundtrip(t *testing.T) {
	for _, v := range []uint32{0, 1, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF} {
		o := NewO()
		o.WriteU32(v)
		i := NewI(o.Bytes())
		if r := i.ReadU32(); r != v {
			t.Errorf("u32: expected %d, got %d", v, r)
		}
	}
}

func TestU64Roundtrip(t *testing.T) {
	for _, v := range []uint64{0, 1, 0x100000000, 0x7FFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF} {
		o := NewO()
		o.WriteU64(v)
		i := NewI(o.Bytes())
		if r := i.ReadU64(); r != v {
			t.Errorf("u64: expected %d, got %d", v, r)
		}
	}
}

func TestI8Roundtrip(t *testing.T) {
	for _, v := range []int8{-128, -1, 0, 1, 127} {
		o := NewO()
		o.WriteI8(v)
		i := NewI(o.Bytes())
		if r := i.ReadI8(); r != v {
			t.Errorf("i8: expected %d, got %d", v, r)
		}
	}
}

func TestI16Roundtrip(t *testing.T) {
	for _, v := range []int16{-32768, -1, 0, 1, 32767} {
		o := NewO()
		o.WriteI16(v)
		i := NewI(o.Bytes())
		if r := i.ReadI16(); r != v {
			t.Errorf("i16: expected %d, got %d", v, r)
		}
	}
}

func TestI32Roundtrip(t *testing.T) {
	for _, v := range []int32{-2147483648, -1, 0, 1, 2147483647} {
		o := NewO()
		o.WriteI32(v)
		i := NewI(o.Bytes())
		if r := i.ReadI32(); r != v {
			t.Errorf("i32: expected %d, got %d", v, r)
		}
	}
}

func TestI64Roundtrip(t *testing.T) {
	for _, v := range []int64{-9223372036854775808, -1, 0, 1, 9223372036854775807} {
		o := NewO()
		o.WriteI64(v)
		i := NewI(o.Bytes())
		if r := i.ReadI64(); r != v {
			t.Errorf("i64: expected %d, got %d", v, r)
		}
	}
}

func TestF32Roundtrip(t *testing.T) {
	for _, v := range []float32{0.0, -1.0, 3.14} {
		o := NewO()
		o.WriteF32(v)
		i := NewI(o.Bytes())
		r := i.ReadF32()
		if r != v {
			t.Errorf("f32: expected %f, got %f", v, r)
		}
	}
}

func TestF64Roundtrip(t *testing.T) {
	for _, v := range []float64{0.0, -1.0, 3.141592653589793} {
		o := NewO()
		o.WriteF64(v)
		i := NewI(o.Bytes())
		r := i.ReadF64()
		if r != v {
			t.Errorf("f64: expected %f, got %f", v, r)
		}
	}
}

func TestBlRoundtrip(t *testing.T) {
	o := NewO()
	o.WriteBl(true)
	o.WriteBl(false)
	i := NewI(o.Bytes())
	if !i.ReadBl() {
		t.Error("bl: expected true")
	}
	if i.ReadBl() {
		t.Error("bl: expected false")
	}
}

func TestChRoundtrip(t *testing.T) {
	for _, v := range []byte{'A', 'z', '0', '\n'} {
		o := NewO()
		o.WriteCh8(v)
		i := NewI(o.Bytes())
		if r := i.ReadCh8(); r != v {
			t.Errorf("ch8: expected %d, got %d", v, r)
		}
	}
}

// ======================== 字符串测试 ========================

func TestStringEmpty(t *testing.T) {
	o := NewO()
	o.WriteString("")
	i := NewI(o.Bytes())
	if r := i.ReadString(); r != "" {
		t.Errorf("expected empty string, got %q", r)
	}
}

func TestStringASCII(t *testing.T) {
	s := "Hello World"
	o := NewO()
	o.WriteString(s)
	i := NewI(o.Bytes())
	if r := i.ReadString(); r != s {
		t.Errorf("expected %q, got %q", s, r)
	}
}

func TestStringUnicode(t *testing.T) {
	s := "你好世界 — 测试"
	o := NewO()
	o.WriteString(s)
	i := NewI(o.Bytes())
	if r := i.ReadString(); r != s {
		t.Errorf("expected %q, got %q", s, r)
	}
}

func TestStringLong(t *testing.T) {
	s := make([]byte, 10000)
	for i := range s {
		s[i] = 'A'
	}
	o := NewO()
	o.WriteString(string(s))
	i := NewI(o.Bytes())
	if r := i.ReadString(); r != string(s) {
		t.Error("long string mismatch")
	}
}

func TestU16String(t *testing.T) {
	s := "你好世界"
	o := NewO()
	o.WriteU16String(s)
	i := NewI(o.Bytes())
	if r := i.ReadU16String(); r != s {
		t.Errorf("u16string: expected %q, got %q", s, r)
	}
}

// ======================== 容器测试 ========================

func TestArrayEmpty(t *testing.T) {
	o := NewO()
	o.WriteArray([]interface{}{}, func(v interface{}) { o.WriteU32(v.(uint32)) })
	i := NewI(o.Bytes())
	result := i.ReadArray(func() interface{} { return i.ReadU32() })
	if len(result) != 0 {
		t.Errorf("expected empty array, got len=%d", len(result))
	}
}

func TestArrayU32(t *testing.T) {
	o := NewO()
	o.WriteArray([]interface{}{uint32(1), uint32(2), uint32(3)}, func(v interface{}) { o.WriteU32(v.(uint32)) })
	i := NewI(o.Bytes())
	result := i.ReadArray(func() interface{} { return i.ReadU32() })
	if len(result) != 3 {
		t.Errorf("expected len=3, got len=%d", len(result))
	}
}

func TestArrayStrings(t *testing.T) {
	data := []string{"Alice", "Bob", "Carol"}
	arr := make([]interface{}, len(data))
	for i, s := range data {
		arr[i] = s
	}
	o := NewO()
	o.WriteArray(arr, func(v interface{}) { o.WriteString(v.(string)) })
	i := NewI(o.Bytes())
	result := i.ReadArray(func() interface{} { return i.ReadString() })
	if len(result) != 3 {
		t.Errorf("expected len=3, got len=%d", len(result))
	}
}

func TestSet(t *testing.T) {
	s := make(map[interface{}]struct{})
	s[uint32(1)] = struct{}{}
	s[uint32(2)] = struct{}{}
	s[uint32(3)] = struct{}{}
	o := NewO()
	o.WriteSet(s, func(v interface{}) { o.WriteU32(v.(uint32)) })
	i := NewI(o.Bytes())
	result := i.ReadSet(func() interface{} { return i.ReadU32() })
	if len(result) != 3 {
		t.Errorf("expected set size=3, got %d", len(result))
	}
}

func TestMap(t *testing.T) {
	m := map[string]int{"key1": 100, "key2": 200}
	im := make(map[interface{}]interface{})
	for k, v := range m {
		im[k] = uint32(v)
	}
	o := NewO()
	o.WriteMap(im,
		func(k interface{}) { o.WriteString(k.(string)) },
		func(v interface{}) { o.WriteU32(v.(uint32)) })
	i := NewI(o.Bytes())
	result := i.ReadMap(
		func() interface{} { return i.ReadString() },
		func() interface{} { return i.ReadU32() })
	if len(result) != 2 {
		t.Errorf("expected map size=2, got %d", len(result))
	}
}

// ======================== 多字段往返测试 ========================

func TestMixedTypes(t *testing.T) {
	o := NewO()
	o.WriteU8(42)
	o.WriteI32(-100)
	o.WriteF64(3.14)
	o.WriteBl(true)
	o.WriteString("hello")

	i := NewI(o.Bytes())
	if r := i.ReadU8(); r != 42 {
		t.Errorf("u8: expected 42, got %d", r)
	}
	if r := i.ReadI32(); r != -100 {
		t.Errorf("i32: expected -100, got %d", r)
	}
	if r := i.ReadF64(); r != 3.14 {
		t.Errorf("f64: expected 3.14, got %f", r)
	}
	if !i.ReadBl() {
		t.Error("bl: expected true")
	}
	if r := i.ReadString(); r != "hello" {
		t.Errorf("string: expected hello, got %q", r)
	}
}

func TestOffsetReader(t *testing.T) {
	o := NewO()
	o.WriteU32(0) // padding
	o.WriteU32(42)
	o.WriteU32(0) // padding

	data := o.Bytes()
	i := NewIOffset(data, 4)
	if r := i.ReadU32(); r != 42 {
		t.Errorf("expected 42, got %d", r)
	}
	if i.Off != 8 {
		t.Errorf("expected offset 8, got %d", i.Off)
	}
}

func TestHasMoreData(t *testing.T) {
	o := NewO()
	o.WriteU32(42)
	i := NewI(o.Bytes())
	if !i.HasMoreData() {
		t.Error("expected HasMoreData=true")
	}
	i.ReadU32()
	if i.HasMoreData() {
		t.Error("expected HasMoreData=false")
	}
}

// ======================== 辅助类型测试 ========================

func TestSpArray(t *testing.T) {
	arr := NewSpArray(3)
	arr.Set(0, "hello")
	arr.Set(1, "world")
	if arr.At(0) != "hello" {
		t.Error("SpArray.At(0) mismatch")
	}
	if arr.At(1) != "world" {
		t.Error("SpArray.At(1) mismatch")
	}
	if arr.Size() != 3 {
		t.Errorf("expected size=3, got %d", arr.Size())
	}
}

func TestSpRef(t *testing.T) {
	ref := SpRef{Value: "hello", Address: 0x1000}
	if ref.Value != "hello" {
		t.Error("SpRef.Value mismatch")
	}
	if ref.Address != 0x1000 {
		t.Error("SpRef.Address mismatch")
	}
}

// ======================== 边界条件测试 ========================

func TestZeroLengthArray(t *testing.T) {
	o := NewO()
	o.WriteArray([]interface{}{}, func(v interface{}) { o.WriteU8(v.(uint8)) })
	i := NewI(o.Bytes())
	result := i.ReadArray(func() interface{} { return i.ReadU8() })
	if len(result) != 0 {
		t.Errorf("expected empty array, got len=%d", len(result))
	}
}

func TestNegativeInArray(t *testing.T) {
	data := []int32{-1, 0, 1, -100, 100}
	arr := make([]interface{}, len(data))
	for i, v := range data {
		arr[i] = v
	}
	o := NewO()
	o.WriteArray(arr, func(v interface{}) { o.WriteI32(v.(int32)) })
	i := NewI(o.Bytes())
	result := i.ReadArray(func() interface{} { return i.ReadI32() })
	if len(result) != len(data) {
		t.Errorf("expected len=%d, got len=%d", len(data), len(result))
	}
}