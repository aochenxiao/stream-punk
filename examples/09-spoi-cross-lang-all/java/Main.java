// 示例 08：SPOI 跨语言数据互查（Java 客户端）
// 展示：Java 客户端通过 TCP 向 C++ 服务器发送 SPOI 查询指令，
//       接收并展示查询结果。
//
// 协议格式说明：
//   SpoiStream: [指令数: u32 LE][指令1][指令2]...
//   指令: [op: u8][路径长度: u32 LE][路径段: u32 LE * N][操作数长度: u32 LE][操作数字节]
//   SpoiCmpExpr: [字段索引: u32 LE][比较符: u8][值长度: u32 LE][值字节]
//   SpoiResult: [结果类型: u8][数据长度: u32 LE][数据字节]
//   字符串: [长度: u32 LE][UTF-8 字节]
//   注意：结果是数据内部的元素计数使用 varint（来自 ReadPipeline::serialize）

import java.io.*;
import java.net.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.*;

// ===== SPOI 结果解析 =====
class ResultParser {
    static final int RESULT_UNDEF    = 0;
    static final int RESULT_SINGLE   = 1;
    static final int RESULT_VECTOR   = 2;
    static final int RESULT_COUNT    = 3;
    static final int RESULT_BOOL     = 4;
    static final int RESULT_OPTIONAL = 5;
    static final int RESULT_ERROR    = 6;

    // 解析从服务器返回的 SpoiResult 二进制数据
    // 格式：[resultType: u8][dataLen: u32 LE][data...]
    public static String parse(byte[] data) {
        if (data == null || data.length == 0) {
            return "(空结果)";
        }

        int resultType = data[0] & 0xFF;
        if (data.length < 5) {
            return "结果类型=" + resultType + " (数据太短)";
        }

        int dataLen = ByteBuffer.wrap(data, 1, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
        byte[] payload = Arrays.copyOfRange(data, 5, 5 + dataLen);

        switch (resultType) {
            case RESULT_COUNT: {
                // data 是 u32 LE 整数
                if (payload.length >= 4) {
                    int count = ByteBuffer.wrap(payload, 0, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
                    return "计数结果: " + count;
                }
                return "计数结果: (数据异常)";
            }
            case RESULT_BOOL: {
                boolean val = payload.length > 0 && payload[0] != 0;
                return "布尔结果: " + val;
            }
            case RESULT_VECTOR: {
                // 数据内部格式：[元素数: varint][元素1][元素2]...
                int[] vecOff = new int[]{0};
                int count = readVarint(payload, vecOff);
                StringBuilder sb = new StringBuilder();
                sb.append("向量结果: ").append(count).append(" 个元素\n");
                byte[] remaining = Arrays.copyOfRange(payload, vecOff[0], payload.length);
                List<String> players = parsePlayerList(remaining, count);
                for (int i = 0; i < players.size(); i++) {
                    sb.append("    [").append(i).append("] ").append(players.get(i)).append("\n");
                }
                return sb.toString().trim();
            }
            case RESULT_SINGLE: {
                String player = parseSinglePlayer(payload);
                return "单个结果: " + player;
            }
            case RESULT_OPTIONAL: {
                boolean hasVal = payload.length > 0 && payload[0] != 0;
                if (hasVal && payload.length > 1) {
                    String player = parseSinglePlayer(Arrays.copyOfRange(payload, 1, payload.length));
                    return "可选结果: 有值 → " + player;
                } else {
                    return "可选结果: 空";
                }
            }
            case RESULT_ERROR: {
                String errMsg = new String(payload, StandardCharsets.UTF_8);
                return "错误: " + errMsg;
            }
            default:
                return "未知结果类型: " + resultType + " (data=" + payload.length + " bytes)";
        }
    }

    // 读取 varint（仅用于结果数据内部的元素计数）
    private static int readVarint(byte[] data, int[] offset) {
        int result = 0;
        int shift = 0;
        int off = offset[0];
        while (off < data.length) {
            int b = data[off] & 0xFF;
            off++;
            result |= (b & 0x7F) << shift;
            if ((b & 0x80) == 0) {
                offset[0] = off;
                return result;
            }
            shift += 7;
        }
        offset[0] = off;
        return result;
    }

    // 解析单个玩家（CrossPlayer 序列化格式：name_len[u32 LE] + name_utf8 + hp[i32 LE] + level[i32 LE] + gold[i32 LE]）
    private static String parseSinglePlayer(byte[] data) {
        try {
            int[] off = new int[]{0};

            // 读取 name（string: [length: u32 LE][utf8 bytes]）
            if (off[0] + 4 > data.length) return "(数据太短)";
            int nameLen = ByteBuffer.wrap(data, off[0], 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
            off[0] += 4;
            if (off[0] + nameLen > data.length) return "(名字数据越界)";
            String name = new String(data, off[0], nameLen, StandardCharsets.UTF_8);
            off[0] += nameLen;

            // 读取 hp, level, gold（i32 LE）
            if (off[0] + 12 > data.length) return "(属性数据越界)";
            ByteBuffer bb = ByteBuffer.wrap(data, off[0], 12).order(ByteOrder.LITTLE_ENDIAN);
            int hp = bb.getInt();
            int level = bb.getInt();
            int gold = bb.getInt();

            return String.format("Player{name='%s', hp=%d, level=%d, gold=%d}", name, hp, level, gold);
        } catch (Exception e) {
            return "(解析失败: " + e.getMessage() + ")";
        }
    }

    // 解析玩家列表
    private static List<String> parsePlayerList(byte[] data, int count) {
        List<String> players = new ArrayList<>();
        int[] off = new int[]{0};
        for (int i = 0; i < count && off[0] < data.length; i++) {
            try {
                if (off[0] + 4 > data.length) break;
                int nameLen = ByteBuffer.wrap(data, off[0], 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
                off[0] += 4;
                if (off[0] + nameLen > data.length) break;
                String name = new String(data, off[0], nameLen, StandardCharsets.UTF_8);
                off[0] += nameLen;

                if (off[0] + 12 > data.length) break;
                ByteBuffer bb = ByteBuffer.wrap(data, off[0], 12).order(ByteOrder.LITTLE_ENDIAN);
                int hp = bb.getInt();
                int level = bb.getInt();
                int gold = bb.getInt();
                off[0] += 12;

                players.add(String.format("Player{name='%s', hp=%d, level=%d, gold=%d}", name, hp, level, gold));
            } catch (Exception e) {
                players.add("(解析失败: " + e.getMessage() + ")");
                break;
            }
        }
        return players;
    }
}

// ===== SPOI 查询 Builder =====
class SpoiBuilder {
    // 操作码
    static final int OP_FILTER   = 0x0C;
    static final int OP_SELECT   = 0x0D;
    static final int OP_SORT     = 0x0E;
    static final int OP_REVERSE  = 0x0F;
    static final int OP_TAKE     = 0x10;
    static final int OP_DROP     = 0x11;
    static final int OP_COUNT    = 0x15;
    static final int OP_ANY      = 0x16;
    static final int OP_ALL      = 0x17;
    static final int OP_FIND     = 0x18;
    static final int OP_KEYS     = 0x19;
    static final int OP_VALUES   = 0x1A;
    static final int OP_JOIN     = 0x1B;
    static final int OP_EXEC     = 0x21;
    // 写操作
    static final int OP_SET      = 0x04;
    static final int OP_ADD      = 0x05;

    // 比较运算符
    static final int CMP_EQ = 0;
    static final int CMP_NE = 1;
    static final int CMP_LT = 2;
    static final int CMP_GT = 3;
    static final int CMP_LE = 4;
    static final int CMP_GE = 5;

    // 字段索引
    static final int PLAYER_NAME  = 0;
    static final int PLAYER_HP    = 1;
    static final int PLAYER_LEVEL = 2;
    static final int PLAYER_GOLD  = 3;

    static final int STATE_PLAYERS    = 0;
    static final int STATE_TICK       = 1;
    static final int STATE_SERVERNAME = 2;

    // 指令列表
    private ArrayList<byte[]> instructions = new ArrayList<>();

    // 写入 u32 LE
    private static byte[] u32le(int v) {
        return ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array();
    }

    // 写入 i32 LE
    private static byte[] i32le(int v) {
        return ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array();
    }

    // 写入 u8
    private static byte[] u8(int v) {
        return new byte[]{(byte)(v & 0xFF)};
    }

    // 构建比较表达式操作数
    // SpoiCmpExpr 格式：[memberIdx: u32 LE][cmpOp: u8][valueLen: u32 LE][value bytes]
    private static byte[] buildCmpOperand(int fieldIdx, int cmpOp, byte[] value) {
        byte[] result = new byte[4 + 1 + 4 + value.length];
        System.arraycopy(u32le(fieldIdx), 0, result, 0, 4);
        result[4] = (byte)cmpOp;
        System.arraycopy(u32le(value.length), 0, result, 5, 4);
        System.arraycopy(value, 0, result, 9, value.length);
        return result;
    }

    // 构建带路径的指令
    // 指令格式：[op: u8][pathLen: u32 LE][path: u32 LE * N][operandLen: u32 LE][operand bytes]
    private void addInst(int op, int[] path, byte[] operand) {
        int pathBytes = 4 * path.length;
        byte[] result = new byte[1 + 4 + pathBytes + 4 + operand.length];
        result[0] = (byte)op;
        System.arraycopy(u32le(path.length), 0, result, 1, 4);
        for (int i = 0; i < path.length; i++) {
            System.arraycopy(u32le(path[i]), 0, result, 5 + i * 4, 4);
        }
        System.arraycopy(u32le(operand.length), 0, result, 5 + pathBytes, 4);
        System.arraycopy(operand, 0, result, 9 + pathBytes, operand.length);
        instructions.add(result);
    }

    // ===== 查询方法 =====

    // 管道入口：导航到 players 字段
    public SpoiBuilder fromPlayers() {
        addInst(OP_FILTER, new int[]{STATE_PLAYERS}, buildCmpOperand(PLAYER_HP, CMP_GE, i32le(0)));
        return this;
    }

    // filter: 按字段过滤
    public SpoiBuilder filter(int field, int cmpOp, int value) {
        addInst(OP_FILTER, new int[]{}, buildCmpOperand(field, cmpOp, i32le(value)));
        return this;
    }

    // filter: 按字符串字段过滤
    public SpoiBuilder filterStr(int field, int cmpOp, String value) {
        byte[] strBytes = value.getBytes(StandardCharsets.UTF_8);
        // 字符串序列化格式：[长度: u32 LE][UTF-8 字节]
        byte[] valBytes = new byte[4 + strBytes.length];
        System.arraycopy(u32le(strBytes.length), 0, valBytes, 0, 4);
        System.arraycopy(strBytes, 0, valBytes, 4, strBytes.length);
        addInst(OP_FILTER, new int[]{}, buildCmpOperand(field, cmpOp, valBytes));
        return this;
    }

    // sort: 按字段排序
    public SpoiBuilder sort(int field, boolean ascending) {
        byte[] operand = new byte[5];
        System.arraycopy(u32le(field), 0, operand, 0, 4);
        operand[4] = (byte)(ascending ? 1 : 0);
        addInst(OP_SORT, new int[]{}, operand);
        return this;
    }

    // reverse: 反转
    public SpoiBuilder reverse() {
        addInst(OP_REVERSE, new int[]{}, new byte[0]);
        return this;
    }

    // take: 取前 N 个
    public SpoiBuilder take(int n) {
        addInst(OP_TAKE, new int[]{}, u32le(n));
        return this;
    }

    // drop: 跳过前 N 个
    public SpoiBuilder drop(int n) {
        addInst(OP_DROP, new int[]{}, u32le(n));
        return this;
    }

    // count: 计数
    public SpoiBuilder count() {
        addInst(OP_COUNT, new int[]{}, new byte[0]);
        return this;
    }

    // any: 存在性检查
    public SpoiBuilder any(int field, int cmpOp, int value) {
        addInst(OP_ANY, new int[]{}, buildCmpOperand(field, cmpOp, i32le(value)));
        return this;
    }

    // find: 查找第一个匹配
    public SpoiBuilder find(int field, int cmpOp, int value) {
        addInst(OP_FIND, new int[]{}, buildCmpOperand(field, cmpOp, i32le(value)));
        return this;
    }

    // find: 按字符串查找
    public SpoiBuilder findStr(int field, String value) {
        byte[] strBytes = value.getBytes(StandardCharsets.UTF_8);
        byte[] valBytes = new byte[4 + strBytes.length];
        System.arraycopy(u32le(strBytes.length), 0, valBytes, 0, 4);
        System.arraycopy(strBytes, 0, valBytes, 4, strBytes.length);
        addInst(OP_FIND, new int[]{}, buildCmpOperand(field, CMP_EQ, valBytes));
        return this;
    }

    // set: 写操作 — 设置字段值
    public SpoiBuilder set(int[] path, int value) {
        addInst(OP_SET, path, i32le(value));
        return this;
    }

    // set: 写操作 — 设置字符串字段
    public SpoiBuilder setStr(int[] path, String value) {
        byte[] strBytes = value.getBytes(StandardCharsets.UTF_8);
        byte[] valBytes = new byte[4 + strBytes.length];
        System.arraycopy(u32le(strBytes.length), 0, valBytes, 0, 4);
        System.arraycopy(strBytes, 0, valBytes, 4, strBytes.length);
        addInst(OP_SET, path, valBytes);
        return this;
    }

    // add: 写操作 — 增量
    public SpoiBuilder add(int[] path, int delta) {
        addInst(OP_ADD, path, i32le(delta));
        return this;
    }

    // 构建完整的 SPOI 二进制数据
    // SpoiStream 格式：[指令数: u32 LE][指令1][指令2]...
    public byte[] build() {
        // 添加 exec 指令
        addInst(OP_EXEC, new int[]{}, new byte[0]);

        // 序列化所有指令
        int totalBytes = 4; // 指令数 u32 LE
        for (byte[] inst : instructions) {
            totalBytes += inst.length;
        }
        byte[] result = new byte[totalBytes];
        System.arraycopy(u32le(instructions.size()), 0, result, 0, 4);
        int offset = 4;
        for (byte[] inst : instructions) {
            System.arraycopy(inst, 0, result, offset, inst.length);
            offset += inst.length;
        }
        return result;
    }

    // 重置 builder
    public SpoiBuilder reset() {
        instructions.clear();
        return this;
    }
}

// ===== 主程序 =====
public class Main {
    private static final String HOST = "127.0.0.1";
    private static final int PORT = 9999;

    public static void main(String[] args) {
        System.out.println("=== SPOI 跨语言数据互查 — Java 客户端 ===\n");

        try (Socket socket = new Socket(HOST, PORT)) {
            System.out.println("已连接到服务器 " + HOST + ":" + PORT + "\n");

            DataOutputStream out = new DataOutputStream(socket.getOutputStream());
            DataInputStream in = new DataInputStream(socket.getInputStream());

            int testNum = 0;

            // ===== 查询 1: 统计玩家总数 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 统计玩家总数 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers().count();
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 2: 过滤 hp > 50 的玩家 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 过滤 hp > 50 的玩家 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers().filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 50);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 3: 过滤 level >= 8，取前 2 个 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 过滤 level >= 8，取前 2 个 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_LEVEL, SpoiBuilder.CMP_GE, 8)
                 .take(2);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 4: 查找名为 "Alice" 的玩家 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 查找名为 \"Alice\" 的玩家 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers().findStr(SpoiBuilder.PLAYER_NAME, "Alice");
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 5: 按 hp 降序排列，取前 3 个 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 按 hp 降序排列，取前 3 个 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .sort(SpoiBuilder.PLAYER_HP, false)  // 降序
                 .take(3);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 6: 检查是否有 hp < 20 的玩家 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 检查是否有 hp < 20 的玩家 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers().any(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_LT, 20);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 7: 检查是否所有玩家 hp > 0 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 检查是否所有玩家 hp > 0 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers();
                q.filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 0);
                q.count();
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 8: 复杂链式查询：filter + sort + reverse + take =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 复杂链式查询（filter + sort + reverse + take） ---");
            System.out.println("    (hp > 30 → 按 level 排序 → 反转 → 取前 2)");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 30)
                 .sort(SpoiBuilder.PLAYER_LEVEL, true)   // 按 level 升序
                 .reverse()                                // 反转 → 降序
                 .take(2);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 9: 写操作 — 修改玩家 hp =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 写操作 — 将玩家[0]的 hp 设置为 99 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.set(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_HP}, 99);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println("  写操作已执行（无返回结果）");
                System.out.println();
            }

            // ===== 查询 10: 验证写操作结果 — 查询玩家[0] =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 验证写操作 — 查找 Alice 的 hp 是否变为 99 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers().findStr(SpoiBuilder.PLAYER_NAME, "Alice");
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 11: 写操作 — 增加金币 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 写操作 — 给玩家[0]增加 100 金币 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.add(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_GOLD}, 100);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println("  写操作已执行（无返回结果）");
                System.out.println();
            }

            // ===== 查询 12: 验证金币增加 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 验证写操作 — 查找 Alice 的金币是否变为 600 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers().findStr(SpoiBuilder.PLAYER_NAME, "Alice");
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 13: filter + drop =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": filter(hp > 20) + drop(2) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 20)
                 .drop(2);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 【L1】多条件组合过滤 =====

            // ===== 查询 14: hp>30 AND level>5 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L1】hp>30 AND level>5 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 30)
                 .filter(SpoiBuilder.PLAYER_LEVEL, SpoiBuilder.CMP_GT, 5);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 15: hp>30 AND level<=5 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L1】hp>30 AND level<=5 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 30)
                 .filter(SpoiBuilder.PLAYER_LEVEL, SpoiBuilder.CMP_LE, 5);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 16: hp>20 AND level>3 AND gold>200 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L1】hp>20 AND level>3 AND gold>200 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 20)
                 .filter(SpoiBuilder.PLAYER_LEVEL, SpoiBuilder.CMP_GT, 3)
                 .filter(SpoiBuilder.PLAYER_GOLD, SpoiBuilder.CMP_GT, 200);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 【L2】边界条件 =====

            // ===== 查询 17: hp>9000 (空结果) =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L2】hp>9000 (空结果) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 9000);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 18: TAKE(100) =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L2】TAKE(100) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers().take(100);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 19: DROP(100) =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L2】DROP(100) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers().drop(100);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 20: DROP(100)+COUNT =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L2】DROP(100)+COUNT ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers().drop(100).count();
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 21: 空管道FIND =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L2】空管道FIND ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .find(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 9000);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 22: 空管道ANY =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L2】空管道ANY ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .any(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 9000);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 【L3】复杂管道 =====

            // ===== 查询 23: SORT(level,asc)+DROP(2)+TAKE(2) =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L3】SORT(level,asc)+DROP(2)+TAKE(2) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .sort(SpoiBuilder.PLAYER_LEVEL, true)
                 .drop(2)
                 .take(2);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 24: SORT(hp,desc)+REVERSE+DROP(1)+TAKE(2)+COUNT =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L3】SORT(hp,desc)+REVERSE+DROP(1)+TAKE(2)+COUNT ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .sort(SpoiBuilder.PLAYER_HP, false)
                 .reverse()
                 .drop(1)
                 .take(2)
                 .count();
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 25: SORT(level,asc)+REVERSE+DROP(1)+TAKE(3)+FILTER(hp>40) =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L3】SORT(level,asc)+REVERSE+DROP(1)+TAKE(3)+FILTER(hp>40) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .sort(SpoiBuilder.PLAYER_LEVEL, true)
                 .reverse()
                 .drop(1)
                 .take(3)
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 40);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 【L4】字符串操作 =====

            // ===== 查询 26: name NE "Alice" =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L4】name NE \"Alice\" ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filterStr(SpoiBuilder.PLAYER_NAME, SpoiBuilder.CMP_NE, "Alice");
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 27: name LT "Carol" =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L4】name LT \"Carol\" ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filterStr(SpoiBuilder.PLAYER_NAME, SpoiBuilder.CMP_LT, "Carol");
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 28: FIND "Zoe" =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L4】FIND \"Zoe\" ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .findStr(SpoiBuilder.PLAYER_NAME, "Zoe");
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 【L5】写后查询 =====

            // ===== 查询 29: SET hp=50, ADD hp=30, FIND Alice =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L5】SET hp=50, ADD hp=30, FIND Alice ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.set(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_HP}, 50)
                 .add(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_HP}, 30)
                 .fromPlayers()
                 .findStr(SpoiBuilder.PLAYER_NAME, "Alice");
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 30: SET Alice hp=999, SET Bob gold=9999, FILTER gold>9000 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L5】SET Alice hp=999, SET Bob gold=9999, FILTER gold>9000 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.set(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_HP}, 999)
                 .set(new int[]{SpoiBuilder.STATE_PLAYERS, 1, SpoiBuilder.PLAYER_GOLD}, 9999)
                 .fromPlayers()
                 .filter(SpoiBuilder.PLAYER_GOLD, SpoiBuilder.CMP_GT, 9000);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 31: ADD gold=-300, FIND Alice =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L5】ADD gold=-300, FIND Alice ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.add(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_GOLD}, -300)
                 .fromPlayers()
                 .findStr(SpoiBuilder.PLAYER_NAME, "Alice");
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 【L6】全比较运算符 =====

            // ===== 查询 32: hp EQ 60 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L6】hp EQ 60 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_EQ, 60);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 33: hp NE 60 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L6】hp NE 60 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_NE, 60);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 34: hp LT 60 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L6】hp LT 60 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_LT, 60);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 35: hp GT 60 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L6】hp GT 60 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 60);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 36: hp LE 60 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L6】hp LE 60 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_LE, 60);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 37: hp GE 60 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L6】hp GE 60 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GE, 60);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 【L7】极限链 =====

            // ===== 查询 38: fromPlayers + sort(level,true) + reverse() + drop(1) + take(4) + filter(hp>20) + sort(hp,false) + reverse() + take(2) + count() =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L7】极限链 ---");
            System.out.println("    (fromPlayers + sort(level,true) + reverse() + drop(1) + take(4) + filter(hp>20) + sort(hp,false) + reverse() + take(2) + count())");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .sort(SpoiBuilder.PLAYER_LEVEL, true)
                 .reverse()
                 .drop(1)
                 .take(4)
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 20)
                 .sort(SpoiBuilder.PLAYER_HP, false)
                 .reverse()
                 .take(2)
                 .count();
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 【L8】管道操作边缘情况 =====

            // ===== 查询 39: REVERSE x2 → 应与原始顺序相同 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L8】REVERSE x2 → 应与原始顺序相同 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .reverse()
                 .reverse();
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 40: TAKE(0) → 取0个元素（空向量） =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L8】TAKE(0) → 取0个元素（空向量） ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers().take(0);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 41: DROP(0) → 丢弃0个（应返回全部） =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L8】DROP(0) → 丢弃0个（应返回全部） ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers().drop(0);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 42: SORT 覆盖 → SORT(level,asc) + SORT(hp,desc)（以最后一次排序为准） =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L8】SORT覆盖 → SORT(level,asc)+SORT(hp,desc) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .sort(SpoiBuilder.PLAYER_LEVEL, true)
                 .sort(SpoiBuilder.PLAYER_HP, false);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 43: REVERSE x3 → 等同于单次 REVERSE =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L8】REVERSE x3 → 等同于单次 REVERSE ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .reverse()
                 .reverse()
                 .reverse();
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 44: DROP 到只剩 1 个 + TAKE(1) =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L8】DROP到只剩1个 + TAKE(1) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .drop(4)
                 .take(1);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 【L9】数值边界与极端值 =====

            // ===== 查询 45: FILTER hp < 0 → 无玩家 hp 为负 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L9】FILTER hp < 0 → 无玩家hp为负 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_LT, 0);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 46: SET hp=0, FILTER hp EQ 0 → 零值精确匹配 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L9】SET hp=0, FILTER hp EQ 0 → 零值精确匹配 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.set(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_HP}, 0)
                 .fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_EQ, 0);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 47: ADD 负值使金币变负, FILTER gold < 0 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L9】ADD负值使金币变负, FILTER gold < 0 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.add(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_GOLD}, -1000)
                 .fromPlayers()
                 .filter(SpoiBuilder.PLAYER_GOLD, SpoiBuilder.CMP_LT, 0);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 48: 互斥条件 → FILTER hp>0, FILTER hp<=0（必然空） =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L9】互斥条件 → hp>0 AND hp<=0（必然空） ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 0)
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_LE, 0);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 49: FILTER level = 0 → 不存在 level=0 的玩家 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L9】FILTER level = 0 → 不存在level=0的玩家 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_LEVEL, SpoiBuilder.CMP_EQ, 0);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 50: FILTER hp >= 0（全部通过） + COUNT =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L9】FILTER hp>=0（全部通过）+COUNT ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GE, 0)
                 .count();
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 【L10】写操作与管道混合 =====

            // ===== 查询 51: 多次 SET 后管道查询 → 改3个玩家hp，然后 FILTER + SORT =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L10】多次SET后管道查询 → 改3个玩家hp, FILTER+SORT ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.set(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_HP}, 45)
                 .set(new int[]{SpoiBuilder.STATE_PLAYERS, 1, SpoiBuilder.PLAYER_HP}, 55)
                 .set(new int[]{SpoiBuilder.STATE_PLAYERS, 2, SpoiBuilder.PLAYER_HP}, 65)
                 .fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 50)
                 .sort(SpoiBuilder.PLAYER_HP, false);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 52: SET + ADD 同一字段后查询 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L10】SET+ADD同一字段后查询 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.set(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_LEVEL}, 10)
                 .add(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_LEVEL}, -2)
                 .fromPlayers()
                 .findStr(SpoiBuilder.PLAYER_NAME, "Alice");
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 53: ADD 全部玩家 level+1, 然后 FILTER + COUNT =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L10】ADD全部玩家level+1, FILTER+COUNT ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.add(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_LEVEL}, 1)
                 .add(new int[]{SpoiBuilder.STATE_PLAYERS, 1, SpoiBuilder.PLAYER_LEVEL}, 1)
                 .add(new int[]{SpoiBuilder.STATE_PLAYERS, 2, SpoiBuilder.PLAYER_LEVEL}, 1)
                 .add(new int[]{SpoiBuilder.STATE_PLAYERS, 3, SpoiBuilder.PLAYER_LEVEL}, 1)
                 .add(new int[]{SpoiBuilder.STATE_PLAYERS, 4, SpoiBuilder.PLAYER_LEVEL}, 1)
                 .fromPlayers()
                 .filter(SpoiBuilder.PLAYER_LEVEL, SpoiBuilder.CMP_GT, 10)
                 .count();
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 54: SET 不存在索引 [0,99] → 应静默忽略，无玩家 hp>9000 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L10】SET不存在索引[0,99] → 应静默忽略 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.set(new int[]{SpoiBuilder.STATE_PLAYERS, 99, SpoiBuilder.PLAYER_HP}, 9999)
                 .fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 9000);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 55: 写入后管道操作 → SET hp=55, SORT hp, REVERSE, TAKE(2) =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L10】写入后管道操作 → SET hp=55, SORT hp, REVERSE, TAKE(2) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.set(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_HP}, 55)
                 .fromPlayers()
                 .sort(SpoiBuilder.PLAYER_HP, true)
                 .reverse()
                 .take(2);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 【L11】交叉字段查询 =====

            // ===== 查询 56: FILTER(hp>30) + SORT(gold) + ANY(level>8) =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L11】FILTER(hp>30)+SORT(gold)+ANY(level>8) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 30)
                 .sort(SpoiBuilder.PLAYER_GOLD, true)
                 .any(SpoiBuilder.PLAYER_LEVEL, SpoiBuilder.CMP_GT, 8);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 57: FILTER(gold>200) + FILTER(hp>50) + SORT(level) =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L11】FILTER(gold>200)+FILTER(hp>50)+SORT(level) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_GOLD, SpoiBuilder.CMP_GT, 200)
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 50)
                 .sort(SpoiBuilder.PLAYER_LEVEL, true);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 58: FILTER(gold>200) + SORT(hp) + FIND(level=12) =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L11】FILTER(gold>200)+SORT(hp)+FIND(level=12) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_GOLD, SpoiBuilder.CMP_GT, 200)
                 .sort(SpoiBuilder.PLAYER_HP, true)
                 .find(SpoiBuilder.PLAYER_LEVEL, SpoiBuilder.CMP_EQ, 12);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 59: FILTER(hp>50) + SORT(level) + REVERSE + ANY(gold>300) =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L11】FILTER(hp>50)+SORT(level)+REVERSE+ANY(gold>300) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 50)
                 .sort(SpoiBuilder.PLAYER_LEVEL, true)
                 .reverse()
                 .any(SpoiBuilder.PLAYER_GOLD, SpoiBuilder.CMP_GT, 300);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 60: 全字段三条件 → hp>25 AND level>4 AND gold>150 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L11】全字段三条件 → hp>25 AND level>4 AND gold>150 ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 25)
                 .filter(SpoiBuilder.PLAYER_LEVEL, SpoiBuilder.CMP_GT, 4)
                 .filter(SpoiBuilder.PLAYER_GOLD, SpoiBuilder.CMP_GT, 150);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 【L12】极限组合压力 =====

            // ===== 查询 61: 15步极限链 → SORT→REVERSE→DROP→TAKE→FILTER→SORT→REVERSE→TAKE→FILTER→SORT→REVERSE→DROP→TAKE→COUNT =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L12】15步极限链 ---");
            System.out.println("    (SORT→REVERSE→DROP→TAKE→FILTER→SORT→REVERSE→TAKE→FILTER→SORT→REVERSE→DROP→TAKE→COUNT)");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .sort(SpoiBuilder.PLAYER_LEVEL, true)
                 .reverse()
                 .drop(1)
                 .take(3)
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 20)
                 .sort(SpoiBuilder.PLAYER_HP, false)
                 .reverse()
                 .take(2)
                 .filter(SpoiBuilder.PLAYER_GOLD, SpoiBuilder.CMP_GT, 100)
                 .sort(SpoiBuilder.PLAYER_GOLD, true)
                 .reverse()
                 .drop(0)
                 .take(2)
                 .count();
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 62: 写入全部5个玩家, 然后复杂链查询 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L12】写入全部5个玩家, 复杂链查询 ---");
            System.out.println("    (SET 5个玩家hp → fromPlayers → FILTER hp>30 → SORT hp → REVERSE → TAKE(3))");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.set(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_HP}, 60)
                 .set(new int[]{SpoiBuilder.STATE_PLAYERS, 1, SpoiBuilder.PLAYER_HP}, 45)
                 .set(new int[]{SpoiBuilder.STATE_PLAYERS, 2, SpoiBuilder.PLAYER_HP}, 80)
                 .set(new int[]{SpoiBuilder.STATE_PLAYERS, 3, SpoiBuilder.PLAYER_HP}, 33)
                 .set(new int[]{SpoiBuilder.STATE_PLAYERS, 4, SpoiBuilder.PLAYER_HP}, 70)
                 .fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 30)
                 .sort(SpoiBuilder.PLAYER_HP, true)
                 .reverse()
                 .take(3);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 63: SORT+REVERSE 循环3次 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L12】SORT+REVERSE循环3次 ---");
            System.out.println("    (SORT(level,asc)→REVERSE→SORT(hp,desc)→REVERSE→SORT(gold,asc)→REVERSE)");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .sort(SpoiBuilder.PLAYER_LEVEL, true)
                 .reverse()
                 .sort(SpoiBuilder.PLAYER_HP, false)
                 .reverse()
                 .sort(SpoiBuilder.PLAYER_GOLD, true)
                 .reverse();
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 64: 过滤到单元素 + 全操作 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L12】过滤到单元素+全操作 → FILTER(hp>85)→SORT→REVERSE→DROP(0)→TAKE(1) ---");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 85)
                 .sort(SpoiBuilder.PLAYER_HP, true)
                 .reverse()
                 .drop(0)
                 .take(1);
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            // ===== 查询 65: 极限混合 =====
            testNum++;
            System.out.println("--- 查询 " + testNum + ": 【L12】极限混合 ---");
            System.out.println("    (SET hp=60→ADD gold=50→fromPlayers→FILTER hp>30→SORT level→REVERSE→DROP(1)→TAKE(3)→FILTER gold>100→SORT hp→REVERSE→TAKE(2)→COUNT)");
            {
                SpoiBuilder q = new SpoiBuilder();
                q.set(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_HP}, 60)
                 .add(new int[]{SpoiBuilder.STATE_PLAYERS, 0, SpoiBuilder.PLAYER_GOLD}, 50)
                 .fromPlayers()
                 .filter(SpoiBuilder.PLAYER_HP, SpoiBuilder.CMP_GT, 30)
                 .sort(SpoiBuilder.PLAYER_LEVEL, true)
                 .reverse()
                 .drop(1)
                 .take(3)
                 .filter(SpoiBuilder.PLAYER_GOLD, SpoiBuilder.CMP_GT, 100)
                 .sort(SpoiBuilder.PLAYER_HP, true)
                 .reverse()
                 .take(2)
                 .count();
                byte[] result = sendQuery(out, in, q.build());
                System.out.println(ResultParser.parse(result));
                System.out.println();
            }

            System.out.println("=== 所有查询完成 ===");

        } catch (ConnectException e) {
            System.err.println("无法连接到服务器 " + HOST + ":" + PORT);
            System.err.println("请确保 C++ 服务器已启动！");
            System.exit(1);
        } catch (IOException e) {
            System.err.println("通信错误: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }

    // 发送查询并接收结果
    private static byte[] sendQuery(DataOutputStream out, DataInputStream in, byte[] queryData) throws IOException {
        // 发送长度前缀（u32 LE）
        out.write(ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(queryData.length).array());
        out.write(queryData);
        out.flush();

        // 接收长度前缀（u32 LE）
        byte[] lenBytes = new byte[4];
        in.readFully(lenBytes);
        int respLen = ByteBuffer.wrap(lenBytes).order(ByteOrder.LITTLE_ENDIAN).getInt();

        // 接收结果数据
        byte[] resultData = new byte[respLen];
        in.readFully(resultData);
        return resultData;
    }
}