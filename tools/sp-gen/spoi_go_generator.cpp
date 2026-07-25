/*
    spoi_go_generator.cpp — Go SPOI 代码生成器

    生成 Go 的 SPOI 查询/更新 builder，包含：
    - 类型常量（成员索引）
    - SpoiQuery 流式 API
    - SpoiUpdate 写操作 API
    - SPOI 二进制序列化
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

std::string generateGoSpoi(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "// ============================================================\n";
    ss << "// SPOI — StreamPunk Operation Instruction\n";
    ss << "// Go 查询/更新 Builder（自动生成）\n";
    ss << "// ============================================================\n\n";
    ss << "package spoi\n\n";
    ss << "import (\n";
    ss << "\t\"encoding/binary\"\n";
    ss << "\t\"fmt\"\n";
    ss << "\t\"math\"\n";
    ss << ")\n\n";

    // ── type_id 常量（与 C++ E_type 枚举值一致） ──
    ss << "// 基本类型 ID（与 C++ E_type 枚举值一致，用于 operand 序列化）\n";
    ss << "const (\n";
    ss << "\tTypeIdU8     uint32 = " << static_cast<uint32_t>(E_type::u8)     << "\n";
    ss << "\tTypeIdU16    uint32 = " << static_cast<uint32_t>(E_type::u16)    << "\n";
    ss << "\tTypeIdU32    uint32 = " << static_cast<uint32_t>(E_type::u32)    << "\n";
    ss << "\tTypeIdU64    uint32 = " << static_cast<uint32_t>(E_type::u64)    << "\n";
    ss << "\tTypeIdI8     uint32 = " << static_cast<uint32_t>(E_type::i8)     << "\n";
    ss << "\tTypeIdI16    uint32 = " << static_cast<uint32_t>(E_type::i16)    << "\n";
    ss << "\tTypeIdI32    uint32 = " << static_cast<uint32_t>(E_type::i32)    << "\n";
    ss << "\tTypeIdI64    uint32 = " << static_cast<uint32_t>(E_type::i64)    << "\n";
    ss << "\tTypeIdF32    uint32 = " << static_cast<uint32_t>(E_type::f32)    << "\n";
    ss << "\tTypeIdF64    uint32 = " << static_cast<uint32_t>(E_type::f64)    << "\n";
    ss << "\tTypeIdString uint32 = " << static_cast<uint32_t>(E_type::string) << "\n";
    ss << "\tTypeIdBool   uint32 = " << static_cast<uint32_t>(E_type::bl)     << "\n";
    ss << ")\n\n";

    // ── 操作码常量 ──
    ss << "// 操作码\n";
    ss << "const (\n";
    ss << "\tOpSet       uint8 = 0x04\n";
    ss << "\tOpAdd       uint8 = 0x05\n";
    ss << "\tOpAppend    uint8 = 0x06\n";
    ss << "\tOpRemove    uint8 = 0x07\n";
    ss << "\tOpInsert    uint8 = 0x08\n";
    ss << "\tOpReplace   uint8 = 0x09\n";
    ss << "\tOpReset     uint8 = 0x0A\n";
    ss << "\tOpSetNull   uint8 = 0x0B\n";
    ss << "\tOpFilter    uint8 = 0x0C\n";
    ss << "\tOpSelect    uint8 = 0x0D\n";
    ss << "\tOpSort      uint8 = 0x0E\n";
    ss << "\tOpReverse   uint8 = 0x0F\n";
    ss << "\tOpTake      uint8 = 0x10\n";
    ss << "\tOpDrop      uint8 = 0x11\n";
    ss << "\tOpTakeWhile uint8 = 0x12\n";
    ss << "\tOpDropWhile uint8 = 0x13\n";
    ss << "\tOpDistinct  uint8 = 0x14\n";
    ss << "\tOpCount     uint8 = 0x15\n";
    ss << "\tOpAny       uint8 = 0x16\n";
    ss << "\tOpAll       uint8 = 0x17\n";
    ss << "\tOpFind      uint8 = 0x18\n";
    ss << "\tOpKeys      uint8 = 0x19\n";
    ss << "\tOpValues    uint8 = 0x1A\n";
    ss << "\tOpJoin      uint8 = 0x1B\n";
    ss << "\tOpEnumerate uint8 = 0x1C\n";
    ss << "\tOpChunk     uint8 = 0x1D\n";
    ss << "\tOpSlide     uint8 = 0x1E\n";
    ss << "\tOpStride    uint8 = 0x1F\n";
    ss << "\tOpAdjacent  uint8 = 0x20\n";
    ss << "\tOpExec      uint8 = 0x21\n";
    ss << ")\n\n";

    ss << "// 比较运算符\n";
    ss << "const (\n";
    ss << "\tCmpEQ uint8 = 0\n";
    ss << "\tCmpNE uint8 = 1\n";
    ss << "\tCmpLT uint8 = 2\n";
    ss << "\tCmpGT uint8 = 3\n";
    ss << "\tCmpLE uint8 = 4\n";
    ss << "\tCmpGE uint8 = 5\n";
    ss << ")\n\n";

    // ── 类型成员常量 ──
    ss << "// 类型成员索引常量\n";
    for (auto& t : types) {
        ss << "// " << t.className << "\n";
        ss << "const (\n";
        for (auto& f : t.fields) {
            ss << "\t" << t.className << "_" << f.name << " uint32 = " << f.index << "\n";
        }
        ss << ")\n\n";
    }

    // ── PathDeref 常量 ──
    ss << "const PathDeref uint32 = 0xFFFF\n\n";

    // ── Varint 编码 ──
    ss << "// Varint 编码\n";
    ss << "func writeVarint(buf *[]byte, v uint32) {\n";
    ss << "\tfor v >= 0x80 {\n";
    ss << "\t\t*buf = append(*buf, byte((v&0x7F)|0x80))\n";
    ss << "\t\tv >>= 7\n";
    ss << "\t}\n";
    ss << "\t*buf = append(*buf, byte(v&0x7F))\n";
    ss << "}\n\n";
    ss << "func writeU32(buf *[]byte, v uint32) {\n";
    ss << "\tvar b [4]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(b[:], v)\n";
    ss << "\t*buf = append(*buf, b[:]...)\n";
    ss << "}\n\n";

    // ── SpoiInstruction ──
    ss << "// SpoiInstruction\n";
    ss << "type SpoiInstruction struct {\n";
    ss << "\tOp      uint8\n";
    ss << "\tPath    []uint32\n";
    ss << "\tOperand []byte\n";
    ss << "}\n\n";
    ss << "func (si *SpoiInstruction) Serialize() []byte {\n";
    ss << "\tvar buf []byte\n";
    ss << "\tbuf = append(buf, si.Op)\n";
    ss << "\twriteVarint(&buf, uint32(len(si.Path)))\n";
    ss << "\tfor _, seg := range si.Path {\n";
    ss << "\t\twriteU32(&buf, seg)\n";
    ss << "\t}\n";
    ss << "\twriteVarint(&buf, uint32(len(si.Operand)))\n";
    ss << "\tbuf = append(buf, si.Operand...)\n";
    ss << "\treturn buf\n";
    ss << "}\n\n";

    // ── SpoiStream ──
    ss << "// SpoiStream\n";
    ss << "type SpoiStream struct {\n";
    ss << "\tInstructions []SpoiInstruction\n";
    ss << "}\n\n";
    ss << "func (ss *SpoiStream) Build() []byte {\n";
    ss << "\tvar buf []byte\n";
    ss << "\twriteVarint(&buf, uint32(len(ss.Instructions)))\n";
    ss << "\tfor _, inst := range ss.Instructions {\n";
    ss << "\t\tbuf = append(buf, inst.Serialize()...)\n";
    ss << "\t}\n";
    ss << "\treturn buf\n";
    ss << "}\n\n";
    ss << "func (ss *SpoiStream) BuildHex() string {\n";
    ss << "\treturn fmt.Sprintf(\"%x\", ss.Build())\n";
    ss << "}\n\n";

    // ── SpoiUpdate ──
    ss << "// SpoiUpdate — 写操作 Builder\n";
    ss << "type SpoiUpdate struct {\n";
    ss << "\tstream SpoiStream\n";
    ss << "}\n\n";
    ss << "func NewSpoiUpdate() *SpoiUpdate { return &SpoiUpdate{} }\n\n";
    ss << "func (su *SpoiUpdate) Set(path []uint32, value []byte) *SpoiUpdate {\n";
    ss << "\tsu.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpSet, Path: path, Operand: value})\n";
    ss << "\treturn su\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) SetI32(path []uint32, value int32) *SpoiUpdate {\n";
    ss << "\tvar buf [8]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[0:4], TypeIdI32)\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[4:8], uint32(value))\n";
    ss << "\treturn su.Set(path, buf[:])\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) SetU32(path []uint32, value uint32) *SpoiUpdate {\n";
    ss << "\tvar buf [8]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[0:4], TypeIdU32)\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[4:8], value)\n";
    ss << "\treturn su.Set(path, buf[:])\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) SetF64(path []uint32, value float64) *SpoiUpdate {\n";
    ss << "\tvar buf [12]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[0:4], TypeIdF64)\n";
    ss << "\tbinary.LittleEndian.PutUint64(buf[4:12], math.Float64bits(value))\n";
    ss << "\treturn su.Set(path, buf[:])\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) SetStr(path []uint32, value string) *SpoiUpdate {\n";
    ss << "\tvar buf []byte\n";
    ss << "\twriteU32(&buf, TypeIdString)\n";
    ss << "\tbuf = append(buf, []byte(value)...)\n";
    ss << "\treturn su.Set(path, buf)\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) SetBool(path []uint32, value bool) *SpoiUpdate {\n";
    ss << "\tvar buf [5]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[0:4], TypeIdBool)\n";
    ss << "\tif value { buf[4] = 1 } else { buf[4] = 0 }\n";
    ss << "\treturn su.Set(path, buf[:])\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) AddI32(path []uint32, delta int32) *SpoiUpdate {\n";
    ss << "\tvar buf [4]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[:], uint32(delta))\n";
    ss << "\tsu.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpAdd, Path: path, Operand: buf[:]})\n";
    ss << "\treturn su\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) Add(path []uint32, value []byte) *SpoiUpdate {\n";
    ss << "\tsu.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpAdd, Path: path, Operand: value})\n";
    ss << "\treturn su\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) Append(path []uint32, value []byte) *SpoiUpdate {\n";
    ss << "\tsu.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpAppend, Path: path, Operand: value})\n";
    ss << "\treturn su\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) Remove(path []uint32, value []byte) *SpoiUpdate {\n";
    ss << "\tsu.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpRemove, Path: path, Operand: value})\n";
    ss << "\treturn su\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) Insert(path []uint32, value []byte) *SpoiUpdate {\n";
    ss << "\tsu.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpInsert, Path: path, Operand: value})\n";
    ss << "\treturn su\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) Replace(path []uint32, value []byte) *SpoiUpdate {\n";
    ss << "\tsu.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpReplace, Path: path, Operand: value})\n";
    ss << "\treturn su\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) Reset(path []uint32) *SpoiUpdate {\n";
    ss << "\tsu.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpReset, Path: path})\n";
    ss << "\treturn su\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) Setnull(path []uint32) *SpoiUpdate {\n";
    ss << "\tsu.stream.Instructions = append(su.stream.Instructions, SpoiInstruction{Op: OpSetNull, Path: path})\n";
    ss << "\treturn su\n";
    ss << "}\n\n";
    ss << "func (su *SpoiUpdate) Build() []byte  { return su.stream.Build() }\n";
    ss << "func (su *SpoiUpdate) BuildHex() string { return su.stream.BuildHex() }\n\n";

    // ── SpoiQuery ──
    ss << "// SpoiQuery — 查询 Builder\n";
    ss << "type SpoiQuery struct {\n";
    ss << "\tstream SpoiStream\n";
    ss << "}\n\n";
    ss << "func NewSpoiQuery() *SpoiQuery { return &SpoiQuery{} }\n\n";
    ss << "func (sq *SpoiQuery) Nav(field uint32) *SpoiQuery {\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpFilter, Path: []uint32{field}})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Filter(field uint32, cmpOp uint8, value []byte) *SpoiQuery {\n";
    ss << "\tvar buf []byte\n";
    ss << "\twriteU32(&buf, field)\n";
    ss << "\tbuf = append(buf, cmpOp)\n";
    ss << "\twriteVarint(&buf, uint32(len(value)))\n";
    ss << "\tbuf = append(buf, value...)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpFilter, Operand: buf})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) FilterI32(field uint32, cmpOp uint8, value int32) *SpoiQuery {\n";
    ss << "\tvar buf [8]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[0:4], TypeIdI32)\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[4:8], uint32(value))\n";
    ss << "\treturn sq.Filter(field, cmpOp, buf[:])\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Select(fields ...uint32) *SpoiQuery {\n";
    ss << "\tvar buf []byte\n";
    ss << "\twriteU32(&buf, uint32(len(fields)))\n";
    ss << "\tfor _, f := range fields {\n";
    ss << "\t\twriteU32(&buf, f)\n";
    ss << "\t}\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpSelect, Operand: buf})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Sort(field uint32, ascending bool) *SpoiQuery {\n";
    ss << "\tvar asc uint8 = 0\n";
    ss << "\tif ascending { asc = 1 }\n";
    ss << "\tvar buf []byte\n";
    ss << "\twriteU32(&buf, field)\n";
    ss << "\tbuf = append(buf, asc)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpSort, Operand: buf})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Reverse() *SpoiQuery {\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpReverse})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Take(count uint32) *SpoiQuery {\n";
    ss << "\tvar buf [4]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[:], count)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpTake, Operand: buf[:]})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Drop(count uint32) *SpoiQuery {\n";
    ss << "\tvar buf [4]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[:], count)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpDrop, Operand: buf[:]})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Distinct() *SpoiQuery {\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpDistinct})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Count() *SpoiQuery {\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpCount})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Keys() *SpoiQuery {\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpKeys})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Values() *SpoiQuery {\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpValues})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Join(field uint32) *SpoiQuery {\n";
    ss << "\tvar buf [4]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[:], field)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpJoin, Operand: buf[:]})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Enumerate(start uint32) *SpoiQuery {\n";
    ss << "\tvar buf [4]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[:], start)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpEnumerate, Operand: buf[:]})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Chunk(size uint32) *SpoiQuery {\n";
    ss << "\tvar buf [4]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[:], size)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpChunk, Operand: buf[:]})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Takewhile(field uint32, cmpOp uint8, value []byte) *SpoiQuery {\n";
    ss << "\tvar buf []byte\n";
    ss << "\twriteU32(&buf, field)\n";
    ss << "\tbuf = append(buf, cmpOp)\n";
    ss << "\twriteVarint(&buf, uint32(len(value)))\n";
    ss << "\tbuf = append(buf, value...)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpTakeWhile, Operand: buf})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Dropwhile(field uint32, cmpOp uint8, value []byte) *SpoiQuery {\n";
    ss << "\tvar buf []byte\n";
    ss << "\twriteU32(&buf, field)\n";
    ss << "\tbuf = append(buf, cmpOp)\n";
    ss << "\twriteVarint(&buf, uint32(len(value)))\n";
    ss << "\tbuf = append(buf, value...)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpDropWhile, Operand: buf})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Any(field uint32, cmpOp uint8, value []byte) *SpoiQuery {\n";
    ss << "\tvar buf []byte\n";
    ss << "\twriteU32(&buf, field)\n";
    ss << "\tbuf = append(buf, cmpOp)\n";
    ss << "\twriteVarint(&buf, uint32(len(value)))\n";
    ss << "\tbuf = append(buf, value...)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpAny, Operand: buf})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) All(field uint32, cmpOp uint8, value []byte) *SpoiQuery {\n";
    ss << "\tvar buf []byte\n";
    ss << "\twriteU32(&buf, field)\n";
    ss << "\tbuf = append(buf, cmpOp)\n";
    ss << "\twriteVarint(&buf, uint32(len(value)))\n";
    ss << "\tbuf = append(buf, value...)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpAll, Operand: buf})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Find(field uint32, cmpOp uint8, value []byte) *SpoiQuery {\n";
    ss << "\tvar buf []byte\n";
    ss << "\twriteU32(&buf, field)\n";
    ss << "\tbuf = append(buf, cmpOp)\n";
    ss << "\twriteVarint(&buf, uint32(len(value)))\n";
    ss << "\tbuf = append(buf, value...)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpFind, Operand: buf})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Slide(size uint32) *SpoiQuery {\n";
    ss << "\tvar buf [4]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[:], size)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpSlide, Operand: buf[:]})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Stride(step uint32) *SpoiQuery {\n";
    ss << "\tvar buf [4]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[:], step)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpStride, Operand: buf[:]})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Adjacent(n uint32) *SpoiQuery {\n";
    ss << "\tvar buf [4]byte\n";
    ss << "\tbinary.LittleEndian.PutUint32(buf[:], n)\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpAdjacent, Operand: buf[:]})\n";
    ss << "\treturn sq\n";
    ss << "}\n\n";
    ss << "func (sq *SpoiQuery) Build() []byte {\n";
    ss << "\tsq.stream.Instructions = append(sq.stream.Instructions, SpoiInstruction{Op: OpExec})\n";
    ss << "\treturn sq.stream.Build()\n";
    ss << "}\n\n";

    return ss.str();
}

int generate_spoi_go(const std::string& output_path, const std::string& meta_path) {
    try {
        auto meta = sp_meta::readMetaFile(meta_path);
        auto types = extractSpoiTypes(meta);
        auto code = generateGoSpoi(types);

        std::ofstream out(output_path);
        if (!out.is_open()) throw std::runtime_error("Cannot open: " + output_path);
        out << code;
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "SPOI Go generator error: " << e.what() << "\n";
        return 1;
    }
}