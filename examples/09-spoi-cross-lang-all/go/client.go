// 示例 09：SPOI 全语言跨语言数据互查（Go 客户端）
// 展示：Go 客户端通过 TCP 向 C++ 服务器发送 SPOI 查询指令，接收并展示查询结果。
// 使用 Go 标准库（encoding/binary, net）手动构建 SPOI 二进制协议。

package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"net"
	"os"
)

// ===== 操作码 =====
const (
	OP_SET     uint8 = 0x04
	OP_ADD     uint8 = 0x05
	OP_FILTER  uint8 = 0x0C
	OP_SELECT  uint8 = 0x0D
	OP_SORT    uint8 = 0x0E
	OP_REVERSE uint8 = 0x0F
	OP_TAKE    uint8 = 0x10
	OP_DROP    uint8 = 0x11
	OP_COUNT   uint8 = 0x15
	OP_ANY     uint8 = 0x16
	OP_ALL     uint8 = 0x17
	OP_FIND    uint8 = 0x18
	OP_KEYS    uint8 = 0x19
	OP_VALUES  uint8 = 0x1A
	OP_JOIN    uint8 = 0x1B
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

const (
	STATE_PLAYERS    uint32 = 0
	STATE_TICK       uint32 = 1
	STATE_SERVERNAME uint32 = 2
)

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

// u32LE 将 uint32 编码为小端字节
func u32LE(v uint32) []byte {
	b := make([]byte, 4)
	binary.LittleEndian.PutUint32(b, v)
	return b
}

// i32LE 将 int32 编码为小端字节
func i32LE(v int32) []byte {
	b := make([]byte, 4)
	binary.LittleEndian.PutUint32(b, uint32(v))
	return b
}

// strBytes 将字符串编码为 [u32 LE 长度][UTF-8 字节]
func strBytes(s string) []byte {
	raw := []byte(s)
	buf := new(bytes.Buffer)
	binary.Write(buf, binary.LittleEndian, uint32(len(raw)))
	buf.Write(raw)
	return buf.Bytes()
}

// cmpExprBytes 构建 SpoiCmpExpr 的二进制表示
// 格式: [memberIdx: u32 LE][cmpOp: u8][value长度: u32 LE][value字节]
func cmpExprBytes(memberIdx uint32, cmpOp uint8, value []byte) []byte {
	buf := new(bytes.Buffer)
	binary.Write(buf, binary.LittleEndian, memberIdx)
	buf.WriteByte(cmpOp)
	binary.Write(buf, binary.LittleEndian, uint32(len(value)))
	buf.Write(value)
	return buf.Bytes()
}

// writeVarint 将 uint32 编码为 7-bit varint
func writeVarint(buf *bytes.Buffer, v uint32) {
	for v >= 0x80 {
		buf.WriteByte(byte(v&0x7F) | 0x80)
		v >>= 7
	}
	buf.WriteByte(byte(v))
}

// readVarint 从字节切片中读取 varint，返回解码值和消耗的字节数
func readVarint(data []byte) (uint32, int) {
	var r uint32
	var s uint
	for i, b := range data {
		r |= uint32(b&0x7F) << s
		if b&0x80 == 0 {
			return r, i + 1
		}
		s += 7
	}
	return 0, 0
}

// ===== 查询构建器 =====

type SpoiQueryBuilder struct {
	instructions [][]byte
}

// addInst 添加一条指令
// 格式: [op: u8][路径长度: u32 LE][路径段: u32 LE × N][操作数长度: u32 LE][操作数字节]
func (q *SpoiQueryBuilder) addInst(op uint8, path []uint32, operand []byte) {
	buf := new(bytes.Buffer)
	buf.WriteByte(op)
	binary.Write(buf, binary.LittleEndian, uint32(len(path)))
	for _, p := range path {
		binary.Write(buf, binary.LittleEndian, p)
	}
	binary.Write(buf, binary.LittleEndian, uint32(len(operand)))
	buf.Write(operand)
	q.instructions = append(q.instructions, buf.Bytes())
}

// fromPlayers 从 players 开始管道：FILTER(players, hp >= 0)
func (q *SpoiQueryBuilder) fromPlayers() *SpoiQueryBuilder {
	q.addInst(OP_FILTER, []uint32{STATE_PLAYERS}, cmpExprBytes(PLAYER_HP, CMP_GE, i32LE(0)))
	return q
}

// filter 过滤操作
func (q *SpoiQueryBuilder) filter(field uint32, cmpOp uint8, value int32) *SpoiQueryBuilder {
	q.addInst(OP_FILTER, nil, cmpExprBytes(field, cmpOp, i32LE(value)))
	return q
}

// filterStr 按字符串过滤
func (q *SpoiQueryBuilder) filterStr(field uint32, cmpOp uint8, value string) *SpoiQueryBuilder {
	q.addInst(OP_FILTER, nil, cmpExprBytes(field, cmpOp, strBytes(value)))
	return q
}

// sort 排序
func (q *SpoiQueryBuilder) sort(field uint32, ascending bool) *SpoiQueryBuilder {
	buf := new(bytes.Buffer)
	buf.Write(u32LE(field))
	if ascending {
		buf.WriteByte(1)
	} else {
		buf.WriteByte(0)
	}
	q.addInst(OP_SORT, nil, buf.Bytes())
	return q
}

// reverse 反转
func (q *SpoiQueryBuilder) reverse() *SpoiQueryBuilder {
	q.addInst(OP_REVERSE, nil, nil)
	return q
}

// take 取前 N 个
func (q *SpoiQueryBuilder) take(n uint32) *SpoiQueryBuilder {
	q.addInst(OP_TAKE, nil, u32LE(n))
	return q
}

// drop 丢弃前 N 个
func (q *SpoiQueryBuilder) drop(n uint32) *SpoiQueryBuilder {
	q.addInst(OP_DROP, nil, u32LE(n))
	return q
}

// count 计数
func (q *SpoiQueryBuilder) count() *SpoiQueryBuilder {
	q.addInst(OP_COUNT, nil, nil)
	return q
}

// any 检查是否存在满足条件的元素
func (q *SpoiQueryBuilder) any(field uint32, cmpOp uint8, value int32) *SpoiQueryBuilder {
	q.addInst(OP_ANY, nil, cmpExprBytes(field, cmpOp, i32LE(value)))
	return q
}

// find 查找
func (q *SpoiQueryBuilder) find(field uint32, cmpOp uint8, value int32) *SpoiQueryBuilder {
	q.addInst(OP_FIND, nil, cmpExprBytes(field, cmpOp, i32LE(value)))
	return q
}

// findStr 按字符串查找
func (q *SpoiQueryBuilder) findStr(field uint32, value string) *SpoiQueryBuilder {
	q.addInst(OP_FIND, nil, cmpExprBytes(field, CMP_EQ, strBytes(value)))
	return q
}

// set 设置值
func (q *SpoiQueryBuilder) set(path []uint32, value int32) *SpoiQueryBuilder {
	q.addInst(OP_SET, path, i32LE(value))
	return q
}

// add 增加值
func (q *SpoiQueryBuilder) add(path []uint32, delta int32) *SpoiQueryBuilder {
	q.addInst(OP_ADD, path, i32LE(delta))
	return q
}

// build 构建最终二进制流，自动追加 EXEC 指令
// 格式: [指令数: u32 LE][指令1][指令2]...
func (q *SpoiQueryBuilder) build() []byte {
	q.addInst(OP_EXEC, nil, nil)
	buf := new(bytes.Buffer)
	binary.Write(buf, binary.LittleEndian, uint32(len(q.instructions)))
	for _, inst := range q.instructions {
		buf.Write(inst)
	}
	return buf.Bytes()
}

// ===== TCP 通信 =====

// sendWithLength 发送 [4字节 u32 LE 数据长度][数据]
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

// recvWithLength 接收 [4字节 u32 LE 数据长度][数据]
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

// ===== 结果解析 =====

// CrossPlayer 反序列化
// 格式: [name长度: u32 LE][name UTF-8][hp: i32 LE][level: i32 LE][gold: i32 LE]
func readPlayer(data []byte) (name string, hp, level, gold int32, consumed int) {
	if len(data) < 4 {
		return "", 0, 0, 0, 0
	}
	nameLen := binary.LittleEndian.Uint32(data[0:4])
	pos := 4
	if pos+int(nameLen) > len(data) {
		return "", 0, 0, 0, 0
	}
	name = string(data[pos : pos+int(nameLen)])
	pos += int(nameLen)
	if pos+12 > len(data) {
		return name, 0, 0, 0, pos
	}
	hp = int32(binary.LittleEndian.Uint32(data[pos : pos+4]))
	pos += 4
	level = int32(binary.LittleEndian.Uint32(data[pos : pos+4]))
	pos += 4
	gold = int32(binary.LittleEndian.Uint32(data[pos : pos+4]))
	pos += 4
	return name, hp, level, gold, pos
}

// printResult 解析并打印 SpoiResult
func printResult(data []byte) {
	if len(data) == 0 {
		fmt.Println("(空结果)")
		return
	}

	// 解析 SpoiResult: [resultType: u8][data长度: u32 LE][data字节]
	if len(data) < 5 {
		fmt.Println("(结果数据不完整)")
		return
	}

	resultType := data[0]
	dataLen := binary.LittleEndian.Uint32(data[1:5])
	if 5+int(dataLen) > len(data) {
		fmt.Println("(结果数据长度不匹配)")
		return
	}
	resultData := data[5 : 5+int(dataLen)]

	switch resultType {
	case RESULT_COUNT:
		if len(resultData) >= 4 {
			count := int32(binary.LittleEndian.Uint32(resultData[0:4]))
			fmt.Printf("计数结果: %d\n", count)
		} else {
			fmt.Println("(计数结果数据不完整)")
		}

	case RESULT_BOOL:
		if len(resultData) >= 1 {
			val := resultData[0] != 0
			fmt.Printf("布尔结果: %v\n", val)
		} else {
			fmt.Println("(布尔结果数据不完整)")
		}

	case RESULT_VECTOR:
		// data 内部格式：[varint count][elements...]
		count, varintLen := readVarint(resultData)
		if varintLen == 0 {
			fmt.Println("(向量结果 varint 解析失败)")
			return
		}
		fmt.Printf("向量结果: %d 个元素\n", count)
		pos := varintLen
		for i := uint32(0); i < count; i++ {
			name, hp, level, gold, consumed := readPlayer(resultData[pos:])
			if consumed == 0 {
				fmt.Printf("    [%d] (解析失败)\n", i)
				break
			}
			fmt.Printf("    [%d] Player{name='%s', hp=%d, level=%d, gold=%d}\n", i, name, hp, level, gold)
			pos += consumed
		}

	case RESULT_SINGLE:
		name, hp, level, gold, _ := readPlayer(resultData)
		fmt.Printf("单个结果: Player{name='%s', hp=%d, level=%d, gold=%d}\n", name, hp, level, gold)

	case RESULT_OPTIONAL:
		if len(resultData) > 0 && resultData[0] != 0 {
			name, hp, level, gold, _ := readPlayer(resultData[1:])
			fmt.Printf("可选结果: 有值 → Player{name='%s', hp=%d, level=%d, gold=%d}\n", name, hp, level, gold)
		} else {
			fmt.Println("可选结果: 空")
		}

	case RESULT_ERROR:
		fmt.Printf("错误: %s\n", string(resultData))

	default:
		fmt.Printf("未知结果类型: %d\n", resultType)
	}
}

// ===== 主程序 =====

func main() {
	fmt.Println("=== SPOI 跨语言数据互查 — Go 客户端 ===\n")

	conn, err := net.Dial("tcp", "127.0.0.1:9999")
	if err != nil {
		fmt.Fprintf(os.Stderr, "无法连接到服务器 127.0.0.1:9999\n请确保 C++ 服务器已启动！\n")
		os.Exit(1)
	}
	defer conn.Close()

	fmt.Println("已连接到服务器 127.0.0.1:9999\n")

	testNum := 0

	// 查询 1: 统计玩家总数
	testNum++
	fmt.Printf("--- 查询 %d: 统计玩家总数 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().count().build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 2: 过滤 hp > 50
	testNum++
	fmt.Printf("--- 查询 %d: 过滤 hp > 50 的玩家 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_GT, 50).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 3: 过滤 level >= 8，取前 2 个
	testNum++
	fmt.Printf("--- 查询 %d: 过滤 level >= 8，取前 2 个 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_LEVEL, CMP_GE, 8).take(2).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 4: 查找名为 "Alice" 的玩家
	testNum++
	fmt.Printf("--- 查询 %d: 查找名为 \"Alice\" 的玩家 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().findStr(PLAYER_NAME, "Alice").build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 5: 按 hp 降序排列，取前 3 个
	testNum++
	fmt.Printf("--- 查询 %d: 按 hp 降序排列，取前 3 个 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().sort(PLAYER_HP, false).take(3).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 6: 检查是否有 hp < 20 的玩家
	testNum++
	fmt.Printf("--- 查询 %d: 检查是否有 hp < 20 的玩家 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().any(PLAYER_HP, CMP_LT, 20).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 7: 统计 hp > 0 的玩家数
	testNum++
	fmt.Printf("--- 查询 %d: 统计 hp > 0 的玩家数 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_GT, 0).count().build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 8: 复杂链式查询
	testNum++
	fmt.Printf("--- 查询 %d: 复杂链式查询（filter + sort + reverse + take） ---\n", testNum)
	fmt.Println("    (hp > 30 → 按 level 排序 → 反转 → 取前 2)")
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().
			filter(PLAYER_HP, CMP_GT, 30).
			sort(PLAYER_LEVEL, true).
			reverse().
			take(2).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 9: 写操作 — 设置 hp
	testNum++
	fmt.Printf("--- 查询 %d: 写操作 — 将玩家[0]的 hp 设置为 99 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.set([]uint32{0, 0, PLAYER_HP}, 99).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 10: 验证写操作
	testNum++
	fmt.Printf("--- 查询 %d: 验证写操作 — 查找 Alice 的 hp 是否变为 99 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().findStr(PLAYER_NAME, "Alice").build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 11: 写操作 — 增加金币
	testNum++
	fmt.Printf("--- 查询 %d: 写操作 — 给玩家[0]增加 100 金币 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.add([]uint32{0, 0, PLAYER_GOLD}, 100).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 12: 验证金币增加
	testNum++
	fmt.Printf("--- 查询 %d: 验证写操作 — 查找 Alice 的金币是否变为 600 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().findStr(PLAYER_NAME, "Alice").build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 13: filter + drop
	testNum++
	fmt.Printf("--- 查询 %d: filter(hp > 20) + drop(2) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_GT, 20).drop(2).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// ===== 【L1】多条件组合过滤 (3个) =====

	// 查询 14: 【L1】hp>30 AND level>5
	testNum++
	fmt.Printf("--- 查询 %d: 【L1】hp>30 AND level>5 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_GT, 30).filter(PLAYER_LEVEL, CMP_GT, 5).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 15: 【L1】hp>30 AND level<=5
	testNum++
	fmt.Printf("--- 查询 %d: 【L1】hp>30 AND level<=5 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_GT, 30).filter(PLAYER_LEVEL, CMP_LE, 5).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 16: 【L1】hp>20 AND level>3 AND gold>200
	testNum++
	fmt.Printf("--- 查询 %d: 【L1】hp>20 AND level>3 AND gold>200 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_GT, 20).filter(PLAYER_LEVEL, CMP_GT, 3).filter(PLAYER_GOLD, CMP_GT, 200).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// ===== 【L2】边界条件 (6个) =====

	// 查询 17: 【L2】hp>9000 (空结果)
	testNum++
	fmt.Printf("--- 查询 %d: 【L2】hp>9000 (空结果) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_GT, 9000).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 18: 【L2】TAKE(100)
	testNum++
	fmt.Printf("--- 查询 %d: 【L2】TAKE(100) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().take(100).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 19: 【L2】DROP(100)
	testNum++
	fmt.Printf("--- 查询 %d: 【L2】DROP(100) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().drop(100).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 20: 【L2】DROP(100)+COUNT
	testNum++
	fmt.Printf("--- 查询 %d: 【L2】DROP(100)+COUNT ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().drop(100).count().build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 21: 【L2】空管道FIND
	testNum++
	fmt.Printf("--- 查询 %d: 【L2】空管道FIND ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.findStr(PLAYER_NAME, "Alice").build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 22: 【L2】空管道ANY
	testNum++
	fmt.Printf("--- 查询 %d: 【L2】空管道ANY ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.any(PLAYER_HP, CMP_GT, 0).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// ===== 【L3】复杂管道 (3个) =====

	// 查询 23: 【L3】SORT(level,asc)+DROP(2)+TAKE(2)
	testNum++
	fmt.Printf("--- 查询 %d: 【L3】SORT(level,asc)+DROP(2)+TAKE(2) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().sort(PLAYER_LEVEL, true).drop(2).take(2).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 24: 【L3】SORT(hp,desc)+REVERSE+DROP(1)+TAKE(2)+COUNT
	testNum++
	fmt.Printf("--- 查询 %d: 【L3】SORT(hp,desc)+REVERSE+DROP(1)+TAKE(2)+COUNT ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().sort(PLAYER_HP, false).reverse().drop(1).take(2).count().build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 25: 【L3】SORT(level,asc)+REVERSE+DROP(1)+TAKE(3)+FILTER(hp>40)
	testNum++
	fmt.Printf("--- 查询 %d: 【L3】SORT(level,asc)+REVERSE+DROP(1)+TAKE(3)+FILTER(hp>40) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().sort(PLAYER_LEVEL, true).reverse().drop(1).take(3).filter(PLAYER_HP, CMP_GT, 40).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// ===== 【L4】字符串操作 (3个) =====

	// 查询 26: 【L4】name NE "Alice"
	testNum++
	fmt.Printf("--- 查询 %d: 【L4】name NE \"Alice\" ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filterStr(PLAYER_NAME, CMP_NE, "Alice").build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 27: 【L4】name LT "Carol"
	testNum++
	fmt.Printf("--- 查询 %d: 【L4】name LT \"Carol\" ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filterStr(PLAYER_NAME, CMP_LT, "Carol").build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 28: 【L4】FIND "Zoe"
	testNum++
	fmt.Printf("--- 查询 %d: 【L4】FIND \"Zoe\" ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().findStr(PLAYER_NAME, "Zoe").build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// ===== 【L5】写后查询 (3个) =====

	// 查询 29: 【L5】SET hp=50, ADD hp=30, FIND Alice
	testNum++
	fmt.Printf("--- 查询 %d: 【L5】SET hp=50, ADD hp=30, FIND Alice ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.set([]uint32{0, 0, PLAYER_HP}, 50).add([]uint32{0, 0, PLAYER_HP}, 30).findStr(PLAYER_NAME, "Alice").build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 30: 【L5】SET Alice hp=999, SET Bob gold=9999, FILTER gold>9000
	testNum++
	fmt.Printf("--- 查询 %d: 【L5】SET Alice hp=999, SET Bob gold=9999, FILTER gold>9000 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.set([]uint32{0, 0, PLAYER_HP}, 999).set([]uint32{0, 1, PLAYER_GOLD}, 9999).fromPlayers().filter(PLAYER_GOLD, CMP_GT, 9000).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 31: 【L5】ADD gold=-300, FIND Alice
	testNum++
	fmt.Printf("--- 查询 %d: 【L5】ADD gold=-300, FIND Alice ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.add([]uint32{0, 0, PLAYER_GOLD}, -300).findStr(PLAYER_NAME, "Alice").build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// ===== 【L6】全比较运算符 (6个) =====

	// 查询 32: 【L6】FILTER(hp EQ 60)
	testNum++
	fmt.Printf("--- 查询 %d: 【L6】FILTER(hp EQ 60) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_EQ, 60).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 33: 【L6】FILTER(hp NE 60)
	testNum++
	fmt.Printf("--- 查询 %d: 【L6】FILTER(hp NE 60) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_NE, 60).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 34: 【L6】FILTER(hp LT 60)
	testNum++
	fmt.Printf("--- 查询 %d: 【L6】FILTER(hp LT 60) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_LT, 60).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 35: 【L6】FILTER(hp GT 60)
	testNum++
	fmt.Printf("--- 查询 %d: 【L6】FILTER(hp GT 60) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_GT, 60).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 36: 【L6】FILTER(hp LE 60)
	testNum++
	fmt.Printf("--- 查询 %d: 【L6】FILTER(hp LE 60) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_LE, 60).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// 查询 37: 【L6】FILTER(hp GE 60)
	testNum++
	fmt.Printf("--- 查询 %d: 【L6】FILTER(hp GE 60) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_GE, 60).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// ===== 【L7】极限链 (1个) =====

	// 查询 38: 【L7】极限链式查询
	testNum++
	fmt.Printf("--- 查询 %d: 【L7】极限链式查询 ---\n", testNum)
	fmt.Println("    (sort(level,asc) → reverse → drop(1) → take(4) → filter(hp>20) → sort(hp,desc) → reverse → take(2) → count)")
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().
			sort(PLAYER_LEVEL, true).
			reverse().
			drop(1).
			take(4).
			filter(PLAYER_HP, CMP_GT, 20).
			sort(PLAYER_HP, false).
			reverse().
			take(2).
			count().build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// ================================================================
	//  进阶查询 L8-L12：难度逐级上升，刁钻组合验证库正确性
	// ================================================================
	fmt.Println("\n========== 高阶查询 L8-L12 ==========\n")

	// ---- 等级 8: 管道操作边缘情况 ----
	testNum++
	fmt.Printf("--- 查询 %d: 【L8】REVERSE x2 — 应与原始顺序相同 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().reverse().reverse().build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L8】TAKE(0) — 取0个元素（空向量） ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().take(0).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L8】DROP(0) — 丢弃0个（应返回全部） ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().drop(0).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L8】SORT 覆盖 — SORT(level,asc) + SORT(hp,desc) ---\n", testNum)
	fmt.Println("    (以最后一次排序为准)")
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().sort(PLAYER_LEVEL, true).sort(PLAYER_HP, false).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L8】REVERSE x3 — 等同于单次 REVERSE ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().reverse().reverse().reverse().build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L8】DROP 到只剩 1 个 + TAKE(1) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().drop(4).take(1).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// ---- 等级 9: 数值边界与极端值 ----
	testNum++
	fmt.Printf("--- 查询 %d: 【L9】FILTER hp < 0 — 无玩家 hp 为负 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_LT, 0).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L9】SET hp=0, FILTER hp EQ 0 — 零值精确匹配 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.set([]uint32{0, 0, PLAYER_HP}, 0).fromPlayers().filter(PLAYER_HP, CMP_EQ, 0).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L9】ADD 负值使金币变负, FILTER gold < 0 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.add([]uint32{0, 4, PLAYER_GOLD}, -200).fromPlayers().filter(PLAYER_GOLD, CMP_LT, 0).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L9】互斥条件 — FILTER hp>0, FILTER hp<=0（必然空） ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_GT, 0).filter(PLAYER_HP, CMP_LE, 0).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L9】FILTER level = 0 — 不存在 level=0 的玩家 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_LEVEL, CMP_EQ, 0).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L9】FILTER hp >= 0（全部通过） + COUNT ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().filter(PLAYER_HP, CMP_GE, 0).count().build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// ---- 等级 10: 写操作与管道混合 ----
	testNum++
	fmt.Printf("--- 查询 %d: 【L10】多次 SET 后管道查询 — 改 3 个玩家 hp，然后 FILTER + SORT ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.set([]uint32{0, 0, PLAYER_HP}, 10).
			set([]uint32{0, 1, PLAYER_HP}, 20).
			set([]uint32{0, 2, PLAYER_HP}, 30).
			fromPlayers().
			filter(PLAYER_HP, CMP_GT, 15).
			sort(PLAYER_HP, true).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L10】SET + ADD 同一字段后查询 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.set([]uint32{0, 0, PLAYER_GOLD}, 100).
			add([]uint32{0, 0, PLAYER_GOLD}, 50).
			fromPlayers().
			findStr(PLAYER_NAME, "Alice").build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L10】ADD 全部玩家 level+1, 然后 FILTER + COUNT ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.add([]uint32{0, 0, PLAYER_LEVEL}, 1).
			add([]uint32{0, 1, PLAYER_LEVEL}, 1).
			add([]uint32{0, 2, PLAYER_LEVEL}, 1).
			add([]uint32{0, 3, PLAYER_LEVEL}, 1).
			add([]uint32{0, 4, PLAYER_LEVEL}, 1).
			fromPlayers().
			filter(PLAYER_LEVEL, CMP_GT, 5).
			count().build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L10】SET 不存在索引 [0,99] — 应静默忽略，无玩家 hp>9000 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.set([]uint32{0, 99, PLAYER_HP}, 9999).
			fromPlayers().
			filter(PLAYER_HP, CMP_GT, 9000).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L10】写入后管道操作 — SET hp=55, SORT hp, REVERSE, TAKE(2) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.set([]uint32{0, 0, PLAYER_HP}, 55).
			fromPlayers().
			sort(PLAYER_HP, true).
			reverse().
			take(2).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// ---- 等级 11: 交叉字段查询 ----
	testNum++
	fmt.Printf("--- 查询 %d: 【L11】FILTER(hp>30) + SORT(gold) + ANY(level>8) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().
			filter(PLAYER_HP, CMP_GT, 30).
			sort(PLAYER_GOLD, true).
			any(PLAYER_LEVEL, CMP_GT, 8).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L11】FILTER(gold>200) + FILTER(hp>50) + SORT(level) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().
			filter(PLAYER_GOLD, CMP_GT, 200).
			filter(PLAYER_HP, CMP_GT, 50).
			sort(PLAYER_LEVEL, true).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L11】FILTER(gold>200) + SORT(hp) + FIND(level=12) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().
			filter(PLAYER_GOLD, CMP_GT, 200).
			sort(PLAYER_HP, true).
			find(PLAYER_LEVEL, CMP_EQ, 12).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L11】FILTER(hp>50) + SORT(level) + REVERSE + ANY(gold>300) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().
			filter(PLAYER_HP, CMP_GT, 50).
			sort(PLAYER_LEVEL, true).
			reverse().
			any(PLAYER_GOLD, CMP_GT, 300).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L11】全字段三条件 — hp>25 AND level>4 AND gold>150 ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().
			filter(PLAYER_HP, CMP_GT, 25).
			filter(PLAYER_LEVEL, CMP_GT, 4).
			filter(PLAYER_GOLD, CMP_GT, 150).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	// ---- 等级 12: 极限组合压力 ----
	testNum++
	fmt.Printf("--- 查询 %d: 【L12】15步极限链 — SORT→REVERSE→DROP→TAKE→FILTER→SORT→REVERSE→TAKE→FILTER→SORT→REVERSE→DROP→TAKE→COUNT ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().
			sort(PLAYER_LEVEL, true).
			reverse().
			drop(1).
			take(4).
			filter(PLAYER_HP, CMP_GT, 20).
			sort(PLAYER_HP, false).
			reverse().
			take(3).
			filter(PLAYER_GOLD, CMP_GT, 100).
			sort(PLAYER_GOLD, true).
			reverse().
			drop(1).
			take(2).
			count().build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L12】写入全部 5 个玩家, 然后复杂链查询 ---\n", testNum)
	fmt.Println("    (SET 5个玩家hp → fromPlayers → FILTER hp>30 → SORT hp → REVERSE → TAKE(3))")
	{
		q := &SpoiQueryBuilder{}
		query := q.set([]uint32{0, 0, PLAYER_HP}, 100).
			set([]uint32{0, 1, PLAYER_HP}, 200).
			set([]uint32{0, 2, PLAYER_HP}, 150).
			set([]uint32{0, 3, PLAYER_HP}, 50).
			set([]uint32{0, 4, PLAYER_HP}, 175).
			fromPlayers().
			filter(PLAYER_HP, CMP_GT, 30).
			sort(PLAYER_HP, true).
			reverse().
			take(3).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L12】SORT+REVERSE 循环 3 次 — 稳定性测试 ---\n", testNum)
	fmt.Println("    (SORT(level,asc)→REVERSE→SORT(hp,desc)→REVERSE→SORT(gold,asc)→REVERSE)")
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().
			sort(PLAYER_LEVEL, true).
			reverse().
			sort(PLAYER_HP, false).
			reverse().
			sort(PLAYER_GOLD, true).
			reverse().build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L12】过滤到单元素 + 全操作 — FILTER(hp>85)→SORT→REVERSE→DROP(0)→TAKE(1) ---\n", testNum)
	{
		q := &SpoiQueryBuilder{}
		query := q.fromPlayers().
			filter(PLAYER_HP, CMP_GT, 85).
			sort(PLAYER_LEVEL, true).
			reverse().
			drop(0).
			take(1).build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	testNum++
	fmt.Printf("--- 查询 %d: 【L12】极限混合 — 写 + 读 + 排序 + 反转 + 过滤 + 计数 ---\n", testNum)
	fmt.Println("    (SET hp=60→ADD gold=50→fromPlayers→FILTER hp>30→SORT level→REVERSE→DROP(1)→TAKE(3)→FILTER gold>100→SORT hp→REVERSE→TAKE(2)→COUNT)")
	{
		q := &SpoiQueryBuilder{}
		query := q.set([]uint32{0, 0, PLAYER_HP}, 60).
			add([]uint32{0, 0, PLAYER_GOLD}, 50).
			fromPlayers().
			filter(PLAYER_HP, CMP_GT, 30).
			sort(PLAYER_LEVEL, true).
			reverse().
			drop(1).
			take(3).
			filter(PLAYER_GOLD, CMP_GT, 100).
			sort(PLAYER_HP, false).
			reverse().
			take(2).
			count().build()
		sendWithLength(conn, query)
		result, _ := recvWithLength(conn)
		printResult(result)
		fmt.Println()
	}

	fmt.Println("=== 所有查询完成 ===")
}