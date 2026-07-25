// 示例 09：SPOI 全语言跨语言数据互查（Go 服务端）
// 展示：Go 服务端托管游戏状态数据，通过 TCP 接收各语言客户端发送的 SPOI 查询指令，
//       执行查询后将结果序列化返回。

package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"net"
	"os"
	"sort"
)

// ===== 操作码 =====
const (
	OP_SET     uint8 = 0x04
	OP_ADD     uint8 = 0x05
	OP_FILTER  uint8 = 0x0C
	OP_SORT    uint8 = 0x0E
	OP_REVERSE uint8 = 0x0F
	OP_TAKE    uint8 = 0x10
	OP_DROP    uint8 = 0x11
	OP_COUNT   uint8 = 0x15
	OP_ANY     uint8 = 0x16
	OP_FIND    uint8 = 0x18
	OP_EXEC    uint8 = 0x21
)

// ===== 比较运算符 =====
const (
	CMP_EQ uint8 = 0
	CMP_NE uint8 = 1
	CMP_LT uint8 = 2
	CMP_GT uint8 = 3
	CMP_LE uint8 = 4
	CMP_GE uint8 = 5
)

// ===== 字段索引 =====
const (
	PLAYER_NAME  uint32 = 0
	PLAYER_HP    uint32 = 1
	PLAYER_LEVEL uint32 = 2
	PLAYER_GOLD  uint32 = 3
)

const STATE_PLAYERS uint32 = 0

// ===== 结果类型 =====
const (
	RESULT_UNDEF    uint8 = 0
	RESULT_SINGLE   uint8 = 1
	RESULT_VECTOR   uint8 = 2
	RESULT_COUNT    uint8 = 3
	RESULT_BOOL     uint8 = 4
	RESULT_OPTIONAL uint8 = 5
	RESULT_ERROR    uint8 = 6
)

// ===== 二进制辅助函数 =====

func u32LE(v uint32) []byte {
	b := make([]byte, 4)
	binary.LittleEndian.PutUint32(b, v)
	return b
}

func i32LE(v int32) []byte {
	b := make([]byte, 4)
	binary.LittleEndian.PutUint32(b, uint32(v))
	return b
}

func writeVarint(v uint32) []byte {
	buf := new(bytes.Buffer)
	for v >= 0x80 {
		buf.WriteByte(byte(v&0x7F) | 0x80)
		v >>= 7
	}
	buf.WriteByte(byte(v))
	return buf.Bytes()
}

func applyCmpInt(a, b int32, op uint8) bool {
	switch op {
	case CMP_EQ: return a == b
	case CMP_NE: return a != b
	case CMP_LT: return a < b
	case CMP_GT: return a > b
	case CMP_LE: return a <= b
	case CMP_GE: return a >= b
	}
	return false
}

func applyCmpStr(a, b string, op uint8) bool {
	switch op {
	case CMP_EQ: return a == b
	case CMP_NE: return a != b
	case CMP_LT: return a < b
	case CMP_GT: return a > b
	case CMP_LE: return a <= b
	case CMP_GE: return a >= b
	}
	return false
}

// ===== 游戏状态 =====

type Player struct {
	Name  string
	HP    int32
	Level int32
	Gold  int32
}

func (p *Player) GetField(idx uint32) interface{} {
	switch idx {
	case PLAYER_NAME:  return p.Name
	case PLAYER_HP:    return p.HP
	case PLAYER_LEVEL: return p.Level
	case PLAYER_GOLD:  return p.Gold
	}
	return nil
}

func (p *Player) SetField(idx uint32, value interface{}) {
	switch idx {
	case PLAYER_NAME:
		if s, ok := value.(string); ok { p.Name = s }
	case PLAYER_HP:
		if v, ok := value.(int32); ok { p.HP = v }
	case PLAYER_LEVEL:
		if v, ok := value.(int32); ok { p.Level = v }
	case PLAYER_GOLD:
		if v, ok := value.(int32); ok { p.Gold = v }
	}
}

func (p *Player) AddField(idx uint32, delta int32) {
	switch idx {
	case PLAYER_HP:    p.HP += delta
	case PLAYER_LEVEL: p.Level += delta
	case PLAYER_GOLD:  p.Gold += delta
	}
}

func (p *Player) Serialize() []byte {
	nameBytes := []byte(p.Name)
	buf := new(bytes.Buffer)
	binary.Write(buf, binary.LittleEndian, uint32(len(nameBytes)))
	buf.Write(nameBytes)
	binary.Write(buf, binary.LittleEndian, p.HP)
	binary.Write(buf, binary.LittleEndian, p.Level)
	binary.Write(buf, binary.LittleEndian, p.Gold)
	return buf.Bytes()
}

type GameState struct {
	Players    []Player
	Tick       int32
	ServerName string
}

func (gs *GameState) Reset() {
	gs.Players = []Player{
		{"Alice", 80, 10, 500},
		{"Bob", 30, 5, 200},
		{"Carol", 60, 8, 300},
		{"Dave", 90, 12, 400},
		{"Eve", 15, 3, 100},
	}
	gs.Tick = 42
	gs.ServerName = "GoServer"
}

// ===== SPOI 指令解析与执行 =====

type CmpExpr struct {
	MemberIdx uint32
	CmpOp     uint8
	Value     []byte
}

func parseCmpExpr(data []byte) CmpExpr {
	memberIdx := binary.LittleEndian.Uint32(data[0:4])
	cmpOp := data[4]
	valueLen := binary.LittleEndian.Uint32(data[5:9])
	value := data[9 : 9+valueLen]
	return CmpExpr{memberIdx, cmpOp, value}
}

func parseCmpValueStr(expr CmpExpr) *string {
	if len(expr.Value) >= 4 {
		slen := binary.LittleEndian.Uint32(expr.Value[0:4])
		if int(4+slen) == len(expr.Value) {
			s := string(expr.Value[4 : 4+slen])
			return &s
		}
	}
	return nil
}

func parseCmpValueI32(expr CmpExpr) *int32 {
	if len(expr.Value) == 4 {
		v := int32(binary.LittleEndian.Uint32(expr.Value))
		return &v
	}
	return nil
}

func comparePlayer(p *Player, expr CmpExpr) bool {
	val := p.GetField(expr.MemberIdx)
	if val == nil {
		return false
	}
	switch v := val.(type) {
	case string:
		cmpVal := parseCmpValueStr(expr)
		if cmpVal == nil { return false }
		return applyCmpStr(v, *cmpVal, expr.CmpOp)
	case int32:
		cmpVal := parseCmpValueI32(expr)
		if cmpVal == nil { return false }
		return applyCmpInt(v, *cmpVal, expr.CmpOp)
	}
	return false
}

func executeQuery(state *GameState, queryData []byte) []byte {
	if len(queryData) < 4 {
		return makeErrorResult("查询数据太短")
	}

	offset := 0
	instCount := binary.LittleEndian.Uint32(queryData[offset:])
	offset += 4

	var pipeline []Player
	pipelineActive := false

	for i := uint32(0); i < instCount; i++ {
		if offset >= len(queryData) {
			break
		}

		op := queryData[offset]
		offset++
		pathLen := binary.LittleEndian.Uint32(queryData[offset:])
		offset += 4
		path := make([]uint32, pathLen)
		for j := uint32(0); j < pathLen; j++ {
			path[j] = binary.LittleEndian.Uint32(queryData[offset:])
			offset += 4
		}
		operandLen := binary.LittleEndian.Uint32(queryData[offset:])
		offset += 4
		operand := queryData[offset : offset+int(operandLen)]
		offset += int(operandLen)

		switch op {
		case OP_FILTER:
			expr := parseCmpExpr(operand)
			if !pipelineActive && pathLen == 1 && path[0] == STATE_PLAYERS {
				for _, p := range state.Players {
					if comparePlayer(&p, expr) {
						pipeline = append(pipeline, p)
					}
				}
				pipelineActive = true
			} else if pipelineActive {
				var filtered []Player
				for _, p := range pipeline {
					if comparePlayer(&p, expr) {
						filtered = append(filtered, p)
					}
				}
				pipeline = filtered
			} else {
				for _, p := range state.Players {
					if comparePlayer(&p, expr) {
						pipeline = append(pipeline, p)
					}
				}
				pipelineActive = true
			}

		case OP_SORT:
			if pipelineActive && len(operand) >= 5 {
				field := binary.LittleEndian.Uint32(operand[0:4])
				ascending := operand[4] != 0
				sort.Slice(pipeline, func(i, j int) bool {
					vi := toInt(pipeline[i].GetField(field))
					vj := toInt(pipeline[j].GetField(field))
					if ascending {
						return vi < vj
					}
					return vi > vj
				})
			}

		case OP_REVERSE:
			if pipelineActive {
				for i, j := 0, len(pipeline)-1; i < j; i, j = i+1, j-1 {
					pipeline[i], pipeline[j] = pipeline[j], pipeline[i]
				}
			}

		case OP_TAKE:
			if pipelineActive && len(operand) >= 4 {
				n := binary.LittleEndian.Uint32(operand[0:4])
				if int(n) < len(pipeline) {
					pipeline = pipeline[:n]
				}
			}

		case OP_DROP:
			if pipelineActive && len(operand) >= 4 {
				n := binary.LittleEndian.Uint32(operand[0:4])
				if int(n) < len(pipeline) {
					pipeline = pipeline[n:]
				} else {
					pipeline = nil
				}
			}

		case OP_COUNT:
			if pipelineActive {
				return makeCountResult(int32(len(pipeline)))
			}

		case OP_ANY:
			if pipelineActive {
				expr := parseCmpExpr(operand)
				for _, p := range pipeline {
					if comparePlayer(&p, expr) {
						return makeBoolResult(true)
					}
				}
				return makeBoolResult(false)
			}

		case OP_FIND:
			if pipelineActive {
				expr := parseCmpExpr(operand)
				for _, p := range pipeline {
					if comparePlayer(&p, expr) {
						return makeOptionalResult(&p)
					}
				}
				return makeOptionalResult(nil)
			}

		case OP_SET:
			if pathLen >= 3 && path[0] == STATE_PLAYERS {
				idx := path[1]
				field := path[2]
				if int(idx) < len(state.Players) && len(operand) >= 4 {
					val := int32(binary.LittleEndian.Uint32(operand[0:4]))
					state.Players[idx].SetField(field, val)
				}
			}
			// 修改后继续处理后续指令

		case OP_ADD:
			if pathLen >= 3 && path[0] == STATE_PLAYERS {
				idx := path[1]
				field := path[2]
				if int(idx) < len(state.Players) && len(operand) >= 4 {
					delta := int32(binary.LittleEndian.Uint32(operand[0:4]))
					state.Players[idx].AddField(field, delta)
				}
			}
			// 修改后继续处理后续指令

		case OP_EXEC:
			if pipelineActive {
				countBuf := writeVarint(uint32(len(pipeline)))
				playersBuf := new(bytes.Buffer)
				for _, p := range pipeline {
					playersBuf.Write(p.Serialize())
				}
				return makeResult(RESULT_VECTOR, append(countBuf, playersBuf.Bytes()...))
			}
			return makeResult(RESULT_UNDEF, nil)
		}
	}

	return makeResult(RESULT_UNDEF, nil)
}

func toInt(v interface{}) int32 {
	switch val := v.(type) {
	case int32: return val
	case string: return 0
	default: return 0
	}
}

// ===== 结果构建 =====

func makeResult(resultType uint8, data []byte) []byte {
	buf := new(bytes.Buffer)
	buf.WriteByte(resultType)
	binary.Write(buf, binary.LittleEndian, uint32(len(data)))
	buf.Write(data)
	return buf.Bytes()
}

func makeCountResult(count int32) []byte {
	return makeResult(RESULT_COUNT, i32LE(count))
}

func makeBoolResult(val bool) []byte {
	if val {
		return makeResult(RESULT_BOOL, []byte{1})
	}
	return makeResult(RESULT_BOOL, []byte{0})
}

func makeOptionalResult(player *Player) []byte {
	if player == nil {
		return makeResult(RESULT_OPTIONAL, []byte{0})
	}
	return makeResult(RESULT_OPTIONAL, append([]byte{1}, player.Serialize()...))
}

func makeErrorResult(msg string) []byte {
	return makeResult(RESULT_ERROR, []byte(msg))
}

// ===== TCP 通信 =====

func sendWithLength(conn net.Conn, data []byte) error {
	lenBuf := make([]byte, 4)
	binary.LittleEndian.PutUint32(lenBuf, uint32(len(data)))
	if _, err := conn.Write(lenBuf); err != nil {
		return err
	}
	if len(data) > 0 {
		_, err := conn.Write(data)
		return err
	}
	return nil
}

func recvWithLength(conn net.Conn) ([]byte, error) {
	lenBuf := make([]byte, 4)
	if _, err := io.ReadFull(conn, lenBuf); err != nil {
		return nil, err
	}
	dataLen := binary.LittleEndian.Uint32(lenBuf)
	if dataLen == 0 {
		return nil, nil
	}
	data := make([]byte, dataLen)
	if _, err := io.ReadFull(conn, data); err != nil {
		return nil, err
	}
	return data, nil
}

func handleClient(conn net.Conn, state *GameState) {
	defer conn.Close()
	state.Reset()

	for {
		queryData, err := recvWithLength(conn)
		if err != nil {
			break
		}
		result := executeQuery(state, queryData)
		sendWithLength(conn, result)
	}
}

// ===== 主程序 =====

func main() {
	fmt.Println("=== SPOI 全语言跨语言数据互查 — Go 服务端 ===\n")

	state := &GameState{}
	state.Reset()

	fmt.Println("游戏状态已初始化：")
	fmt.Printf("  服务器名称: %s\n", state.ServerName)
	fmt.Printf("  tick: %d\n", state.Tick)
	fmt.Printf("  玩家数: %d\n", len(state.Players))
	for _, p := range state.Players {
		fmt.Printf("    %s: hp=%d level=%d gold=%d\n", p.Name, p.HP, p.Level, p.Gold)
	}

	ln, err := net.Listen("tcp", "127.0.0.1:9999")
	if err != nil {
		fmt.Fprintf(os.Stderr, "监听失败: %v\n", err)
		os.Exit(1)
	}
	defer ln.Close()

	fmt.Printf("\n服务器正在监听 127.0.0.1:9999，等待客户端连接...\n")

	clientNum := 0
	for {
		conn, err := ln.Accept()
		if err != nil {
			fmt.Fprintf(os.Stderr, "接受连接失败: %v\n", err)
			continue
		}
		clientNum++
		fmt.Printf("\n[客户端 #%d] 已连接 (%s)\n", clientNum, conn.RemoteAddr().String())
		handleClient(conn, state)
		fmt.Printf("[客户端 #%d] 已断开连接\n", clientNum)
	}
}