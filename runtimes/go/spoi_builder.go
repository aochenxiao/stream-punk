//go:build ignore

// ============================================================
// SPOI — StreamPunk Operation Instruction
// Go 查询/更新 Builder（自动生成）
// ============================================================

package main

import (
	"encoding/binary"
	"fmt"
	"math"
)

// 操作码
const (
	OpSet       uint8 = 0x04
	OpAdd       uint8 = 0x05
	OpAppend    uint8 = 0x06
	OpRemove    uint8 = 0x07
	OpInsert    uint8 = 0x08
	OpReplace   uint8 = 0x09
	OpReset     uint8 = 0x0A
	OpSetNull   uint8 = 0x0B
	OpFilter    uint8 = 0x0C
	OpSelect    uint8 = 0x0D
	OpSort      uint8 = 0x0E
	OpReverse   uint8 = 0x0F
	OpTake      uint8 = 0x10
	OpDrop      uint8 = 0x11
	OpTakeWhile uint8 = 0x12
	OpDropWhile uint8 = 0x13
	OpDistinct  uint8 = 0x14
	OpCount     uint8 = 0x15
	OpAny       uint8 = 0x16
	OpAll       uint8 = 0x17
	OpFind      uint8 = 0x18
	OpKeys      uint8 = 0x19
	OpValues    uint8 = 0x1A
	OpJoin      uint8 = 0x1B
	OpEnumerate uint8 = 0x1C
	OpChunk     uint8 = 0x1D
	OpSlide     uint8 = 0x1E
	OpStride    uint8 = 0x1F
	OpAdjacent  uint8 = 0x20
	OpExec      uint8 = 0x21
)

// 比较运算符
const (
	CmpEQ uint8 = 0
	CmpNE uint8 = 1
	CmpLT uint8 = 2
	CmpGT uint8 = 3
	CmpLE uint8 = 4
	CmpGE uint8 = 5
)

// 类型成员索引常量
// SpoiTestPlayer
const (
	SpoiTestPlayer_name uint32 = 0
	SpoiTestPlayer_hp uint32 = 1
	SpoiTestPlayer_level uint32 = 2
	SpoiTestPlayer_posX uint32 = 3
)

// SpoiTestState
const (
	SpoiTestState_tick uint32 = 0
	SpoiTestState_currentMap uint32 = 1
	SpoiTestState_players uint32 = 2
)

// SpoiItem
const (
	SpoiItem_name uint32 = 0
	SpoiItem_value uint32 = 1
)

// SpoiInventory
const (
	SpoiInventory_items uint32 = 0
	SpoiInventory_equipped uint32 = 1
	SpoiInventory_gold uint32 = 2
)

// SpoiCharacter
const (
	SpoiCharacter_name uint32 = 0
	SpoiCharacter_hp uint32 = 1
	SpoiCharacter_inventory uint32 = 2
	SpoiCharacter_weapon uint32 = 3
	SpoiCharacter_petLevel uint32 = 4
)

// SpoiWorld
const (
	SpoiWorld_worldName uint32 = 0
	SpoiWorld_tick uint32 = 1
	SpoiWorld_characters uint32 = 2
)

const PathDeref uint32 = 0xFFFF

// Varint 编码
func writeVarint(buf *[]byte, v uint32) {
	for v >= 0x80 {
		*buf = append(*buf, byte((v&0x7F)|0x80))
		v >>= 7
	}
	*buf = append(*buf, byte(v&0x7F))
}

func writeU32(buf *[]byte, v uint32) {
	var b [4]byte
	binary.LittleEndian.PutUint32(b[:], v)
	*buf = append(*buf, b[:]...)
}

// SpoiInstruction
type SpoiInstruction struct {
	Op      uint8
	Path    []uint32
	Operand []byte
}

func (si *SpoiInstruction) Serialize() []byte {
	var buf []byte
	buf = append(buf, si.Op)
	writeVarint(&buf, uint32(len(si.Path)))
	for _, seg := range si.Path {
		writeU32(&buf, seg)
	}
	writeVarint(&buf, uint32(len(si.Operand)))
	buf = append(buf, si.Operand...)
	return buf
}

// SpoiStream
type SpoiStream struct {
	Instructions []SpoiInstruction
}

func (ss *SpoiStream) Build() []byte {
	var buf []byte
	writeVarint(&buf, uint32(len(ss.Instructions)))
	for _, inst := range ss.Instructions {
		buf = append(buf, inst.Serialize()...)
	}
	return buf
}

func (ss *SpoiStream) BuildHex() string {
	return fmt.Sprintf("%x", ss.Build())
}

// SpoiUpdate — 写操作 Builder
type SpoiUpdate struct {
	stream SpoiStream
}

func NewSpoiUpdate() *SpoiUpdate { return &SpoiUpdate{} }

func (su *SpoiUpdate) Set(path []uint32, value []byte) *SpoiUpdate {
	su.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpSet, Path: path, Operand: value})
	return su
}

func (su *SpoiUpdate) SetI32(path []uint32, value int32) *SpoiUpdate {
	var buf [8]byte
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdI32)
	binary.LittleEndian.PutUint32(buf[4:8], uint32(value))
	return su.Set(path, buf[:])
}

func (su *SpoiUpdate) SetU32(path []uint32, value uint32) *SpoiUpdate {
	var buf [8]byte
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(buf[4:8], value)
	return su.Set(path, buf[:])
}

func (su *SpoiUpdate) SetF64(path []uint32, value float64) *SpoiUpdate {
	var buf [12]byte
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdF64)
	binary.LittleEndian.PutUint64(buf[4:12], math.Float64bits(value))
	return su.Set(path, buf[:])
}

func (su *SpoiUpdate) SetStr(path []uint32, value string) *SpoiUpdate {
	var buf []byte
	writeU32(&buf, TypeIdString)
	buf = append(buf, []byte(value)...)
	return su.Set(path, buf)
}

func (su *SpoiUpdate) SetBool(path []uint32, value bool) *SpoiUpdate {
	var buf [5]byte
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdBool)
	if value { buf[4] = 1 } else { buf[4] = 0 }
	return su.Set(path, buf[:])
}

func (su *SpoiUpdate) AddI32(path []uint32, delta int32) *SpoiUpdate {
	var buf [4]byte
	binary.LittleEndian.PutUint32(buf[:], uint32(delta))
	su.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpAdd, Path: path, Operand: buf[:]})
	return su
}

func (su *SpoiUpdate) Add(path []uint32, value []byte) *SpoiUpdate {
	su.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpAdd, Path: path, Operand: value})
	return su
}

func (su *SpoiUpdate) Append(path []uint32, value []byte) *SpoiUpdate {
	su.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpAppend, Path: path, Operand: value})
	return su
}

func (su *SpoiUpdate) Remove(path []uint32, value []byte) *SpoiUpdate {
	su.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpRemove, Path: path, Operand: value})
	return su
}

func (su *SpoiUpdate) Insert(path []uint32, value []byte) *SpoiUpdate {
	su.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpInsert, Path: path, Operand: value})
	return su
}

func (su *SpoiUpdate) Replace(path []uint32, value []byte) *SpoiUpdate {
	su.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpReplace, Path: path, Operand: value})
	return su
}

func (su *SpoiUpdate) Reset(path []uint32) *SpoiUpdate {
	su.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpReset, Path: path})
	return su
}

func (su *SpoiUpdate) Setnull(path []uint32) *SpoiUpdate {
	su.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpSetNull, Path: path})
	return su
}

func (su *SpoiUpdate) Build() []byte  { return su.stream.Build() }
func (su *SpoiUpdate) BuildHex() string { return su.stream.BuildHex() }

// SpoiQuery — 查询 Builder
type SpoiQuery struct {
	stream SpoiStream
}

func NewSpoiQuery() *SpoiQuery { return &SpoiQuery{} }

func (sq *SpoiQuery) Nav(field uint32) *SpoiQuery {
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpFilter, Path: []uint32{field}})
	return sq
}

func (sq *SpoiQuery) Filter(field uint32, cmpOp uint8, value []byte) *SpoiQuery {
	var buf []byte
	writeU32(&buf, field)
	buf = append(buf, cmpOp)
	writeVarint(&buf, uint32(len(value)))
	buf = append(buf, value...)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpFilter, Operand: buf})
	return sq
}

func (sq *SpoiQuery) FilterI32(field uint32, cmpOp uint8, value int32) *SpoiQuery {
	var buf [8]byte
	binary.LittleEndian.PutUint32(buf[0:4], TypeIdI32)
	binary.LittleEndian.PutUint32(buf[4:8], uint32(value))
	return sq.Filter(field, cmpOp, buf[:])
}

func (sq *SpoiQuery) Select(fields ...uint32) *SpoiQuery {
	var buf []byte
	writeU32(&buf, uint32(len(fields)))
	for _, f := range fields {
		writeU32(&buf, f)
	}
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpSelect, Operand: buf})
	return sq
}

func (sq *SpoiQuery) Sort(field uint32, ascending bool) *SpoiQuery {
	var asc uint8 = 0
	if ascending { asc = 1 }
	var buf []byte
	writeU32(&buf, field)
	buf = append(buf, asc)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpSort, Operand: buf})
	return sq
}

func (sq *SpoiQuery) Reverse() *SpoiQuery {
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpReverse})
	return sq
}

func (sq *SpoiQuery) Take(count uint32) *SpoiQuery {
	var buf [4]byte
	binary.LittleEndian.PutUint32(buf[:], count)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpTake, Operand: buf[:]})
	return sq
}

func (sq *SpoiQuery) Drop(count uint32) *SpoiQuery {
	var buf [4]byte
	binary.LittleEndian.PutUint32(buf[:], count)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpDrop, Operand: buf[:]})
	return sq
}

func (sq *SpoiQuery) Distinct() *SpoiQuery {
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpDistinct})
	return sq
}

func (sq *SpoiQuery) Count() *SpoiQuery {
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpCount})
	return sq
}

func (sq *SpoiQuery) Keys() *SpoiQuery {
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpKeys})
	return sq
}

func (sq *SpoiQuery) Values() *SpoiQuery {
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpValues})
	return sq
}

func (sq *SpoiQuery) Join(field uint32) *SpoiQuery {
	var buf [4]byte
	binary.LittleEndian.PutUint32(buf[:], field)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpJoin, Operand: buf[:]})
	return sq
}

func (sq *SpoiQuery) Enumerate(start uint32) *SpoiQuery {
	var buf [4]byte
	binary.LittleEndian.PutUint32(buf[:], start)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpEnumerate, Operand: buf[:]})
	return sq
}

func (sq *SpoiQuery) Chunk(size uint32) *SpoiQuery {
	var buf [4]byte
	binary.LittleEndian.PutUint32(buf[:], size)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpChunk, Operand: buf[:]})
	return sq
}

func (sq *SpoiQuery) Takewhile(field uint32, cmpOp uint8, value []byte) *SpoiQuery {
	var buf []byte
	writeU32(&buf, field)
	buf = append(buf, cmpOp)
	writeVarint(&buf, uint32(len(value)))
	buf = append(buf, value...)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpTakeWhile, Operand: buf})
	return sq
}

func (sq *SpoiQuery) Dropwhile(field uint32, cmpOp uint8, value []byte) *SpoiQuery {
	var buf []byte
	writeU32(&buf, field)
	buf = append(buf, cmpOp)
	writeVarint(&buf, uint32(len(value)))
	buf = append(buf, value...)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpDropWhile, Operand: buf})
	return sq
}

func (sq *SpoiQuery) Any(field uint32, cmpOp uint8, value []byte) *SpoiQuery {
	var buf []byte
	writeU32(&buf, field)
	buf = append(buf, cmpOp)
	writeVarint(&buf, uint32(len(value)))
	buf = append(buf, value...)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpAny, Operand: buf})
	return sq
}

func (sq *SpoiQuery) All(field uint32, cmpOp uint8, value []byte) *SpoiQuery {
	var buf []byte
	writeU32(&buf, field)
	buf = append(buf, cmpOp)
	writeVarint(&buf, uint32(len(value)))
	buf = append(buf, value...)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpAll, Operand: buf})
	return sq
}

func (sq *SpoiQuery) Find(field uint32, cmpOp uint8, value []byte) *SpoiQuery {
	var buf []byte
	writeU32(&buf, field)
	buf = append(buf, cmpOp)
	writeVarint(&buf, uint32(len(value)))
	buf = append(buf, value...)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpFind, Operand: buf})
	return sq
}

func (sq *SpoiQuery) Slide(size uint32) *SpoiQuery {
	var buf [4]byte
	binary.LittleEndian.PutUint32(buf[:], size)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpSlide, Operand: buf[:]})
	return sq
}

func (sq *SpoiQuery) Stride(step uint32) *SpoiQuery {
	var buf [4]byte
	binary.LittleEndian.PutUint32(buf[:], step)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpStride, Operand: buf[:]})
	return sq
}

func (sq *SpoiQuery) Adjacent(n uint32) *SpoiQuery {
	var buf [4]byte
	binary.LittleEndian.PutUint32(buf[:], n)
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpAdjacent, Operand: buf[:]})
	return sq
}

func (sq *SpoiQuery) Build() []byte {
	sq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpExec})
	return sq.stream.Build()
}

