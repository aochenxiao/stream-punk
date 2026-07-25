# 示例 09：SPOI 全语言跨语言数据互查（Python 服务端）
# 展示：Python 服务端托管游戏状态数据，通过 TCP 接收各语言客户端发送的 SPOI 查询指令，
#       执行查询后将结果序列化返回。
#
# 协议格式说明：
#   TCP 帧: [数据长度: u32 LE][数据]
#   SpoiStream: [指令数: u32 LE][指令1][指令2]...
#   指令: [op: u8][路径长度: u32 LE][路径段: u32 LE * N][操作数长度: u32 LE][操作数字节]
#   SpoiCmpExpr: [字段索引: u32 LE][比较符: u8][值长度: u32 LE][值字节]
#   SpoiResult: [结果类型: u8][数据长度: u32 LE][数据字节]
#   CrossPlayer: [name长度: u32 LE][name UTF-8][hp: i32 LE][level: i32 LE][gold: i32 LE]

import struct
import socket
import sys
from typing import Optional


# =============================== 常量 ===============================

# 操作码
OP_SET     = 0x04
OP_ADD     = 0x05
OP_FILTER  = 0x0C
OP_SELECT  = 0x0D
OP_SORT    = 0x0E
OP_REVERSE = 0x0F
OP_TAKE    = 0x10
OP_DROP    = 0x11
OP_COUNT   = 0x15
OP_ANY     = 0x16
OP_ALL     = 0x17
OP_FIND    = 0x18
OP_EXEC    = 0x21

# 比较运算符
CMP_EQ = 0
CMP_NE = 1
CMP_LT = 2
CMP_GT = 3
CMP_LE = 4
CMP_GE = 5

# 字段索引 — CrossPlayer
PLAYER_NAME  = 0
PLAYER_HP    = 1
PLAYER_LEVEL = 2
PLAYER_GOLD  = 3

# 字段索引 — CrossGameState
STATE_PLAYERS    = 0
STATE_TICK       = 1
STATE_SERVERNAME = 2

# 结果类型
RESULT_UNDEF    = 0
RESULT_SINGLE   = 1
RESULT_VECTOR   = 2
RESULT_COUNT    = 3
RESULT_BOOL     = 4
RESULT_OPTIONAL = 5
RESULT_ERROR    = 6

HOST = "127.0.0.1"
PORT = 9999


# =============================== 辅助函数 ===============================

def u32le(v: int) -> bytes:
    return struct.pack("<I", v & 0xFFFFFFFF)

def i32le(v: int) -> bytes:
    return struct.pack("<i", v)

def read_u32le(data: bytes, offset: int) -> tuple[int, int]:
    """读取 u32 LE，返回 (值, 新偏移)"""
    return struct.unpack_from("<I", data, offset)[0], offset + 4

def read_i32le(data: bytes, offset: int) -> tuple[int, int]:
    """读取 i32 LE，返回 (值, 新偏移)"""
    return struct.unpack_from("<i", data, offset)[0], offset + 4

def read_u8(data: bytes, offset: int) -> tuple[int, int]:
    """读取 u8，返回 (值, 新偏移)"""
    return data[offset], offset + 1

def write_varint(v: int) -> bytes:
    """将非负整数编码为 varint"""
    result = []
    while v >= 0x80:
        result.append((v & 0x7F) | 0x80)
        v >>= 7
    result.append(v & 0x7F)
    return bytes(result)

def apply_cmp(a, op: int, b) -> bool:
    """执行比较操作"""
    if op == CMP_EQ: return a == b
    if op == CMP_NE: return a != b
    if op == CMP_LT: return a < b
    if op == CMP_GT: return a > b
    if op == CMP_LE: return a <= b
    if op == CMP_GE: return a >= b
    return False


# =============================== 游戏状态 ===============================

class Player:
    def __init__(self, name: str, hp: int, level: int, gold: int):
        self.name = name
        self.hp = hp
        self.level = level
        self.gold = gold

    def get_field(self, idx: int):
        if idx == PLAYER_NAME:  return self.name
        if idx == PLAYER_HP:    return self.hp
        if idx == PLAYER_LEVEL: return self.level
        if idx == PLAYER_GOLD:  return self.gold
        return None

    def set_field(self, idx: int, value):
        if idx == PLAYER_NAME:  self.name = value
        elif idx == PLAYER_HP:    self.hp = value
        elif idx == PLAYER_LEVEL: self.level = value
        elif idx == PLAYER_GOLD:  self.gold = value

    def add_field(self, idx: int, delta: int):
        if idx == PLAYER_HP:    self.hp += delta
        elif idx == PLAYER_LEVEL: self.level += delta
        elif idx == PLAYER_GOLD:  self.gold += delta

    def serialize(self) -> bytes:
        name_bytes = self.name.encode("utf-8")
        return u32le(len(name_bytes)) + name_bytes + i32le(self.hp) + i32le(self.level) + i32le(self.gold)


class GameState:
    def __init__(self):
        self.players: list[Player] = []
        self.tick = 42
        self.server_name = "PythonServer"

    def reset(self):
        self.players = [
            Player("Alice", 80, 10, 500),
            Player("Bob",   30, 5,  200),
            Player("Carol", 60, 8,  300),
            Player("Dave",  90, 12, 400),
            Player("Eve",   15, 3,  100),
        ]
        self.tick = 42
        self.server_name = "PythonServer"


# =============================== SPOI 指令解析与执行 ===============================

def parse_cmp_expr(data: bytes, offset: int) -> tuple[dict, int]:
    """解析 SpoiCmpExpr，返回 (表达式字典, 新偏移)"""
    member_idx, offset = read_u32le(data, offset)
    cmp_op, offset = read_u8(data, offset)
    value_len, offset = read_u32le(data, offset)
    value = data[offset:offset + value_len]
    offset += value_len
    return {"member_idx": member_idx, "cmp_op": cmp_op, "value": value}, offset


def parse_cmp_value_int(expr: dict) -> Optional[int]:
    """如果比较表达式的值是 i32 格式，返回该整数"""
    if len(expr["value"]) == 4:
        return struct.unpack("<i", expr["value"])[0]
    return None


def parse_cmp_value_str(expr: dict) -> Optional[str]:
    """如果比较表达式的值是 [u32 LE 长度][UTF-8] 格式，返回该字符串"""
    v = expr["value"]
    if len(v) >= 4:
        slen = struct.unpack("<I", v[:4])[0]
        if 4 + slen == len(v):
            return v[4:4 + slen].decode("utf-8")
    return None


def parse_cmp_value_i32(expr: dict) -> Optional[int]:
    """如果比较表达式的值是 i32 LE 格式，返回该整数"""
    if len(expr["value"]) == 4:
        return struct.unpack("<i", expr["value"])[0]
    return None


def compare_player(player: Player, expr: dict) -> bool:
    """判断玩家是否匹配比较表达式"""
    val = player.get_field(expr["member_idx"])
    if val is None:
        return False

    if isinstance(val, str):
        cmp_val = parse_cmp_value_str(expr)
        if cmp_val is None:
            return False
        return apply_cmp(val, expr["cmp_op"], cmp_val)
    elif isinstance(val, int):
        cmp_val = parse_cmp_value_i32(expr)
        if cmp_val is None:
            return False
        return apply_cmp(val, expr["cmp_op"], cmp_val)
    return False


def execute_query(state: GameState, query_data: bytes) -> bytes:
    """执行 SPOI 查询，返回结果字节"""
    try:
        if len(query_data) < 4:
            return make_error_result("查询数据太短")

        inst_count, offset = read_u32le(query_data, 0)

        # 当前管道数据（玩家列表）
        pipeline: list[Player] = []
        pipeline_active = False

        for _ in range(inst_count):
            if offset >= len(query_data):
                break

            op, offset = read_u8(query_data, offset)
            path_len, offset = read_u32le(query_data, offset)
            path = []
            for _ in range(path_len):
                seg, offset = read_u32le(query_data, offset)
                path.append(seg)
            operand_len, offset = read_u32le(query_data, offset)
            operand = query_data[offset:offset + operand_len]
            offset += operand_len

            if op == OP_FILTER:
                if not pipeline_active and len(path) == 1 and path[0] == STATE_PLAYERS:
                    # 从 players 开始管道
                    expr, _ = parse_cmp_expr(operand, 0)
                    pipeline = [p for p in state.players if compare_player(p, expr)]
                    pipeline_active = True
                elif pipeline_active:
                    # 在当前管道上过滤
                    expr, _ = parse_cmp_expr(operand, 0)
                    pipeline = [p for p in pipeline if compare_player(p, expr)]
                else:
                    # 直接路径过滤
                    expr, _ = parse_cmp_expr(operand, 0)
                    pipeline = [p for p in state.players if compare_player(p, expr)]
                    pipeline_active = True

            elif op == OP_SORT:
                if pipeline_active and len(operand) >= 5:
                    field = struct.unpack_from("<I", operand, 0)[0]
                    ascending = operand[4] != 0
                    pipeline.sort(key=lambda p: p.get_field(field) or 0, reverse=not ascending)

            elif op == OP_REVERSE:
                if pipeline_active:
                    pipeline.reverse()

            elif op == OP_TAKE:
                if pipeline_active and len(operand) >= 4:
                    n = struct.unpack_from("<I", operand, 0)[0]
                    pipeline = pipeline[:n]

            elif op == OP_DROP:
                if pipeline_active and len(operand) >= 4:
                    n = struct.unpack_from("<I", operand, 0)[0]
                    pipeline = pipeline[n:]

            elif op == OP_COUNT:
                if pipeline_active:
                    return make_count_result(len(pipeline))

            elif op == OP_ANY:
                if pipeline_active:
                    expr, _ = parse_cmp_expr(operand, 0)
                    result = any(compare_player(p, expr) for p in pipeline)
                    return make_bool_result(result)

            elif op == OP_FIND:
                if pipeline_active:
                    expr, _ = parse_cmp_expr(operand, 0)
                    for p in pipeline:
                        if compare_player(p, expr):
                            return make_optional_result(p)
                    return make_optional_result(None)

            elif op == OP_SET:
                # 写操作：path=[STATE_PLAYERS, index, field], operand=[i32 LE value]
                if len(path) >= 3 and path[0] == STATE_PLAYERS:
                    idx = path[1]
                    field = path[2]
                    if 0 <= idx < len(state.players) and len(operand) >= 4:
                        val = struct.unpack_from("<i", operand, 0)[0]
                        state.players[idx].set_field(field, val)
                # 修改后继续处理后续指令

            elif op == OP_ADD:
                # 写操作：path=[STATE_PLAYERS, index, field], operand=[i32 LE delta]
                if len(path) >= 3 and path[0] == STATE_PLAYERS:
                    idx = path[1]
                    field = path[2]
                    if 0 <= idx < len(state.players) and len(operand) >= 4:
                        delta = struct.unpack_from("<i", operand, 0)[0]
                        state.players[idx].add_field(field, delta)
                # 修改后继续处理后续指令

            elif op == OP_EXEC:
                # 执行：返回当前管道结果
                if pipeline_active:
                    # 序列化玩家列表
                    count_varint = write_varint(len(pipeline))
                    players_bytes = b"".join(p.serialize() for p in pipeline)
                    return make_result(RESULT_VECTOR, count_varint + players_bytes)
                else:
                    return make_result(RESULT_UNDEF, b"")

        return make_result(RESULT_UNDEF, b"")

    except Exception as e:
        return make_error_result(f"执行错误: {e}")


# =============================== 结果构建 ===============================

def make_result(result_type: int, data: bytes) -> bytes:
    """构建 SpoiResult 二进制"""
    return bytes([result_type]) + u32le(len(data)) + data

def make_count_result(count: int) -> bytes:
    return make_result(RESULT_COUNT, i32le(count))

def make_bool_result(val: bool) -> bytes:
    return make_result(RESULT_BOOL, b"\x01" if val else b"\x00")

def make_optional_result(player: Optional[Player]) -> bytes:
    if player is None:
        return make_result(RESULT_OPTIONAL, b"\x00")
    return make_result(RESULT_OPTIONAL, b"\x01" + player.serialize())

def make_single_result(player: Player) -> bytes:
    return make_result(RESULT_SINGLE, player.serialize())

def make_vector_result(players: list[Player]) -> bytes:
    data = write_varint(len(players)) + b"".join(p.serialize() for p in players)
    return make_result(RESULT_VECTOR, data)

def make_error_result(msg: str) -> bytes:
    return make_result(RESULT_ERROR, msg.encode("utf-8"))


# =============================== TCP 通信 ===============================

def recv_exact(sock: socket.socket, n: int) -> bytes:
    """精确接收 n 字节"""
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            return b""
        data += chunk
    return data


def handle_client(client_sock: socket.socket, state: GameState):
    """处理单个客户端连接"""
    state.reset()
    try:
        while True:
            # 接收长度前缀
            len_bytes = recv_exact(client_sock, 4)
            if not len_bytes:
                break
            data_len = struct.unpack("<I", len_bytes)[0]

            # 接收数据
            query_data = recv_exact(client_sock, data_len)
            if not query_data and data_len > 0:
                break

            # 执行查询
            result = execute_query(state, query_data)

            # 发送结果
            client_sock.sendall(u32le(len(result)) + result)
    except (ConnectionResetError, BrokenPipeError, OSError):
        pass
    finally:
        client_sock.close()


# =============================== 主程序 ===============================

def main():
    print("=== SPOI 全语言跨语言数据互查 — Python 服务端 ===\n")

    state = GameState()
    state.reset()

    print("游戏状态已初始化：")
    print(f"  服务器名称: {state.server_name}")
    print(f"  tick: {state.tick}")
    print(f"  玩家数: {len(state.players)}")
    for p in state.players:
        print(f"    {p.name}: hp={p.hp} level={p.level} gold={p.gold}")

    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind((HOST, PORT))
    server_sock.listen(5)

    print(f"\n服务器正在监听 {HOST}:{PORT}，等待客户端连接...")

    client_num = 0
    try:
        while True:
            client_sock, addr = server_sock.accept()
            client_num += 1
            print(f"\n[客户端 #{client_num}] 已连接 ({addr[0]}:{addr[1]})")
            handle_client(client_sock, state)
            print(f"[客户端 #{client_num}] 已断开连接")
    except KeyboardInterrupt:
        print("\n服务器正在关闭...")
    finally:
        server_sock.close()

    print("\n服务器已关闭。")


if __name__ == "__main__":
    main()