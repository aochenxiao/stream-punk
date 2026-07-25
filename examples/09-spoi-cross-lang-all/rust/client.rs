// 示例 09：SPOI 全语言跨语言数据互查（Rust 客户端）
// 展示：Rust 客户端通过 TCP 向 C++ 服务器发送 SPOI 查询指令，接收并展示查询结果。
// 使用 Rust 标准库手动构建 SPOI 二进制协议。

use std::io::{Read, Write, Cursor};
use std::net::TcpStream;

// ===== 操作码 =====
const OP_SET: u8     = 0x04;
const OP_ADD: u8     = 0x05;
const OP_FILTER: u8  = 0x0C;
const OP_SELECT: u8  = 0x0D;
const OP_SORT: u8    = 0x0E;
const OP_REVERSE: u8 = 0x0F;
const OP_TAKE: u8    = 0x10;
const OP_DROP: u8    = 0x11;
const OP_COUNT: u8   = 0x15;
const OP_ANY: u8     = 0x16;
const OP_ALL: u8     = 0x17;
const OP_FIND: u8    = 0x18;
const OP_KEYS: u8    = 0x19;
const OP_VALUES: u8  = 0x1A;
const OP_JOIN: u8    = 0x1B;
const OP_EXEC: u8    = 0x21;

// ===== 比较运算符 =====
const CMP_EQ: u8 = 0;
const CMP_NE: u8 = 1;
const CMP_LT: u8 = 2;
const CMP_GT: u8 = 3;
const CMP_LE: u8 = 4;
const CMP_GE: u8 = 5;

// ===== 字段索引 =====
const PLAYER_NAME: u32  = 0;
const PLAYER_HP: u32    = 1;
const PLAYER_LEVEL: u32 = 2;
const PLAYER_GOLD: u32  = 3;

const STATE_PLAYERS: u32    = 0;
const STATE_TICK: u32       = 1;
const STATE_SERVERNAME: u32 = 2;

// ===== 结果类型 =====
const RESULT_UNDEF: u8    = 0;
const RESULT_SINGLE: u8   = 1;
const RESULT_VECTOR: u8   = 2;
const RESULT_COUNT: u8    = 3;
const RESULT_BOOL: u8     = 4;
const RESULT_OPTIONAL: u8 = 5;
const RESULT_ERROR: u8    = 6;

// ===== 字节序列化辅助函数 =====

fn u32le(v: u32) -> [u8; 4] {
    v.to_le_bytes()
}

fn i32le(v: i32) -> [u8; 4] {
    v.to_le_bytes()
}

fn read_u32le(cursor: &mut Cursor<&[u8]>) -> Option<u32> {
    let mut buf = [0u8; 4];
    cursor.read_exact(&mut buf).ok()?;
    Some(u32::from_le_bytes(buf))
}

fn read_i32le(cursor: &mut Cursor<&[u8]>) -> Option<i32> {
    let mut buf = [0u8; 4];
    cursor.read_exact(&mut buf).ok()?;
    Some(i32::from_le_bytes(buf))
}

fn read_u8(cursor: &mut Cursor<&[u8]>) -> Option<u8> {
    let mut buf = [0u8; 1];
    cursor.read_exact(&mut buf).ok()?;
    Some(buf[0])
}

/// 读取 varint（用于结果数据中 vector 的元素计数）
fn read_varint(cursor: &mut Cursor<&[u8]>) -> Option<u32> {
    let mut result: u32 = 0;
    let mut shift: u32 = 0;
    loop {
        let b = read_u8(cursor)?;
        result |= ((b & 0x7F) as u32) << shift;
        if b & 0x80 == 0 {
            return Some(result);
        }
        shift += 7;
    }
}

/// 构建字符串长度前缀（u32 LE + UTF-8 字节）
fn str_bytes(s: &str) -> Vec<u8> {
    let mut buf = Vec::new();
    let b = s.as_bytes();
    buf.extend_from_slice(&u32le(b.len() as u32));
    buf.extend_from_slice(b);
    buf
}

// ===== TCP 通信 =====

fn send_with_length(stream: &mut TcpStream, data: &[u8]) -> std::io::Result<()> {
    let len = data.len() as u32;
    stream.write_all(&len.to_le_bytes())?;
    stream.write_all(data)?;
    Ok(())
}

fn recv_with_length(stream: &mut TcpStream) -> std::io::Result<Vec<u8>> {
    let mut len_buf = [0u8; 4];
    stream.read_exact(&mut len_buf)?;
    let len = u32::from_le_bytes(len_buf) as usize;
    let mut data = vec![0u8; len];
    if len > 0 {
        stream.read_exact(&mut data)?;
    }
    Ok(data)
}

// ===== SPOI 查询构建器 =====

struct SpoiQueryBuilder {
    instructions: Vec<Vec<u8>>,
}

impl SpoiQueryBuilder {
    fn new() -> Self {
        SpoiQueryBuilder { instructions: Vec::new() }
    }

    /// 构建比较表达式：[memberIdx: u32 LE][cmpOp: u8][value长度: u32 LE][value字节]
    fn build_cmp_expr(member_idx: u32, cmp_op: u8, value: &[u8]) -> Vec<u8> {
        let mut buf = Vec::new();
        buf.extend_from_slice(&u32le(member_idx));
        buf.push(cmp_op);
        buf.extend_from_slice(&u32le(value.len() as u32));
        buf.extend_from_slice(value);
        buf
    }

    /// 构建指令：[op: u8][路径长度: u32 LE][路径段: u32 LE × N][操作数长度: u32 LE][操作数字节]
    fn build_instruction(op: u8, path: &[u32], operand: &[u8]) -> Vec<u8> {
        let mut buf = Vec::new();
        buf.push(op);
        buf.extend_from_slice(&u32le(path.len() as u32));
        for &seg in path {
            buf.extend_from_slice(&u32le(seg));
        }
        buf.extend_from_slice(&u32le(operand.len() as u32));
        buf.extend_from_slice(operand);
        buf
    }

    fn add_inst(&mut self, op: u8, path: &[u32], operand: &[u8]) {
        self.instructions.push(Self::build_instruction(op, path, operand));
    }

    /// 从 players 开始管道：FILTER 路径=[STATE_PLAYERS]，操作数=hp>=0
    fn from_players(&mut self) -> &mut Self {
        let cmp = Self::build_cmp_expr(PLAYER_HP, CMP_GE, &i32le(0));
        self.add_inst(OP_FILTER, &[STATE_PLAYERS], &cmp);
        self
    }

    fn filter(&mut self, field: u32, cmp_op: u8, value: i32) -> &mut Self {
        let cmp = Self::build_cmp_expr(field, cmp_op, &i32le(value));
        self.add_inst(OP_FILTER, &[], &cmp);
        self
    }

    fn filter_str(&mut self, field: u32, cmp_op: u8, value: &str) -> &mut Self {
        let cmp = Self::build_cmp_expr(field, cmp_op, &str_bytes(value));
        self.add_inst(OP_FILTER, &[], &cmp);
        self
    }

    fn sort(&mut self, field: u32, ascending: bool) -> &mut Self {
        let mut operand = Vec::new();
        operand.extend_from_slice(&u32le(field));
        operand.push(if ascending { 1 } else { 0 });
        self.add_inst(OP_SORT, &[], &operand);
        self
    }

    fn reverse(&mut self) -> &mut Self {
        self.add_inst(OP_REVERSE, &[], &[]);
        self
    }

    fn take(&mut self, n: u32) -> &mut Self {
        self.add_inst(OP_TAKE, &[], &u32le(n));
        self
    }

    fn drop(&mut self, n: u32) -> &mut Self {
        self.add_inst(OP_DROP, &[], &u32le(n));
        self
    }

    fn count(&mut self) -> &mut Self {
        self.add_inst(OP_COUNT, &[], &[]);
        self
    }

    fn any(&mut self, field: u32, cmp_op: u8, value: i32) -> &mut Self {
        let cmp = Self::build_cmp_expr(field, cmp_op, &i32le(value));
        self.add_inst(OP_ANY, &[], &cmp);
        self
    }

    fn find(&mut self, field: u32, cmp_op: u8, value: i32) -> &mut Self {
        let cmp = Self::build_cmp_expr(field, cmp_op, &i32le(value));
        self.add_inst(OP_FIND, &[], &cmp);
        self
    }

    fn find_str(&mut self, field: u32, value: &str) -> &mut Self {
        let cmp = Self::build_cmp_expr(field, CMP_EQ, &str_bytes(value));
        self.add_inst(OP_FIND, &[], &cmp);
        self
    }

    fn set(&mut self, path: &[u32], value: i32) -> &mut Self {
        self.add_inst(OP_SET, path, &i32le(value));
        self
    }

    fn add(&mut self, path: &[u32], delta: i32) -> &mut Self {
        self.add_inst(OP_ADD, path, &i32le(delta));
        self
    }

    /// 构建二进制流：[指令数: u32 LE][指令1][指令2]...，末尾追加 EXEC 指令
    fn build(&mut self) -> Vec<u8> {
        self.add_inst(OP_EXEC, &[], &[]);
        let mut buf = Vec::new();
        buf.extend_from_slice(&u32le(self.instructions.len() as u32));
        for inst in &self.instructions {
            buf.extend_from_slice(inst);
        }
        buf
    }
}

// ===== 结果解析 =====

/// 解析单个 CrossPlayer：[name长度: u32 LE][name UTF-8][hp: i32 LE][level: i32 LE][gold: i32 LE]
fn parse_player(cursor: &mut Cursor<&[u8]>) -> Option<(String, i32, i32, i32)> {
    let name_len = read_u32le(cursor)? as usize;
    let mut name_buf = vec![0u8; name_len];
    cursor.read_exact(&mut name_buf).ok()?;
    let name = String::from_utf8(name_buf).unwrap_or_default();
    let hp = read_i32le(cursor)?;
    let level = read_i32le(cursor)?;
    let gold = read_i32le(cursor)?;
    Some((name, hp, level, gold))
}

fn print_result(data: &[u8]) {
    if data.is_empty() {
        println!("(空结果)");
        return;
    }

    let mut cursor = Cursor::new(data);

    let result_type = match read_u8(&mut cursor) {
        Some(t) => t,
        None => { println!("(无法解析结果)"); return; }
    };

    let data_len = match read_u32le(&mut cursor) {
        Some(l) => l as usize,
        None => { println!("(无法解析数据长度)"); return; }
    };

    let start = cursor.position() as usize;
    let end = start + data_len;
    if end > data.len() {
        println!("(数据长度超出范围)");
        return;
    }
    let inner = &data[start..end];

    match result_type {
        RESULT_COUNT => {
            if inner.len() >= 4 {
                let count = i32::from_le_bytes([inner[0], inner[1], inner[2], inner[3]]);
                println!("计数结果: {}", count);
            }
        }
        RESULT_BOOL => {
            if !inner.is_empty() {
                println!("布尔结果: {}", if inner[0] != 0 { "true" } else { "false" });
            }
        }
        RESULT_VECTOR => {
            let mut inner_cursor = Cursor::new(inner);
            let count = match read_varint(&mut inner_cursor) {
                Some(c) => c,
                None => { println!("(无法解析向量元素数)"); return; }
            };
            println!("向量结果: {} 个元素", count);
            for idx in 0..count {
                match parse_player(&mut inner_cursor) {
                    Some((name, hp, level, gold)) => {
                        println!("    [{}] Player{{name='{}', hp={}, level={}, gold={}}}",
                            idx, name, hp, level, gold);
                    }
                    None => {
                        println!("    [{}] (解析失败)", idx);
                    }
                }
            }
        }
        RESULT_SINGLE => {
            let mut inner_cursor = Cursor::new(inner);
            match parse_player(&mut inner_cursor) {
                Some((name, hp, level, gold)) => {
                    println!("单个结果: Player{{name='{}', hp={}, level={}, gold={}}}",
                        name, hp, level, gold);
                }
                None => println!("(解析单个结果失败)"),
            }
        }
        RESULT_OPTIONAL => {
            if inner.is_empty() {
                println!("可选结果: 空");
            } else if inner[0] != 0 {
                let mut inner_cursor = Cursor::new(&inner[1..]);
                match parse_player(&mut inner_cursor) {
                    Some((name, hp, level, gold)) => {
                        println!("可选结果: 有值 → Player{{name='{}', hp={}, level={}, gold={}}}",
                            name, hp, level, gold);
                    }
                    None => println!("可选结果: 有值（解析失败）"),
                }
            } else {
                println!("可选结果: 空");
            }
        }
        RESULT_ERROR => {
            let err_msg = String::from_utf8_lossy(inner);
            println!("错误: {}", err_msg);
        }
        _ => {
            println!("未知结果类型: {}", result_type);
        }
    }
}

// ===== 主程序 =====

fn main() -> std::io::Result<()> {
    println!("=== SPOI 全语言跨语言数据互查 — Rust 客户端 ===\n");

    let mut stream = match TcpStream::connect("127.0.0.1:9999") {
        Ok(s) => s,
        Err(e) => {
            eprintln!("无法连接到服务器 127.0.0.1:9999: {}", e);
            eprintln!("请确保 C++ 服务器已启动！");
            return Err(e);
        }
    };

    println!("已连接到服务器 127.0.0.1:9999\n");

    let mut test_num = 0u32;

    // 查询 1: 统计玩家总数
    test_num += 1;
    println!("--- 查询 {}: 统计玩家总数 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().count().build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 2: 过滤 hp > 50
    test_num += 1;
    println!("--- 查询 {}: 过滤 hp > 50 的玩家 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GT, 50).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 3: 过滤 level >= 8，取前 2
    test_num += 1;
    println!("--- 查询 {}: 过滤 level >= 8，取前 2 个 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_LEVEL, CMP_GE, 8).take(2).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 4: 查找名为 "Alice" 的玩家
    test_num += 1;
    println!("--- 查询 {}: 查找名为 \"Alice\" 的玩家 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().find_str(PLAYER_NAME, "Alice").build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 5: 按 hp 降序排列，取前 3
    test_num += 1;
    println!("--- 查询 {}: 按 hp 降序排列，取前 3 个 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().sort(PLAYER_HP, false).take(3).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 6: 检查是否有 hp < 20 的玩家
    test_num += 1;
    println!("--- 查询 {}: 检查是否有 hp < 20 的玩家 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().any(PLAYER_HP, CMP_LT, 20).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 7: 统计 hp > 0 的玩家数
    test_num += 1;
    println!("--- 查询 {}: 统计 hp > 0 的玩家数 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GT, 0).count().build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 8: 复杂链式：hp>30 → 按level升序 → 反转 → 取前2
    test_num += 1;
    println!("--- 查询 {}: 复杂链式查询（filter + sort + reverse + take） ---", test_num);
    println!("    (hp > 30 → 按 level 排序 → 反转 → 取前 2)");
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_HP, CMP_GT, 30)
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .take(2).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 9: 写操作 — SET player[0].hp = 99
    test_num += 1;
    println!("--- 查询 {}: 写操作 — 将玩家[0]的 hp 设置为 99 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_HP], 99).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 10: 验证写操作 — 查找 Alice
    test_num += 1;
    println!("--- 查询 {}: 验证写操作 — 查找 Alice 的 hp 是否变为 99 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().find_str(PLAYER_NAME, "Alice").build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 11: 写操作 — ADD player[0].gold += 100
    test_num += 1;
    println!("--- 查询 {}: 写操作 — 给玩家[0]增加 100 金币 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.add(&[0, 0, PLAYER_GOLD], 100).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 12: 验证金币增加 — 查找 Alice
    test_num += 1;
    println!("--- 查询 {}: 验证写操作 — 查找 Alice 的金币是否变为 600 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().find_str(PLAYER_NAME, "Alice").build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 13: filter(hp > 20) + drop(2)
    test_num += 1;
    println!("--- 查询 {}: filter(hp > 20) + drop(2) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GT, 20).drop(2).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ===== 【L1】多条件组合过滤 (3个) =====

    // 查询 14: hp>30 AND level>5
    test_num += 1;
    println!("--- 查询 {}: 【L1】多条件组合过滤 — hp>30 AND level>5 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GT, 30).filter(PLAYER_LEVEL, CMP_GT, 5).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 15: hp>30 AND level<=5
    test_num += 1;
    println!("--- 查询 {}: 【L1】多条件组合过滤 — hp>30 AND level<=5 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GT, 30).filter(PLAYER_LEVEL, CMP_LE, 5).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 16: hp>20 AND level>3 AND gold>200
    test_num += 1;
    println!("--- 查询 {}: 【L1】多条件组合过滤 — hp>20 AND level>3 AND gold>200 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_HP, CMP_GT, 20)
            .filter(PLAYER_LEVEL, CMP_GT, 3)
            .filter(PLAYER_GOLD, CMP_GT, 200).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ===== 【L2】边界条件 (6个) =====

    // 查询 17: 空结果 hp>9000
    test_num += 1;
    println!("--- 查询 {}: 【L2】边界条件 — 空结果 hp>9000 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GT, 9000).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 18: TAKE(100)
    test_num += 1;
    println!("--- 查询 {}: 【L2】边界条件 — TAKE(100) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().take(100).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 19: DROP(100)
    test_num += 1;
    println!("--- 查询 {}: 【L2】边界条件 — DROP(100) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().drop(100).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 20: DROP(100)+COUNT
    test_num += 1;
    println!("--- 查询 {}: 【L2】边界条件 — DROP(100)+COUNT ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().drop(100).count().build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 21: 空管道FIND
    test_num += 1;
    println!("--- 查询 {}: 【L2】边界条件 — 空管道FIND ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GT, 9000).find(PLAYER_HP, CMP_GT, 0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 22: 空管道ANY
    test_num += 1;
    println!("--- 查询 {}: 【L2】边界条件 — 空管道ANY ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GT, 9000).any(PLAYER_HP, CMP_GT, 0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ===== 【L3】复杂管道 (3个) =====

    // 查询 23: SORT(level,asc)+DROP(2)+TAKE(2)
    test_num += 1;
    println!("--- 查询 {}: 【L3】复杂管道 — SORT(level,asc)+DROP(2)+TAKE(2) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().sort(PLAYER_LEVEL, true).drop(2).take(2).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 24: SORT(hp,desc)+REVERSE+DROP(1)+TAKE(2)+COUNT
    test_num += 1;
    println!("--- 查询 {}: 【L3】复杂管道 — SORT(hp,desc)+REVERSE+DROP(1)+TAKE(2)+COUNT ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .sort(PLAYER_HP, false)
            .reverse()
            .drop(1)
            .take(2)
            .count().build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 25: SORT(level,asc)+REVERSE+DROP(1)+TAKE(3)+FILTER(hp>40)
    test_num += 1;
    println!("--- 查询 {}: 【L3】复杂管道 — SORT(level,asc)+REVERSE+DROP(1)+TAKE(3)+FILTER(hp>40) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .drop(1)
            .take(3)
            .filter(PLAYER_HP, CMP_GT, 40).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ===== 【L4】字符串操作 (3个) =====

    // 查询 26: name NE "Alice"
    test_num += 1;
    println!("--- 查询 {}: 【L4】字符串操作 — name NE \"Alice\" ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter_str(PLAYER_NAME, CMP_NE, "Alice").build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 27: name LT "Carol"
    test_num += 1;
    println!("--- 查询 {}: 【L4】字符串操作 — name LT \"Carol\" ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter_str(PLAYER_NAME, CMP_LT, "Carol").build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 28: FIND "Zoe"
    test_num += 1;
    println!("--- 查询 {}: 【L4】字符串操作 — FIND \"Zoe\" ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().find_str(PLAYER_NAME, "Zoe").build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ===== 【L5】写后查询 (3个) =====

    // 查询 29: SET hp=50, ADD hp=30, FIND Alice
    test_num += 1;
    println!("--- 查询 {}: 【L5】写后查询 — SET hp=50, ADD hp=30, FIND Alice ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_HP], 50)
            .add(&[0, 0, PLAYER_HP], 30)
            .from_players()
            .find_str(PLAYER_NAME, "Alice").build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 30: SET Alice hp=999, SET Bob gold=9999, FILTER gold>9000
    test_num += 1;
    println!("--- 查询 {}: 【L5】写后查询 — SET Alice hp=999, SET Bob gold=9999, FILTER gold>9000 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_HP], 999)
            .set(&[0, 1, PLAYER_GOLD], 9999)
            .from_players()
            .filter(PLAYER_GOLD, CMP_GT, 9000).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 31: ADD gold=-300, FIND Alice
    test_num += 1;
    println!("--- 查询 {}: 【L5】写后查询 — ADD gold=-300, FIND Alice ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.add(&[0, 0, PLAYER_GOLD], -300)
            .from_players()
            .find_str(PLAYER_NAME, "Alice").build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ===== 【L6】全比较运算符 (6个) =====

    // 查询 32: filter(hp, EQ, 60)
    test_num += 1;
    println!("--- 查询 {}: 【L6】全比较运算符 — filter(hp, EQ, 60) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_EQ, 60).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 33: filter(hp, NE, 60)
    test_num += 1;
    println!("--- 查询 {}: 【L6】全比较运算符 — filter(hp, NE, 60) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_NE, 60).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 34: filter(hp, LT, 60)
    test_num += 1;
    println!("--- 查询 {}: 【L6】全比较运算符 — filter(hp, LT, 60) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_LT, 60).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 35: filter(hp, GT, 60)
    test_num += 1;
    println!("--- 查询 {}: 【L6】全比较运算符 — filter(hp, GT, 60) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GT, 60).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 36: filter(hp, LE, 60)
    test_num += 1;
    println!("--- 查询 {}: 【L6】全比较运算符 — filter(hp, LE, 60) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_LE, 60).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // 查询 37: filter(hp, GE, 60)
    test_num += 1;
    println!("--- 查询 {}: 【L6】全比较运算符 — filter(hp, GE, 60) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GE, 60).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ===== 【L7】极限链 (1个) =====

    // 查询 38: FILTER → SORT(level,asc) → REVERSE → DROP(1) → TAKE(4) → FILTER(hp>20) → SORT(hp,desc) → REVERSE → TAKE(2) → COUNT
    test_num += 1;
    println!("--- 查询 {}: 【L7】极限链 — FILTER+SORT+REVERSE+DROP+TAKE+FILTER+SORT+REVERSE+TAKE+COUNT ---", test_num);
    println!("    (fromPlayers → filter(hp>20) → sort(level,asc) → reverse → drop(1) → take(4) → filter(hp>20) → sort(hp,desc) → reverse → take(2) → count)");
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_HP, CMP_GT, 20)
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .drop(1)
            .take(4)
            .filter(PLAYER_HP, CMP_GT, 20)
            .sort(PLAYER_HP, false)
            .reverse()
            .take(2)
            .count().build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ================================================================
    //  进阶查询 L8-L12：难度逐级上升，刁钻组合验证库正确性
    // ================================================================
    println!("\n========== 高阶查询 L8-L12 ==========\n");

    // ---- 等级 8: 管道操作边缘情况 ----
    test_num += 1;
    println!("--- 查询 {}: 【L8】REVERSE x2 — 应与原始顺序相同 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().reverse().reverse().build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L8】TAKE(0) — 取0个元素（空向量） ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().take(0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L8】DROP(0) — 丢弃0个（应返回全部） ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().drop(0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L8】SORT 覆盖 — SORT(level,asc) + SORT(hp,desc) ---", test_num);
    println!("    (以最后一次排序为准)");
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().sort(PLAYER_LEVEL, true).sort(PLAYER_HP, false).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L8】REVERSE x3 — 等同于单次 REVERSE ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().reverse().reverse().reverse().build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L8】DROP 到只剩 1 个 + TAKE(1) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().drop(4).take(1).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ---- 等级 9: 数值边界与极端值 ----
    test_num += 1;
    println!("--- 查询 {}: 【L9】FILTER hp < 0 — 无玩家 hp 为负 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_LT, 0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L9】SET hp=0, FILTER hp EQ 0 — 零值精确匹配 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_HP], 0).from_players().filter(PLAYER_HP, CMP_EQ, 0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L9】ADD 负值使金币变负, FILTER gold < 0 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.add(&[0, 4, PLAYER_GOLD], -200).from_players().filter(PLAYER_GOLD, CMP_LT, 0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L9】互斥条件 — FILTER hp>0, FILTER hp<=0（必然空） ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GT, 0).filter(PLAYER_HP, CMP_LE, 0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L9】FILTER level = 0 — 不存在 level=0 的玩家 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_LEVEL, CMP_EQ, 0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L9】FILTER hp >= 0（全部通过） + COUNT ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GE, 0).count().build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ---- 等级 10: 写操作与管道混合 ----
    test_num += 1;
    println!("--- 查询 {}: 【L10】多次 SET 后管道查询 — 改 3 个玩家 hp，然后 FILTER + SORT ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_HP], 10)
            .set(&[0, 1, PLAYER_HP], 20)
            .set(&[0, 2, PLAYER_HP], 30)
            .from_players()
            .filter(PLAYER_HP, CMP_GT, 15)
            .sort(PLAYER_HP, true)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L10】SET + ADD 同一字段后查询 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_GOLD], 100)
            .add(&[0, 0, PLAYER_GOLD], 50)
            .from_players()
            .find_str(PLAYER_NAME, "Alice")
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L10】ADD 全部玩家 level+1, 然后 FILTER + COUNT ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.add(&[0, 0, PLAYER_LEVEL], 1)
            .add(&[0, 1, PLAYER_LEVEL], 1)
            .add(&[0, 2, PLAYER_LEVEL], 1)
            .add(&[0, 3, PLAYER_LEVEL], 1)
            .add(&[0, 4, PLAYER_LEVEL], 1)
            .from_players()
            .filter(PLAYER_LEVEL, CMP_GT, 5)
            .count()
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L10】SET 不存在索引 [0,99] — 应静默忽略，无玩家 hp>9000 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 99, PLAYER_HP], 9999)
            .from_players()
            .filter(PLAYER_HP, CMP_GT, 9000)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L10】写入后管道操作 — SET hp=55, SORT hp, REVERSE, TAKE(2) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_HP], 55)
            .from_players()
            .sort(PLAYER_HP, true)
            .reverse()
            .take(2)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ---- 等级 11: 交叉字段查询 ----
    test_num += 1;
    println!("--- 查询 {}: 【L11】FILTER(hp>30) + SORT(gold) + ANY(level>8) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_HP, CMP_GT, 30)
            .sort(PLAYER_GOLD, true)
            .any(PLAYER_LEVEL, CMP_GT, 8)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L11】FILTER(gold>200) + FILTER(hp>50) + SORT(level) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_GOLD, CMP_GT, 200)
            .filter(PLAYER_HP, CMP_GT, 50)
            .sort(PLAYER_LEVEL, true)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L11】FILTER(gold>200) + SORT(hp) + FIND(level=12) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_GOLD, CMP_GT, 200)
            .sort(PLAYER_HP, true)
            .find(PLAYER_LEVEL, CMP_EQ, 12)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L11】FILTER(hp>50) + SORT(level) + REVERSE + ANY(gold>300) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_HP, CMP_GT, 50)
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .any(PLAYER_GOLD, CMP_GT, 300)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L11】全字段三条件 — hp>25 AND level>4 AND gold>150 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_HP, CMP_GT, 25)
            .filter(PLAYER_LEVEL, CMP_GT, 4)
            .filter(PLAYER_GOLD, CMP_GT, 150)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ---- 等级 12: 极限组合压力 ----
    test_num += 1;
    println!("--- 查询 {}: 【L12】15步极限链 — SORT→REVERSE→DROP→TAKE→FILTER→SORT→REVERSE→TAKE→FILTER→SORT→REVERSE→DROP→TAKE→COUNT ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .drop(1)
            .take(4)
            .filter(PLAYER_HP, CMP_GT, 20)
            .sort(PLAYER_HP, false)
            .reverse()
            .take(3)
            .filter(PLAYER_GOLD, CMP_GT, 100)
            .sort(PLAYER_GOLD, true)
            .reverse()
            .drop(1)
            .take(2)
            .count()
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L12】写入全部 5 个玩家, 然后复杂链查询 ---", test_num);
    println!("    (SET 5个玩家hp → fromPlayers → FILTER hp>30 → SORT hp → REVERSE → TAKE(3))");
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_HP], 100)
            .set(&[0, 1, PLAYER_HP], 200)
            .set(&[0, 2, PLAYER_HP], 150)
            .set(&[0, 3, PLAYER_HP], 50)
            .set(&[0, 4, PLAYER_HP], 175)
            .from_players()
            .filter(PLAYER_HP, CMP_GT, 30)
            .sort(PLAYER_HP, true)
            .reverse()
            .take(3)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L12】SORT+REVERSE 循环 3 次 — 稳定性测试 ---", test_num);
    println!("    (SORT(level,asc)→REVERSE→SORT(hp,desc)→REVERSE→SORT(gold,asc)→REVERSE)");
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .sort(PLAYER_HP, false)
            .reverse()
            .sort(PLAYER_GOLD, true)
            .reverse()
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L12】过滤到单元素 + 全操作 — FILTER(hp>85)→SORT→REVERSE→DROP(0)→TAKE(1) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_HP, CMP_GT, 85)
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .drop(0)
            .take(1)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L12】极限混合 — 写 + 读 + 排序 + 反转 + 过滤 + 计数 ---", test_num);
    println!("    (SET hp=60→ADD gold=50→fromPlayers→FILTER hp>30→SORT level→REVERSE→DROP(1)→TAKE(3)→FILTER gold>100→SORT hp→REVERSE→TAKE(2)→COUNT)");
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_HP], 60)
            .add(&[0, 0, PLAYER_GOLD], 50)
            .from_players()
            .filter(PLAYER_HP, CMP_GT, 30)
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .drop(1)
            .take(3)
            .filter(PLAYER_GOLD, CMP_GT, 100)
            .sort(PLAYER_HP, false)
            .reverse()
            .take(2)
            .count()
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    println!("=== 所有查询完成 ===");

    Ok(())
}