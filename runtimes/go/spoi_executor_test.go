// SPOI Executor 测试
// 运行: go test -v -run "TestVarint|TestParse|TestNavigate|TestSet|TestAdd|TestPipe|TestFilter|TestSelect|TestSort|TestTake|TestDrop|TestReverse|TestDistinct|TestCount|TestAny|TestAll|TestFind|TestPipeline"

package main

import (
	"encoding/binary"
	"testing"
)

// =============================== 测试数据类型 ===============================

type Item struct {
	Name  string
	Price uint64
}

type Player struct {
	Name     string
	Level    uint64
	Health   uint64
	Items    []interface{}
	Metadata map[string]interface{}
}

// =============================== 访问器（手动定义，对应 Player 和 Item） ===============================

type PlayerAccessor struct{}
func (a PlayerAccessor) FieldCount() int { return 5 }
func (a PlayerAccessor) GetField(obj any, idx int) any {
	o := obj.(*Player)
	switch idx {
	case 0: return o.Name
	case 1: return o.Level
	case 2: return o.Health
	case 3: return o.Items
	case 4: return o.Metadata
	default: panic("invalid field index for Player")
	}
}
func (a PlayerAccessor) SetField(obj any, idx int, val any) {
	o := obj.(*Player)
	switch idx {
	case 0:
		if val == nil {
			o.Name = ""
		} else {
			o.Name = val.(string)
		}
	case 1:
		if val == nil {
			o.Level = 0
		} else {
			o.Level = val.(uint64)
		}
	case 2:
		if val == nil {
			o.Health = 0
		} else {
			o.Health = val.(uint64)
		}
	case 3:
		if val == nil {
			o.Items = nil
		} else {
			o.Items = val.([]interface{})
		}
	case 4:
		if val == nil {
			o.Metadata = nil
		} else {
			o.Metadata = val.(map[string]interface{})
		}
	default: panic("invalid field index for Player")
	}
}

type ItemAccessor struct{}
func (a ItemAccessor) FieldCount() int { return 2 }
func (a ItemAccessor) GetField(obj any, idx int) any {
	o := obj.(*Item)
	switch idx {
	case 0: return o.Name
	case 1: return o.Price
	default: panic("invalid field index for Item")
	}
}
func (a ItemAccessor) SetField(obj any, idx int, val any) {
	o := obj.(*Item)
	switch idx {
	case 0: o.Name = val.(string)
	case 1: o.Price = val.(uint64)
	default: panic("invalid field index for Item")
	}
}

// 访问器注册表（用于 executor 测试，与 spoi_accessor.go 中的 ExecutorTestRegistry 分开）
var ExecutorTestRegistry = map[string]SpoiAccessor{
	"Player": PlayerAccessor{},
	"Item":   ItemAccessor{},
}

// =============================== 辅助函数 ===============================

func buildSpoiStream(instructions []SpoiInstruction) []byte {
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

func makeInst(op byte, path []int, operand []byte) SpoiInstruction {
	if path == nil {
		path = []int{}
	}
	if operand == nil {
		operand = []byte{}
	}
	return SpoiInstruction{Op: op, Path: path, Operand: operand}
}

func setInt(path []int, value uint64) SpoiInstruction {
	// 新格式: [type_id(u32) + value_bytes]
	operand := make([]byte, 12)
	binary.LittleEndian.PutUint32(operand[0:4], TypeIdU64)
	binary.LittleEndian.PutUint64(operand[4:12], value)
	return makeInst(OP_SET, path, operand)
}

func setStr(path []int, value string) SpoiInstruction {
	// 新格式: [type_id(u32) + str_bytes]
	operand := make([]byte, 4+len(value))
	binary.LittleEndian.PutUint32(operand[0:4], TypeIdString)
	copy(operand[4:], value)
	return makeInst(OP_SET, path, operand)
}

func setInt32(path []int, value uint32) SpoiInstruction {
	// 新格式: [type_id(u32) + value_bytes]
	operand := make([]byte, 8)
	binary.LittleEndian.PutUint32(operand[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(operand[4:8], value)
	return makeInst(OP_SET, path, operand)
}

func pipeInst(path []int) SpoiInstruction {
	return makeInst(OP_PIPE, path, nil)
}

func filterGt(path []int, memberIdx uint32, value uint32) SpoiInstruction {
	// 新格式: memberIdx(u32) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
	valuePart := make([]byte, 8)
	binary.LittleEndian.PutUint32(valuePart[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(valuePart[4:8], value)
	operand := make([]byte, 5+1+len(valuePart)) // memberIdx(4) + cmpOp(1) + varint_len(1) + value
	binary.LittleEndian.PutUint32(operand[0:4], memberIdx)
	operand[4] = CMP_GT
	operand[5] = byte(len(valuePart)) // varint 编码（值 < 128 时占 1 字节）
	copy(operand[6:], valuePart)
	return makeInst(OP_FILTER, path, operand)
}

func filterEq(path []int, memberIdx uint32, value uint32) SpoiInstruction {
	valuePart := make([]byte, 8)
	binary.LittleEndian.PutUint32(valuePart[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(valuePart[4:8], value)
	operand := make([]byte, 5+1+len(valuePart))
	binary.LittleEndian.PutUint32(operand[0:4], memberIdx)
	operand[4] = CMP_EQ
	operand[5] = byte(len(valuePart))
	copy(operand[6:], valuePart)
	return makeInst(OP_FILTER, path, operand)
}

func selectInst(path []int) SpoiInstruction {
	return makeInst(OP_SELECT, path, nil)
}

func sortInst(path []int) SpoiInstruction {
	return makeInst(OP_SORT, path, nil)
}

func takeInst(n uint32) SpoiInstruction {
	operand := make([]byte, 4)
	binary.LittleEndian.PutUint32(operand, n)
	return makeInst(OP_TAKE, nil, operand)
}

func dropInst(n uint32) SpoiInstruction {
	operand := make([]byte, 4)
	binary.LittleEndian.PutUint32(operand, n)
	return makeInst(OP_DROP, nil, operand)
}

func reverseInst() SpoiInstruction {
	return makeInst(OP_REVERSE, nil, nil)
}

func distinctInst() SpoiInstruction {
	return makeInst(OP_DISTINCT, nil, nil)
}

func countInst() SpoiInstruction {
	return makeInst(OP_COUNT, nil, nil)
}

func anyInst(memberIdx uint32, cmpOp byte, value uint32) SpoiInstruction {
	// 新格式: memberIdx(u32) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
	valuePart := make([]byte, 8)
	binary.LittleEndian.PutUint32(valuePart[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(valuePart[4:8], value)
	operand := make([]byte, 5+1+len(valuePart))
	binary.LittleEndian.PutUint32(operand[0:4], memberIdx)
	operand[4] = cmpOp
	operand[5] = byte(len(valuePart))
	copy(operand[6:], valuePart)
	return makeInst(OP_ANY, nil, operand)
}

func allInst(memberIdx uint32, cmpOp byte, value uint32) SpoiInstruction {
	// 新格式: memberIdx(u32) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
	valuePart := make([]byte, 8)
	binary.LittleEndian.PutUint32(valuePart[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(valuePart[4:8], value)
	operand := make([]byte, 5+1+len(valuePart))
	binary.LittleEndian.PutUint32(operand[0:4], memberIdx)
	operand[4] = cmpOp
	operand[5] = byte(len(valuePart))
	copy(operand[6:], valuePart)
	return makeInst(OP_ALL, nil, operand)
}

func findInst(memberIdx uint32, cmpOp byte, value uint32) SpoiInstruction {
	// 新格式: memberIdx(u32) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
	valuePart := make([]byte, 8)
	binary.LittleEndian.PutUint32(valuePart[0:4], TypeIdU32)
	binary.LittleEndian.PutUint32(valuePart[4:8], value)
	operand := make([]byte, 5+1+len(valuePart))
	binary.LittleEndian.PutUint32(operand[0:4], memberIdx)
	operand[4] = cmpOp
	operand[5] = byte(len(valuePart))
	copy(operand[6:], valuePart)
	return makeInst(OP_FIND, nil, operand)
}

func execInst() SpoiInstruction {
	return makeInst(OP_EXEC, nil, nil)
}

// =============================== 测试：Varint 编解码 ===============================

func TestVarint(t *testing.T) {
	values := []int{0, 1, 127, 128, 255, 256, 300, 16383, 16384, 100000, 0x7FFFFFFF}
	for _, v := range values {
		var buf []byte
		writeVarint(&buf, v)
		decoded, off := readVarint(buf, 0)
		if decoded != v {
			t.Errorf("Varint roundtrip: expected %d, got %d", v, decoded)
		}
		if off != len(buf) {
			t.Errorf("Varint offset: expected %d, got %d (value=%d)", len(buf), off, v)
		}
	}
}

// =============================== 测试：SPOI 指令流解析 ===============================

func TestParseSpoiStream(t *testing.T) {
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		countInst(),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	parsed := parseSpoiStream(data)

	if len(parsed) != 3 {
		t.Errorf("Expected 3 instructions, got %d", len(parsed))
	}
	if parsed[0].Op != OP_PIPE {
		t.Errorf("Instruction 0: expected OP_PIPE (0x%02X), got 0x%02X", OP_PIPE, parsed[0].Op)
	}
	if parsed[1].Op != OP_COUNT {
		t.Errorf("Instruction 1: expected OP_COUNT (0x%02X), got 0x%02X", OP_COUNT, parsed[1].Op)
	}
	if parsed[2].Op != OP_EXEC {
		t.Errorf("Instruction 2: expected OP_EXEC (0x%02X), got 0x%02X", OP_EXEC, parsed[2].Op)
	}
}

func TestParseSpoiStreamWithPath(t *testing.T) {
	instructions := []SpoiInstruction{
		pipeInst([]int{0, 1, 2}),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	parsed := parseSpoiStream(data)

	if len(parsed) != 2 {
		t.Errorf("Expected 2 instructions, got %d", len(parsed))
	}
	if len(parsed[0].Path) != 3 {
		t.Errorf("Expected path length 3, got %d", len(parsed[0].Path))
	}
	for i, v := range []int{0, 1, 2} {
		if parsed[0].Path[i] != v {
			t.Errorf("Path[%d]: expected %d, got %d", i, v, parsed[0].Path[i])
		}
	}
}

// =============================== 测试：基础导航 ===============================

func TestNavigate(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	player := &Player{
		Name:   "Alice",
		Level:  10,
		Health: 100,
		Items:  []interface{}{},
	}

	// PIPE 加载 player，SELECT 导航到 Level (index 1)，EXEC
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		selectInst([]int{1}),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(player, data)

	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("Expected RESULT_SINGLE, got %v", result["resultType"])
	}
	if result["value"] != uint64(10) {
		t.Errorf("Expected Level=10, got %v", result["value"])
	}
}

func TestNavigateName(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	player := &Player{Name: "Bob", Level: 5, Health: 80}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		selectInst([]int{0}), // Name
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(player, data)

	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("Expected RESULT_SINGLE, got %v", result["resultType"])
	}
	if result["value"] != "Bob" {
		t.Errorf("Expected Name=Bob, got %v", result["value"])
	}
}

// =============================== 测试：SET 操作 ===============================

func TestSpoiSet(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	player := &Player{Name: "Alice", Level: 1, Health: 50}

	// SET Level (index 1) = 100
	instructions := []SpoiInstruction{
		setInt([]int{1}, 100),
	}
	data := buildSpoiStream(instructions)
	executor.Execute(player, data)

	if player.Level != 100 {
		t.Errorf("Expected Level=100, got %d", player.Level)
	}
}

func TestSetName(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	player := &Player{Name: "OldName", Level: 1, Health: 50}

	instructions := []SpoiInstruction{
		setStr([]int{0}, "NewName"),
	}
	data := buildSpoiStream(instructions)
	executor.Execute(player, data)

	if player.Name != "NewName" {
		t.Errorf("Expected Name=NewName, got %s", player.Name)
	}
}

func TestSetMultipleFields(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	player := &Player{Name: "X", Level: 1, Health: 10}

	instructions := []SpoiInstruction{
		setStr([]int{0}, "Warrior"),
		setInt([]int{1}, 99),
		setInt([]int{2}, 999),
	}
	data := buildSpoiStream(instructions)
	executor.Execute(player, data)

	if player.Name != "Warrior" {
		t.Errorf("Expected Name=Warrior, got %s", player.Name)
	}
	if player.Level != 99 {
		t.Errorf("Expected Level=99, got %d", player.Level)
	}
	if player.Health != 999 {
		t.Errorf("Expected Health=999, got %d", player.Health)
	}
}

// =============================== 测试：ADD 操作 ===============================

func TestAdd(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	player := &Player{Name: "Alice", Level: 10, Health: 100}

	// ADD 50 to Level (index 1) — 使用 type_id + value 格式
	operand := make([]byte, 12)
	binary.LittleEndian.PutUint32(operand[0:4], TypeIdU64)
	binary.LittleEndian.PutUint64(operand[4:12], 50)
	instructions := []SpoiInstruction{
		makeInst(OP_ADD, []int{1}, operand),
	}
	data := buildSpoiStream(instructions)
	executor.Execute(player, data)

	if player.Level != 60 {
		t.Errorf("Expected Level=60, got %d", player.Level)
	}
}

func TestAddHealth(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	player := &Player{Name: "Bob", Level: 5, Health: 30}

	// ADD 20 to Health (index 2) — 使用 type_id + value 格式
	operand := make([]byte, 12)
	binary.LittleEndian.PutUint32(operand[0:4], TypeIdU64)
	binary.LittleEndian.PutUint64(operand[4:12], 20)
	instructions := []SpoiInstruction{
		makeInst(OP_ADD, []int{2}, operand),
	}
	data := buildSpoiStream(instructions)
	executor.Execute(player, data)

	if player.Health != 50 {
		t.Errorf("Expected Health=50, got %d", player.Health)
	}
}

// =============================== 测试：PIPE 操作 ===============================

func TestPipe(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 200},
		&Player{Name: "Carol", Level: 30, Health: 300},
	}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["resultType"] != RESULT_VECTOR {
		t.Errorf("Expected RESULT_VECTOR, got %v", result["resultType"])
	}
	values := result["value"].([]interface{})
	if len(values) != 3 {
		t.Errorf("Expected 3 items, got %d", len(values))
	}
}

// =============================== 测试：FILTER 操作 ===============================

func TestFilter(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 3, Health: 100},
		&Player{Name: "Bob", Level: 10, Health: 200},
		&Player{Name: "Carol", Level: 7, Health: 300},
		&Player{Name: "Dave", Level: 15, Health: 400},
	}

	// FILTER Level > 5, memberIdx=1, cmpOp=GT(3), value=5
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		filterGt([]int{}, 1, 5),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["resultType"] != RESULT_VECTOR {
		t.Errorf("Expected RESULT_VECTOR, got %v", result["resultType"])
	}
	values := result["value"].([]interface{})
	if len(values) != 3 {
		t.Errorf("Expected 3 items after filter, got %d", len(values))
	}
}

func TestFilterEq(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 200},
		&Player{Name: "Carol", Level: 10, Health: 300},
	}

	// FILTER Level == 10, memberIdx=1, cmpOp=EQ(0), value=10
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		filterEq([]int{}, 1, 10),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("Expected 2 items with Level==10, got %d", len(values))
	}
}

// =============================== 测试：SELECT 操作 ===============================

func TestSelect(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 200},
		&Player{Name: "Carol", Level: 30, Health: 300},
	}

	// SELECT Name (index 0)
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		selectInst([]int{0}),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if len(values) != 3 {
		t.Errorf("Expected 3 names, got %d", len(values))
	}
	if values[0] != "Alice" {
		t.Errorf("Expected Alice, got %v", values[0])
	}
	if values[1] != "Bob" {
		t.Errorf("Expected Bob, got %v", values[1])
	}
}

func TestSelectLevel(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 200},
	}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		selectInst([]int{1}), // Level
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if values[0] != uint64(10) {
		t.Errorf("Expected 10, got %v", values[0])
	}
	if values[1] != uint64(20) {
		t.Errorf("Expected 20, got %v", values[1])
	}
}

// =============================== 测试：SORT 操作 ===============================

func TestSort(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Carol", Level: 30, Health: 300},
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 200},
	}

	// SORT by Level (index 1)
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		sortInst([]int{1}),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	first := values[0].(*Player)
	if first.Level != 10 {
		t.Errorf("Expected first Level=10, got %d", first.Level)
	}
	second := values[1].(*Player)
	if second.Level != 20 {
		t.Errorf("Expected second Level=20, got %d", second.Level)
	}
	third := values[2].(*Player)
	if third.Level != 30 {
		t.Errorf("Expected third Level=30, got %d", third.Level)
	}
}

func TestSortByName(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Carol", Level: 30, Health: 300},
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 200},
	}

	// SORT by Name (index 0)
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		sortInst([]int{0}),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if values[0].(*Player).Name != "Alice" {
		t.Errorf("Expected Alice, got %s", values[0].(*Player).Name)
	}
	if values[1].(*Player).Name != "Bob" {
		t.Errorf("Expected Bob, got %s", values[1].(*Player).Name)
	}
	if values[2].(*Player).Name != "Carol" {
		t.Errorf("Expected Carol, got %s", values[2].(*Player).Name)
	}
}

// =============================== 测试：TAKE 操作 ===============================

func TestTake(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 200},
		&Player{Name: "Carol", Level: 30, Health: 300},
	}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		takeInst(2),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("Expected 2 items, got %d", len(values))
	}
	if values[0].(*Player).Name != "Alice" {
		t.Errorf("Expected Alice, got %s", values[0].(*Player).Name)
	}
}

func TestTakeMoreThanAvailable(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 200},
	}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		takeInst(5),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("Expected 2 items, got %d", len(values))
	}
}

// =============================== 测试：DROP 操作 ===============================

func TestDrop(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 200},
		&Player{Name: "Carol", Level: 30, Health: 300},
	}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		dropInst(1),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("Expected 2 items after drop, got %d", len(values))
	}
	if values[0].(*Player).Name != "Bob" {
		t.Errorf("Expected Bob, got %s", values[0].(*Player).Name)
	}
}

func TestDropAll(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
	}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		dropInst(5),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("Expected RESULT_UNDEF, got %v", result["resultType"])
	}
}

// =============================== 测试：REVERSE 操作 ===============================

func TestReverse(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 200},
		&Player{Name: "Carol", Level: 30, Health: 300},
	}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		reverseInst(),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if values[0].(*Player).Name != "Carol" {
		t.Errorf("Expected Carol, got %s", values[0].(*Player).Name)
	}
	if values[2].(*Player).Name != "Alice" {
		t.Errorf("Expected Alice, got %s", values[2].(*Player).Name)
	}
}

// =============================== 测试：DISTINCT 操作 ===============================

func TestDistinct(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	values := []interface{}{1, 2, 2, 3, 3, 3, 4}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		distinctInst(),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(values, data)

	vals := result["value"].([]interface{})
	if len(vals) != 4 {
		t.Errorf("Expected 4 distinct values, got %d", len(vals))
	}
}

func TestDistinctStrings(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	values := []interface{}{"a", "b", "a", "c", "b"}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		distinctInst(),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(values, data)

	vals := result["value"].([]interface{})
	if len(vals) != 3 {
		t.Errorf("Expected 3 distinct values, got %d", len(vals))
	}
}

// =============================== 测试：COUNT 操作 ===============================

func TestCount(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 200},
		&Player{Name: "Carol", Level: 30, Health: 300},
	}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		countInst(),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("Expected RESULT_SINGLE, got %v", result["resultType"])
	}
	if result["value"] != 3 {
		t.Errorf("Expected count=3, got %v", result["value"])
	}
}

func TestCountEmpty(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	empty := []interface{}{}

	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		countInst(),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(empty, data)

	if result["value"] != 0 {
		t.Errorf("Expected count=0, got %v", result["value"])
	}
}

// =============================== 测试：ANY 操作 ===============================

func TestAny(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 3, Health: 100},
		&Player{Name: "Bob", Level: 10, Health: 200},
		&Player{Name: "Carol", Level: 5, Health: 300},
	}

	// ANY Level > 5, memberIdx=1, cmpOp=GT(3), value=5
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		anyInst(1, CMP_GT, 5),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("Expected RESULT_SINGLE, got %v", result["resultType"])
	}
	if result["value"] != true {
		t.Errorf("Expected true, got %v", result["value"])
	}
}

func TestAnyFalse(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 1, Health: 100},
		&Player{Name: "Bob", Level: 2, Health: 200},
	}

	// ANY Level > 5 → should be false
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		anyInst(1, CMP_GT, 5),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["value"] != false {
		t.Errorf("Expected false, got %v", result["value"])
	}
}

// =============================== 测试：ALL 操作 ===============================

func TestAll(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 20, Health: 200},
		&Player{Name: "Carol", Level: 30, Health: 300},
	}

	// ALL Level > 5
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		allInst(1, CMP_GT, 5),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["value"] != true {
		t.Errorf("Expected true, got %v", result["value"])
	}
}

func TestAllFalse(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Bob", Level: 3, Health: 200},
		&Player{Name: "Carol", Level: 30, Health: 300},
	}

	// ALL Level > 5 → Bob has Level=3, should be false
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		allInst(1, CMP_GT, 5),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["value"] != false {
		t.Errorf("Expected false, got %v", result["value"])
	}
}

// =============================== 测试：FIND 操作 ===============================

func TestFind(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 3, Health: 100},
		&Player{Name: "Bob", Level: 10, Health: 200},
		&Player{Name: "Carol", Level: 15, Health: 300},
	}

	// FIND Level == 10
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		findInst(1, CMP_EQ, 10),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["resultType"] != RESULT_SINGLE {
		t.Errorf("Expected RESULT_SINGLE, got %v", result["resultType"])
	}
	found := result["value"].(*Player)
	if found.Name != "Bob" {
		t.Errorf("Expected Bob, got %s", found.Name)
	}
}

func TestFindNotFound(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 3, Health: 100},
		&Player{Name: "Bob", Level: 10, Health: 200},
	}

	// FIND Level == 99 → not found
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		findInst(1, CMP_EQ, 99),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	if result["resultType"] != RESULT_UNDEF {
		t.Errorf("Expected RESULT_UNDEF, got %v", result["resultType"])
	}
}

// =============================== 测试：完整流水线 ===============================

func TestPipeline(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Alice", Level: 3, Health: 100},
		&Player{Name: "Bob", Level: 10, Health: 200},
		&Player{Name: "Carol", Level: 7, Health: 300},
		&Player{Name: "Dave", Level: 15, Health: 400},
		&Player{Name: "Eve", Level: 12, Health: 500},
	}

	// PIPE → FILTER(Level > 5) → SELECT(Name) → TAKE(2) → EXEC
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		filterGt([]int{}, 1, 5),
		selectInst([]int{0}),
		takeInst(2),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if len(values) != 2 {
		t.Errorf("Expected 2 results, got %d", len(values))
	}
	// Bob (Level 10) and Carol (Level 7) should be first two after filter
	if values[0] != "Bob" {
		t.Errorf("Expected Bob, got %v", values[0])
	}
	if values[1] != "Carol" {
		t.Errorf("Expected Carol, got %v", values[1])
	}
}

func TestPipelineSortTake(t *testing.T) {
	executor := NewSpoiExecutor(ExecutorTestRegistry)

	players := []interface{}{
		&Player{Name: "Carol", Level: 30, Health: 300},
		&Player{Name: "Alice", Level: 10, Health: 100},
		&Player{Name: "Dave", Level: 40, Health: 400},
		&Player{Name: "Bob", Level: 20, Health: 200},
	}

	// PIPE → SORT(Level) → TAKE(3) → SELECT(Name) → EXEC
	instructions := []SpoiInstruction{
		pipeInst([]int{}),
		sortInst([]int{1}),
		takeInst(3),
		selectInst([]int{0}),
		execInst(),
	}
	data := buildSpoiStream(instructions)
	result := executor.Execute(players, data)

	values := result["value"].([]interface{})
	if len(values) != 3 {
		t.Errorf("Expected 3 results, got %d", len(values))
	}
	if values[0] != "Alice" {
		t.Errorf("Expected Alice, got %v", values[0])
	}
	if values[1] != "Bob" {
		t.Errorf("Expected Bob, got %v", values[1])
	}
	if values[2] != "Carol" {
		t.Errorf("Expected Carol, got %v", values[2])
	}
}