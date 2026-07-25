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