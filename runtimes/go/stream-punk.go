/**
 * Stream-Punk Go Runtime Library
 * Zero-dependency binary serialization/deserialization for Go.
 * Compatible with the C++ StreamPunk binary format (little-endian).
 */

package main

import (
	"encoding/binary"
	"math"
	"unicode/utf16"
)

/* =================== 辅助类型 =================== */

type SpRef struct {
	Value   interface{}
	Address uint64
}

func NewSpRef() SpRef {
	return SpRef{Value: nil, Address: 0}
}

type SpArray struct {
	Data []interface{}
}

func NewSpArray(size int) SpArray {
	return SpArray{Data: make([]interface{}, size)}
}

func (a *SpArray) At(index int) interface{} {
	return a.Data[index]
}

func (a *SpArray) Set(index int, value interface{}) {
	a.Data[index] = value
}

func (a *SpArray) Size() int {
	return len(a.Data)
}

type SpVariant struct {
	TypeIndex uint32
	Value     interface{}
}

func NewSpVariant() SpVariant {
	return SpVariant{TypeIndex: 0, Value: nil}
}

func (v *SpVariant) WriteVariant(o *O) {
	o.WriteU32(v.TypeIndex)
	if v.Value != nil {
		if obj, ok := v.Value.(SpBase); ok {
			WriteObj(o, obj)
		}
	}
}

func ReadVariant(i *I) SpVariant {
	typeIndex := i.ReadU32()
	value := ReadObj(i)
	return SpVariant{TypeIndex: typeIndex, Value: value}
}

/* =================== SpBase 接口 =================== */

type SpBase interface {
	TypeID() uint32
	From_(i *I)
	To(o *O)
}

/* =================== 反序列化读取器 =================== */

type I struct {
	buf    []byte
	Off    int
	objMap map[uint64]interface{}
}

func NewI(data []byte) *I {
	return &I{buf: data, Off: 0, objMap: make(map[uint64]interface{})}
}

func NewIOffset(data []byte, offset int) *I {
	return &I{buf: data, Off: offset, objMap: make(map[uint64]interface{})}
}

func (i *I) HasMoreData() bool {
	return i.Off < len(i.buf)
}

func (i *I) ReadU8() uint8 {
	v := i.buf[i.Off]
	i.Off++
	return v
}

func (i *I) ReadU16() uint16 {
	v := binary.LittleEndian.Uint16(i.buf[i.Off:])
	i.Off += 2
	return v
}

func (i *I) ReadU32() uint32 {
	v := binary.LittleEndian.Uint32(i.buf[i.Off:])
	i.Off += 4
	return v
}

func (i *I) ReadU64() uint64 {
	v := binary.LittleEndian.Uint64(i.buf[i.Off:])
	i.Off += 8
	return v
}

func (i *I) ReadI8() int8 {
	v := int8(i.buf[i.Off])
	i.Off++
	return v
}

func (i *I) ReadI16() int16 {
	v := int16(binary.LittleEndian.Uint16(i.buf[i.Off:]))
	i.Off += 2
	return v
}

func (i *I) ReadI32() int32 {
	v := int32(binary.LittleEndian.Uint32(i.buf[i.Off:]))
	i.Off += 4
	return v
}

func (i *I) ReadI64() int64 {
	v := int64(binary.LittleEndian.Uint64(i.buf[i.Off:]))
	i.Off += 8
	return v
}

func (i *I) ReadF32() float32 {
	bits := binary.LittleEndian.Uint32(i.buf[i.Off:])
	i.Off += 4
	return math.Float32frombits(bits)
}

func (i *I) ReadF64() float64 {
	bits := binary.LittleEndian.Uint64(i.buf[i.Off:])
	i.Off += 8
	return math.Float64frombits(bits)
}

func (i *I) ReadCh() byte {
	v := i.buf[i.Off]
	i.Off++
	return v
}

func (i *I) ReadCh8() byte {
	v := i.buf[i.Off]
	i.Off++
	return v
}

func (i *I) ReadCh16() uint16 {
	v := binary.LittleEndian.Uint16(i.buf[i.Off:])
	i.Off += 2
	return v
}

func (i *I) ReadCh32() uint32 {
	v := binary.LittleEndian.Uint32(i.buf[i.Off:])
	i.Off += 4
	return v
}

func (i *I) ReadBl() bool {
	v := i.buf[i.Off]
	i.Off++
	return v != 0
}

func (i *I) ReadSz() int {
	return int(i.ReadU32())
}

func (i *I) ReadBytes(length int) {
	i.Off += length
}

func (i *I) ReadString() string {
	length := i.ReadSz()
	if length == 0 {
		return ""
	}
	v := string(i.buf[i.Off : i.Off+length])
	i.Off += length
	return v
}

func (i *I) ReadU8String() string {
	return i.ReadString()
}

func (i *I) ReadU16String() string {
	length := i.ReadSz()
	if length == 0 {
		return ""
	}
	byteLen := length * 2
	u16s := make([]uint16, length)
	for j := 0; j < length; j++ {
		u16s[j] = binary.LittleEndian.Uint16(i.buf[i.Off+j*2:])
	}
	i.Off += byteLen
	return string(utf16.Decode(u16s))
}

func (i *I) ReadU32String() []uint32 {
	length := i.ReadSz()
	byteLen := length * 4
	result := make([]uint32, length)
	for j := 0; j < length; j++ {
		result[j] = binary.LittleEndian.Uint32(i.buf[i.Off+j*4:])
	}
	i.Off += byteLen
	return result
}

func (i *I) ReadPtrWithTypeID() SpRef {
	addr := i.ReadU64()
	if addr == 0 {
		return NewSpRef()
	}
	if ref, ok := i.objMap[addr]; ok {
		return ref.(SpRef)
	}
	ref := SpRef{Value: nil, Address: addr}
	i.objMap[addr] = ref
	ref.Value = ReadObj(i)
	i.objMap[addr] = ref
	return ref
}

func (i *I) ReadPtr(reader func() interface{}) SpRef {
	addr := i.ReadU64()
	if addr == 0 {
		return NewSpRef()
	}
	if ref, ok := i.objMap[addr]; ok {
		return ref.(SpRef)
	}
	ref := SpRef{Value: nil, Address: addr}
	i.objMap[addr] = ref
	ref.Value = reader()
	i.objMap[addr] = ref
	return ref
}

func (i *I) ReadArray(reader func() interface{}) []interface{} {
	length := i.ReadSz()
	result := make([]interface{}, length)
	for j := 0; j < length; j++ {
		result[j] = reader()
	}
	return result
}

func (i *I) ReadSet(reader func() interface{}) map[interface{}]struct{} {
	length := i.ReadSz()
	result := make(map[interface{}]struct{})
	for j := 0; j < length; j++ {
		result[reader()] = struct{}{}
	}
	return result
}

func (i *I) ReadMap(keyReader func() interface{}, valueReader func() interface{}) map[interface{}]interface{} {
	length := i.ReadSz()
	result := make(map[interface{}]interface{})
	for j := 0; j < length; j++ {
		result[keyReader()] = valueReader()
	}
	return result
}

func (i *I) ReadSpArray(size int, reader func() interface{}) SpArray {
	arr := NewSpArray(size)
	for j := 0; j < size; j++ {
		arr.Set(j, reader())
	}
	return arr
}

/* =================== 序列化写入器 =================== */

type O struct {
	buf []byte
}

func NewO() *O {
	return &O{buf: make([]byte, 0, 256)}
}

func (o *O) Bytes() []byte {
	return o.buf
}

func (o *O) WriteU8(v uint8) {
	o.buf = append(o.buf, v)
}

func (o *O) WriteU16(v uint16) {
	b := make([]byte, 2)
	binary.LittleEndian.PutUint16(b, v)
	o.buf = append(o.buf, b...)
}

func (o *O) WriteU32(v uint32) {
	b := make([]byte, 4)
	binary.LittleEndian.PutUint32(b, v)
	o.buf = append(o.buf, b...)
}

func (o *O) WriteU64(v uint64) {
	b := make([]byte, 8)
	binary.LittleEndian.PutUint64(b, v)
	o.buf = append(o.buf, b...)
}

func (o *O) WriteI8(v int8) {
	o.buf = append(o.buf, byte(v))
}

func (o *O) WriteI16(v int16) {
	b := make([]byte, 2)
	binary.LittleEndian.PutUint16(b, uint16(v))
	o.buf = append(o.buf, b...)
}

func (o *O) WriteI32(v int32) {
	b := make([]byte, 4)
	binary.LittleEndian.PutUint32(b, uint32(v))
	o.buf = append(o.buf, b...)
}

func (o *O) WriteI64(v int64) {
	b := make([]byte, 8)
	binary.LittleEndian.PutUint64(b, uint64(v))
	o.buf = append(o.buf, b...)
}

func (o *O) WriteF32(v float32) {
	b := make([]byte, 4)
	binary.LittleEndian.PutUint32(b, math.Float32bits(v))
	o.buf = append(o.buf, b...)
}

func (o *O) WriteF64(v float64) {
	b := make([]byte, 8)
	binary.LittleEndian.PutUint64(b, math.Float64bits(v))
	o.buf = append(o.buf, b...)
}

func (o *O) WriteCh(v byte) {
	o.buf = append(o.buf, v)
}

func (o *O) WriteCh8(v byte) {
	o.buf = append(o.buf, v)
}

func (o *O) WriteCh16(v uint16) {
	b := make([]byte, 2)
	binary.LittleEndian.PutUint16(b, v)
	o.buf = append(o.buf, b...)
}

func (o *O) WriteCh32(v uint32) {
	b := make([]byte, 4)
	binary.LittleEndian.PutUint32(b, v)
	o.buf = append(o.buf, b...)
}

func (o *O) WriteBl(v bool) {
	if v {
		o.buf = append(o.buf, 1)
	} else {
		o.buf = append(o.buf, 0)
	}
}

func (o *O) WriteSz(v int) {
	o.WriteU32(uint32(v))
}

func (o *O) WriteString(v string) {
	o.WriteSz(len(v))
	if len(v) > 0 {
		o.buf = append(o.buf, []byte(v)...)
	}
}

func (o *O) WriteU8String(v string) {
	o.WriteString(v)
}

func (o *O) WriteU16String(v string) {
	runes := []rune(v)
	o.WriteSz(len(runes))
	for _, r := range runes {
		b := make([]byte, 2)
		binary.LittleEndian.PutUint16(b, uint16(r))
		o.buf = append(o.buf, b...)
	}
}

func (o *O) WriteU32String(v []uint32) {
	o.WriteSz(len(v))
	for _, r := range v {
		b := make([]byte, 4)
		binary.LittleEndian.PutUint32(b, r)
		o.buf = append(o.buf, b...)
	}
}

func (o *O) WritePtrWithTypeID(value interface{}) {
	if value == nil {
		o.WriteU64(0)
		return
	}
	if obj, ok := value.(SpBase); ok {
		o.WriteU32(obj.TypeID())
		WriteObj(o, obj)
	} else {
		o.WriteU64(0)
	}
}

func (o *O) WritePtr(value interface{}, address uint64, writer func(interface{})) {
	if value == nil {
		o.WriteU64(0)
		return
	}
	o.WriteU64(address)
	if address != 0 {
		writer(value)
	}
}

func (o *O) WriteArray(arr []interface{}, writer func(interface{})) {
	o.WriteSz(len(arr))
	for _, v := range arr {
		writer(v)
	}
}

func (o *O) WriteSet(s map[interface{}]struct{}, writer func(interface{})) {
	o.WriteSz(len(s))
	for k := range s {
		writer(k)
	}
}

func (o *O) WriteMap(m map[interface{}]interface{}, keyWriter func(interface{}), valueWriter func(interface{})) {
	o.WriteSz(len(m))
	for k, v := range m {
		keyWriter(k)
		valueWriter(v)
	}
}

func (o *O) WriteSpArray(arr SpArray, writer func(interface{})) {
	for i := 0; i < arr.Size(); i++ {
		writer(arr.At(i))
	}
}

// =================== 类型分发（由 sp-gen 生成具体实现） ===================
// ReadObj / WriteObj 由 sp-gen 代码生成器产出，此处不定义桩函数
// 避免与生成器产出的实现冲突导致 Go 编译器报 redeclared 错误