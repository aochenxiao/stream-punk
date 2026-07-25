// 示例 09：SPOI 全语言跨语言数据互查（JavaScript/Node.js 客户端）
// 展示：Node.js 客户端通过 TCP 向 C++ 服务器发送 SPOI 查询指令，接收并展示查询结果。
// 使用 Node.js 标准库（net, Buffer）手工构建 SPOI 二进制协议。

'use strict';

const net = require('net');

// =============================== 常量 ===============================

// 操作码
const OP = {
    SET:     0x04,
    ADD:     0x05,
    FILTER:  0x0C,
    SELECT:  0x0D,
    SORT:    0x0E,
    REVERSE: 0x0F,
    TAKE:    0x10,
    DROP:    0x11,
    COUNT:   0x15,
    ANY:     0x16,
    ALL:     0x17,
    FIND:    0x18,
    KEYS:    0x19,
    VALUES:  0x1A,
    JOIN:    0x1B,
    EXEC:    0x21,
};

// 比较运算符
const CMP = {
    EQ: 0,
    NE: 1,
    LT: 2,
    GT: 3,
    LE: 4,
    GE: 5,
};

// 字段索引
const PLAYER_NAME  = 0;
const PLAYER_HP    = 1;
const PLAYER_LEVEL = 2;
const PLAYER_GOLD  = 3;

const STATE_PLAYERS    = 0;
const STATE_TICK       = 1;
const STATE_SERVERNAME = 2;

// 结果类型
const RESULT_TYPE = {
    UNDEF:    0,
    SINGLE:   1,
    VECTOR:   2,
    COUNT:    3,
    BOOL:     4,
    OPTIONAL: 5,
    ERROR:    6,
};

// =============================== 二进制辅助函数 ===============================

/** 写入 u32 LE 到 Buffer */
function writeU32LE(buf, offset, value) {
    buf.writeUInt32LE(value, offset);
    return offset + 4;
}

/** 写入 i32 LE 到 Buffer */
function writeI32LE(buf, offset, value) {
    buf.writeInt32LE(value, offset);
    return offset + 4;
}

/** 写入 u8 到 Buffer */
function writeU8(buf, offset, value) {
    buf.writeUInt8(value, offset);
    return offset + 1;
}

/** 创建 u32 LE 字节数组 */
function u32LE(value) {
    const buf = Buffer.allocUnsafe(4);
    buf.writeUInt32LE(value, 0);
    return buf;
}

/** 创建 i32 LE 字节数组 */
function i32LE(value) {
    const buf = Buffer.allocUnsafe(4);
    buf.writeInt32LE(value, 0);
    return buf;
}

/** 创建包含长度前缀的字节数组（vector<u8> 格式：[u32 LE 长度][数据]） */
function lengthPrefixed(data) {
    const lenBuf = u32LE(data.length);
    return Buffer.concat([lenBuf, data]);
}

/** 创建包含长度前缀的 u32 数组（vector<u32> 格式：[u32 LE 长度][u32 LE × N]） */
function vectorU32(arr) {
    const buf = Buffer.allocUnsafe(4 + arr.length * 4);
    buf.writeUInt32LE(arr.length, 0);
    for (let i = 0; i < arr.length; i++) {
        buf.writeUInt32LE(arr[i], 4 + i * 4);
    }
    return buf;
}

/** 读取 varint（用于 vector 结果内部元素计数） */
function readVarint(buf, offset) {
    let result = 0;
    let shift = 0;
    while (offset < buf.length) {
        const b = buf[offset++];
        result |= (b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    return { value: result, nextOffset: offset };
}

// =============================== SPOI 查询构建器 ===============================

/**
 * 构建比较表达式：[memberIdx: u32 LE][cmpOp: u8][value长度: u32 LE][value字节]
 */
function cmpExprBytes(memberIdx, cmpOp, value) {
    const buf = Buffer.allocUnsafe(4 + 1 + 4 + value.length);
    let offset = 0;
    offset = writeU32LE(buf, offset, memberIdx);
    offset = writeU8(buf, offset, cmpOp);
    offset = writeU32LE(buf, offset, value.length);
    buf.set(value, offset);
    return buf;
}

/**
 * 构建单条指令：
 * [op: u8][路径长度: u32 LE][路径段: u32 LE × N][操作数长度: u32 LE][操作数字节]
 */
function buildInstruction(op, path, operand) {
    const pathBuf = vectorU32(path);
    const operandLenBuf = u32LE(operand.length);
    const header = Buffer.allocUnsafe(1 + pathBuf.length);
    let offset = 0;
    offset = writeU8(header, offset, op);
    pathBuf.copy(header, offset);
    return Buffer.concat([header, operandLenBuf, operand]);
}

class SpoiQueryBuilder {
    constructor() {
        this.instructions = [];
    }

    /** 添加指令 */
    addInst(op, path, operand) {
        this.instructions.push(buildInstruction(op, path, operand));
    }

    /** 从 players 开始管道：FILTER path=[0] operand=cmp(PLAYER_HP=1, GE=5, value=i32le(0)) */
    fromPlayers() {
        this.addInst(OP.FILTER, [STATE_PLAYERS], cmpExprBytes(PLAYER_HP, CMP.GE, i32LE(0)));
        return this;
    }

    /** filter 操作 */
    filter(field, cmpOp, value) {
        this.addInst(OP.FILTER, [], cmpExprBytes(field, cmpOp, i32LE(value)));
        return this;
    }

    /** filter by string */
    filterStr(field, cmpOp, value) {
        const strBytes = Buffer.from(value, 'utf-8');
        // 字符串值需要包裹 [长度: u32 LE][UTF-8字节] 格式
        const valBytes = Buffer.concat([u32LE(strBytes.length), strBytes]);
        this.addInst(OP.FILTER, [], cmpExprBytes(field, cmpOp, valBytes));
        return this;
    }

    /** sort 操作：operand = [field: u32 LE][ascending: u8] */
    sort(field, ascending) {
        const operand = Buffer.allocUnsafe(4 + 1);
        writeU32LE(operand, 0, field);
        writeU8(operand, 4, ascending ? 1 : 0);
        this.addInst(OP.SORT, [], operand);
        return this;
    }

    /** reverse 操作 */
    reverse() {
        this.addInst(OP.REVERSE, [], Buffer.alloc(0));
        return this;
    }

    /** take 操作：operand = [n: u32 LE] */
    take(n) {
        this.addInst(OP.TAKE, [], u32LE(n));
        return this;
    }

    /** drop 操作：operand = [n: u32 LE] */
    drop(n) {
        this.addInst(OP.DROP, [], u32LE(n));
        return this;
    }

    /** count 操作 */
    count() {
        this.addInst(OP.COUNT, [], Buffer.alloc(0));
        return this;
    }

    /** any 操作 */
    any(field, cmpOp, value) {
        this.addInst(OP.ANY, [], cmpExprBytes(field, cmpOp, i32LE(value)));
        return this;
    }

    /** find 操作 */
    find(field, cmpOp, value) {
        this.addInst(OP.FIND, [], cmpExprBytes(field, cmpOp, i32LE(value)));
        return this;
    }

    /** find by string */
    findStr(field, value) {
        const strBytes = Buffer.from(value, 'utf-8');
        // 字符串值需要包裹 [长度: u32 LE][UTF-8字节] 格式
        const valBytes = Buffer.concat([u32LE(strBytes.length), strBytes]);
        this.addInst(OP.FIND, [], cmpExprBytes(field, CMP.EQ, valBytes));
        return this;
    }

    /** set 操作：operand = [value: i32 LE] */
    set(path, value) {
        this.addInst(OP.SET, path, i32LE(value));
        return this;
    }

    /** add 操作：operand = [delta: i32 LE] */
    add(path, delta) {
        this.addInst(OP.ADD, path, i32LE(delta));
        return this;
    }

    /** 构建二进制 SpoiStream：末尾追加 EXEC 指令 */
    build() {
        this.addInst(OP.EXEC, [], Buffer.alloc(0));
        // SpoiStream 格式：[指令数: u32 LE][指令1][指令2]...
        const countBuf = u32LE(this.instructions.length);
        return Buffer.concat([countBuf, ...this.instructions]);
    }
}

// =============================== 结果解析 ===============================

/**
 * 解析单个 CrossPlayer：[name长度: u32 LE][name UTF-8][hp: i32 LE][level: i32 LE][gold: i32 LE]
 * 返回 { player, nextOffset }
 */
function parsePlayer(buf, offset) {
    const nameLen = buf.readUInt32LE(offset);
    offset += 4;
    const name = buf.toString('utf-8', offset, offset + nameLen);
    offset += nameLen;
    const hp = buf.readInt32LE(offset);
    offset += 4;
    const level = buf.readInt32LE(offset);
    offset += 4;
    const gold = buf.readInt32LE(offset);
    offset += 4;
    return { player: { name, hp, level, gold }, nextOffset: offset };
}

/** 解析并打印单个 SpoiResult */
function printResult(data) {
    if (data.length === 0) {
        console.log('(空结果)');
        return;
    }

    // SpoiResult 格式：[resultType: u8][data长度: u32 LE][data字节]
    let offset = 0;
    const resultType = data.readUInt8(offset);
    offset += 1;
    const dataLen = data.readUInt32LE(offset);
    offset += 4;
    const innerData = data.slice(offset, offset + dataLen);

    switch (resultType) {
        case RESULT_TYPE.COUNT: {
            // count 内部：[count: u32 LE] 4 字节
            if (innerData.length >= 4) {
                const count = innerData.readUInt32LE(0);
                console.log('计数结果: ' + count);
            }
            break;
        }
        case RESULT_TYPE.BOOL: {
            // bool 内部：[0x00 或 0x01] 1 字节
            console.log('布尔结果: ' + (innerData[0] ? 'true' : 'false'));
            break;
        }
        case RESULT_TYPE.VECTOR: {
            // vector 内部：[元素数: varint][元素1][元素2]...
            let innerOffset = 0;
            const { value: count, nextOffset } = readVarint(innerData, innerOffset);
            innerOffset = nextOffset;
            console.log('向量结果: ' + count + ' 个元素');
            for (let i = 0; i < count; i++) {
                const { player, nextOffset: no } = parsePlayer(innerData, innerOffset);
                innerOffset = no;
                console.log('    [' + i + '] Player{name=\'' + player.name + '\', hp=' + player.hp +
                    ', level=' + player.level + ', gold=' + player.gold + '}');
            }
            break;
        }
        case RESULT_TYPE.SINGLE: {
            // single 内部：[元素字节]
            const { player } = parsePlayer(innerData, 0);
            console.log('单个结果: Player{name=\'' + player.name + '\', hp=' + player.hp +
                ', level=' + player.level + ', gold=' + player.gold + '}');
            break;
        }
        case RESULT_TYPE.OPTIONAL: {
            // optional 内部：[has_value: u8][元素字节 若有值]
            if (innerData.length > 0 && innerData[0] !== 0) {
                const { player } = parsePlayer(innerData, 1);
                console.log('可选结果: 有值 → Player{name=\'' + player.name + '\', hp=' + player.hp +
                    ', level=' + player.level + ', gold=' + player.gold + '}');
            } else {
                console.log('可选结果: 空');
            }
            break;
        }
        case RESULT_TYPE.ERROR: {
            // error 内部：[utf8 字符串]
            const errMsg = innerData.toString('utf-8');
            console.log('错误: ' + errMsg);
            break;
        }
        default:
            console.log('未知结果类型: ' + resultType);
            break;
    }
}

// =============================== TCP 通信 ===============================

function sendWithLength(socket, data) {
    const lenBuf = u32LE(data.length);
    socket.write(Buffer.concat([lenBuf, data]));
}

/**
 * 从 socket 读取一条长度前缀帧
 * 使用持久化缓冲区确保不丢失跨 chunk 的数据
 */
function recvWithLength(socket) {
    return new Promise((resolve, reject) => {
        // 使用闭包持久化缓冲区
        if (!socket._recvBuffer) {
            socket._recvBuffer = Buffer.alloc(0);
            socket._recvPending = [];

            socket.on('data', (chunk) => {
                socket._recvBuffer = Buffer.concat([socket._recvBuffer, chunk]);

                // 尝试满足所有待处理的读取请求
                while (socket._recvPending.length > 0) {
                    if (socket._recvBuffer.length < 4) break;

                    const expectedLength = socket._recvBuffer.readUInt32LE(0);
                    const totalNeeded = 4 + expectedLength;

                    if (socket._recvBuffer.length < totalNeeded) break;

                    const data = socket._recvBuffer.slice(4, totalNeeded);
                    socket._recvBuffer = socket._recvBuffer.slice(totalNeeded);

                    const pending = socket._recvPending.shift();
                    pending.resolve(data);
                }
            });

            socket.on('error', (err) => {
                while (socket._recvPending.length > 0) {
                    socket._recvPending.shift().reject(err);
                }
            });

            socket.on('close', () => {
                while (socket._recvPending.length > 0) {
                    socket._recvPending.shift().resolve(Buffer.alloc(0));
                }
            });
        }

        // 检查缓冲区中是否已有完整消息
        if (socket._recvBuffer.length >= 4) {
            const expectedLength = socket._recvBuffer.readUInt32LE(0);
            const totalNeeded = 4 + expectedLength;
            if (socket._recvBuffer.length >= totalNeeded) {
                const data = socket._recvBuffer.slice(4, totalNeeded);
                socket._recvBuffer = socket._recvBuffer.slice(totalNeeded);
                resolve(data);
                return;
            }
        }

        // 缓冲区中数据不足，加入等待队列
        socket._recvPending.push({ resolve, reject });
    });
}

// =============================== 主程序 ===============================

async function main() {
    console.log('=== SPOI 跨语言数据互查 — JavaScript/Node.js 客户端 ===');
    console.log('');

    const HOST = '127.0.0.1';
    const PORT = 9999;

    const socket = new net.Socket();

    await new Promise((resolve, reject) => {
        socket.connect(PORT, HOST, () => {
            console.log('已连接到服务器 ' + HOST + ':' + PORT);
            console.log('');
            resolve();
        });
        socket.on('error', (err) => {
            console.error('无法连接到服务器 ' + HOST + ':' + PORT);
            console.error('请确保 C++ 服务器已启动！');
            console.error(err.message);
            reject(err);
        });
    });

    let testNum = 0;

    // 查询 1: 统计玩家总数
    console.log('--- 查询 ' + (++testNum) + ': 统计玩家总数 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().count().build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 2: 过滤 hp > 50
    console.log('--- 查询 ' + (++testNum) + ': 过滤 hp > 50 的玩家 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 50).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 3: 过滤 level >= 8，取前 2 个
    console.log('--- 查询 ' + (++testNum) + ': 过滤 level >= 8，取前 2 个 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_LEVEL, CMP.GE, 8).take(2).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 4: 查找名为 "Alice" 的玩家
    console.log('--- 查询 ' + (++testNum) + ': 查找名为 "Alice" 的玩家 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().findStr(PLAYER_NAME, 'Alice').build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 5: 按 hp 降序排列，取前 3 个
    console.log('--- 查询 ' + (++testNum) + ': 按 hp 降序排列，取前 3 个 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().sort(PLAYER_HP, false).take(3).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 6: 检查是否有 hp < 20 的玩家
    console.log('--- 查询 ' + (++testNum) + ': 检查是否有 hp < 20 的玩家 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().any(PLAYER_HP, CMP.LT, 20).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 7: 统计 hp > 0 的玩家数
    console.log('--- 查询 ' + (++testNum) + ': 统计 hp > 0 的玩家数 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 0).count().build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 8: 复杂链式查询
    console.log('--- 查询 ' + (++testNum) + ': 复杂链式查询（filter + sort + reverse + take） ---');
    console.log('    (hp > 30 → 按 level 排序 → 反转 → 取前 2)');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers()
            .filter(PLAYER_HP, CMP.GT, 30)
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .take(2).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 9: 写操作 — 设置 hp
    console.log('--- 查询 ' + (++testNum) + ': 写操作 — 将玩家[0]的 hp 设置为 99 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.set([0, 0, PLAYER_HP], 99).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 10: 验证写操作
    console.log('--- 查询 ' + (++testNum) + ': 验证写操作 — 查找 Alice 的 hp 是否变为 99 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().findStr(PLAYER_NAME, 'Alice').build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 11: 写操作 — 增加金币
    console.log('--- 查询 ' + (++testNum) + ': 写操作 — 给玩家[0]增加 100 金币 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.add([0, 0, PLAYER_GOLD], 100).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 12: 验证金币增加
    console.log('--- 查询 ' + (++testNum) + ': 验证写操作 — 查找 Alice 的金币是否变为 600 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().findStr(PLAYER_NAME, 'Alice').build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 13: filter + drop
    console.log('--- 查询 ' + (++testNum) + ': filter(hp > 20) + drop(2) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 20).drop(2).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // =============================== 进阶查询 ===============================

    // 【L1】多条件组合过滤
    // 查询 14: hp>30 AND level>5
    console.log('--- 查询 ' + (++testNum) + ': 【L1】hp>30 AND level>5 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 30).filter(PLAYER_LEVEL, CMP.GT, 5).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 15: hp>30 AND level<=5
    console.log('--- 查询 ' + (++testNum) + ': 【L1】hp>30 AND level<=5 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 30).filter(PLAYER_LEVEL, CMP.LE, 5).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 16: hp>20 AND level>3 AND gold>200
    console.log('--- 查询 ' + (++testNum) + ': 【L1】hp>20 AND level>3 AND gold>200 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 20).filter(PLAYER_LEVEL, CMP.GT, 3).filter(PLAYER_GOLD, CMP.GT, 200).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 【L2】边界条件
    // 查询 17: hp>9000（空结果）
    console.log('--- 查询 ' + (++testNum) + ': 【L2】hp>9000（空结果） ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 9000).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 18: TAKE(100)
    console.log('--- 查询 ' + (++testNum) + ': 【L2】TAKE(100) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().take(100).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 19: DROP(100)
    console.log('--- 查询 ' + (++testNum) + ': 【L2】DROP(100) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().drop(100).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 20: DROP(100)+COUNT
    console.log('--- 查询 ' + (++testNum) + ': 【L2】DROP(100)+COUNT ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().drop(100).count().build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 21: 空管道FIND
    console.log('--- 查询 ' + (++testNum) + ': 【L2】空管道FIND ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 9000).find(PLAYER_HP, CMP.GT, 0).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 22: 空管道ANY
    console.log('--- 查询 ' + (++testNum) + ': 【L2】空管道ANY ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 9000).any(PLAYER_HP, CMP.GT, 0).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 【L3】复杂管道
    // 查询 23: SORT(level,asc)+DROP(2)+TAKE(2)
    console.log('--- 查询 ' + (++testNum) + ': 【L3】SORT(level,asc)+DROP(2)+TAKE(2) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().sort(PLAYER_LEVEL, true).drop(2).take(2).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 24: SORT(hp,desc)+REVERSE+DROP(1)+TAKE(2)+COUNT
    console.log('--- 查询 ' + (++testNum) + ': 【L3】SORT(hp,desc)+REVERSE+DROP(1)+TAKE(2)+COUNT ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().sort(PLAYER_HP, false).reverse().drop(1).take(2).count().build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 25: SORT(level,asc)+REVERSE+DROP(1)+TAKE(3)+FILTER(hp>40)
    console.log('--- 查询 ' + (++testNum) + ': 【L3】SORT(level,asc)+REVERSE+DROP(1)+TAKE(3)+FILTER(hp>40) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().sort(PLAYER_LEVEL, true).reverse().drop(1).take(3).filter(PLAYER_HP, CMP.GT, 40).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 【L4】字符串操作
    // 查询 26: name NE "Alice"
    console.log('--- 查询 ' + (++testNum) + ': 【L4】name NE "Alice" ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filterStr(PLAYER_NAME, CMP.NE, 'Alice').build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 27: name LT "Carol"
    console.log('--- 查询 ' + (++testNum) + ': 【L4】name LT "Carol" ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filterStr(PLAYER_NAME, CMP.LT, 'Carol').build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 28: FIND "Zoe"
    console.log('--- 查询 ' + (++testNum) + ': 【L4】FIND "Zoe" ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().findStr(PLAYER_NAME, 'Zoe').build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 【L5】写后查询
    // 查询 29: SET hp=50, ADD hp=30, FIND Alice
    console.log('--- 查询 ' + (++testNum) + ': 【L5】SET hp=50, ADD hp=30, FIND Alice ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.set([0, 0, PLAYER_HP], 50).add([0, 0, PLAYER_HP], 30).fromPlayers().findStr(PLAYER_NAME, 'Alice').build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 30: SET Alice hp=999, SET Bob gold=9999, FILTER gold>9000
    console.log('--- 查询 ' + (++testNum) + ': 【L5】SET Alice hp=999, SET Bob gold=9999, FILTER gold>9000 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.set([0, 0, PLAYER_HP], 999).set([0, 1, PLAYER_GOLD], 9999).fromPlayers().filter(PLAYER_GOLD, CMP.GT, 9000).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 31: ADD gold=-300, FIND Alice
    console.log('--- 查询 ' + (++testNum) + ': 【L5】ADD gold=-300, FIND Alice ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.add([0, 0, PLAYER_GOLD], -300).fromPlayers().findStr(PLAYER_NAME, 'Alice').build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 【L6】全比较运算符 — 对 hp=60
    // 查询 32: hp EQ 60
    console.log('--- 查询 ' + (++testNum) + ': 【L6】hp EQ 60 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.EQ, 60).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 33: hp NE 60
    console.log('--- 查询 ' + (++testNum) + ': 【L6】hp NE 60 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.NE, 60).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 34: hp LT 60
    console.log('--- 查询 ' + (++testNum) + ': 【L6】hp LT 60 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.LT, 60).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 35: hp GT 60
    console.log('--- 查询 ' + (++testNum) + ': 【L6】hp GT 60 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 60).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 36: hp LE 60
    console.log('--- 查询 ' + (++testNum) + ': 【L6】hp LE 60 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.LE, 60).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 37: hp GE 60
    console.log('--- 查询 ' + (++testNum) + ': 【L6】hp GE 60 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GE, 60).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 【L7】极限链
    // 查询 38: 极限链式查询
    console.log('--- 查询 ' + (++testNum) + ': 【L7】极限链式查询 ---');
    console.log('    (sort(level,asc) + reverse + drop(1) + take(4) + filter(hp>20) + sort(hp,desc) + reverse + take(2) + count)');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers()
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .drop(1)
            .take(4)
            .filter(PLAYER_HP, CMP.GT, 20)
            .sort(PLAYER_HP, false)
            .reverse()
            .take(2)
            .count().build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // =============================== L8-L12 进阶查询 ===============================

    // ---- L8: 管道操作边缘情况 ----

    // 查询 39: REVERSE x2
    console.log('--- 查询 ' + (++testNum) + ': 【L8】REVERSE x2 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().reverse().reverse().build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 40: TAKE(0)
    console.log('--- 查询 ' + (++testNum) + ': 【L8】TAKE(0) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().take(0).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 41: DROP(0)
    console.log('--- 查询 ' + (++testNum) + ': 【L8】DROP(0) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().drop(0).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 42: SORT 覆盖
    console.log('--- 查询 ' + (++testNum) + ': 【L8】SORT 覆盖（level,asc + hp,desc） ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().sort(PLAYER_LEVEL, true).sort(PLAYER_HP, false).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 43: REVERSE x3
    console.log('--- 查询 ' + (++testNum) + ': 【L8】REVERSE x3 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().reverse().reverse().reverse().build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 44: DROP(4)+TAKE(1)
    console.log('--- 查询 ' + (++testNum) + ': 【L8】DROP(4)+TAKE(1) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().drop(4).take(1).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // ---- L9: 数值边界与极端值 ----

    // 查询 45: FILTER hp<0
    console.log('--- 查询 ' + (++testNum) + ': 【L9】FILTER hp<0（空结果） ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.LT, 0).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 46: SET hp=0 + FILTER hp EQ 0
    console.log('--- 查询 ' + (++testNum) + ': 【L9】SET hp=0 + FILTER hp EQ 0 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.set([0, 0, PLAYER_HP], 0).fromPlayers().filter(PLAYER_HP, CMP.EQ, 0).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 47: ADD -10000 gold + FILTER gold<0
    console.log('--- 查询 ' + (++testNum) + ': 【L9】ADD -10000 gold + FILTER gold<0 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.add([0, 0, PLAYER_GOLD], -10000).fromPlayers().filter(PLAYER_GOLD, CMP.LT, 0).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 48: 互斥条件
    console.log('--- 查询 ' + (++testNum) + ': 【L9】互斥条件 hp>0 AND hp<=0（必然空） ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 0).filter(PLAYER_HP, CMP.LE, 0).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 49: FILTER level=0
    console.log('--- 查询 ' + (++testNum) + ': 【L9】FILTER level=0（空结果） ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_LEVEL, CMP.EQ, 0).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 50: FILTER hp>=0 + COUNT
    console.log('--- 查询 ' + (++testNum) + ': 【L9】FILTER hp>=0 + COUNT ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GE, 0).count().build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // ---- L10: 写操作与管道混合 ----

    // 查询 51: SET 3玩家hp + FILTER + SORT
    console.log('--- 查询 ' + (++testNum) + ': 【L10】SET 3玩家hp + FILTER + SORT ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.set([0, 0, PLAYER_HP], 77).set([0, 1, PLAYER_HP], 88).set([0, 2, PLAYER_HP], 99)
            .fromPlayers().filter(PLAYER_HP, CMP.GT, 70).sort(PLAYER_HP, true).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 52: SET hp=40 + ADD hp=10 + FIND Alice
    console.log('--- 查询 ' + (++testNum) + ': 【L10】SET hp=40 + ADD hp=10 + FIND Alice ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.set([0, 0, PLAYER_HP], 40).add([0, 0, PLAYER_HP], 10)
            .fromPlayers().findStr(PLAYER_NAME, 'Alice').build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 53: ADD 全部 level+1 + FILTER + COUNT
    console.log('--- 查询 ' + (++testNum) + ': 【L10】ADD 全部 level+1 + FILTER + COUNT ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.add([0, 0, PLAYER_LEVEL], 1).add([0, 1, PLAYER_LEVEL], 1)
            .add([0, 2, PLAYER_LEVEL], 1).add([0, 3, PLAYER_LEVEL], 1)
            .add([0, 4, PLAYER_LEVEL], 1)
            .fromPlayers().filter(PLAYER_LEVEL, CMP.GT, 10).count().build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 54: SET [0,99] hp=9999（不存在索引）
    console.log('--- 查询 ' + (++testNum) + ': 【L10】SET [0,99] hp=9999（不存在索引） ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.set([0, 99, PLAYER_HP], 9999).fromPlayers().filter(PLAYER_HP, CMP.GT, 9000).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 55: SET hp=55 + SORT hp + REVERSE + TAKE(2)
    console.log('--- 查询 ' + (++testNum) + ': 【L10】SET hp=55 + SORT hp + REVERSE + TAKE(2) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.set([0, 0, PLAYER_HP], 55).fromPlayers()
            .sort(PLAYER_HP, true).reverse().take(2).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // ---- L11: 交叉字段查询 ----

    // 查询 56: FILTER(hp>30)+SORT(gold)+ANY(level>8)
    console.log('--- 查询 ' + (++testNum) + ': 【L11】FILTER(hp>30)+SORT(gold)+ANY(level>8) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 30)
            .sort(PLAYER_GOLD, true).any(PLAYER_LEVEL, CMP.GT, 8).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 57: FILTER(gold>200)+FILTER(hp>50)+SORT(level)
    console.log('--- 查询 ' + (++testNum) + ': 【L11】FILTER(gold>200)+FILTER(hp>50)+SORT(level) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_GOLD, CMP.GT, 200)
            .filter(PLAYER_HP, CMP.GT, 50).sort(PLAYER_LEVEL, true).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 58: FILTER(gold>200)+SORT(hp)+FIND(level=12)
    console.log('--- 查询 ' + (++testNum) + ': 【L11】FILTER(gold>200)+SORT(hp)+FIND(level=12) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_GOLD, CMP.GT, 200)
            .sort(PLAYER_HP, true).find(PLAYER_LEVEL, CMP.EQ, 12).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 59: FILTER(hp>50)+SORT(level)+REVERSE+ANY(gold>300)
    console.log('--- 查询 ' + (++testNum) + ': 【L11】FILTER(hp>50)+SORT(level)+REVERSE+ANY(gold>300) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 50)
            .sort(PLAYER_LEVEL, true).reverse().any(PLAYER_GOLD, CMP.GT, 300).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 60: hp>25 AND level>4 AND gold>150
    console.log('--- 查询 ' + (++testNum) + ': 【L11】hp>25 AND level>4 AND gold>150 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 25)
            .filter(PLAYER_LEVEL, CMP.GT, 4).filter(PLAYER_GOLD, CMP.GT, 150).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // ---- L12: 极限组合压力 ----

    // 查询 61: 15步极限链
    console.log('--- 查询 ' + (++testNum) + ': 【L12】15步极限链 ---');
    console.log('    (fromPlayers→sort→reverse→drop→take→filter→sort→reverse→take→filter→sort→reverse→drop→take→count)');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers()
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .drop(1)
            .take(3)
            .filter(PLAYER_HP, CMP.GT, 20)
            .sort(PLAYER_HP, false)
            .reverse()
            .take(2)
            .filter(PLAYER_GOLD, CMP.GT, 100)
            .sort(PLAYER_GOLD, true)
            .reverse()
            .drop(1)
            .take(1)
            .count().build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 62: SET 5玩家hp + 复杂链
    console.log('--- 查询 ' + (++testNum) + ': 【L12】SET 5玩家hp + 复杂链 ---');
    console.log('    (SET 5玩家→fromPlayers→FILTER hp>30→SORT hp→REVERSE→TAKE(3))');
    {
        const q = new SpoiQueryBuilder();
        const query = q.set([0, 0, PLAYER_HP], 70).set([0, 1, PLAYER_HP], 80)
            .set([0, 2, PLAYER_HP], 90).set([0, 3, PLAYER_HP], 60)
            .set([0, 4, PLAYER_HP], 50)
            .fromPlayers().filter(PLAYER_HP, CMP.GT, 30)
            .sort(PLAYER_HP, true).reverse().take(3).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 63: SORT+REVERSE 循环3次
    console.log('--- 查询 ' + (++testNum) + ': 【L12】SORT+REVERSE 循环3次 ---');
    console.log('    (SORT(level,asc)→REVERSE→SORT(hp,desc)→REVERSE→SORT(gold,asc)→REVERSE)');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers()
            .sort(PLAYER_LEVEL, true).reverse()
            .sort(PLAYER_HP, false).reverse()
            .sort(PLAYER_GOLD, true).reverse().build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 64: FILTER(hp>85)→SORT→REVERSE→DROP(0)→TAKE(1)
    console.log('--- 查询 ' + (++testNum) + ': 【L12】FILTER(hp>85)→SORT→REVERSE→DROP(0)→TAKE(1) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 85)
            .sort(PLAYER_LEVEL, true).reverse().drop(0).take(1).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 65: 极限混合
    console.log('--- 查询 ' + (++testNum) + ': 【L12】极限混合（写+管+过滤+排序+反转+截取+计数） ---');
    console.log('    (SET hp=60→ADD gold=50→fromPlayers→FILTER hp>30→SORT level→REVERSE→DROP→TAKE→FILTER gold>100→SORT hp→REVERSE→TAKE→COUNT)');
    {
        const q = new SpoiQueryBuilder();
        const query = q.set([0, 0, PLAYER_HP], 60).add([0, 0, PLAYER_GOLD], 50)
            .fromPlayers().filter(PLAYER_HP, CMP.GT, 30)
            .sort(PLAYER_LEVEL, true).reverse().drop(1).take(3)
            .filter(PLAYER_GOLD, CMP.GT, 100)
            .sort(PLAYER_HP, true).reverse().take(2)
            .count().build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    console.log('=== 所有查询完成 ===');
    socket.destroy();
}

main().catch((err) => {
    console.error('客户端异常:', err);
    process.exit(1);
});