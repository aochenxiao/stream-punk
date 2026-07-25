# 示例 09：SPOI 全语言跨语言数据互查（Python 客户端）
# 展示：Python 客户端通过 TCP 向 C++ 服务器发送 SPOI 查询指令，接收并展示查询结果。
# 使用 Python 标准库（struct, socket）实现，不依赖第三方库。
#
# 协议格式说明：
#   TCP 帧: [数据长度: u32 LE][数据]
#   SpoiStream: [指令数: u32 LE][指令1][指令2]...
#   指令: [op: u8][路径长度: u32 LE][路径段: u32 LE * N][操作数长度: u32 LE][操作数字节]
#   SpoiCmpExpr: [字段索引: u32 LE][比较符: u8][值长度: u32 LE][值字节]
#   SpoiResult: [结果类型: u8][数据长度: u32 LE][数据字节]
#   字符串: [长度: u32 LE][UTF-8 字节]
#   注意：结果数据内部的元素计数使用 varint

import struct
import socket


# =============================== 常量 ===============================

# 操作码
OP_SET    = 0x04
OP_ADD    = 0x05
OP_FILTER = 0x0C
OP_SELECT = 0x0D
OP_SORT   = 0x0E
OP_REVERSE = 0x0F
OP_TAKE   = 0x10
OP_DROP   = 0x11
OP_COUNT  = 0x15
OP_ANY    = 0x16
OP_ALL    = 0x17
OP_FIND   = 0x18
OP_KEYS   = 0x19
OP_VALUES = 0x1A
OP_JOIN   = 0x1B
OP_EXEC   = 0x21

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

# 服务器连接
HOST = "127.0.0.1"
PORT = 9999


# =============================== 辅助函数 ===============================

def u32le(v: int) -> bytes:
    """将整数编码为 u32 小端字节序。"""
    return struct.pack("<I", v)


def i32le(v: int) -> bytes:
    """将整数编码为 i32 小端字节序。"""
    return struct.pack("<i", v)


def read_varint(data: bytes, offset: int) -> tuple:
    """从字节数组中读取 varint 编码的整数，返回 (值, 新偏移)。"""
    result = 0
    shift = 0
    pos = offset
    while pos < len(data):
        b = data[pos]
        pos += 1
        result |= (b & 0x7F) << shift
        if not (b & 0x80):
            return result, pos
        shift += 7
    return result, pos


# =============================== SPOI 查询构建器 ===============================

class SpoiBuilder:
    """SPOI 查询构建器，用于构建 SPOI 指令流的二进制数据。"""

    def __init__(self):
        self._instructions: list[bytes] = []

    def _build_cmp_operand(self, field_idx: int, cmp_op: int, value: bytes) -> bytes:
        """构建比较表达式操作数。
        SpoiCmpExpr 格式：[memberIdx: u32 LE][cmpOp: u8][valueLen: u32 LE][value bytes]
        """
        return u32le(field_idx) + bytes([cmp_op]) + u32le(len(value)) + value

    def _add_inst(self, op: int, path: list[int], operand: bytes) -> None:
        """添加一条指令。
        指令格式：[op: u8][pathLen: u32 LE][path: u32 LE * N][operandLen: u32 LE][operand bytes]
        """
        path_bytes = b"".join(u32le(seg) for seg in path)
        inst = (
            bytes([op])
            + u32le(len(path))
            + path_bytes
            + u32le(len(operand))
            + operand
        )
        self._instructions.append(inst)

    # ===== 查询方法 =====

    def from_players(self) -> "SpoiBuilder":
        """管道入口：导航到 players 字段。
        发送 FILTER 指令，路径=[STATE_PLAYERS]，操作数=比较表达式(memberIdx=PLAYER_HP, cmpOp=GE, value=i32le(0))
        """
        self._add_inst(OP_FILTER, [STATE_PLAYERS],
                       self._build_cmp_operand(PLAYER_HP, CMP_GE, i32le(0)))
        return self

    def filter(self, field: int, cmp_op: int, value: int) -> "SpoiBuilder":
        """按整数字段过滤。"""
        self._add_inst(OP_FILTER, [],
                       self._build_cmp_operand(field, cmp_op, i32le(value)))
        return self

    def filter_str(self, field: int, cmp_op: int, value: str) -> "SpoiBuilder":
        """按字符串字段过滤。字符串格式：[长度: u32 LE][UTF-8 字节]"""
        str_bytes = value.encode("utf-8")
        val_bytes = u32le(len(str_bytes)) + str_bytes
        self._add_inst(OP_FILTER, [],
                       self._build_cmp_operand(field, cmp_op, val_bytes))
        return self

    def sort(self, field: int, ascending: bool) -> "SpoiBuilder":
        """按字段排序。操作数格式：[field: u32 LE][ascending: u8]"""
        operand = u32le(field) + bytes([1 if ascending else 0])
        self._add_inst(OP_SORT, [], operand)
        return self

    def reverse(self) -> "SpoiBuilder":
        """反转结果顺序。"""
        self._add_inst(OP_REVERSE, [], b"")
        return self

    def take(self, n: int) -> "SpoiBuilder":
        """取前 N 个元素。"""
        self._add_inst(OP_TAKE, [], u32le(n))
        return self

    def drop(self, n: int) -> "SpoiBuilder":
        """跳过前 N 个元素。"""
        self._add_inst(OP_DROP, [], u32le(n))
        return self

    def count(self) -> "SpoiBuilder":
        """统计元素数量。"""
        self._add_inst(OP_COUNT, [], b"")
        return self

    def any(self, field: int, cmp_op: int, value: int) -> "SpoiBuilder":
        """检查是否存在满足条件的元素。"""
        self._add_inst(OP_ANY, [],
                       self._build_cmp_operand(field, cmp_op, i32le(value)))
        return self

    def find(self, field: int, cmp_op: int, value: int) -> "SpoiBuilder":
        """查找第一个匹配的元素（按整数比较）。"""
        self._add_inst(OP_FIND, [],
                       self._build_cmp_operand(field, cmp_op, i32le(value)))
        return self

    def find_str(self, field: int, value: str) -> "SpoiBuilder":
        """查找第一个匹配的元素（按字符串比较，默认 EQ）。"""
        str_bytes = value.encode("utf-8")
        val_bytes = u32le(len(str_bytes)) + str_bytes
        self._add_inst(OP_FIND, [],
                       self._build_cmp_operand(field, CMP_EQ, val_bytes))
        return self

    def set(self, path: list[int], value: int) -> "SpoiBuilder":
        """写操作：设置字段值。"""
        self._add_inst(OP_SET, path, i32le(value))
        return self

    def add(self, path: list[int], delta: int) -> "SpoiBuilder":
        """写操作：增加字段值。"""
        self._add_inst(OP_ADD, path, i32le(delta))
        return self

    def build(self) -> bytes:
        """构建完整的 SPOI 二进制数据。
        SpoiStream 格式：[指令数: u32 LE][指令1][指令2]...
        注意：自动在末尾追加 EXEC 指令。
        """
        self._add_inst(OP_EXEC, [], b"")
        return u32le(len(self._instructions)) + b"".join(self._instructions)


# =============================== SPOI 结果解析器 ===============================

class ResultParser:
    """SPOI 结果解析器，用于解析服务器返回的 SpoiResult 二进制数据。"""

    @staticmethod
    def _parse_single_player(data: bytes, offset: int) -> tuple:
        """解析单个玩家对象，返回 (描述字符串, 新偏移)。
        CrossPlayer 序列化格式：[nameLen: u32 LE][name UTF-8][hp: i32 LE][level: i32 LE][gold: i32 LE]
        """
        pos = offset
        # 读取 name
        name_len = struct.unpack_from("<I", data, pos)[0]
        pos += 4
        name = data[pos:pos + name_len].decode("utf-8")
        pos += name_len
        # 读取 hp, level, gold
        hp, level, gold = struct.unpack_from("<iii", data, pos)
        pos += 12
        return f"Player{{name='{name}', hp={hp}, level={level}, gold={gold}}}", pos

    @staticmethod
    def parse(result_data: bytes) -> str:
        """解析从服务器返回的 SpoiResult 二进制数据。
        格式：[resultType: u8][dataLen: u32 LE][data...]
        """
        if not result_data:
            return "(空结果)"

        if len(result_data) < 5:
            return f"结果类型={result_data[0]} (数据太短)"

        result_type = result_data[0]
        data_len = struct.unpack_from("<I", result_data, 1)[0]
        payload = result_data[5:5 + data_len]

        if result_type == RESULT_COUNT:
            # 数据是 u32 LE 整数
            if len(payload) >= 4:
                count = struct.unpack_from("<I", payload, 0)[0]
                return f"计数结果: {count}"
            return "计数结果: (数据异常)"

        elif result_type == RESULT_BOOL:
            val = len(payload) > 0 and payload[0] != 0
            return f"布尔结果: {val}"

        elif result_type == RESULT_VECTOR:
            # 数据内部格式：[元素数: varint][元素1][元素2]...
            count, pos = read_varint(payload, 0)
            lines = [f"向量结果: {count} 个元素"]
            for i in range(count):
                player_str, pos = ResultParser._parse_single_player(payload, pos)
                lines.append(f"    [{i}] {player_str}")
            return "\n".join(lines)

        elif result_type == RESULT_SINGLE:
            player_str, _ = ResultParser._parse_single_player(payload, 0)
            return f"单个结果: {player_str}"

        elif result_type == RESULT_OPTIONAL:
            has_val = len(payload) > 0 and payload[0] != 0
            if has_val and len(payload) > 1:
                player_str, _ = ResultParser._parse_single_player(payload, 1)
                return f"可选结果: 有值 → {player_str}"
            else:
                return "可选结果: 空"

        elif result_type == RESULT_ERROR:
            err_msg = payload.decode("utf-8")
            return f"错误: {err_msg}"

        else:
            return f"未知结果类型: {result_type} (data={len(payload)} bytes)"


# =============================== TCP 通信 ===============================

def send_query(sock: socket.socket, query_data: bytes) -> None:
    """发送带长度前缀的查询数据。"""
    sock.sendall(u32le(len(query_data)) + query_data)


def recv_result(sock: socket.socket) -> bytes:
    """接收带长度前缀的结果数据。"""
    # 接收 4 字节长度
    len_bytes = _recv_exact(sock, 4)
    if not len_bytes:
        return b""
    data_len = struct.unpack("<I", len_bytes)[0]
    # 接收数据
    return _recv_exact(sock, data_len)


def _recv_exact(sock: socket.socket, n: int) -> bytes:
    """精确接收 n 字节数据。"""
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            return b""
        data += chunk
    return data


# =============================== 主程序 ===============================

def main():
    print("=== SPOI 跨语言数据互查 — Python 客户端 ===\n")

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((HOST, PORT))
    except ConnectionRefusedError:
        print(f"无法连接到服务器 {HOST}:{PORT}")
        print("请确保 C++ 服务器已启动！")
        return

    print(f"已连接到服务器 {HOST}:{PORT}\n")

    test_num = 0

    # ===== 查询 1: 统计玩家总数 =====
    test_num += 1
    print(f"--- 查询 {test_num}: 统计玩家总数 ---")
    q = SpoiBuilder()
    q.from_players().count()
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 查询 2: 过滤 hp > 50 的玩家 =====
    test_num += 1
    print(f"--- 查询 {test_num}: 过滤 hp > 50 的玩家 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GT, 50)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 查询 3: 过滤 level >= 8，取前 2 个 =====
    test_num += 1
    print(f"--- 查询 {test_num}: 过滤 level >= 8，取前 2 个 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_LEVEL, CMP_GE, 8).take(2)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 查询 4: 查找名为 "Alice" 的玩家 =====
    test_num += 1
    print(f'--- 查询 {test_num}: 查找名为 "Alice" 的玩家 ---')
    q = SpoiBuilder()
    q.from_players().find_str(PLAYER_NAME, "Alice")
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 查询 5: 按 hp 降序排列，取前 3 个 =====
    test_num += 1
    print(f"--- 查询 {test_num}: 按 hp 降序排列，取前 3 个 ---")
    q = SpoiBuilder()
    q.from_players().sort(PLAYER_HP, False).take(3)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 查询 6: 检查是否有 hp < 20 的玩家 =====
    test_num += 1
    print(f"--- 查询 {test_num}: 检查是否有 hp < 20 的玩家 ---")
    q = SpoiBuilder()
    q.from_players().any(PLAYER_HP, CMP_LT, 20)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 查询 7: 统计 hp > 0 的玩家数 =====
    test_num += 1
    print(f"--- 查询 {test_num}: 统计 hp > 0 的玩家数 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GT, 0).count()
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 查询 8: 复杂链式查询 =====
    test_num += 1
    print(f"--- 查询 {test_num}: 复杂链式查询（filter + sort + reverse + take） ---")
    print("    (hp > 30 → 按 level 排序 → 反转 → 取前 2)")
    q = SpoiBuilder()
    q.from_players() \
     .filter(PLAYER_HP, CMP_GT, 30) \
     .sort(PLAYER_LEVEL, True) \
     .reverse() \
     .take(2)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 查询 9: 写操作 — 设置 hp =====
    test_num += 1
    print(f"--- 查询 {test_num}: 写操作 — 将玩家[0]的 hp 设置为 99 ---")
    q = SpoiBuilder()
    q.set([STATE_PLAYERS, 0, PLAYER_HP], 99)
    send_query(sock, q.build())
    _ = recv_result(sock)  # 必须读取响应，否则缓冲区错位
    print("  写操作已执行（无返回结果）")
    print()

    # ===== 查询 10: 验证写操作 =====
    test_num += 1
    print(f"--- 查询 {test_num}: 验证写操作 — 查找 Alice 的 hp 是否变为 99 ---")
    q = SpoiBuilder()
    q.from_players().find_str(PLAYER_NAME, "Alice")
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 查询 11: 写操作 — 增加金币 =====
    test_num += 1
    print(f"--- 查询 {test_num}: 写操作 — 给玩家[0]增加 100 金币 ---")
    q = SpoiBuilder()
    q.add([STATE_PLAYERS, 0, PLAYER_GOLD], 100)
    send_query(sock, q.build())
    _ = recv_result(sock)  # 必须读取响应，否则缓冲区错位
    print("  写操作已执行（无返回结果）")
    print()

    # ===== 查询 12: 验证金币增加 =====
    test_num += 1
    print(f"--- 查询 {test_num}: 验证写操作 — 查找 Alice 的金币是否变为 600 ---")
    q = SpoiBuilder()
    q.from_players().find_str(PLAYER_NAME, "Alice")
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 查询 13: filter + drop =====
    test_num += 1
    print(f"--- 查询 {test_num}: filter(hp > 20) + drop(2) ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GT, 20).drop(2)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 【L1】多条件组合过滤 =====

    # Q14: hp>30 AND level>5
    test_num += 1
    print(f"--- 查询 {test_num}: 【L1】hp>30 AND level>5 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GT, 30).filter(PLAYER_LEVEL, CMP_GT, 5)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q15: hp>30 AND level<=5
    test_num += 1
    print(f"--- 查询 {test_num}: 【L1】hp>30 AND level<=5 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GT, 30).filter(PLAYER_LEVEL, CMP_LE, 5)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q16: hp>20 AND level>3 AND gold>200
    test_num += 1
    print(f"--- 查询 {test_num}: 【L1】hp>20 AND level>3 AND gold>200 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GT, 20).filter(PLAYER_LEVEL, CMP_GT, 3).filter(PLAYER_GOLD, CMP_GT, 200)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 【L2】边界条件 =====

    # Q17: hp>9000 (空结果)
    test_num += 1
    print(f"--- 查询 {test_num}: 【L2】hp>9000（空结果） ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GT, 9000)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q18: TAKE(100)
    test_num += 1
    print(f"--- 查询 {test_num}: 【L2】TAKE(100) ---")
    q = SpoiBuilder()
    q.from_players().take(100)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q19: DROP(100)
    test_num += 1
    print(f"--- 查询 {test_num}: 【L2】DROP(100) ---")
    q = SpoiBuilder()
    q.from_players().drop(100)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q20: DROP(100)+COUNT
    test_num += 1
    print(f"--- 查询 {test_num}: 【L2】DROP(100)+COUNT ---")
    q = SpoiBuilder()
    q.from_players().drop(100).count()
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q21: 空管道FIND
    test_num += 1
    print(f"--- 查询 {test_num}: 【L2】空管道FIND ---")
    q = SpoiBuilder()
    q.find(PLAYER_HP, CMP_EQ, 50)
    send_query(sock, q.build())
    _ = recv_result(sock)
    print()
    print()

    # Q22: 空管道ANY
    test_num += 1
    print(f"--- 查询 {test_num}: 【L2】空管道ANY ---")
    q = SpoiBuilder()
    q.any(PLAYER_HP, CMP_GT, 0)
    send_query(sock, q.build())
    _ = recv_result(sock)
    print()
    print()

    # ===== 【L3】复杂管道 =====

    # Q23: SORT(level,asc)+DROP(2)+TAKE(2)
    test_num += 1
    print(f"--- 查询 {test_num}: 【L3】SORT(level,asc)+DROP(2)+TAKE(2) ---")
    q = SpoiBuilder()
    q.from_players().sort(PLAYER_LEVEL, True).drop(2).take(2)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q24: SORT(hp,desc)+REVERSE+DROP(1)+TAKE(2)+COUNT
    test_num += 1
    print(f"--- 查询 {test_num}: 【L3】SORT(hp,desc)+REVERSE+DROP(1)+TAKE(2)+COUNT ---")
    q = SpoiBuilder()
    q.from_players().sort(PLAYER_HP, False).reverse().drop(1).take(2).count()
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q25: SORT(level,asc)+REVERSE+DROP(1)+TAKE(3)+FILTER(hp>40)
    test_num += 1
    print(f"--- 查询 {test_num}: 【L3】SORT(level,asc)+REVERSE+DROP(1)+TAKE(3)+FILTER(hp>40) ---")
    q = SpoiBuilder()
    q.from_players().sort(PLAYER_LEVEL, True).reverse().drop(1).take(3).filter(PLAYER_HP, CMP_GT, 40)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 【L4】字符串操作 =====

    # Q26: name NE "Alice"
    test_num += 1
    print(f'--- 查询 {test_num}: 【L4】name NE "Alice" ---')
    q = SpoiBuilder()
    q.from_players().filter_str(PLAYER_NAME, CMP_NE, "Alice")
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q27: name LT "Carol"
    test_num += 1
    print(f'--- 查询 {test_num}: 【L4】name LT "Carol" ---')
    q = SpoiBuilder()
    q.from_players().filter_str(PLAYER_NAME, CMP_LT, "Carol")
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q28: FIND "Zoe"
    test_num += 1
    print(f'--- 查询 {test_num}: 【L4】FIND "Zoe" ---')
    q = SpoiBuilder()
    q.from_players().find_str(PLAYER_NAME, "Zoe")
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 【L5】写后查询 =====

    # Q29: SET hp=50, ADD hp=30, FIND Alice
    test_num += 1
    print(f"--- 查询 {test_num}: 【L5】SET hp=50, ADD hp=30, FIND Alice ---")
    q = SpoiBuilder()
    q.set([STATE_PLAYERS, 0, PLAYER_HP], 50)
    q.add([STATE_PLAYERS, 0, PLAYER_HP], 30)
    q.from_players().find_str(PLAYER_NAME, "Alice")
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q30: SET Alice hp=999, SET Bob gold=9999, FILTER gold>9000
    test_num += 1
    print(f"--- 查询 {test_num}: 【L5】SET Alice hp=999, SET Bob gold=9999, FILTER gold>9000 ---")
    q = SpoiBuilder()
    q.set([STATE_PLAYERS, 0, PLAYER_HP], 999)
    q.set([STATE_PLAYERS, 1, PLAYER_GOLD], 9999)
    q.from_players().filter(PLAYER_GOLD, CMP_GT, 9000)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q31: ADD gold=-300, FIND Alice
    test_num += 1
    print(f"--- 查询 {test_num}: 【L5】ADD gold=-300, FIND Alice ---")
    q = SpoiBuilder()
    q.add([STATE_PLAYERS, 0, PLAYER_GOLD], -300)
    q.from_players().find_str(PLAYER_NAME, "Alice")
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 【L6】全比较运算符 — hp=60 =====

    # Q32: hp EQ 60
    test_num += 1
    print(f"--- 查询 {test_num}: 【L6】hp EQ 60 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_EQ, 60)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q33: hp NE 60
    test_num += 1
    print(f"--- 查询 {test_num}: 【L6】hp NE 60 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_NE, 60)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q34: hp LT 60
    test_num += 1
    print(f"--- 查询 {test_num}: 【L6】hp LT 60 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_LT, 60)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q35: hp GT 60
    test_num += 1
    print(f"--- 查询 {test_num}: 【L6】hp GT 60 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GT, 60)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q36: hp LE 60
    test_num += 1
    print(f"--- 查询 {test_num}: 【L6】hp LE 60 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_LE, 60)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q37: hp GE 60
    test_num += 1
    print(f"--- 查询 {test_num}: 【L6】hp GE 60 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GE, 60)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 【L7】极限链 =====

    # Q38: from_players + sort(level,true) + reverse() + drop(1) + take(4) + filter(hp>20) + sort(hp,false) + reverse() + take(2) + count()
    test_num += 1
    print(f"--- 查询 {test_num}: 【L7】极限链 — sort(level,asc)+reverse+drop(1)+take(4)+filter(hp>20)+sort(hp,desc)+reverse+take(2)+count ---")
    q = SpoiBuilder()
    q.from_players() \
     .sort(PLAYER_LEVEL, True) \
     .reverse() \
     .drop(1) \
     .take(4) \
     .filter(PLAYER_HP, CMP_GT, 20) \
     .sort(PLAYER_HP, False) \
     .reverse() \
     .take(2) \
     .count()
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 【L8】管道操作边缘情况 =====

    # Q39: REVERSE x2 → 应与原始顺序相同
    test_num += 1
    print(f"--- 查询 {test_num}: 【L8】REVERSE x2 → 应与原始顺序相同 ---")
    q = SpoiBuilder()
    q.from_players().reverse().reverse()
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q40: TAKE(0) → 取0个元素（空向量）
    test_num += 1
    print(f"--- 查询 {test_num}: 【L8】TAKE(0) → 取0个元素（空向量） ---")
    q = SpoiBuilder()
    q.from_players().take(0)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q41: DROP(0) → 丢弃0个（应返回全部）
    test_num += 1
    print(f"--- 查询 {test_num}: 【L8】DROP(0) → 丢弃0个（应返回全部） ---")
    q = SpoiBuilder()
    q.from_players().drop(0)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q42: SORT 覆盖 → SORT(level,asc) + SORT(hp,desc)（以最后一次排序为准）
    test_num += 1
    print(f"--- 查询 {test_num}: 【L8】SORT覆盖 → SORT(level,asc)+SORT(hp,desc) ---")
    q = SpoiBuilder()
    q.from_players().sort(PLAYER_LEVEL, True).sort(PLAYER_HP, False)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q43: REVERSE x3 → 等同于单次 REVERSE
    test_num += 1
    print(f"--- 查询 {test_num}: 【L8】REVERSE x3 → 等同于单次 REVERSE ---")
    q = SpoiBuilder()
    q.from_players().reverse().reverse().reverse()
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q44: DROP 到只剩 1 个 + TAKE(1)
    test_num += 1
    print(f"--- 查询 {test_num}: 【L8】DROP到只剩1个 + TAKE(1) ---")
    q = SpoiBuilder()
    q.from_players().drop(4).take(1)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 【L9】数值边界与极端值 =====

    # Q45: FILTER hp < 0 → 无玩家 hp 为负
    test_num += 1
    print(f"--- 查询 {test_num}: 【L9】FILTER hp < 0 → 无玩家hp为负 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_LT, 0)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q46: SET hp=0, FILTER hp EQ 0 → 零值精确匹配
    test_num += 1
    print(f"--- 查询 {test_num}: 【L9】SET hp=0, FILTER hp EQ 0 → 零值精确匹配 ---")
    q = SpoiBuilder()
    q.set([STATE_PLAYERS, 0, PLAYER_HP], 0)
    q.from_players().filter(PLAYER_HP, CMP_EQ, 0)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q47: ADD 负值使金币变负, FILTER gold < 0
    test_num += 1
    print(f"--- 查询 {test_num}: 【L9】ADD负值使金币变负, FILTER gold < 0 ---")
    q = SpoiBuilder()
    q.add([STATE_PLAYERS, 0, PLAYER_GOLD], -1000)
    q.from_players().filter(PLAYER_GOLD, CMP_LT, 0)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q48: 互斥条件 → FILTER hp>0, FILTER hp<=0（必然空）
    test_num += 1
    print(f"--- 查询 {test_num}: 【L9】互斥条件 → hp>0 AND hp<=0（必然空） ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GT, 0).filter(PLAYER_HP, CMP_LE, 0)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q49: FILTER level = 0 → 不存在 level=0 的玩家
    test_num += 1
    print(f"--- 查询 {test_num}: 【L9】FILTER level = 0 → 不存在level=0的玩家 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_LEVEL, CMP_EQ, 0)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q50: FILTER hp >= 0（全部通过） + COUNT
    test_num += 1
    print(f"--- 查询 {test_num}: 【L9】FILTER hp>=0（全部通过）+COUNT ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GE, 0).count()
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 【L10】写操作与管道混合 =====

    # Q51: 多次 SET 后管道查询 → 改3个玩家hp，然后 FILTER + SORT
    test_num += 1
    print(f"--- 查询 {test_num}: 【L10】多次SET后管道查询 → 改3个玩家hp, FILTER+SORT ---")
    q = SpoiBuilder()
    q.set([STATE_PLAYERS, 0, PLAYER_HP], 45)
    q.set([STATE_PLAYERS, 1, PLAYER_HP], 55)
    q.set([STATE_PLAYERS, 2, PLAYER_HP], 65)
    q.from_players().filter(PLAYER_HP, CMP_GT, 50).sort(PLAYER_HP, False)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q52: SET + ADD 同一字段后查询
    test_num += 1
    print(f"--- 查询 {test_num}: 【L10】SET+ADD同一字段后查询 ---")
    q = SpoiBuilder()
    q.set([STATE_PLAYERS, 0, PLAYER_LEVEL], 10)
    q.add([STATE_PLAYERS, 0, PLAYER_LEVEL], -2)
    q.from_players().find_str(PLAYER_NAME, "Alice")
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q53: ADD 全部玩家 level+1, 然后 FILTER + COUNT
    test_num += 1
    print(f"--- 查询 {test_num}: 【L10】ADD全部玩家level+1, FILTER+COUNT ---")
    q = SpoiBuilder()
    q.add([STATE_PLAYERS, 0, PLAYER_LEVEL], 1)
    q.add([STATE_PLAYERS, 1, PLAYER_LEVEL], 1)
    q.add([STATE_PLAYERS, 2, PLAYER_LEVEL], 1)
    q.add([STATE_PLAYERS, 3, PLAYER_LEVEL], 1)
    q.add([STATE_PLAYERS, 4, PLAYER_LEVEL], 1)
    q.from_players().filter(PLAYER_LEVEL, CMP_GT, 10).count()
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q54: SET 不存在索引 [0,99] → 应静默忽略，无玩家 hp>9000
    test_num += 1
    print(f"--- 查询 {test_num}: 【L10】SET不存在索引[0,99] → 应静默忽略 ---")
    q = SpoiBuilder()
    q.set([STATE_PLAYERS, 99, PLAYER_HP], 9999)
    q.from_players().filter(PLAYER_HP, CMP_GT, 9000)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q55: 写入后管道操作 → SET hp=55, SORT hp, REVERSE, TAKE(2)
    test_num += 1
    print(f"--- 查询 {test_num}: 【L10】写入后管道操作 → SET hp=55, SORT hp, REVERSE, TAKE(2) ---")
    q = SpoiBuilder()
    q.set([STATE_PLAYERS, 0, PLAYER_HP], 55)
    q.from_players().sort(PLAYER_HP, True).reverse().take(2)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 【L11】交叉字段查询 =====

    # Q56: FILTER(hp>30) + SORT(gold) + ANY(level>8)
    test_num += 1
    print(f"--- 查询 {test_num}: 【L11】FILTER(hp>30)+SORT(gold)+ANY(level>8) ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GT, 30).sort(PLAYER_GOLD, True).any(PLAYER_LEVEL, CMP_GT, 8)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q57: FILTER(gold>200) + FILTER(hp>50) + SORT(level)
    test_num += 1
    print(f"--- 查询 {test_num}: 【L11】FILTER(gold>200)+FILTER(hp>50)+SORT(level) ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_GOLD, CMP_GT, 200).filter(PLAYER_HP, CMP_GT, 50).sort(PLAYER_LEVEL, True)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q58: FILTER(gold>200) + SORT(hp) + FIND(level=12)
    test_num += 1
    print(f"--- 查询 {test_num}: 【L11】FILTER(gold>200)+SORT(hp)+FIND(level=12) ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_GOLD, CMP_GT, 200).sort(PLAYER_HP, True).find(PLAYER_LEVEL, CMP_EQ, 12)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q59: FILTER(hp>50) + SORT(level) + REVERSE + ANY(gold>300)
    test_num += 1
    print(f"--- 查询 {test_num}: 【L11】FILTER(hp>50)+SORT(level)+REVERSE+ANY(gold>300) ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GT, 50).sort(PLAYER_LEVEL, True).reverse().any(PLAYER_GOLD, CMP_GT, 300)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q60: 全字段三条件 → hp>25 AND level>4 AND gold>150
    test_num += 1
    print(f"--- 查询 {test_num}: 【L11】全字段三条件 → hp>25 AND level>4 AND gold>150 ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GT, 25).filter(PLAYER_LEVEL, CMP_GT, 4).filter(PLAYER_GOLD, CMP_GT, 150)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # ===== 【L12】极限组合压力 =====

    # Q61: 15步极限链 → SORT→REVERSE→DROP→TAKE→FILTER→SORT→REVERSE→TAKE→FILTER→SORT→REVERSE→DROP→TAKE→COUNT
    test_num += 1
    print(f"--- 查询 {test_num}: 【L12】15步极限链 ---")
    print("    (SORT→REVERSE→DROP→TAKE→FILTER→SORT→REVERSE→TAKE→FILTER→SORT→REVERSE→DROP→TAKE→COUNT)")
    q = SpoiBuilder()
    q.from_players() \
     .sort(PLAYER_LEVEL, True) \
     .reverse() \
     .drop(1) \
     .take(3) \
     .filter(PLAYER_HP, CMP_GT, 20) \
     .sort(PLAYER_HP, False) \
     .reverse() \
     .take(2) \
     .filter(PLAYER_GOLD, CMP_GT, 100) \
     .sort(PLAYER_GOLD, True) \
     .reverse() \
     .drop(0) \
     .take(2) \
     .count()
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q62: 写入全部5个玩家, 然后复杂链查询
    test_num += 1
    print(f"--- 查询 {test_num}: 【L12】写入全部5个玩家, 复杂链查询 ---")
    print("    (SET 5个玩家hp → from_players → FILTER hp>30 → SORT hp → REVERSE → TAKE(3))")
    q = SpoiBuilder()
    q.set([STATE_PLAYERS, 0, PLAYER_HP], 60)
    q.set([STATE_PLAYERS, 1, PLAYER_HP], 45)
    q.set([STATE_PLAYERS, 2, PLAYER_HP], 80)
    q.set([STATE_PLAYERS, 3, PLAYER_HP], 33)
    q.set([STATE_PLAYERS, 4, PLAYER_HP], 70)
    q.from_players().filter(PLAYER_HP, CMP_GT, 30).sort(PLAYER_HP, True).reverse().take(3)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q63: SORT+REVERSE 循环3次
    test_num += 1
    print(f"--- 查询 {test_num}: 【L12】SORT+REVERSE循环3次 ---")
    print("    (SORT(level,asc)→REVERSE→SORT(hp,desc)→REVERSE→SORT(gold,asc)→REVERSE)")
    q = SpoiBuilder()
    q.from_players() \
     .sort(PLAYER_LEVEL, True) \
     .reverse() \
     .sort(PLAYER_HP, False) \
     .reverse() \
     .sort(PLAYER_GOLD, True) \
     .reverse()
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q64: 过滤到单元素 + 全操作
    test_num += 1
    print(f"--- 查询 {test_num}: 【L12】过滤到单元素+全操作 → FILTER(hp>85)→SORT→REVERSE→DROP(0)→TAKE(1) ---")
    q = SpoiBuilder()
    q.from_players().filter(PLAYER_HP, CMP_GT, 85).sort(PLAYER_HP, True).reverse().drop(0).take(1)
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    # Q65: 极限混合
    test_num += 1
    print(f"--- 查询 {test_num}: 【L12】极限混合 ---")
    print("    (SET hp=60→ADD gold=50→from_players→FILTER hp>30→SORT level→REVERSE→DROP(1)→TAKE(3)→FILTER gold>100→SORT hp→REVERSE→TAKE(2)→COUNT)")
    q = SpoiBuilder()
    q.set([STATE_PLAYERS, 0, PLAYER_HP], 60)
    q.add([STATE_PLAYERS, 0, PLAYER_GOLD], 50)
    q.from_players() \
     .filter(PLAYER_HP, CMP_GT, 30) \
     .sort(PLAYER_LEVEL, True) \
     .reverse() \
     .drop(1) \
     .take(3) \
     .filter(PLAYER_GOLD, CMP_GT, 100) \
     .sort(PLAYER_HP, True) \
     .reverse() \
     .take(2) \
     .count()
    send_query(sock, q.build())
    print(ResultParser.parse(recv_result(sock)))
    print()

    print("=== 所有查询完成 ===")

    sock.close()


if __name__ == "__main__":
    main()