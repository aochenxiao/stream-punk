/**
 * StreamPunk SPOI Executor — Go Runtime（v2: 访问器驱动，零反射）
 *
 * SPOI = StreamPunk Operation Instruction
 * 执行 SPOI 指令流，对 Go 对象进行查询/更新操作。
 *
 * 与 v1 的区别：
 *   - 使用 spoi_accessor.go 中的类型特化访问器替代反射
 *   - 使用 DeserializeValue（基于 type_id 前缀）替代字节长度启发式
 *   - 导航和字段设置通过访问器的 switch 跳转表，O(1) 且无反射开销
 */

package main

import (
	"encoding/binary"
	"fmt"
	"sort"
	"strings"
)

// =============================== 操作码常量 ===============================

const (
	// 导航
	OP_NAV    = 0x00
	OP_IDX    = 0x01
	OP_DEREF  = 0x02
	OP_UNWRAP = 0x03
	// 写操作
	OP_SET     = 0x04
	OP_ADD     = 0x05
	OP_APPEND  = 0x06
	OP_REMOVE  = 0x07
	OP_INSERT  = 0x08
	OP_REPLACE = 0x09
	OP_RESET   = 0x0A
	OP_SETNULL = 0x0B
	// 读操作
	OP_FILTER    = 0x0C
	OP_SELECT    = 0x0D
	OP_SORT      = 0x0E
	OP_REVERSE   = 0x0F
	OP_TAKE      = 0x10
	OP_DROP      = 0x11
	OP_TAKEWHILE = 0x12
	OP_DROPWHILE = 0x13
	OP_DISTINCT  = 0x14
	// 聚合
	OP_COUNT = 0x15
	OP_ANY   = 0x16
	OP_ALL   = 0x17
	OP_FIND  = 0x18
	// 容器
	OP_KEYS   = 0x19
	OP_VALUES = 0x1A
	OP_JOIN   = 0x1B
	// 控制
	OP_EXEC = 0x21
	OP_PIPE = 0x22
)

// 路径特殊标记
const (
	PATH_DEREF  = 0xFFFF
	PATH_MAPKEY = 0xFFFE
)

// 结果类型
const (
	RESULT_UNDEF    = 0
	RESULT_SINGLE   = 1
	RESULT_VECTOR   = 2
	RESULT_COUNT    = 3
	RESULT_BOOL     = 4
	RESULT_OPTIONAL = 5
	RESULT_ERROR    = 6
)

// 比较操作符
const (
	CMP_EQ = 0
	CMP_NE = 1
	CMP_LT = 2
	CMP_GT = 3
	CMP_LE = 4
	CMP_GE = 5
)

// =============================== Varint 编解码 ===============================

func readVarint(data []byte, offset int) (int, int) {
	result := 0
	shift := 0
	for offset < len(data) {
		b := data[offset]
		offset++
		result |= int(b&0x7F) << shift
		if b&0x80 == 0 {
			return result, offset
		}
		shift += 7
	}
	return result, offset
}

func writeVarint(buf *[]byte, v int) {
	for v >= 0x80 {
		*buf = append(*buf, byte((v&0x7F)|0x80))
		v >>= 7
	}
	*buf = append(*buf, byte(v&0x7F))
}

// =============================== SPOI 指令 ===============================

type SpoiInstruction struct {
	Op      byte
	Path    []int
	Operand []byte
}

// =============================== SPOI 指令解析 ===============================

func parseSpoiStream(data []byte) []SpoiInstruction {
	offset := 0
	count, offset := readVarint(data, offset)
	instructions := make([]SpoiInstruction, count)

	for idx := 0; idx < count; idx++ {
		op := data[offset]
		offset++

		pathLen, newoff := readVarint(data, offset)
		offset = newoff
		path := make([]int, pathLen)
		for j := 0; j < pathLen; j++ {
			seg, newoff2 := readVarint(data, offset)
			offset = newoff2
			path[j] = seg
		}

		operandLen, newoff3 := readVarint(data, offset)
		offset = newoff3
		operand := data[offset : offset+operandLen]
		offset += operandLen

		instructions[idx] = SpoiInstruction{Op: op, Path: path, Operand: operand}
	}

	return instructions
}

// =============================== 类型名称提取（辅助） ===============================

// getTypeName 从对象提取类型名（用于查找访问器）
func getTypeName(obj interface{}) string {
	if obj == nil {
		return ""
	}
	// 指针类型去掉 *
	name := fmt.Sprintf("%T", obj)
	name = strings.TrimPrefix(name, "*")
	// 去掉包名前缀（如 "main."）
	if idx := strings.LastIndex(name, "."); idx != -1 {
		name = name[idx+1:]
	}
	return name
}

// =============================== SPOI 执行器 ===============================

type SpoiExecutor struct {
	accessors map[string]SpoiAccessor // 访问器注册表（替代 types map[string][]string）
	pipeData  []interface{}
}

func NewSpoiExecutor(accessors map[string]SpoiAccessor) *SpoiExecutor {
	return &SpoiExecutor{accessors: accessors, pipeData: nil}
}

func (e *SpoiExecutor) Execute(root interface{}, data []byte) map[string]interface{} {
	instructions := parseSpoiStream(data)
	e.pipeData = nil

	for _, inst := range instructions {
		e.dispatch(inst, root)
	}

	return e.makeResult()
}

func (e *SpoiExecutor) makeResult() map[string]interface{} {
	if e.pipeData == nil || len(e.pipeData) == 0 {
		return map[string]interface{}{
			"resultType": RESULT_UNDEF,
			"data":       []byte{},
		}
	}
	if len(e.pipeData) == 1 {
		return map[string]interface{}{
			"resultType": RESULT_SINGLE,
			"value":      e.pipeData[0],
		}
	}
	return map[string]interface{}{
		"resultType": RESULT_VECTOR,
		"value":      e.pipeData,
	}
}

func (e *SpoiExecutor) dispatch(inst SpoiInstruction, root interface{}) {
	switch inst.Op {
	// 写操作
	case OP_SET:
		e.opSet(root, inst.Path, inst.Operand)
	case OP_ADD:
		e.opAdd(root, inst.Path, inst.Operand)
	case OP_APPEND:
		e.opAppend(root, inst.Path, inst.Operand)
	case OP_REMOVE:
		e.opRemove(root, inst.Path, inst.Operand)
	case OP_INSERT:
		e.opInsert(root, inst.Path, inst.Operand)
	case OP_REPLACE:
		e.opReplace(root, inst.Path, inst.Operand)
	case OP_RESET:
		e.opReset(root, inst.Path)
	case OP_SETNULL:
		e.opSetNull(root, inst.Path)
	// 读操作
	case OP_FILTER:
		e.opFilter(root, inst.Path, inst.Operand)
	case OP_SELECT:
		e.opSelect(root, inst.Path)
	case OP_SORT:
		e.opSort(inst.Path)
	case OP_REVERSE:
		e.opReverse()
	case OP_TAKE:
		e.opTake(inst.Operand)
	case OP_DROP:
		e.opDrop(inst.Operand)
	case OP_TAKEWHILE:
		e.opTakeWhile(root, inst.Path, inst.Operand)
	case OP_DROPWHILE:
		e.opDropWhile(root, inst.Path, inst.Operand)
	case OP_DISTINCT:
		e.opDistinct()
	// 聚合
	case OP_COUNT:
		e.opCount()
	case OP_ANY:
		e.opAny(root, inst.Path, inst.Operand)
	case OP_ALL:
		e.opAll(root, inst.Path, inst.Operand)
	case OP_FIND:
		e.opFind(root, inst.Path, inst.Operand)
	// 容器
	case OP_KEYS:
		e.opKeys()
	case OP_VALUES:
		e.opValues()
	case OP_JOIN:
		e.opJoin()
	// 控制
	case OP_EXEC:
		// 执行结束
	case OP_PIPE:
		e.opPipe(root, inst.Path)
	default:
		panic(fmt.Sprintf("Unknown SPOI opcode: 0x%02X", inst.Op))
	}
}

// =============================== 导航（访问器驱动） ===============================

func (e *SpoiExecutor) navStep(obj interface{}, seg int) interface{} {
	// 指针解引用
	if seg == PATH_DEREF {
		// 尝试通过访问器获取字段
		acc := e.getAccessor(obj)
		if acc != nil {
			// 尝试获取第一个字段（通常是指针指向的对象）
			return acc.GetField(obj, seg)
		}
		return obj
	}

	// 容器索引访问
	switch v := obj.(type) {
	case []interface{}:
		return v[seg]
	case []int:
		return v[seg]
	case []string:
		return v[seg]
	case *[]interface{}:
		return (*v)[seg]
	default:
		// 检查是否为 slice（通过类型断言链）
	}

	// 结构体成员访问 — 使用访问器
	acc := e.getAccessor(obj)
	if acc != nil {
		return acc.GetField(obj, seg)
	}

	// 回退：检查 slice 类型
	switch v := obj.(type) {
	case []interface{}:
		return v[seg]
	case []int:
		return v[seg]
	case []string:
		return v[seg]
	case *[]interface{}:
		return (*v)[seg]
	}

	panic(fmt.Sprintf("Cannot navigate segment %d on %T", seg, obj))
}

func (e *SpoiExecutor) navigate(obj interface{}, path []int) interface{} {
	current := obj
	for _, seg := range path {
		current = e.navStep(current, seg)
	}
	return current
}

func (e *SpoiExecutor) navSet(obj interface{}, path []int, value interface{}) {
	if len(path) == 0 {
		return
	}
	if len(path) == 1 {
		e.setField(obj, path[0], value)
		return
	}

	target := obj
	for _, seg := range path[:len(path)-1] {
		target = e.navStep(target, seg)
	}
	e.setField(target, path[len(path)-1], value)
}

func (e *SpoiExecutor) setField(obj interface{}, seg int, value interface{}) {
	// 容器
	switch v := obj.(type) {
	case []interface{}:
		v[seg] = value
		return
	case *[]interface{}:
		(*v)[seg] = value
		return
	}

	// 结构体 — 使用访问器
	acc := e.getAccessor(obj)
	if acc != nil {
		acc.SetField(obj, seg, value)
		return
	}

	panic(fmt.Sprintf("Cannot set field %d on %T", seg, obj))
}

// getAccessor 获取对象对应的访问器
func (e *SpoiExecutor) getAccessor(obj interface{}) SpoiAccessor {
	if obj == nil || e.accessors == nil {
		return nil
	}
	typeName := getTypeName(obj)
	if typeName == "" {
		return nil
	}
	return e.accessors[typeName]
}

// =============================== 写操作 ===============================

func (e *SpoiExecutor) opSet(root interface{}, path []int, operand []byte) {
	value := DeserializeValue(operand)
	e.navSet(root, path, value)
}

func (e *SpoiExecutor) opAdd(root interface{}, path []int, operand []byte) {
	delta := DeserializeValue(operand)
	target := e.navigate(root, path)

	// 数值加法
	switch dv := delta.(type) {
	case uint8:
		switch tv := target.(type) {
		case uint8:
			e.navSet(root, path, tv+dv)
			return
		case uint16:
			e.navSet(root, path, tv+uint16(dv))
			return
		case uint32:
			e.navSet(root, path, tv+uint32(dv))
			return
		case uint64:
			e.navSet(root, path, tv+uint64(dv))
			return
		case int8:
			e.navSet(root, path, tv+int8(dv))
			return
		case int16:
			e.navSet(root, path, tv+int16(dv))
			return
		case int32:
			e.navSet(root, path, tv+int32(dv))
			return
		case int64:
			e.navSet(root, path, tv+int64(dv))
			return
		case float32:
			e.navSet(root, path, tv+float32(dv))
			return
		case float64:
			e.navSet(root, path, tv+float64(dv))
			return
		}
	case uint16:
		switch tv := target.(type) {
		case uint16:
			e.navSet(root, path, tv+dv)
			return
		case uint32:
			e.navSet(root, path, tv+uint32(dv))
			return
		case uint64:
			e.navSet(root, path, tv+uint64(dv))
			return
		case int32:
			e.navSet(root, path, tv+int32(dv))
			return
		case int64:
			e.navSet(root, path, tv+int64(dv))
			return
		case float64:
			e.navSet(root, path, tv+float64(dv))
			return
		}
	case uint32:
		switch tv := target.(type) {
		case uint32:
			e.navSet(root, path, tv+dv)
			return
		case uint64:
			e.navSet(root, path, tv+uint64(dv))
			return
		case int32:
			e.navSet(root, path, tv+int32(dv))
			return
		case int64:
			e.navSet(root, path, tv+int64(dv))
			return
		case float64:
			e.navSet(root, path, tv+float64(dv))
			return
		}
	case uint64:
		switch tv := target.(type) {
		case uint64:
			e.navSet(root, path, tv+dv)
			return
		case int64:
			e.navSet(root, path, tv+int64(dv))
			return
		case float64:
			e.navSet(root, path, tv+float64(dv))
			return
		}
	case int8:
		switch tv := target.(type) {
		case int8:
			e.navSet(root, path, tv+dv)
			return
		case int16:
			e.navSet(root, path, tv+int16(dv))
			return
		case int32:
			e.navSet(root, path, tv+int32(dv))
			return
		case int64:
			e.navSet(root, path, tv+int64(dv))
			return
		case float32:
			e.navSet(root, path, tv+float32(dv))
			return
		case float64:
			e.navSet(root, path, tv+float64(dv))
			return
		}
	case int16:
		switch tv := target.(type) {
		case int16:
			e.navSet(root, path, tv+dv)
			return
		case int32:
			e.navSet(root, path, tv+int32(dv))
			return
		case int64:
			e.navSet(root, path, tv+int64(dv))
			return
		case float64:
			e.navSet(root, path, tv+float64(dv))
			return
		}
	case int32:
		switch tv := target.(type) {
		case int32:
			e.navSet(root, path, tv+dv)
			return
		case int64:
			e.navSet(root, path, tv+int64(dv))
			return
		case float64:
			e.navSet(root, path, tv+float64(dv))
			return
		}
	case int64:
		switch tv := target.(type) {
		case int64:
			e.navSet(root, path, tv+dv)
			return
		case float64:
			e.navSet(root, path, tv+float64(dv))
			return
		}
	case float32:
		switch tv := target.(type) {
		case float32:
			e.navSet(root, path, tv+dv)
			return
		case float64:
			e.navSet(root, path, tv+float64(dv))
			return
		}
	case float64:
		switch tv := target.(type) {
		case float64:
			e.navSet(root, path, tv+dv)
			return
		}
	case string:
		switch tv := target.(type) {
		case string:
			e.navSet(root, path, tv+dv)
			return
		}
	}

	// 默认：字符串拼接
	result := fmt.Sprintf("%v%v", target, delta)
	e.navSet(root, path, result)
}

func (e *SpoiExecutor) opAppend(root interface{}, path []int, operand []byte) {
	value := DeserializeValue(operand)
	target := e.navigate(root, path)

	switch v := target.(type) {
	case []interface{}:
		e.navSet(root, path, append(v, value))
		return
	case *[]interface{}:
		e.navSet(root, path, append(*v, value))
		return
	case []int:
		if iv, ok := value.(int); ok {
			e.navSet(root, path, append(v, iv))
		} else {
			e.navSet(root, path, append(v, value.(int)))
		}
		return
	case []string:
		e.navSet(root, path, append(v, value.(string)))
		return
	}

	panic(fmt.Sprintf("Cannot append to %T", target))
}

func (e *SpoiExecutor) opRemove(root interface{}, path []int, operand []byte) {
	target := e.navigate(root, path)
	if len(operand) >= 4 {
		idx := int(binary.LittleEndian.Uint32(operand[:4]))
		switch v := target.(type) {
		case []interface{}:
			if idx >= 0 && idx < len(v) {
				e.navSet(root, path, append(v[:idx], v[idx+1:]...))
				return
			}
		case *[]interface{}:
			if idx >= 0 && idx < len(*v) {
				e.navSet(root, path, append((*v)[:idx], (*v)[idx+1:]...))
				return
			}
		}
	}
	panic(fmt.Sprintf("Cannot remove from %T", target))
}

func (e *SpoiExecutor) opInsert(root interface{}, path []int, operand []byte) {
	if len(operand) < 4 {
		return
	}
	idx := int(binary.LittleEndian.Uint32(operand[:4]))
	value := DeserializeValue(operand[4:])
	target := e.navigate(root, path)

	switch v := target.(type) {
	case []interface{}:
		if idx >= 0 && idx <= len(v) {
			result := make([]interface{}, len(v)+1)
			copy(result[:idx], v[:idx])
			result[idx] = value
			copy(result[idx+1:], v[idx:])
			e.navSet(root, path, result)
			return
		}
	}
	panic(fmt.Sprintf("Cannot insert into %T", target))
}

func (e *SpoiExecutor) opReplace(root interface{}, path []int, operand []byte) {
	if len(operand) < 4 {
		return
	}
	idx := int(binary.LittleEndian.Uint32(operand[:4]))
	value := DeserializeValue(operand[4:])
	target := e.navigate(root, path)

	switch v := target.(type) {
	case []interface{}:
		if idx >= 0 && idx < len(v) {
			v[idx] = value
			return
		}
	}
	panic(fmt.Sprintf("Cannot replace in %T", target))
}

func (e *SpoiExecutor) opReset(root interface{}, path []int) {
	e.navSet(root, path, nil)
}

func (e *SpoiExecutor) opSetNull(root interface{}, path []int) {
	e.navSet(root, path, nil)
}

// =============================== 读操作 ===============================

func (e *SpoiExecutor) opPipe(root interface{}, path []int) {
	var data interface{}
	if len(path) > 0 {
		data = e.navigate(root, path)
	} else {
		data = root
	}

	switch v := data.(type) {
	case []interface{}:
		e.pipeData = v
	case []int:
		e.pipeData = make([]interface{}, len(v))
		for i, val := range v {
			e.pipeData[i] = val
		}
	case []string:
		e.pipeData = make([]interface{}, len(v))
		for i, val := range v {
			e.pipeData[i] = val
		}
	case *[]interface{}:
		e.pipeData = *v
	default:
		e.pipeData = []interface{}{data}
	}
}

func (e *SpoiExecutor) opFilter(root interface{}, _path []int, operand []byte) {
	result := make([]interface{}, 0)
	for _, obj := range e.pipeData {
		if e.matches(obj, _path, operand) {
			result = append(result, obj)
		}
	}
	e.pipeData = result
}

func (e *SpoiExecutor) opSelect(root interface{}, path []int) {
	if len(path) > 0 {
		result := make([]interface{}, len(e.pipeData))
		for i, obj := range e.pipeData {
			result[i] = e.navigate(obj, path)
		}
		e.pipeData = result
	}
}

func (e *SpoiExecutor) opSort(path []int) {
	if len(path) > 0 {
		sort.Slice(e.pipeData, func(i, j int) bool {
			a := e.navigate(e.pipeData[i], path)
			b := e.navigate(e.pipeData[j], path)
			return e.compareValues(a, b) < 0
		})
	} else {
		sort.Slice(e.pipeData, func(i, j int) bool {
			return e.sortKey(e.pipeData[i]) < e.sortKey(e.pipeData[j])
		})
	}
}

func (e *SpoiExecutor) sortKey(obj interface{}) string {
	return fmt.Sprintf("%v", obj)
}

func (e *SpoiExecutor) compareValues(a, b interface{}) int {
	// 数值比较
	aFloat, aOk := toFloat64(a)
	bFloat, bOk := toFloat64(b)
	if aOk && bOk {
		if aFloat < bFloat {
			return -1
		} else if aFloat > bFloat {
			return 1
		}
		return 0
	}

	// 字符串比较
	sa := fmt.Sprintf("%v", a)
	sb := fmt.Sprintf("%v", b)
	return strings.Compare(sa, sb)
}

func toFloat64(v interface{}) (float64, bool) {
	switch val := v.(type) {
	case uint8:
		return float64(val), true
	case uint16:
		return float64(val), true
	case uint32:
		return float64(val), true
	case uint64:
		return float64(val), true
	case int8:
		return float64(val), true
	case int16:
		return float64(val), true
	case int32:
		return float64(val), true
	case int64:
		return float64(val), true
	case float32:
		return float64(val), true
	case float64:
		return val, true
	case int:
		return float64(val), true
	}
	return 0, false
}

func (e *SpoiExecutor) opReverse() {
	for i, j := 0, len(e.pipeData)-1; i < j; i, j = i+1, j-1 {
		e.pipeData[i], e.pipeData[j] = e.pipeData[j], e.pipeData[i]
	}
}

func (e *SpoiExecutor) opTake(operand []byte) {
	var n int
	if len(operand) >= 4 {
		n = int(binary.LittleEndian.Uint32(operand[:4]))
	}
	if n > len(e.pipeData) {
		n = len(e.pipeData)
	}
	e.pipeData = e.pipeData[:n]
}

func (e *SpoiExecutor) opDrop(operand []byte) {
	var n int
	if len(operand) >= 4 {
		n = int(binary.LittleEndian.Uint32(operand[:4]))
	}
	if n >= len(e.pipeData) {
		e.pipeData = nil
	} else {
		e.pipeData = e.pipeData[n:]
	}
}

func (e *SpoiExecutor) opTakeWhile(root interface{}, _path []int, operand []byte) {
	result := make([]interface{}, 0)
	for _, obj := range e.pipeData {
		if e.matches(obj, _path, operand) {
			result = append(result, obj)
		} else {
			break
		}
	}
	e.pipeData = result
}

func (e *SpoiExecutor) opDropWhile(root interface{}, _path []int, operand []byte) {
	idx := len(e.pipeData)
	for i, obj := range e.pipeData {
		if !e.matches(obj, _path, operand) {
			idx = i
			break
		}
	}
	e.pipeData = e.pipeData[idx:]
}

func (e *SpoiExecutor) opDistinct() {
	seen := make(map[string]bool)
	result := make([]interface{}, 0)
	for _, obj := range e.pipeData {
		key := e.distinctKey(obj)
		if !seen[key] {
			seen[key] = true
			result = append(result, obj)
		}
	}
	e.pipeData = result
}

func (e *SpoiExecutor) distinctKey(obj interface{}) string {
	switch v := obj.(type) {
	case int, int8, int16, int32, int64, uint, uint8, uint16, uint32, uint64:
		return fmt.Sprintf("%d", v)
	case float32, float64:
		return fmt.Sprintf("%v", v)
	case string:
		return v
	case bool:
		if v {
			return "true"
		}
		return "false"
	default:
		return fmt.Sprintf("%v", obj)
	}
}

// =============================== 聚合 ===============================

func (e *SpoiExecutor) opCount() {
	e.pipeData = []interface{}{len(e.pipeData)}
}

func (e *SpoiExecutor) opAny(root interface{}, _path []int, operand []byte) {
	for _, obj := range e.pipeData {
		if e.matches(obj, _path, operand) {
			e.pipeData = []interface{}{true}
			return
		}
	}
	e.pipeData = []interface{}{false}
}

func (e *SpoiExecutor) opAll(root interface{}, _path []int, operand []byte) {
	for _, obj := range e.pipeData {
		if !e.matches(obj, _path, operand) {
			e.pipeData = []interface{}{false}
			return
		}
	}
	e.pipeData = []interface{}{true}
}

func (e *SpoiExecutor) opFind(root interface{}, _path []int, operand []byte) {
	for _, obj := range e.pipeData {
		if e.matches(obj, _path, operand) {
			e.pipeData = []interface{}{obj}
			return
		}
	}
	e.pipeData = nil
}

// =============================== 容器操作 ===============================

func (e *SpoiExecutor) opKeys() {
	result := make([]interface{}, 0)
	for _, obj := range e.pipeData {
		switch v := obj.(type) {
		case map[string]interface{}:
			for k := range v {
				result = append(result, k)
			}
		case map[string]string:
			for k := range v {
				result = append(result, k)
			}
		case map[string]int:
			for k := range v {
				result = append(result, k)
			}
		}
	}
	e.pipeData = result
}

func (e *SpoiExecutor) opValues() {
	result := make([]interface{}, 0)
	for _, obj := range e.pipeData {
		switch v := obj.(type) {
		case map[string]interface{}:
			for _, val := range v {
				result = append(result, val)
			}
		case map[string]string:
			for _, val := range v {
				result = append(result, val)
			}
		case map[string]int:
			for _, val := range v {
				result = append(result, val)
			}
		}
	}
	e.pipeData = result
}

func (e *SpoiExecutor) opJoin() {
	result := make([]interface{}, 0)
	for _, obj := range e.pipeData {
		switch v := obj.(type) {
		case []interface{}:
			result = append(result, v...)
		case []int:
			for _, val := range v {
				result = append(result, val)
			}
		case []string:
			for _, val := range v {
				result = append(result, val)
			}
		default:
			result = append(result, obj)
		}
	}
	e.pipeData = result
}

// =============================== 比较匹配（访问器驱动） ===============================

func (e *SpoiExecutor) matches(obj interface{}, _path []int, operand []byte) bool {
	// operand 格式: memberIdx(u32) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
	if len(operand) < 9 {
		return true
	}
	memberIdx := int(binary.LittleEndian.Uint32(operand[:4]))
	cmpOp := operand[4]
	// value_len 是 varint 编码的，跳过它
	valueLen, offset := readVarint(operand, 5)
	_ = valueLen
	valueBytes := operand[offset:]

	fieldValue := e.navStep(obj, memberIdx)
	expected := DeserializeValue(valueBytes)

	return e.compare(cmpOp, fieldValue, expected)
}

func (e *SpoiExecutor) compare(cmpOp byte, a, b interface{}) bool {
	switch cmpOp {
	case CMP_EQ:
		return e.valuesEqual(a, b)
	case CMP_NE:
		return !e.valuesEqual(a, b)
	case CMP_LT:
		return e.compareValues(a, b) < 0
	case CMP_GT:
		return e.compareValues(a, b) > 0
	case CMP_LE:
		return e.compareValues(a, b) <= 0
	case CMP_GE:
		return e.compareValues(a, b) >= 0
	}
	return true
}

func (e *SpoiExecutor) valuesEqual(a, b interface{}) bool {
	if a == nil && b == nil {
		return true
	}
	if a == nil || b == nil {
		return false
	}

	// 数值比较
	aFloat, aOk := toFloat64(a)
	bFloat, bOk := toFloat64(b)
	if aOk && bOk {
		return aFloat == bFloat
	}

	// 字符串比较
	return fmt.Sprintf("%v", a) == fmt.Sprintf("%v", b)
}