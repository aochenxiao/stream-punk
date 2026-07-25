// 示例 09：SPOI 全语言跨语言数据互查（Java 服务端）
// 展示：Java 服务端托管游戏状态数据，通过 TCP 接收各语言客户端发送的 SPOI 查询指令，
//       执行查询后将结果序列化返回。

import java.io.*;
import java.net.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.function.Predicate;

// ===== 操作码 =====
class Op {
    static final int SET     = 0x04;
    static final int ADD     = 0x05;
    static final int FILTER  = 0x0C;
    static final int SORT    = 0x0E;
    static final int REVERSE = 0x0F;
    static final int TAKE    = 0x10;
    static final int DROP    = 0x11;
    static final int COUNT   = 0x15;
    static final int ANY     = 0x16;
    static final int FIND    = 0x18;
    static final int EXEC    = 0x21;
}

// ===== 比较运算符 =====
class Cmp {
    static final int EQ = 0, NE = 1, LT = 2, GT = 3, LE = 4, GE = 5;
}

// ===== 字段索引 =====
class Field {
    static final int PLAYER_NAME  = 0;
    static final int PLAYER_HP    = 1;
    static final int PLAYER_LEVEL = 2;
    static final int PLAYER_GOLD  = 3;
    static final int STATE_PLAYERS = 0;
}

// ===== 结果类型 =====
class Rt {
    static final int UNDEF = 0, SINGLE = 1, VECTOR = 2, COUNT = 3, BOOL = 4, OPTIONAL = 5, ERROR = 6;
}

// ===== 二进制辅助 =====
class Bin {
    static byte[] u32le(int v) {
        return ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array();
    }
    static byte[] i32le(int v) {
        return ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array();
    }
    static byte[] writeVarint(int v) {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        while (v >= 0x80) {
            bos.write((v & 0x7F) | 0x80);
            v >>>= 7;
        }
        bos.write(v & 0x7F);
        return bos.toByteArray();
    }
    static int readU32le(byte[] data, int off) {
        return ByteBuffer.wrap(data, off, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
    }
    static int readI32le(byte[] data, int off) {
        return ByteBuffer.wrap(data, off, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
    }
    static boolean applyCmpInt(int a, int cmpOp, int b) {
        switch (cmpOp) {
            case Cmp.EQ: return a == b;
            case Cmp.NE: return a != b;
            case Cmp.LT: return a < b;
            case Cmp.GT: return a > b;
            case Cmp.LE: return a <= b;
            case Cmp.GE: return a >= b;
            default: return false;
        }
    }
    static boolean applyCmpStr(String a, int cmpOp, String b) {
        int r = a.compareTo(b);
        switch (cmpOp) {
            case Cmp.EQ: return r == 0;
            case Cmp.NE: return r != 0;
            case Cmp.LT: return r < 0;
            case Cmp.GT: return r > 0;
            case Cmp.LE: return r <= 0;
            case Cmp.GE: return r >= 0;
            default: return false;
        }
    }
}

// ===== 比较表达式 =====
class CmpExpr {
    int memberIdx;
    int cmpOp;
    byte[] value;

    static CmpExpr parse(byte[] data) {
        CmpExpr e = new CmpExpr();
        e.memberIdx = Bin.readU32le(data, 0);
        e.cmpOp = data[4] & 0xFF;
        int valueLen = Bin.readU32le(data, 5);
        e.value = Arrays.copyOfRange(data, 9, 9 + valueLen);
        return e;
    }

    String getStrValue() {
        if (value.length >= 4) {
            int slen = Bin.readU32le(value, 0);
            if (4 + slen == value.length) {
                return new String(value, 4, slen, StandardCharsets.UTF_8);
            }
        }
        return null;
    }

    Integer getI32Value() {
        if (value.length == 4) return Bin.readI32le(value, 0);
        return null;
    }
}

// ===== 玩家 =====
class Player {
    String name;
    int hp, level, gold;

    Player(String name, int hp, int level, int gold) {
        this.name = name; this.hp = hp; this.level = level; this.gold = gold;
    }

    Object getField(int idx) {
        switch (idx) {
            case Field.PLAYER_NAME:  return name;
            case Field.PLAYER_HP:    return hp;
            case Field.PLAYER_LEVEL: return level;
            case Field.PLAYER_GOLD:  return gold;
            default: return null;
        }
    }

    void setField(int idx, Object val) {
        switch (idx) {
            case Field.PLAYER_NAME:  name = (String)val; break;
            case Field.PLAYER_HP:    hp = (Integer)val; break;
            case Field.PLAYER_LEVEL: level = (Integer)val; break;
            case Field.PLAYER_GOLD:  gold = (Integer)val; break;
        }
    }

    void addField(int idx, int delta) {
        switch (idx) {
            case Field.PLAYER_HP:    hp += delta; break;
            case Field.PLAYER_LEVEL: level += delta; break;
            case Field.PLAYER_GOLD:  gold += delta; break;
        }
    }

    byte[] serialize() {
        byte[] nameBytes = name.getBytes(StandardCharsets.UTF_8);
        byte[] result = new byte[4 + nameBytes.length + 12];
        System.arraycopy(Bin.u32le(nameBytes.length), 0, result, 0, 4);
        System.arraycopy(nameBytes, 0, result, 4, nameBytes.length);
        int off = 4 + nameBytes.length;
        System.arraycopy(Bin.i32le(hp), 0, result, off, 4);
        System.arraycopy(Bin.i32le(level), 0, result, off + 4, 4);
        System.arraycopy(Bin.i32le(gold), 0, result, off + 8, 4);
        return result;
    }
}

// ===== 游戏状态 =====
class GameState {
    List<Player> players = new ArrayList<>();
    int tick = 42;
    String serverName = "JavaServer";

    void reset() {
        players.clear();
        players.add(new Player("Alice", 80, 10, 500));
        players.add(new Player("Bob",   30, 5,  200));
        players.add(new Player("Carol", 60, 8,  300));
        players.add(new Player("Dave",  90, 12, 400));
        players.add(new Player("Eve",   15, 3,  100));
        tick = 42;
        serverName = "JavaServer";
    }

    boolean comparePlayer(Player p, CmpExpr expr) {
        Object val = p.getField(expr.memberIdx);
        if (val == null) return false;
        if (val instanceof String) {
            String cmpVal = expr.getStrValue();
            if (cmpVal == null) return false;
            return Bin.applyCmpStr((String)val, expr.cmpOp, cmpVal);
        } else if (val instanceof Integer) {
            Integer cmpVal = expr.getI32Value();
            if (cmpVal == null) return false;
            return Bin.applyCmpInt((Integer)val, expr.cmpOp, cmpVal);
        }
        return false;
    }
}

// ===== SPOI 执行器 =====
class SpoiExecutor {
    static byte[] execute(GameState state, byte[] queryData) {
        try {
            if (queryData.length < 4) return makeError("查询数据太短");

            int offset = 0;
            int instCount = Bin.readU32le(queryData, offset);
            offset += 4;

            List<Player> pipeline = new ArrayList<>();
            boolean pipelineActive = false;

            for (int i = 0; i < instCount; i++) {
                if (offset >= queryData.length) break;

                int op = queryData[offset++] & 0xFF;
                int pathLen = Bin.readU32le(queryData, offset); offset += 4;
                int[] path = new int[pathLen];
                for (int j = 0; j < pathLen; j++) {
                    path[j] = Bin.readU32le(queryData, offset);
                    offset += 4;
                }
                int operandLen = Bin.readU32le(queryData, offset); offset += 4;
                byte[] operand = Arrays.copyOfRange(queryData, offset, offset + operandLen);
                offset += operandLen;

                switch (op) {
                    case Op.FILTER: {
                        CmpExpr expr = CmpExpr.parse(operand);
                        if (!pipelineActive && pathLen == 1 && path[0] == Field.STATE_PLAYERS) {
                            for (Player p : state.players) {
                                if (state.comparePlayer(p, expr)) pipeline.add(p);
                            }
                            pipelineActive = true;
                        } else if (pipelineActive) {
                            List<Player> filtered = new ArrayList<>();
                            for (Player p : pipeline) {
                                if (state.comparePlayer(p, expr)) filtered.add(p);
                            }
                            pipeline = filtered;
                        } else {
                            for (Player p : state.players) {
                                if (state.comparePlayer(p, expr)) pipeline.add(p);
                            }
                            pipelineActive = true;
                        }
                        break;
                    }
                    case Op.SORT: {
                        if (pipelineActive && operand.length >= 5) {
                            final int field = Bin.readU32le(operand, 0);
                            final boolean ascending = operand[4] != 0;
                            pipeline.sort((a, b) -> {
                                int va = (Integer)a.getField(field);
                                int vb = (Integer)b.getField(field);
                                return ascending ? Integer.compare(va, vb) : Integer.compare(vb, va);
                            });
                        }
                        break;
                    }
                    case Op.REVERSE:
                        if (pipelineActive) Collections.reverse(pipeline);
                        break;
                    case Op.TAKE:
                        if (pipelineActive && operand.length >= 4) {
                            int n = Bin.readU32le(operand, 0);
                            if (n < pipeline.size()) pipeline = pipeline.subList(0, n);
                        }
                        break;
                    case Op.DROP:
                        if (pipelineActive && operand.length >= 4) {
                            int n = Bin.readU32le(operand, 0);
                            if (n < pipeline.size()) pipeline = pipeline.subList(n, pipeline.size());
                            else pipeline.clear();
                        }
                        break;
                    case Op.COUNT:
                        if (pipelineActive) return makeCount(pipeline.size());
                        break;
                    case Op.ANY: {
                        if (pipelineActive) {
                            CmpExpr expr = CmpExpr.parse(operand);
                            for (Player p : pipeline) {
                                if (state.comparePlayer(p, expr)) return makeBool(true);
                            }
                            return makeBool(false);
                        }
                        break;
                    }
                    case Op.FIND: {
                        if (pipelineActive) {
                            CmpExpr expr = CmpExpr.parse(operand);
                            for (Player p : pipeline) {
                                if (state.comparePlayer(p, expr)) return makeOptional(p);
                            }
                            return makeOptional(null);
                        }
                        break;
                    }
                    case Op.SET:
                        if (pathLen >= 3 && path[0] == Field.STATE_PLAYERS) {
                            int idx = path[1], field = path[2];
                            if (idx >= 0 && idx < state.players.size() && operand.length >= 4) {
                                int val = Bin.readI32le(operand, 0);
                                state.players.get(idx).setField(field, val);
                            }
                        }
                        // 修改后继续处理后续指令
                        break;
                    case Op.ADD:
                        if (pathLen >= 3 && path[0] == Field.STATE_PLAYERS) {
                            int idx = path[1], field = path[2];
                            if (idx >= 0 && idx < state.players.size() && operand.length >= 4) {
                                int delta = Bin.readI32le(operand, 0);
                                state.players.get(idx).addField(field, delta);
                            }
                        }
                        // 修改后继续处理后续指令
                        break;
                    case Op.EXEC:
                        if (pipelineActive) {
                            byte[] countBuf = Bin.writeVarint(pipeline.size());
                            ByteArrayOutputStream bos = new ByteArrayOutputStream();
                            try { bos.write(countBuf); } catch (Exception ignored) {}
                            for (Player p : pipeline) {
                                try { bos.write(p.serialize()); } catch (Exception ignored) {}
                            }
                            return makeResult(Rt.VECTOR, bos.toByteArray());
                        }
                        return makeResult(Rt.UNDEF, new byte[0]);
                }
            }
            return makeResult(Rt.UNDEF, new byte[0]);
        } catch (Exception e) {
            return makeError("执行错误: " + e.getMessage());
        }
    }

    static byte[] makeResult(int type, byte[] data) {
        byte[] result = new byte[5 + data.length];
        result[0] = (byte)type;
        System.arraycopy(Bin.u32le(data.length), 0, result, 1, 4);
        System.arraycopy(data, 0, result, 5, data.length);
        return result;
    }

    static byte[] makeCount(int count) { return makeResult(Rt.COUNT, Bin.i32le(count)); }
    static byte[] makeBool(boolean v) { return makeResult(Rt.BOOL, new byte[]{(byte)(v ? 1 : 0)}); }

    static byte[] makeOptional(Player p) {
        if (p == null) return makeResult(Rt.OPTIONAL, new byte[]{0});
        byte[] playerBytes = p.serialize();
        byte[] data = new byte[1 + playerBytes.length];
        data[0] = 1;
        System.arraycopy(playerBytes, 0, data, 1, playerBytes.length);
        return makeResult(Rt.OPTIONAL, data);
    }

    static byte[] makeError(String msg) {
        return makeResult(Rt.ERROR, msg.getBytes(StandardCharsets.UTF_8));
    }
}

// ===== TCP 通信 =====
class Tcp {
    static void sendWithLength(OutputStream out, byte[] data) throws IOException {
        out.write(Bin.u32le(data.length));
        out.write(data);
        out.flush();
    }

    static byte[] recvWithLength(InputStream in) throws IOException {
        byte[] lenBuf = new byte[4];
        int read = 0;
        while (read < 4) {
            int n = in.read(lenBuf, read, 4 - read);
            if (n == -1) throw new EOFException();
            read += n;
        }
        int len = Bin.readU32le(lenBuf, 0);
        byte[] data = new byte[len];
        read = 0;
        while (read < len) {
            int n = in.read(data, read, len - read);
            if (n == -1) throw new EOFException();
            read += n;
        }
        return data;
    }
}

// ===== 主程序 =====
public class Server {
    public static void main(String[] args) {
        System.out.println("=== SPOI 全语言跨语言数据互查 — Java 服务端 ===\n");

        GameState state = new GameState();
        state.reset();

        System.out.println("游戏状态已初始化：");
        System.out.println("  服务器名称: " + state.serverName);
        System.out.println("  tick: " + state.tick);
        System.out.println("  玩家数: " + state.players.size());
        for (Player p : state.players) {
            System.out.printf("    %s: hp=%d level=%d gold=%d%n", p.name, p.hp, p.level, p.gold);
        }

        try (ServerSocket serverSocket = new ServerSocket(9999, 50, InetAddress.getByName("127.0.0.1"))) {
            System.out.println("\n服务器正在监听 127.0.0.1:9999，等待客户端连接...");

            int clientNum = 0;
            while (true) {
                Socket client = serverSocket.accept();
                clientNum++;
                System.out.printf("%n[客户端 #%d] 已连接 (%s)%n", clientNum, client.getRemoteSocketAddress());
                handleClient(client, state);
                System.out.printf("[客户端 #%d] 已断开连接%n", clientNum);
            }
        } catch (IOException e) {
            System.err.println("服务器错误: " + e.getMessage());
        }
    }

    static void handleClient(Socket client, GameState state) {
        try {
            state.reset();
            InputStream in = client.getInputStream();
            OutputStream out = client.getOutputStream();

            while (true) {
                byte[] queryData = Tcp.recvWithLength(in);
                byte[] result = SpoiExecutor.execute(state, queryData);
                Tcp.sendWithLength(out, result);
            }
        } catch (EOFException e) {
            // 正常断开
        } catch (IOException e) {
            // 连接错误
        } finally {
            try { client.close(); } catch (IOException ignored) {}
        }
    }
}