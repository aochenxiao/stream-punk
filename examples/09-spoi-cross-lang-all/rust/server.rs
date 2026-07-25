// 示例 09：SPOI 全语言跨语言数据互查（Rust 服务端）
// 展示：Rust 服务端托管游戏状态数据，通过 TCP 接收各语言客户端发送的 SPOI 查询指令，
//       执行查询后将结果序列化返回。

use std::io::{Read, Write, Cursor};
use std::net::{TcpListener, TcpStream};

// ===== 操作码 =====
const OP_SET: u8     = 0x04;
const OP_ADD: u8     = 0x05;
const OP_FILTER: u8  = 0x0C;
const OP_SORT: u8    = 0x0E;
const OP_REVERSE: u8 = 0x0F;
const OP_TAKE: u8    = 0x10;
const OP_DROP: u8    = 0x11;
const OP_COUNT: u8   = 0x15;
const OP_ANY: u8     = 0x16;
const OP_FIND: u8    = 0x18;
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
const STATE_PLAYERS: u32 = 0;

// ===== 结果类型 =====
const RESULT_UNDEF: u8    = 0;
const RESULT_SINGLE: u8   = 1;
const RESULT_VECTOR: u8   = 2;
const RESULT_COUNT: u8    = 3;
const RESULT_BOOL: u8     = 4;
const RESULT_OPTIONAL: u8 = 5;
const RESULT_ERROR: u8    = 6;

// ===== 二进制辅助函数 =====

fn u32le(v: u32) -> [u8; 4] { v.to_le_bytes() }
fn i32le(v: i32) -> [u8; 4] { v.to_le_bytes() }

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

fn write_varint(v: u32) -> Vec<u8> {
    let mut v = v;
    let mut result = Vec::new();
    while v >= 0x80 {
        result.push((v as u8 & 0x7F) | 0x80);
        v >>= 7;
    }
    result.push(v as u8);
    result
}

fn apply_cmp_int(a: i32, op: u8, b: i32) -> bool {
    match op {
        CMP_EQ => a == b,
        CMP_NE => a != b,
        CMP_LT => a < b,
        CMP_GT => a > b,
        CMP_LE => a <= b,
        CMP_GE => a >= b,
        _ => false,
    }
}

fn apply_cmp_str(a: &str, op: u8, b: &str) -> bool {
    match op {
        CMP_EQ => a == b,
        CMP_NE => a != b,
        CMP_LT => a < b,
        CMP_GT => a > b,
        CMP_LE => a <= b,
        CMP_GE => a >= b,
        _ => false,
    }
}

// ===== 游戏状态 =====

#[derive(Clone)]
struct Player {
    name: String,
    hp: i32,
    level: i32,
    gold: i32,
}

impl Player {
    fn new(name: &str, hp: i32, level: i32, gold: i32) -> Self {
        Player { name: name.to_string(), hp, level, gold }
    }

    fn get_field(&self, idx: u32) -> Option<FieldValue> {
        match idx {
            PLAYER_NAME  => Some(FieldValue::Str(self.name.clone())),
            PLAYER_HP    => Some(FieldValue::Int(self.hp)),
            PLAYER_LEVEL => Some(FieldValue::Int(self.level)),
            PLAYER_GOLD  => Some(FieldValue::Int(self.gold)),
            _ => None,
        }
    }

    fn set_field(&mut self, idx: u32, value: FieldValue) {
        match (idx, value) {
            (PLAYER_NAME, FieldValue::Str(s))  => self.name = s,
            (PLAYER_HP, FieldValue::Int(v))    => self.hp = v,
            (PLAYER_LEVEL, FieldValue::Int(v)) => self.level = v,
            (PLAYER_GOLD, FieldValue::Int(v))  => self.gold = v,
            _ => {}
        }
    }

    fn add_field(&mut self, idx: u32, delta: i32) {
        match idx {
            PLAYER_HP    => self.hp += delta,
            PLAYER_LEVEL => self.level += delta,
            PLAYER_GOLD  => self.gold += delta,
            _ => {}
        }
    }

    fn serialize(&self) -> Vec<u8> {
        let name_bytes = self.name.as_bytes();
        let mut buf = Vec::new();
        buf.extend_from_slice(&u32le(name_bytes.len() as u32));
        buf.extend_from_slice(name_bytes);
        buf.extend_from_slice(&i32le(self.hp));
        buf.extend_from_slice(&i32le(self.level));
        buf.extend_from_slice(&i32le(self.gold));
        buf
    }
}

#[derive(Clone)]
enum FieldValue {
    Int(i32),
    Str(String),
}

struct GameState {
    players: Vec<Player>,
    tick: i32,
    server_name: String,
}

impl GameState {
    fn new() -> Self {
        GameState { players: Vec::new(), tick: 42, server_name: "RustServer".to_string() }
    }

    fn reset(&mut self) {
        self.players = vec![
            Player::new("Alice", 80, 10, 500),
            Player::new("Bob",   30, 5,  200),
            Player::new("Carol", 60, 8,  300),
            Player::new("Dave",  90, 12, 400),
            Player::new("Eve",   15, 3,  100),
        ];
        self.tick = 42;
        self.server_name = "RustServer".to_string();
    }
}

// ===== SPOI 指令解析与执行 =====

struct CmpExpr {
    member_idx: u32,
    cmp_op: u8,
    value: Vec<u8>,
}

fn parse_cmp_expr(data: &[u8]) -> CmpExpr {
    let member_idx = u32::from_le_bytes([data[0], data[1], data[2], data[3]]);
    let cmp_op = data[4];
    let value_len = u32::from_le_bytes([data[5], data[6], data[7], data[8]]) as usize;
    let value = data[9..9 + value_len].to_vec();
    CmpExpr { member_idx, cmp_op, value }
}

fn parse_cmp_value_str(expr: &CmpExpr) -> Option<String> {
    if expr.value.len() >= 4 {
        let slen = u32::from_le_bytes([expr.value[0], expr.value[1], expr.value[2], expr.value[3]]) as usize;
        if 4 + slen == expr.value.len() {
            return String::from_utf8(expr.value[4..4 + slen].to_vec()).ok();
        }
    }
    None
}

fn parse_cmp_value_i32(expr: &CmpExpr) -> Option<i32> {
    if expr.value.len() == 4 {
        Some(i32::from_le_bytes([expr.value[0], expr.value[1], expr.value[2], expr.value[3]]))
    } else {
        None
    }
}

fn compare_player(player: &Player, expr: &CmpExpr) -> bool {
    match player.get_field(expr.member_idx) {
        Some(FieldValue::Str(s)) => {
            if let Some(cmp_val) = parse_cmp_value_str(expr) {
                apply_cmp_str(&s, expr.cmp_op, &cmp_val)
            } else {
                false
            }
        }
        Some(FieldValue::Int(v)) => {
            if let Some(cmp_val) = parse_cmp_value_i32(expr) {
                apply_cmp_int(v, expr.cmp_op, cmp_val)
            } else {
                false
            }
        }
        None => false,
    }
}

fn execute_query(state: &mut GameState, query_data: &[u8]) -> Vec<u8> {
    if query_data.len() < 4 {
        return make_error_result("查询数据太短");
    }

    let mut offset = 0;
    let inst_count = u32::from_le_bytes([query_data[0], query_data[1], query_data[2], query_data[3]]) as usize;
    offset += 4;

    let mut pipeline: Vec<Player> = Vec::new();
    let mut pipeline_active = false;

    for _ in 0..inst_count {
        if offset >= query_data.len() { break; }

        let op = query_data[offset]; offset += 1;
        let path_len = u32::from_le_bytes([query_data[offset], query_data[offset+1], query_data[offset+2], query_data[offset+3]]) as usize;
        offset += 4;
        let mut path = Vec::new();
        for _ in 0..path_len {
            path.push(u32::from_le_bytes([query_data[offset], query_data[offset+1], query_data[offset+2], query_data[offset+3]]));
            offset += 4;
        }
        let operand_len = u32::from_le_bytes([query_data[offset], query_data[offset+1], query_data[offset+2], query_data[offset+3]]) as usize;
        offset += 4;
        let operand = &query_data[offset..offset + operand_len];
        offset += operand_len;

        match op {
            OP_FILTER => {
                let expr = parse_cmp_expr(operand);
                if !pipeline_active && path_len == 1 && path[0] == STATE_PLAYERS {
                    pipeline = state.players.iter()
                        .filter(|p| compare_player(p, &expr))
                        .cloned()
                        .collect();
                    pipeline_active = true;
                } else if pipeline_active {
                    pipeline = pipeline.into_iter()
                        .filter(|p| compare_player(p, &expr))
                        .collect();
                } else {
                    pipeline = state.players.iter()
                        .filter(|p| compare_player(p, &expr))
                        .cloned()
                        .collect();
                    pipeline_active = true;
                }
            }
            OP_SORT => {
                if pipeline_active && operand.len() >= 5 {
                    let field = u32::from_le_bytes([operand[0], operand[1], operand[2], operand[3]]);
                    let ascending = operand[4] != 0;
                    pipeline.sort_by(|a, b| {
                        let va = match a.get_field(field) {
                            Some(FieldValue::Int(v)) => v,
                            _ => 0,
                        };
                        let vb = match b.get_field(field) {
                            Some(FieldValue::Int(v)) => v,
                            _ => 0,
                        };
                        if ascending { va.cmp(&vb) } else { vb.cmp(&va) }
                    });
                }
            }
            OP_REVERSE => {
                if pipeline_active { pipeline.reverse(); }
            }
            OP_TAKE => {
                if pipeline_active && operand.len() >= 4 {
                    let n = u32::from_le_bytes([operand[0], operand[1], operand[2], operand[3]]) as usize;
                    pipeline.truncate(n);
                }
            }
            OP_DROP => {
                if pipeline_active && operand.len() >= 4 {
                    let n = u32::from_le_bytes([operand[0], operand[1], operand[2], operand[3]]) as usize;
                    if n < pipeline.len() {
                        pipeline = pipeline.split_off(n);
                    } else {
                        pipeline.clear();
                    }
                }
            }
            OP_COUNT => {
                if pipeline_active {
                    return make_count_result(pipeline.len() as i32);
                }
            }
            OP_ANY => {
                if pipeline_active {
                    let expr = parse_cmp_expr(operand);
                    let result = pipeline.iter().any(|p| compare_player(p, &expr));
                    return make_bool_result(result);
                }
            }
            OP_FIND => {
                if pipeline_active {
                    let expr = parse_cmp_expr(operand);
                    let found = pipeline.iter().find(|p| compare_player(p, &expr));
                    return make_optional_result(found);
                }
            }
            OP_SET => {
                if path_len >= 3 && path[0] == STATE_PLAYERS {
                    let idx = path[1] as usize;
                    let field = path[2];
                    if idx < state.players.len() && operand.len() >= 4 {
                        let val = i32::from_le_bytes([operand[0], operand[1], operand[2], operand[3]]);
                        state.players[idx].set_field(field, FieldValue::Int(val));
                    }
                }
                // 修改后继续处理后续指令（如 FILTER + EXEC）
            }
            OP_ADD => {
                if path_len >= 3 && path[0] == STATE_PLAYERS {
                    let idx = path[1] as usize;
                    let field = path[2];
                    if idx < state.players.len() && operand.len() >= 4 {
                        let delta = i32::from_le_bytes([operand[0], operand[1], operand[2], operand[3]]);
                        state.players[idx].add_field(field, delta);
                    }
                }
                // 修改后继续处理后续指令
            }
            OP_EXEC => {
                if pipeline_active {
                    let count_buf = write_varint(pipeline.len() as u32);
                    let players_buf: Vec<u8> = pipeline.iter().flat_map(|p| p.serialize()).collect();
                    let mut data = count_buf;
                    data.extend(players_buf);
                    return make_result(RESULT_VECTOR, &data);
                }
                return make_result(RESULT_UNDEF, &[]);
            }
            _ => {}
        }
    }

    make_result(RESULT_UNDEF, &[])
}

// ===== 结果构建 =====

fn make_result(result_type: u8, data: &[u8]) -> Vec<u8> {
    let mut buf = Vec::new();
    buf.push(result_type);
    buf.extend_from_slice(&u32le(data.len() as u32));
    buf.extend_from_slice(data);
    buf
}

fn make_count_result(count: i32) -> Vec<u8> {
    make_result(RESULT_COUNT, &i32le(count))
}

fn make_bool_result(val: bool) -> Vec<u8> {
    make_result(RESULT_BOOL, &[if val { 1 } else { 0 }])
}

fn make_optional_result(player: Option<&Player>) -> Vec<u8> {
    match player {
        Some(p) => {
            let mut data = vec![1u8];
            data.extend(p.serialize());
            make_result(RESULT_OPTIONAL, &data)
        }
        None => make_result(RESULT_OPTIONAL, &[0]),
    }
}

fn make_error_result(msg: &str) -> Vec<u8> {
    make_result(RESULT_ERROR, msg.as_bytes())
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

fn handle_client(mut stream: TcpStream, state: &mut GameState) {
    state.reset();
    loop {
        let query_data = match recv_with_length(&mut stream) {
            Ok(d) => d,
            Err(_) => break,
        };
        let result = execute_query(state, &query_data);
        if send_with_length(&mut stream, &result).is_err() {
            break;
        }
    }
}

// ===== 主程序 =====

fn main() -> std::io::Result<()> {
    println!("=== SPOI 全语言跨语言数据互查 — Rust 服务端 ===\n");

    let mut state = GameState::new();
    state.reset();

    println!("游戏状态已初始化：");
    println!("  服务器名称: {}", state.server_name);
    println!("  tick: {}", state.tick);
    println!("  玩家数: {}", state.players.len());
    for p in &state.players {
        println!("    {}: hp={} level={} gold={}", p.name, p.hp, p.level, p.gold);
    }

    let listener = TcpListener::bind("127.0.0.1:9999")?;
    println!("\n服务器正在监听 127.0.0.1:9999，等待客户端连接...");

    let mut client_num = 0u32;
    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                client_num += 1;
                let addr = stream.peer_addr().unwrap();
                println!("\n[客户端 #{}] 已连接 ({})", client_num, addr);
                handle_client(stream, &mut state);
                println!("[客户端 #{}] 已断开连接", client_num);
            }
            Err(e) => {
                eprintln!("接受连接失败: {}", e);
            }
        }
    }

    println!("\n服务器已关闭。");
    Ok(())
}