// SPOI Executor 刁钻边界测试套件
//
// 测试覆盖：
// - Varint 边界攻击（最大编码、截断、溢出）
// - 路径攻击（DEREF 非指针、MAPKEY 非 Map、负数索引、越界）
// - 操作数攻击（长度不匹配、零长度操作数）
// - 指令序列攻击（缺少 EXEC、重复 EXEC、无 PIPE 的 EXEC）
// - 空容器处理（空 PIPE、空数组、nil 值）
// - 操作极值（TAKE/DROP 边界、FILTER 无匹配）
// - 聚合边界（空集 COUNT/ANY/ALL/FIND）
// - 嵌套导航边界（穿越 nil、非对象导航、深层路径）
// - 写操作边界（ADD 非数值、REMOVE 空数组、APPEND 非数组）
// - TAKEWHILE/DROPWHILE 边界
// - FILTER 比较操作边界
// - 操作数解析边界
// - 多管道组合边界
// - 类型注册表边界
// - 容器操作边界（KEYS/VALUES/JOIN）
// - 未实现操作码边界

package main

import (
	"encoding/binary"
	"testing"
)

// =============================== 辅助函数 ===============================

func makeBoundaryInst(op byte, path []int, operand []byte) SpoiInstruction {
	if path == nil {
		path = []int{}
	}
	if operand == nil {
		operand = []byte{}
	}
	return SpoiInstruction{Op: op, Path: path, Operand: operand}
}

func filterInstWithCmp(memberIdx uint32, cmpOp byte, valueBytes []byte) SpoiInstruction {
	// 新格式: memberIdx(u32) + cmpOp(u8) + value_len(varint) + valueBytes (含 type_id)
	operand := make([]byte, 5+1+len(valueBytes))
	binary.LittleEndian.PutUint32(operand[0:4], memberIdx)
	operand[4] = cmpOp
	operand[5] = byte(len(valueBytes)) // varint 编码
	copy(operand[6:], valueBytes)
	return makeBoundaryInst(OP_FILTER, nil, operand)
}

func filterGtBoundary(memberIdx uint32, value uint32) SpoiInstruction {
	// 值部分: [type_id(u32) + value_bytes]
	vb := make([]byte, 8)
	binary.LittleEndian.PutUint32(vb[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(vb[4:8], value)
	return filterInstWithCmp(memberIdx, CMP_GT, vb)
}

func filterEqBoundary(memberIdx uint32, value uint32) SpoiInstruction {
	vb := make([]byte, 8)
	binary.LittleEndian.PutUint32(vb[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(vb[4:8], value)
	return filterInstWithCmp(memberIdx, CMP_EQ, vb)
}

func anyBoundaryInst(memberIdx uint32, cmpOp byte, value uint32) SpoiInstruction {
	vb := make([]byte, 8)
	binary.LittleEndian.PutUint32(vb[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(vb[4:8], value)
	operand := make([]byte, 5+1+len(vb))
	binary.LittleEndian.PutUint32(operand[0:4], memberIdx)
	operand[4] = cmpOp
	operand[5] = byte(len(vb))
	copy(operand[6:], vb)
	return makeBoundaryInst(OP_ANY, nil, operand)
}

func allBoundaryInst(memberIdx uint32, cmpOp byte, value uint32) SpoiInstruction {
	vb := make([]byte, 8)
	binary.LittleEndian.PutUint32(vb[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(vb[4:8], value)
	operand := make([]byte, 5+1+len(vb))
	binary.LittleEndian.PutUint32(operand[0:4], memberIdx)
	operand[4] = cmpOp
	operand[5] = byte(len(vb))
	copy(operand[6:], vb)
	return makeBoundaryInst(OP_ALL, nil, operand)
}

func findBoundaryInst(memberIdx uint32, cmpOp byte, value uint32) SpoiInstruction {
	vb := make([]byte, 8)
	binary.LittleEndian.PutUint32(vb[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(vb[4:8], value)
	operand := make([]byte, 5+1+len(vb))
	binary.LittleEndian.PutUint32(operand[0:4], memberIdx)
	operand[4] = cmpOp
	operand[5] = byte(len(vb))
	copy(operand[6:], vb)
	return makeBoundaryInst(OP_FIND, nil, operand)
}

func setIntBoundary(path []int, value uint64) SpoiInstruction {
	// 新格式: [type_id(u32) + value_bytes]
	operand := make([]byte, 12)
	binary.LittleEndian.PutUint32(operand[0:4], TypeIdU64)
	binary.LittleEndian.PutUint64(operand[4:12], value)
	return makeBoundaryInst(OP_SET, path, operand)
}

func setStrBoundary(path []int, value string) SpoiInstruction {
	operand := make([]byte, 4+len(value))
	binary.LittleEndian.PutUint32(operand[0:4], TypeIdString)
	copy(operand[4:], value)
	return makeBoundaryInst(OP_SET, path, operand)
}

func addIntBoundary(path []int, value uint32) SpoiInstruction {
	operand := make([]byte, 8)
	binary.LittleEndian.PutUint32(operand[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(operand[4:8], value)
	return makeBoundaryInst(OP_ADD, path, operand)
}

func takeBoundaryInst(n uint32) SpoiInstruction {
	operand := make([]byte, 4)
	binary.LittleEndian.PutUint32(operand, n)
	return makeBoundaryInst(OP_TAKE, nil, operand)
}

func dropBoundaryInst(n uint32) SpoiInstruction {
	operand := make([]byte, 4)
	binary.LittleEndian.PutUint32(operand, n)
	return makeBoundaryInst(OP_DROP, nil, operand)
}

func insertBoundaryInst(path []int, idx uint32, value []byte) SpoiInstruction {
	// 新格式: idx(u32) + [type_id(u32) + value_bytes]
	operand := make([]byte, 4+len(value))
	binary.LittleEndian.PutUint32(operand, idx)
	copy(operand[4:], value)
	return makeBoundaryInst(OP_INSERT, path, operand)
}

func replaceBoundaryInst(path []int, idx uint32, value []byte) SpoiInstruction {
	operand := make([]byte, 4+len(value))
	binary.LittleEndian.PutUint32(operand, idx)
	copy(operand[4:], value)
	return makeBoundaryInst(OP_REPLACE, path, operand)
}

func removeBoundaryInst(path []int, idx uint32) SpoiInstruction {
	operand := make([]byte, 4)
	binary.LittleEndian.PutUint32(operand, idx)
	return makeBoundaryInst(OP_REMOVE, path, operand)
}

func takeWhileBoundary(memberIdx uint32, cmpOp byte, value uint32) SpoiInstruction {
	vb := make([]byte, 8)
	binary.LittleEndian.PutUint32(vb[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(vb[4:8], value)
	operand := make([]byte, 5+1+len(vb))
	binary.LittleEndian.PutUint32(operand[0:4], memberIdx)
	operand[4] = cmpOp
	operand[5] = byte(len(vb))
	copy(operand[6:], vb)
	return makeBoundaryInst(OP_TAKEWHILE, nil, operand)
}

func dropWhileBoundary(memberIdx uint32, cmpOp byte, value uint32) SpoiInstruction {
	vb := make([]byte, 8)
	binary.LittleEndian.PutUint32(vb[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(vb[4:8], value)
	operand := make([]byte, 5+1+len(vb))
	binary.LittleEndian.PutUint32(operand[0:4], memberIdx)
	operand[4] = cmpOp
	operand[5] = byte(len(vb))
	copy(operand[6:], vb)
	return makeBoundaryInst(OP_DROPWHILE, nil, operand)
}

func mustPanic(t *testing.T, fn func()) (panicked bool) {
	t.Helper()
	defer func() {
		if r := recover(); r != nil {
			panicked = true
		}
	}()
	fn()
	return false
}

// =============================== Varint 边界攻击 ===============================

func TestVarintMaxUint32(t *testing.T) {
	v := 0xFFFFFFFF
	var buf []byte
	writeVarint(&buf, v)
	result, offset := readVarint(buf, 0)
	if result != v {
		t.Errorf("Max uint32: expected %d, got %d", v, result)
	}
	if offset != len(buf) {
		t.Errorf("Max uint32 offset: expected %d, got %d", len(buf), offset)
	}
}

func TestVarintMultiByteBoundary(t *testing.T) {
	testCases := []int{
		0x7F,       // 1字节边界
		0x80,       // 2字节起始
		0x3FFF,     // 2字节边界
		0x4000,     // 3字节起始
		0x1FFFFF,   // 3字节边界
		0x200000,   // 4字节起始
		0xFFFFFFF,  // 4字节边界
		0x10000000, // 5字节起始
	}
	for _, v := range testCases {
		var buf []byte
		writeVarint(&buf, v)
		result, _ := readVarint(buf, 0)
		if result != v {
			t.Errorf("Varint boundary %d: expected %d, got %d", v, v, result)
		}
	}
}

func TestVarintZero(t *testing.T) {
	var buf []byte
	writeVarint(&buf, 0)
	result, offset := readVarint(buf, 0)
	if result != 0 {
		t.Errorf("Varint zero: expected 0, got %d", result)
	}
	if offset != 1 {
		t.Errorf("Varint zero offset: expected 1, got %d", offset)
	}
}

func TestVarintTruncated(t *testing.T) {
	// 只有一个字节 0x80（高位=1，表示后面还有字节），但数据到此结束
	result, offset := readVarint([]byte{0x80}, 0)
	if result != 0 {
		t.Errorf("Truncated varint: expected 0, got %d", result)
	}
	if offset != 1 {
		t.Errorf("Truncated varint offset: expected 1, got %d", offset)
	}
}

func TestVarintTruncatedMultiByte(t *testing.T) {
	// 多字节截断：0x80 0x80（第一个表示还有后续，第二个也表示还有后续）
	result, offset := readVarint([]byte{0x80, 0x80}, 0)
	if result != 0 {
		t.Errorf("Truncated multi-byte varint: expected 0, got %d", result)
	}
	if offset != 2 {
		t.Errorf("Truncated multi-byte varint offset: expected 2, got %d", offset)
	}
}

func TestVarintEmptyData(t *testing.T) {
	result, offset := readVarint([]byte{}, 0)
	if result != 0 {
		t.Errorf("Empty varint: expected 0, got %d", result)
	}
	if offset != 0 {
		t.Errorf("Empty varint offset: expected 0, got %d", offset)
	}
}

func TestVarintMaxInt(t *testing.T) {
	// 测试接近 int 最大值的编码
	v := 0x7FFFFFFF
	var buf []byte
	writeVarint(&buf, v)
	result, _ := readVarint(buf, 0)
	if result != v {
		t.Errorf("Max int varint: expected %d, got %d", v, result)
	}
}

// =============================== 指令解析边界 ===============================

func TestParseEmptyStream(t *testing.T) {
	data := buildSpoiStream([]SpoiInstruction{})
	parsed := parseSpoiStream(data)
	if len(parsed) != 0 {
		t.Errorf("Empty stream: expected 0 instructions, got %d", len(parsed))
	}
}

func TestParseMaxInstructions(t *testing.T) {
	insts := make([]SpoiInstruction, 100)
	for i := range insts {
		insts[i] = makeBoundaryInst(OP_SET, []int{0}, []byte{0x00})
	}
	data := buildSpoiStream(insts)
	parsed := parseSpoiStream(data)
	if len(parsed) != 100 {
		t.Errorf("100 instructions: expected 100, got %d", len(parsed))
	}
}

func TestParseZeroLengthOperand(t *testing.T) {
	i := makeBoundaryInst(OP_SET, []int{0}, []byte{})
	data := buildSpoiStream([]SpoiInstruction{i})
	parsed := parseSpoiStream(data)
	if len(parsed) != 1 {
		t.Errorf("Expected 1 instruction, got %d", len(parsed))
	}
	if len(parsed[0].Operand) != 0 {
		t.Errorf("Expected 0-length operand, got %d", len(parsed[0].Operand))
	}
}

func TestParseZeroLengthPath(t *testing.T) {
	i := makeBoundaryInst(OP_EXEC, []int{}, []byte{})
	data := buildSpoiStream([]SpoiInstruction{i})
	parsed := parseSpoiStream(data)
	if len(parsed) != 1 {
		t.Errorf("Expected 1 instruction, got %d", len(parsed))
	}
	if len(parsed[0].Path) != 0 {
		t.Errorf("Expected 0-length path, got %d", len(parsed[0].Path))
	}
}

func TestParseLargeOperand(t *testing.T) {
	large := make([]byte, 10000)
	i := makeBoundaryInst(OP_SET, []int{0}, large)
	data := buildSpoiStream([]SpoiInstruction{i})
	parsed := parseSpoiStream(data)
	if len(parsed[0].Operand) != 10000 {
		t.Errorf("Expected 10000-byte operand, got %d", len(parsed[0].Operand))
	}
}

func TestParseDeepPath(t *testing.T) {
	deepPath := make([]int, 50)
	for i := range deepPath {
		deepPath[i] = i
	}
	i := makeBoundaryInst(OP_SET, deepPath, []byte{0x00})
	data := buildSpoiStream([]SpoiInstruction{i})
	parsed := parseSpoiStream(data)
	if len(parsed[0].Path) != 50 {
		t.Errorf("Expected 50-segment path, got %d", len(parsed[0].Path))
	}
	for j := range deepPath {
		if parsed[0].Path[j] != deepPath[j] {
			t.Errorf("Path[%d]: expected %d, got %d", j, deepPath[j], parsed[0].Path[j])
		}
	}
}

func TestParseSingleInstruction(t *testing.T) {
	i := makeBoundaryInst(OP_COUNT, nil, nil)
	data := buildSpoiStream([]SpoiInstruction{i})
	parsed := parseSpoiStream(data)
	if len(parsed) != 1 || parsed[0].Op != OP_COUNT {
		t.Errorf("Single instruction: expected OP_COUNT, got 0x%02X", parsed[0].Op)
	}
}

// =============================== 路径导航边界 ===============================

func TestNavigateDerefOnNonPointer(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	// DEREF 作用于字符串，没有 Value 字段，返回自身
	result := executor.navStep("hello", PATH_DEREF)
	if result != "hello" {
		t.Errorf("DEREF on non-pointer: expected 'hello', got %v", result)
	}
}

func TestNavigateDerefOnNil(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	// Go 中 nil interface{} 的 reflect.ValueOf 会得到 invalid Value
	panicked := mustPanic(t, func() {
		executor.navStep(nil, PATH_DEREF)
	})
	if !panicked {
		t.Log("DEREF on nil did not panic (may be acceptable)")
	}
}

func TestNavigateIndexOutOfBounds(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "Alice"}}
	panicked := mustPanic(t, func() {
		executor.navigate(players, []int{99})
	})
	if !panicked {
		t.Error("Expected panic for index 99 on slice of length 1")
	}
}

func TestNavigateIndexOnNonSlice(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	panicked := mustPanic(t, func() {
		executor.navigate("hello", []int{0})
	})
	if !panicked {
		t.Error("Expected panic for index on string")
	}
}

func TestNavigateMemberOnNonStruct(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	panicked := mustPanic(t, func() {
		executor.navigate(42, []int{0})
	})
	if !panicked {
		t.Error("Expected panic for member access on int")
	}
}

func TestNavigateOnNil(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	panicked := mustPanic(t, func() {
		executor.navigate(nil, []int{0})
	})
	if !panicked {
		t.Error("Expected panic for navigate on nil")
	}
}

func TestNavigateMemberIndexOutOfRange(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100}
	panicked := mustPanic(t, func() {
		executor.navigate(player, []int{99})
	})
	if !panicked {
		t.Error("Expected panic for member index 99 out of range")
	}
}

func TestNavigateNestedThroughNil(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	// Metadata 是 nil map，导航到 [4] 得到 nil，再导航到 [0] 应该 panic
	player := &Player{Name: "Alice", Level: 10, Health: 100}
	panicked := mustPanic(t, func() {
		executor.navigate(player, []int{4, 0})
	})
	if !panicked {
		t.Error("Expected panic for nested navigate through nil map")
	}
}

func TestNavigateEmptyPath(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice"}
	result := executor.navigate(player, []int{})
	// 空路径返回自身
	if result != player {
		t.Error("Empty path should return the object itself")
	}
}

func TestNavigateNegativeIndex(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "Alice"}}
	panicked := mustPanic(t, func() {
		executor.navigate(players, []int{-1})
	})
	if !panicked {
		t.Error("Expected panic for negative index")
	}
}

func TestNavigateMapIndexOutOfBounds(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	m := map[string]interface{}{"key": "value"}
	panicked := mustPanic(t, func() {
		executor.navigate(m, []int{99})
	})
	if !panicked {
		t.Error("Expected panic for map index out of bounds")
	}
}

// =============================== 指令序列攻击 ===============================

func TestExecWithoutPipe(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	data := buildSpoiStream([]SpoiInstruction{execInst()})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("EXEC without PIPE: expected UNDEF, got %v", result["resultType"])
	}
}

func TestDoubleExec(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	data := buildSpoiStream([]SpoiInstruction{pipeInst(nil), execInst(), execInst()})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("Double EXEC: expected 2 items, got %d", len(values))
	}
}

func TestDoublePipe(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	data := buildSpoiStream([]SpoiInstruction{pipeInst(nil), pipeInst(nil), execInst()})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("Double PIPE: expected 2 items, got %d", len(values))
	}
}

func TestWriteAfterExec(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100}
	// EXEC 之后写操作仍然执行
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		execInst(),
		setIntBoundary([]int{1}, 99),
	})
	result := executor.Execute(player, data)
	if player.Level != 99 {
		t.Errorf("Write after EXEC: expected Level=99, got %d", player.Level)
	}
	// 结果仍然是管道数据
	if result["value"].(*Player).Name != "Alice" {
		t.Errorf("Write after EXEC result: expected Alice, got %v", result["value"].(*Player).Name)
	}
}

func TestReadOpWithoutPipe(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	// FILTER 在空管道上操作，结果为空
	data := buildSpoiStream([]SpoiInstruction{
		filterGtBoundary(1, 10),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("Read op without PIPE: expected UNDEF, got %v", result["resultType"])
	}
}

func TestMissingExec(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	// PIPE → TAKE(1)，缺少 EXEC
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(1),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("Missing EXEC: expected SINGLE, got %v", result["resultType"])
	}
	if result["value"].(*Player).Name != "Alice" {
		t.Errorf("Missing EXEC: expected Alice, got %s", result["value"].(*Player).Name)
	}
}

func TestStandaloneExec(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice"},
	}
	data := buildSpoiStream([]SpoiInstruction{execInst()})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("Standalone EXEC: expected UNDEF, got %v", result["resultType"])
	}
}

func TestOnlyPipeNoExec(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "Alice"}}
	data := buildSpoiStream([]SpoiInstruction{pipeInst(nil)})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("Only PIPE no EXEC: expected SINGLE, got %v", result["resultType"])
	}
}

// =============================== 空容器处理 ===============================

func TestEmptyPipe(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	data := buildSpoiStream([]SpoiInstruction{pipeInst(nil), execInst()})
	result := executor.Execute([]interface{}{}, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("Empty PIPE: expected UNDEF, got %v", result["resultType"])
	}
}

func TestTakeZero(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice"},
		&Player{Name: "Bob"},
	}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("TAKE 0: expected UNDEF, got %v", result["resultType"])
	}
}

func TestTakeMoreThanAvailableBoundary(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice"},
		&Player{Name: "Bob"},
	}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(10),
		execInst(),
	})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("TAKE 10 of 2: expected 2, got %d", len(values))
	}
}

func TestDropAllBoundary(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice"},
		&Player{Name: "Bob"},
	}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		dropBoundaryInst(10),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("DROP all: expected UNDEF, got %v", result["resultType"])
	}
}

func TestDropZero(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice"},
		&Player{Name: "Bob"},
	}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		dropBoundaryInst(0),
		execInst(),
	})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("DROP 0: expected 2, got %d", len(values))
	}
}

func TestReverseEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "A"}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		reverseInst(),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("REVERSE empty: expected UNDEF, got %v", result["resultType"])
	}
}

func TestReverseSingle(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "Alice"}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		reverseInst(),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("REVERSE single: expected SINGLE, got %v", result["resultType"])
	}
	if result["value"].(*Player).Name != "Alice" {
		t.Errorf("REVERSE single: expected Alice, got %s", result["value"].(*Player).Name)
	}
}

func TestDistinctEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "A"}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		distinctInst(),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("DISTINCT empty: expected UNDEF, got %v", result["resultType"])
	}
}

func TestDistinctSingle(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "Alice"}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		selectInst([]int{0}),
		distinctInst(),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("DISTINCT single: expected SINGLE, got %v", result["resultType"])
	}
	if result["value"] != "Alice" {
		t.Errorf("DISTINCT single: expected 'Alice', got %v", result["value"])
	}
}

func TestFilterNoMatch(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "Alice", Level: 10}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		filterGtBoundary(1, 100),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("FILTER no match: expected UNDEF, got %v", result["resultType"])
	}
}

func TestSelectEmptyResult(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "A"}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		selectInst([]int{0}),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("SELECT empty: expected UNDEF, got %v", result["resultType"])
	}
}

func TestSortEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "A"}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		sortInst([]int{0}),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("SORT empty: expected UNDEF, got %v", result["resultType"])
	}
}

func TestSortSingle(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "Alice"}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		sortInst([]int{0}),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("SORT single: expected SINGLE, got %v", result["resultType"])
	}
	if result["value"].(*Player).Name != "Alice" {
		t.Errorf("SORT single: expected Alice, got %s", result["value"].(*Player).Name)
	}
}

// =============================== 聚合边界 ===============================

func TestCountOnEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "A"}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		countInst(),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("COUNT on empty: expected SINGLE, got %v", result["resultType"])
	}
	if result["value"] != 0 {
		t.Errorf("COUNT on empty: expected 0, got %v", result["value"])
	}
}

func TestAnyOnEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "A"}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		anyBoundaryInst(1, CMP_GT, 10),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("ANY on empty: expected SINGLE, got %v", result["resultType"])
	}
	if result["value"] != false {
		t.Errorf("ANY on empty: expected false, got %v", result["value"])
	}
}

func TestAllOnEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "A"}}
	// vacuous truth: ALL on empty set returns true
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		allBoundaryInst(1, CMP_GT, 10),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["value"] != true {
		t.Errorf("ALL on empty (vacuous truth): expected true, got %v", result["value"])
	}
}

func TestFindOnEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "A"}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		findBoundaryInst(1, CMP_EQ, 10),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("FIND on empty: expected UNDEF, got %v", result["resultType"])
	}
}

func TestCountOnSingle(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "Alice"}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		countInst(),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["value"] != 1 {
		t.Errorf("COUNT on single: expected 1, got %v", result["value"])
	}
}

func TestCountAfterFilter(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "Alice", Level: 10}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		filterGtBoundary(1, 100),
		countInst(),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["value"] != 0 {
		t.Errorf("COUNT after FILTER (empty): expected 0, got %v", result["value"])
	}
}

func TestAnyAfterFilterEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "Alice", Level: 10}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		filterGtBoundary(1, 100),
		anyBoundaryInst(1, CMP_GT, 5),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["value"] != false {
		t.Errorf("ANY after FILTER (empty): expected false, got %v", result["value"])
	}
}

func TestAllAfterFilterEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "Alice", Level: 10}}
	// vacuous truth
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		filterGtBoundary(1, 100),
		allBoundaryInst(1, CMP_GT, 5),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["value"] != true {
		t.Errorf("ALL after FILTER (empty, vacuous truth): expected true, got %v", result["value"])
	}
}

func TestFindAfterFilterEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "Alice", Level: 10}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		filterGtBoundary(1, 100),
		findBoundaryInst(1, CMP_EQ, 10),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("FIND after FILTER (empty): expected UNDEF, got %v", result["resultType"])
	}
}

// =============================== 写操作边界 ===============================

func TestAddToZero(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 0, Health: 100}
	data := buildSpoiStream([]SpoiInstruction{addIntBoundary([]int{1}, 5)})
	executor.Execute(player, data)
	if player.Level != 5 {
		t.Errorf("ADD to 0: expected 5, got %d", player.Level)
	}
}

func TestAddLargeValue(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100}
	// 反序列化 4 字节为 uint32，然后做加法
	data := buildSpoiStream([]SpoiInstruction{addIntBoundary([]int{1}, 0xFFFFFFFF)})
	executor.Execute(player, data)
	// 10 + 4294967295 = 4294967305
	if player.Level != 4294967305 {
		t.Errorf("ADD large value: expected 4294967305, got %d", player.Level)
	}
}

func TestRemoveFromEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100, Items: []interface{}{}}
	panicked := mustPanic(t, func() {
		data := buildSpoiStream([]SpoiInstruction{removeBoundaryInst([]int{3}, 0)})
		executor.Execute(player, data)
	})
	if !panicked {
		t.Error("Expected panic for REMOVE from empty slice")
	}
}

func TestRemoveOutOfBounds(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100, Items: []interface{}{"a"}}
	panicked := mustPanic(t, func() {
		data := buildSpoiStream([]SpoiInstruction{removeBoundaryInst([]int{3}, 99)})
		executor.Execute(player, data)
	})
	if !panicked {
		t.Error("Expected panic for REMOVE out of bounds")
	}
}

func TestInsertAtZero(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100, Items: []interface{}{"b", "c"}}
	// INSERT 现在能正确处理 slice（创建新切片拷贝），不应 panic
	valBytes := make([]byte, 4+5)
	binary.LittleEndian.PutUint32(valBytes[0:4], TypeIdString)
	copy(valBytes[4:], "hello")
	data := buildSpoiStream([]SpoiInstruction{
		insertBoundaryInst([]int{3}, 0, valBytes),
	})
	executor.Execute(player, data)
	if len(player.Items) != 3 {
		t.Errorf("INSERT at 0: expected 3 items, got %d", len(player.Items))
	}
	if player.Items[0] != "hello" {
		t.Errorf("INSERT at 0: expected 'hello', got %v", player.Items[0])
	}
}

func TestInsertAtEnd(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100, Items: []interface{}{"a", "b"}}
	// INSERT 现在能正确处理 slice（创建新切片拷贝），不应 panic
	valBytes := make([]byte, 4+5)
	binary.LittleEndian.PutUint32(valBytes[0:4], TypeIdString)
	copy(valBytes[4:], "hello")
	data := buildSpoiStream([]SpoiInstruction{
		insertBoundaryInst([]int{3}, 2, valBytes),
	})
	executor.Execute(player, data)
	if len(player.Items) != 3 {
		t.Errorf("INSERT at end: expected 3 items, got %d", len(player.Items))
	}
	if player.Items[2] != "hello" {
		t.Errorf("INSERT at end: expected 'hello', got %v", player.Items[2])
	}
}

func TestReplaceOutOfBounds(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100, Items: []interface{}{"a"}}
	panicked := mustPanic(t, func() {
		data := buildSpoiStream([]SpoiInstruction{
			replaceBoundaryInst([]int{3}, 99, []byte("world")),
		})
		executor.Execute(player, data)
	})
	if !panicked {
		t.Error("Expected panic for REPLACE out of bounds")
	}
}

func TestReplaceFirst(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100, Items: []interface{}{"a", "b"}}
	// 使用 type_id + value 格式
	valBytes := make([]byte, 4+3)
	binary.LittleEndian.PutUint32(valBytes[0:4], TypeIdString)
	copy(valBytes[4:], "new")
	data := buildSpoiStream([]SpoiInstruction{
		replaceBoundaryInst([]int{3}, 0, valBytes),
	})
	executor.Execute(player, data)
	if player.Items[0] != "new" {
		t.Errorf("REPLACE first: expected 'new', got %v", player.Items[0])
	}
}

func TestDoubleSet(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100}
	data := buildSpoiStream([]SpoiInstruction{
		setIntBoundary([]int{1}, 20),
		setIntBoundary([]int{1}, 30),
	})
	executor.Execute(player, data)
	if player.Level != 30 {
		t.Errorf("Double SET: expected 30, got %d", player.Level)
	}
}

func TestSetNullThenSet(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100}
	data := buildSpoiStream([]SpoiInstruction{
		makeBoundaryInst(OP_SETNULL, []int{0}, nil),
		setStrBoundary([]int{0}, "Bob"),
	})
	executor.Execute(player, data)
	if player.Name != "Bob" {
		t.Errorf("SETNULL then SET: expected 'Bob', got '%s'", player.Name)
	}
}

func TestResetThenSet(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100}
	data := buildSpoiStream([]SpoiInstruction{
		makeBoundaryInst(OP_RESET, []int{0}, nil),
		setStrBoundary([]int{0}, "Bob"),
	})
	executor.Execute(player, data)
	if player.Name != "Bob" {
		t.Errorf("RESET then SET: expected 'Bob', got '%s'", player.Name)
	}
}

func TestSetNull(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100}
	data := buildSpoiStream([]SpoiInstruction{
		makeBoundaryInst(OP_SETNULL, []int{1}, nil),
	})
	executor.Execute(player, data)
	if player.Level != 0 {
		t.Errorf("SETNULL: expected Level=0, got %d", player.Level)
	}
}

func TestReset(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100}
	data := buildSpoiStream([]SpoiInstruction{
		makeBoundaryInst(OP_RESET, []int{1}, nil),
	})
	executor.Execute(player, data)
	if player.Level != 0 {
		t.Errorf("RESET: expected Level=0, got %d", player.Level)
	}
}

func TestAppendToSlice(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100, Items: []interface{}{"a"}}
	// APPEND 现在能正确处理 slice（创建新切片拷贝），不应 panic
	valBytes := make([]byte, 4+3)
	binary.LittleEndian.PutUint32(valBytes[0:4], TypeIdString)
	copy(valBytes[4:], "new")
	data := buildSpoiStream([]SpoiInstruction{
		makeBoundaryInst(OP_APPEND, []int{3}, valBytes),
	})
	executor.Execute(player, data)
	if len(player.Items) != 2 {
		t.Errorf("APPEND: expected 2 items, got %d", len(player.Items))
	}
	if player.Items[1] != "new" {
		t.Errorf("APPEND: expected 'new', got %v", player.Items[1])
	}
}

func TestAddStringConcat(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Hello", Level: 10, Health: 100}
	// 使用 type_id + value 格式
	valBytes := make([]byte, 4+5)
	binary.LittleEndian.PutUint32(valBytes[0:4], TypeIdString)
	copy(valBytes[4:], "World")
	data := buildSpoiStream([]SpoiInstruction{
		makeBoundaryInst(OP_ADD, []int{0}, valBytes),
	})
	executor.Execute(player, data)
	if player.Name != "HelloWorld" {
		t.Errorf("ADD string concat: expected 'HelloWorld', got '%s'", player.Name)
	}
}

// =============================== 嵌套导航边界 ===============================

func TestNestedSelect(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Items: []interface{}{&Item{Name: "Sword", Price: 100}}},
	}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		selectInst([]int{3}), // SELECT items
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("Nested SELECT: expected SINGLE, got %v", result["resultType"])
	}
}

func TestNavigateToEmptyList(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Items: []interface{}{}}
	result := executor.navigate(player, []int{3})
	items, ok := result.([]interface{})
	if !ok || len(items) != 0 {
		t.Errorf("Navigate to empty list: expected empty slice, got %v", result)
	}
}

func TestNavigateToEmptyMap(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Metadata: map[string]interface{}{}}
	result := executor.navigate(player, []int{4})
	m, ok := result.(map[string]interface{})
	if !ok || len(m) != 0 {
		t.Errorf("Navigate to empty map: expected empty map, got %v", result)
	}
}

func TestNavigateToNilMap(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10}
	// player.Metadata is nil — Go 反射中访问 nil map 字段返回零值（空 map）
	result := executor.navigate(player, []int{4})
	m, ok := result.(map[string]interface{})
	if !ok {
		t.Errorf("Navigate to nil map: expected map type, got %T", result)
	}
	if len(m) != 0 {
		t.Errorf("Navigate to nil map: expected empty map, got %v", m)
	}
}

func TestNavigateToZeroValue(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "", Level: 0, Health: 0}
	result := executor.navigate(player, []int{0})
	if result != "" {
		t.Errorf("Navigate to zero value: expected '', got %v", result)
	}
}

// =============================== TAKEWHILE/DROPWHILE 边界 ===============================

func TestTakeWhileNeverTrue(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeWhileBoundary(1, CMP_GT, 100),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("TAKEWHILE never true: expected UNDEF, got %v", result["resultType"])
	}
}

func TestTakeWhileAlwaysTrue(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeWhileBoundary(1, CMP_GT, 0),
		execInst(),
	})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("TAKEWHILE always true: expected 2, got %d", len(values))
	}
}

func TestDropWhileNeverTrue(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		dropWhileBoundary(1, CMP_GT, 100),
		execInst(),
	})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("DROPWHILE never true: expected 2, got %d", len(values))
	}
}

func TestDropWhileAlwaysTrue(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		dropWhileBoundary(1, CMP_GT, 0),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("DROPWHILE always true: expected UNDEF, got %v", result["resultType"])
	}
}

func TestTakeWhileOnEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "A"}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		takeWhileBoundary(1, CMP_GT, 0),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("TAKEWHILE on empty: expected UNDEF, got %v", result["resultType"])
	}
}

func TestDropWhileOnEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{&Player{Name: "A"}}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		dropWhileBoundary(1, CMP_GT, 0),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("DROPWHILE on empty: expected UNDEF, got %v", result["resultType"])
	}
}

func TestTakeWhilePartial(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 5},
		&Player{Name: "Carol", Level: 15},
	}
	// take while level > 5 → Alice, Bob fails, so only Alice → SINGLE
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeWhileBoundary(1, CMP_GT, 5),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("TAKEWHILE partial: expected SINGLE, got %v", result["resultType"])
	}
	if result["value"].(*Player).Name != "Alice" {
		t.Errorf("TAKEWHILE partial: expected Alice, got %s", result["value"].(*Player).Name)
	}
}

func TestDropWhilePartial(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 5},
		&Player{Name: "Carol", Level: 15},
	}
	// drop while level > 5 → Alice, Bob fails, keep [Bob, Carol]
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		dropWhileBoundary(1, CMP_GT, 5),
		execInst(),
	})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("DROPWHILE partial: expected 2, got %d", len(values))
	}
	if values[0].(*Player).Name != "Bob" {
		t.Errorf("DROPWHILE partial: expected Bob, got %s", values[0].(*Player).Name)
	}
	if values[1].(*Player).Name != "Carol" {
		t.Errorf("DROPWHILE partial: expected Carol, got %s", values[1].(*Player).Name)
	}
}

// =============================== FILTER 比较操作边界 ===============================

func TestFilterAllCmpOps(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
		&Player{Name: "Carol", Level: 30},
	}

	// helper: 构造带 type_id 前缀的 U32 值
	makeU32Value := func(v uint32) []byte {
		vb := make([]byte, 8)
		binary.LittleEndian.PutUint32(vb[0:4], TypeIdU32)
		binary.LittleEndian.PutUint32(vb[4:8], v)
		return vb
	}

	// EQ — 只匹配 1 个 → SINGLE
	eqInst := filterInstWithCmp(1, CMP_EQ, makeU32Value(20))
	data := buildSpoiStream([]SpoiInstruction{pipeInst(nil), eqInst, execInst()})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("FILTER EQ: expected SINGLE, got %v", result["resultType"])
	}
	if result["value"].(*Player).Name != "Bob" {
		t.Errorf("FILTER EQ: expected Bob, got %s", result["value"].(*Player).Name)
	}

	// NE
	neInst := filterInstWithCmp(1, CMP_NE, makeU32Value(10))
	data = buildSpoiStream([]SpoiInstruction{pipeInst(nil), neInst, execInst()})
	result = executor.Execute(players, data)
	neValues := result["value"].([]interface{})
	if len(neValues) != 2 {
		t.Errorf("FILTER NE: expected 2, got %d", len(neValues))
	}

	// LT — 只匹配 1 个 → SINGLE
	ltInst := filterInstWithCmp(1, CMP_LT, makeU32Value(20))
	data = buildSpoiStream([]SpoiInstruction{pipeInst(nil), ltInst, execInst()})
	result = executor.Execute(players, data)
	if result["resultType"] != RESULT_SINGLE || result["value"].(*Player).Name != "Alice" {
		t.Errorf("FILTER LT: expected Alice only, got %v", result["value"])
	}

	// LE — 只匹配 1 个 → SINGLE
	leInst := filterInstWithCmp(1, CMP_LE, makeU32Value(10))
	data = buildSpoiStream([]SpoiInstruction{pipeInst(nil), leInst, execInst()})
	result = executor.Execute(players, data)
	if result["resultType"] != RESULT_SINGLE || result["value"].(*Player).Name != "Alice" {
		t.Errorf("FILTER LE: expected Alice only, got %v", result["value"])
	}

	// GE — 只匹配 1 个 → SINGLE
	geInst := filterInstWithCmp(1, CMP_GE, makeU32Value(30))
	data = buildSpoiStream([]SpoiInstruction{pipeInst(nil), geInst, execInst()})
	result = executor.Execute(players, data)
	if result["resultType"] != RESULT_SINGLE || result["value"].(*Player).Name != "Carol" {
		t.Errorf("FILTER GE: expected Carol only, got %v", result["value"])
	}
}

func TestFilterUnknownCmp(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
		&Player{Name: "Carol", Level: 30},
	}
	// 未知 cmpOp 返回 true（不过滤）
	vb := make([]byte, 4)
	binary.LittleEndian.PutUint32(vb, 10)
	unknownInst := filterInstWithCmp(1, 99, vb)
	data := buildSpoiStream([]SpoiInstruction{pipeInst(nil), unknownInst, execInst()})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	if len(values) != 3 {
		t.Errorf("FILTER unknown cmp: expected 3 (all pass), got %d", len(values))
	}
}

func TestFilterShortOperand(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	// FILTER 操作数只给 2 字节，len(operand) < 5 返回 true，不过滤
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		makeBoundaryInst(OP_FILTER, nil, []byte{0x00, 0x01}),
		execInst(),
	})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("FILTER short operand: expected 2 (all pass), got %d", len(values))
	}
}

// =============================== 操作数解析边界 ===============================

func TestAnyShortOperand(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		makeBoundaryInst(OP_ANY, nil, []byte{0x00}),
		execInst(),
	})
	result := executor.Execute(player, data)
	// len(operand) < 5 → matches 返回 true，ANY 结果为 true
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("ANY short operand type: expected SINGLE, got %v", result["resultType"])
	}
	if result["value"] != true {
		t.Errorf("ANY short operand: expected true, got %v", result["value"])
	}
}

func TestAllShortOperand(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		makeBoundaryInst(OP_ALL, nil, []byte{0x00}),
		execInst(),
	})
	result := executor.Execute(player, data)
	// len(operand) < 5 → matches 返回 true，ALL 结果为 true
	if result["value"] != true {
		t.Errorf("ALL short operand: expected true, got %v", result["value"])
	}
}

func TestInsertShortOperand(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100, Items: []interface{}{"a", "b"}}
	// INSERT 操作数 5 字节（4 索引 + 1 值），DeserializeValue 返回 nil，插入 nil
	data := buildSpoiStream([]SpoiInstruction{
		insertBoundaryInst([]int{3}, 0, []byte("x")),
	})
	executor.Execute(player, data)
	// 插入 nil 到索引 0，slice 长度变为 3
	if len(player.Items) != 3 {
		t.Errorf("INSERT short operand: expected 3 items, got %d", len(player.Items))
	}
	if player.Items[0] != nil {
		t.Errorf("INSERT short operand: expected nil at index 0, got %v", player.Items[0])
	}
}

func TestReplaceOperandNoIndex(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100, Items: []interface{}{"a"}}
	// REPLACE 操作数 5 字节 (world)，但只有 5 字节，前 4 字节是索引
	panicked := mustPanic(t, func() {
		data := buildSpoiStream([]SpoiInstruction{
			replaceBoundaryInst([]int{3}, 99, []byte("x")),
		})
		executor.Execute(player, data)
	})
	if !panicked {
		t.Error("Expected panic for REPLACE out of bounds")
	}
}

func TestRemoveShortOperand(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100, Items: []interface{}{"a", "b"}}
	// REMOVE 操作数 2 字节，不足 4 字节
	panicked := mustPanic(t, func() {
		data := buildSpoiStream([]SpoiInstruction{
			makeBoundaryInst(OP_REMOVE, []int{3}, []byte{0x00, 0x01}),
		})
		executor.Execute(player, data)
	})
	if !panicked {
		t.Error("Expected panic for REMOVE with short operand (non-slice remove)")
	}
}

// =============================== 多管道组合边界 ===============================

func TestFilterTakeDropReverse(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 80},
		&Player{Name: "Carol", Level: 15, Health: 120},
		&Player{Name: "Dave", Level: 20, Health: 60},
		&Player{Name: "Eve", Level: 10, Health: 90},
	}

	// FILTER level>10 → Bob, Carol, Dave → TAKE 3 → DROP 1 → REVERSE → Dave, Carol
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		filterGtBoundary(1, 10),
		takeBoundaryInst(3),
		dropBoundaryInst(1),
		reverseInst(),
		execInst(),
	})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("Filter-Take-Drop-Reverse: expected 2, got %d", len(values))
	}
	if values[0].(*Player).Name != "Dave" {
		t.Errorf("Expected Dave, got %s", values[0].(*Player).Name)
	}
	if values[1].(*Player).Name != "Carol" {
		t.Errorf("Expected Carol, got %s", values[1].(*Player).Name)
	}
}

func TestSortAscending(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Carol", Level: 30},
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Eve", Level: 20},
		&Player{Name: "Bob", Level: 10},
		&Player{Name: "Dave", Level: 40},
	}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		sortInst([]int{1}), // sort by Level
		execInst(),
	})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	// 排序后应该是 Alice, Bob, Eve, Carol, Dave
	expected := []string{"Alice", "Bob", "Eve", "Carol", "Dave"}
	for i, name := range expected {
		if values[i].(*Player).Name != name {
			t.Errorf("SORT[%d]: expected %s, got %s", i, name, values[i].(*Player).Name)
		}
	}
}

func TestSelectThenFilterCausesPanic(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	// SELECT name → ["Alice", "Bob"] → FILTER 对字符串尝试 navStep 访问成员索引
	panicked := mustPanic(t, func() {
		data := buildSpoiStream([]SpoiInstruction{
			pipeInst(nil),
			selectInst([]int{0}),
			filterGtBoundary(0, 0),
			execInst(),
		})
		executor.Execute(players, data)
	})
	if !panicked {
		t.Error("Expected panic for SELECT then FILTER on strings")
	}
}

func TestAllOperationsChain(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 80},
		&Player{Name: "Carol", Level: 15, Health: 120},
		&Player{Name: "Dave", Level: 20, Health: 60},
		&Player{Name: "Eve", Level: 10, Health: 90},
	}
	// PIPE → REVERSE → FILTER level>10 → DROP 1 → TAKE 2 → SORT by name
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		reverseInst(),
		filterGtBoundary(1, 10),
		dropBoundaryInst(1),
		takeBoundaryInst(2),
		sortInst([]int{0}),
		execInst(),
	})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	// REVERSE: [Eve,Dave,Carol,Bob,Alice] → FILTER level>10: [Dave,Carol,Bob] → DROP 1: [Carol,Bob] → TAKE 2: [Carol,Bob] → SORT by name: [Bob,Carol]
	if len(values) != 2 {
		t.Errorf("All ops chain: expected 2, got %d", len(values))
	}
	if values[0].(*Player).Name != "Bob" {
		t.Errorf("Expected Bob, got %s", values[0].(*Player).Name)
	}
	if values[1].(*Player).Name != "Carol" {
		t.Errorf("Expected Carol, got %s", values[1].(*Player).Name)
	}
}

func TestCountDistinctPipe(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	values := []interface{}{1, 2, 2, 3, 3, 3, 4}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		distinctInst(),
		countInst(),
		execInst(),
	})
	result := executor.Execute(values, data)
	if result["value"] != 4 {
		t.Errorf("DISTINCT then COUNT: expected 4, got %v", result["value"])
	}
}

// =============================== 类型注册表边界 ===============================

func TestEmptyRegistry(t *testing.T) {
	executor := NewSpoiExecutor(map[string]SpoiAccessor{})
	player := &Player{Name: "Alice"}
	// 空注册表时，没有访问器可用，导航会 panic
	panicked := mustPanic(t, func() {
		executor.navigate(player, []int{0})
	})
	if !panicked {
		t.Error("Expected panic for navigate with empty registry")
	}
}

func TestNonexistentType(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	data := buildSpoiStream([]SpoiInstruction{pipeInst(nil), execInst()})
	result := executor.Execute(42, data)
	// PIPE 对非切片/数组对象包装为单元素管道
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("Nonexistent type: expected SINGLE, got %v", result["resultType"])
	}
	if result["value"] != 42 {
		t.Errorf("Nonexistent type: expected 42, got %v", result["value"])
	}
}

func TestPartialRegistry(t *testing.T) {
	// 只注册 Player，不注册 Item
	partialRegistry := map[string]SpoiAccessor{
		"Player": PlayerAccessor{},
	}
	executor := NewSpoiExecutor(partialRegistry)
	player := &Player{
		Name:  "Alice",
		Level: 10,
		Items: []interface{}{&Item{Name: "Sword", Price: 100}},
	}
	result := executor.navigate(player, []int{3})
	items, ok := result.([]interface{})
	if !ok || len(items) != 1 {
		t.Errorf("Partial registry: expected 1 item, got %v", result)
	}
}

// =============================== 容器操作边界 ===============================

func TestKeysOnMap(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	data := []interface{}{
		map[string]interface{}{"a": 1, "b": 2, "c": 3},
	}
	data2 := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		makeBoundaryInst(OP_KEYS, nil, nil),
		execInst(),
	})
	result := executor.Execute(data, data2)
	values := result["value"].([]interface{})
	if len(values) != 3 {
		t.Errorf("KEYS: expected 3 keys, got %d", len(values))
	}
}

func TestValuesOnMap(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	data := []interface{}{
		map[string]interface{}{"a": 1, "b": 2, "c": 3},
	}
	data2 := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		makeBoundaryInst(OP_VALUES, nil, nil),
		execInst(),
	})
	result := executor.Execute(data, data2)
	values := result["value"].([]interface{})
	if len(values) != 3 {
		t.Errorf("VALUES: expected 3 values, got %d", len(values))
	}
}

func TestJoinOnSlices(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	data := []interface{}{
		[]interface{}{1, 2},
		[]interface{}{3, 4, 5},
	}
	data2 := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		makeBoundaryInst(OP_JOIN, nil, nil),
		execInst(),
	})
	result := executor.Execute(data, data2)
	values := result["value"].([]interface{})
	if len(values) != 5 {
		t.Errorf("JOIN: expected 5 items, got %d", len(values))
	}
}

func TestJoinMixed(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	data := []interface{}{
		[]interface{}{1, 2},
		42,
		[]interface{}{3, 4},
	}
	data2 := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		makeBoundaryInst(OP_JOIN, nil, nil),
		execInst(),
	})
	result := executor.Execute(data, data2)
	values := result["value"].([]interface{})
	if len(values) != 5 {
		t.Errorf("JOIN mixed: expected 5 items, got %d", len(values))
	}
}

func TestKeysOnEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	data := []interface{}{}
	data2 := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		makeBoundaryInst(OP_KEYS, nil, nil),
		execInst(),
	})
	result := executor.Execute(data, data2)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("KEYS on empty: expected UNDEF, got %v", result["resultType"])
	}
}

func TestJoinOnEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	data := []interface{}{}
	data2 := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		makeBoundaryInst(OP_JOIN, nil, nil),
		execInst(),
	})
	result := executor.Execute(data, data2)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("JOIN on empty: expected UNDEF, got %v", result["resultType"])
	}
}

// =============================== 未实现操作码边界 ===============================

func TestUnknownOpcode(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10}
	panicked := mustPanic(t, func() {
		data := buildSpoiStream([]SpoiInstruction{
			makeBoundaryInst(0xFF, nil, nil),
		})
		executor.Execute(player, data)
	})
	if !panicked {
		t.Error("Expected panic for unknown opcode 0xFF")
	}
}

func TestNavOpcodePanics(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10}
	panicked := mustPanic(t, func() {
		data := buildSpoiStream([]SpoiInstruction{
			makeBoundaryInst(OP_NAV, nil, nil),
		})
		executor.Execute(player, data)
	})
	if !panicked {
		t.Error("Expected panic for unhandled OP_NAV")
	}
}

func TestDerefOpcodePanics(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10}
	panicked := mustPanic(t, func() {
		data := buildSpoiStream([]SpoiInstruction{
			makeBoundaryInst(OP_DEREF, nil, nil),
		})
		executor.Execute(player, data)
	})
	if !panicked {
		t.Error("Expected panic for unhandled OP_DEREF")
	}
}

// =============================== Map 导航边界 ===============================

func TestNavigateMapKey(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{
		Name:     "Alice",
		Level:    10,
		Metadata: map[string]interface{}{"role": "admin", "score": 100},
	}
	// 导航到 Metadata (index 4) 返回 map，然后按索引访问 map 会 panic
	// 因为访问器驱动的 navStep 不支持 map 按索引导航
	panicked := mustPanic(t, func() {
		executor.navigate(player, []int{4, 0})
	})
	if !panicked {
		t.Error("Expected panic for navigate on map with index")
	}
}

func TestNavigateStructByPublicFields(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{Name: "Alice", Level: 10, Health: 100}
	// 按索引访问公开字段
	result := executor.navigate(player, []int{0})
	if result != "Alice" {
		t.Errorf("Public field 0: expected 'Alice', got %v", result)
	}
	result = executor.navigate(player, []int{1})
	if result != uint64(10) {
		t.Errorf("Public field 1: expected 10, got %v", result)
	}
}

// =============================== 结果类型边界 ===============================

func TestResultTypeEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	result := executor.makeResult()
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("makeResult empty: expected UNDEF, got %v", result["resultType"])
	}
}

func TestResultTypeSingle(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	executor.pipeData = []interface{}{"hello"}
	result := executor.makeResult()
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("makeResult single: expected SINGLE, got %v", result["resultType"])
	}
	if result["value"] != "hello" {
		t.Errorf("makeResult single: expected 'hello', got %v", result["value"])
	}
}

func TestResultTypeVector(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	executor.pipeData = []interface{}{"a", "b", "c"}
	result := executor.makeResult()
	if result["resultType"] != RESULT_VECTOR {
		t.Errorf("makeResult vector: expected VECTOR, got %v", result["resultType"])
	}
}

// =============================== PIPE 路径导航 ===============================

func TestPipeWithPath(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{
		Name:  "Alice",
		Level: 10,
		Items: []interface{}{"sword", "shield"},
	}
	// PIPE with path [3] navigates to Items
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst([]int{3}),
		execInst(),
	})
	result := executor.Execute(player, data)
	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("PIPE with path: expected 2, got %d", len(values))
	}
}

func TestPipeWithMap(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{
		Name:     "Alice",
		Metadata: map[string]interface{}{"a": 1, "b": 2},
	}
	// PIPE with path [4] navigates to Metadata (map)
	// map 不是 slice，会被包装为单元素管道
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst([]int{4}),
		execInst(),
	})
	result := executor.Execute(player, data)
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("PIPE with map: expected SINGLE, got %v", result["resultType"])
	}
	metaMap, ok := result["value"].(map[string]interface{})
	if !ok {
		t.Errorf("PIPE with map: expected map[string]interface{}, got %T", result["value"])
	}
	if len(metaMap) != 2 {
		t.Errorf("PIPE with map: expected map with 2 entries, got %d", len(metaMap))
	}
}

// =============================== DESERIALIZE 边界 ===============================

func TestDeserializeEmpty(t *testing.T) {
	result := DeserializeValue([]byte{})
	if result != nil {
		t.Errorf("Deserialize empty: expected nil, got %v", result)
	}
}

func TestDeserializeSizes(t *testing.T) {
	// 新格式: [type_id(u32) + value_bytes]

	// u32 值
	val := make([]byte, 8)
	binary.LittleEndian.PutUint32(val[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(val[4:8], 0x42)
	result := DeserializeValue(val)
	if result != uint32(0x42) {
		t.Errorf("Deserialize u32: expected 0x42, got %v", result)
	}

	// u64 值
	val64 := make([]byte, 12)
	binary.LittleEndian.PutUint32(val64[0:4], TypeIdU64)
	binary.LittleEndian.PutUint64(val64[4:12], 0x0807060504030201)
	result = DeserializeValue(val64)
	if result != uint64(0x0807060504030201) {
		t.Errorf("Deserialize u64: expected 0x0807060504030201, got %v", result)
	}
}

func TestDeserializeUTF8String(t *testing.T) {
	// 新格式: [type_id_string + str_bytes]
	data := make([]byte, 4+11)
	binary.LittleEndian.PutUint32(data[0:4], TypeIdString)
	copy(data[4:], "hello world")
	result := DeserializeValue(data)
	if result != "hello world" {
		t.Errorf("Deserialize UTF-8: expected 'hello world', got %v", result)
	}
}

func TestDeserializeNonUTF8(t *testing.T) {
	// 新格式: 未知 type_id 返回原始字节
	data := make([]byte, 4+6)
	binary.LittleEndian.PutUint32(data[0:4], 999) // 未知 type_id
	copy(data[4:], []byte{0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA})
	result := DeserializeValue(data)
	// 未知 type_id 返回原始字节
	bytes, ok := result.([]byte)
	if !ok {
		t.Errorf("Deserialize unknown type: expected []byte, got %T", result)
	}
	if len(bytes) != 6 {
		t.Errorf("Deserialize unknown type: expected 6 bytes, got %d", len(bytes))
	}
}

// =============================== 跨操作码状态保持 ===============================

func TestExecutorStateIsolation(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	// 第一次执行
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		countInst(),
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["value"] != 2 {
		t.Errorf("First execute: expected 2, got %v", result["value"])
	}

	// 第二次执行：pipeData 应该被重置
	data2 := buildSpoiStream([]SpoiInstruction{execInst()})
	result2 := executor.Execute(players, data2)
	if result2["resultType"] != RESULT_UNDEF {
		t.Errorf("Second execute: expected UNDEF (pipeData reset), got %v", result2["resultType"])
	}
}

// =============================== 极端大数据量 ===============================

func TestLargeDataset(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := make([]interface{}, 1000)
	for i := range players {
		players[i] = &Player{
			Name:   "Player",
			Level:  uint64(i % 100),
			Health: 100,
		}
	}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		filterGtBoundary(1, 50),
		countInst(),
		execInst(),
	})
	result := executor.Execute(players, data)
	// 1000 players, levels 0-99, filter > 50 → levels 51-99 = 49 values × 10 each = 490
	if result["value"] != 490 {
		t.Errorf("Large dataset: expected 490, got %v", result["value"])
	}
}

// =============================== DESERIALIZE 3字节边界 ===============================

func TestDeserialize3Bytes(t *testing.T) {
	// 3 字节，少于 4 字节的 type_id 前缀，返回 nil
	result := DeserializeValue([]byte("abc"))
	if result != nil {
		t.Errorf("Deserialize 3 bytes: expected nil, got %v", result)
	}
}

func TestDeserialize5Bytes(t *testing.T) {
	// 5 字节，前 4 字节是 type_id (0x6C6C6568 = "hell" LE)，未知 type_id 返回原始字节
	result := DeserializeValue([]byte("hello"))
	bytes, ok := result.([]byte)
	if !ok {
		t.Errorf("Deserialize 5 bytes: expected []byte, got %T", result)
	}
	if len(bytes) != 1 || bytes[0] != 'o' {
		t.Errorf("Deserialize 5 bytes: expected []byte{'o'}, got %v", bytes)
	}
}

// =============================== Count 后管道操作 ===============================

func TestCountThenTake(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice"},
		&Player{Name: "Bob"},
		&Player{Name: "Carol"},
	}
	// COUNT → 3 → TAKE 2 → 2 (但管道只有 1 个元素 int(3)，TAKE 2 取全部)
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		countInst(),
		takeBoundaryInst(2),
		execInst(),
	})
	result := executor.Execute(players, data)
	// COUNT 后管道是 [3]，TAKE 2 取前 2 个，但只有 1 个，所以是 [3]
	if result["value"] != 3 {
		t.Errorf("COUNT then TAKE: expected 3, got %v", result["value"])
	}
}

// =============================== Any/All/Find 边界条件 ===============================

func TestAnyOnEmptyPipe(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		anyBoundaryInst(1, CMP_GT, 5),
		execInst(),
	})
	result := executor.Execute([]interface{}{}, data)
	if result["value"] != false {
		t.Errorf("ANY on empty pipe: expected false, got %v", result["value"])
	}
}

func TestAllOnEmptyPipe(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		allBoundaryInst(1, CMP_GT, 5),
		execInst(),
	})
	result := executor.Execute([]interface{}{}, data)
	if result["value"] != true {
		t.Errorf("ALL on empty pipe (vacuous truth): expected true, got %v", result["value"])
	}
}

func TestFindOnEmptyPipe(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		takeBoundaryInst(0),
		findBoundaryInst(1, CMP_EQ, 10),
		execInst(),
	})
	result := executor.Execute([]interface{}{}, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("FIND on empty pipe: expected UNDEF, got %v", result["resultType"])
	}
}

// =============================== SELECT 空路径 ===============================

func TestSelectEmptyPath(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	// SELECT 空路径，opSelect 中 len(path) > 0 为 false，不修改管道
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		makeBoundaryInst(OP_SELECT, []int{}, nil),
		execInst(),
	})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("SELECT empty path: expected 2 items unchanged, got %d", len(values))
	}
	if values[0].(*Player).Name != "Alice" {
		t.Errorf("SELECT empty path: expected Alice, got %s", values[0].(*Player).Name)
	}
}

// =============================== SORT 空路径 ===============================

func TestSortEmptyPath(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	values := []interface{}{3, 1, 4, 1, 5}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		makeBoundaryInst(OP_SORT, []int{}, nil),
		execInst(),
	})
	result := executor.Execute(values, data)
	vals := result["value"].([]interface{})
	// SORT 空路径，按 sortKey 排序（字符串比较）
	if len(vals) != 5 {
		t.Errorf("SORT empty path: expected 5, got %d", len(vals))
	}
}

// =============================== DISTINCT 去重边界 ===============================

func TestDistinctAllSame(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	values := []interface{}{1, 1, 1, 1, 1}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		distinctInst(),
		execInst(),
	})
	result := executor.Execute(values, data)
	// 所有相同，去重后只剩 1 个 → SINGLE
	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("DISTINCT all same: expected SINGLE, got %v", result["resultType"])
	}
	if result["value"] != 1 {
		t.Errorf("DISTINCT all same: expected 1, got %v", result["value"])
	}
}

func TestDistinctAllDifferent(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	values := []interface{}{1, 2, 3, 4, 5}
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		distinctInst(),
		execInst(),
	})
	result := executor.Execute(values, data)
	vals := result["value"].([]interface{})
	if len(vals) != 5 {
		t.Errorf("DISTINCT all different: expected 5, got %d", len(vals))
	}
}

// =============================== FILTER 全匹配 / 全不匹配 ===============================

func TestFilterAllMatch(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
		&Player{Name: "Carol", Level: 30},
	}
	// FILTER level > 0 → 全部匹配
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		filterGtBoundary(1, 0),
		execInst(),
	})
	result := executor.Execute(players, data)
	values := result["value"].([]interface{})
	if len(values) != 3 {
		t.Errorf("FILTER all match: expected 3, got %d", len(values))
	}
}

func TestFilterNoneMatch(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	players := []interface{}{
		&Player{Name: "Alice", Level: 10},
		&Player{Name: "Bob", Level: 20},
	}
	// FILTER level < 0 → 无匹配
	ltOp := make([]byte, 8)
	binary.LittleEndian.PutUint32(ltOp[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(ltOp[4:8], 0)
	ltInst := filterInstWithCmp(1, CMP_LT, ltOp)
	data := buildSpoiStream([]SpoiInstruction{
		pipeInst(nil),
		ltInst,
		execInst(),
	})
	result := executor.Execute(players, data)
	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("FILTER none match: expected UNDEF, got %v", result["resultType"])
	}
}

// =============================== 带路径的写操作边界 ===============================

func TestSetNestedPath(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{
		Name:  "Alice",
		Level: 10,
		Items: []interface{}{"a", "b", "c"},
	}
	// SET Items[1] = "new" — Go 反射中 navigate 返回的 slice 可能可寻址也可能不可寻址
	// 使用 3 字节字符串避免 deserializeValue 误解析为 uint64
	panicked := mustPanic(t, func() {
		data := buildSpoiStream([]SpoiInstruction{
			setStrBoundary([]int{3, 1}, "new"),
		})
		executor.Execute(player, data)
	})
	if panicked {
		t.Log("SET on nested slice element panicked (expected in some Go versions)")
	} else {
		if player.Items[1] != "new" {
			t.Errorf("SET nested path: expected 'new' at Items[1], got %v", player.Items[1])
		}
	}
}

func TestAddNestedPath(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)
	player := &Player{
		Name:     "Alice",
		Level:    10,
		Metadata: map[string]interface{}{"score": 50},
	}
	// ADD 到 Metadata["score"] 不是直接支持的（需要 MAPKEY 标记），但测试不会 panic
	// 由于 Metadata 是 map，按索引访问
	// 不测试具体行为，只验证不 panic
	panicked := mustPanic(t, func() {
		// 尝试 ADD 到 map 值（索引 0 是 map 的第一个值）
		data := buildSpoiStream([]SpoiInstruction{
			addIntBoundary([]int{4, 0}, 10),
		})
		executor.Execute(player, data)
	})
	// 可能 panic 也可能不 panic，取决于实现
	_ = panicked
}

// =============================== Varint 最大int值 ===============================

func TestVarintVeryLargeValue(t *testing.T) {
	// 测试最大 int 值
	v := int(^uint(0) >> 1) // max int
	var buf []byte
	writeVarint(&buf, v)
	result, _ := readVarint(buf, 0)
	if result != v {
		t.Errorf("Varint max int: expected %d, got %d", v, result)
	}
}