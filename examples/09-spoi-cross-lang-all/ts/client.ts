// 示例 09：SPOI 全语言跨语言数据互查（TypeScript/Node.js 客户端）
// 展示：TypeScript 客户端通过 TCP 向 C++ 服务器发送 SPOI 查询指令，接收并展示查询结果。
// 使用 Node.js 标准库（net, Buffer）手工构建 SPOI 二进制协议。

import * as net from 'net';

// =============================== 常量 ===============================

/** 操作码 */
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
} as const;

/** 比较运算符 */
const CMP = {
    EQ: 0,
    NE: 1,
    LT: 2,
    GT: 3,
    LE: 4,
    GE: 5,
} as const;

/** 字段索引 */
const PLAYER_NAME  = 0;
const PLAYER_HP    = 1;
const PLAYER_LEVEL = 2;
const PLAYER_GOLD  = 3;

const STATE_PLAYERS    = 0;
const STATE_TICK       = 1;
const STATE_SERVERNAME = 2;

/** 结果类型 */
const enum ResultType {
    UNDEF    = 0,
    SINGLE   = 1,
    VECTOR   = 2,
    COUNT    = 3,
    BOOL     = 4,
    OPTIONAL = 5,
    ERROR    = 6,
}

// =============================== 类型定义 ===============================

/** 玩家数据 */
interface Player {
    name: string;
    hp: number;
    level: number;
    gold: number;
}

// =============================== 二进制辅助函数 ===============================

/** 创建 u32 LE 的 Buffer */
function u32LE(value: number): Buffer {
    const buf = Buffer.allocUnsafe(4);
    buf.writeUInt32LE(value, 0);
    return buf;
}

/** 创建 i32 LE 的 Buffer */
function i32LE(value: number): Buffer {
    const buf = Buffer.allocUnsafe(4);
    buf.writeInt32LE(value, 0);
    return buf;
}

/** 创建带 u32 LE 长度前缀的 Buffer（vector<u8> 或 vector<u32> 格式） */
function vectorU32(arr: number[]): Buffer {
    const buf = Buffer.allocUnsafe(4 + arr.length * 4);
    buf.writeUInt32LE(arr.length, 0);
    for (let i = 0; i < arr.length; i++) {
        buf.writeUInt32LE(arr[i], 4 + i * 4);
    }
    return buf;
}

/** 读取 varint（用于 vector 结果内部的元素计数） */
function readVarint(buf: Buffer, offset: number): { value: number; nextOffset: number } {
    let result = 0;
    let shift = 0;
    while (offset < buf.length) {
        const b = buf[offset++];
        result |= (b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    return { value: result >>> 0, nextOffset: offset };
}

// =============================== SPOI 查询构建器 ===============================

/**
 * 构建比较表达式操作数：[memberIdx: u32 LE][cmpOp: u8][value长度: u32 LE][value字节]
 */
function cmpExprBytes(memberIdx: number, cmpOp: number, value: Buffer): Buffer {
    const buf = Buffer.allocUnsafe(4 + 1 + 4 + value.length);
    let offset = 0;
    buf.writeUInt32LE(memberIdx, offset); offset += 4;
    buf.writeUInt8(cmpOp, offset);         offset += 1;
    buf.writeUInt32LE(value.length, offset); offset += 4;
    value.copy(buf, offset);
    return buf;
}

/**
 * 构建单条指令：[op: u8][路径长度: u32 LE][路径段: u32 LE × N][操作数长度: u32 LE][操作数字节]
 */
function buildInstruction(op: number, path: number[], operand: Buffer): Buffer {
    const pathBuf = vectorU32(path);
    const operandLenBuf = u32LE(operand.length);
    const header = Buffer.allocUnsafe(1 + pathBuf.length);
    let offset = 0;
    header.writeUInt8(op, offset); offset += 1;
    pathBuf.copy(header, offset);
    return Buffer.concat([header, operandLenBuf, operand]);
}

/** SPOI 查询构建器 */
class SpoiQueryBuilder {
    private instructions: Buffer[] = [];

    /** 添加一条指令 */
    private addInst(op: number, path: number[], operand: Buffer): void {
        this.instructions.push(buildInstruction(op, path, operand));
    }

    /**
     * 管道入口：从 players 开始
     * 发送 FILTER 指令，路径=[STATE_PLAYERS=0]，操作数=cmpExpr(PLAYER_HP, GE, 0)
     */
    fromPlayers(): this {
        this.addInst(OP.FILTER, [STATE_PLAYERS], cmpExprBytes(PLAYER_HP, CMP.GE, i32LE(0)));
        return this;
    }

    /** filter：按整数字段过滤 */
    filter(field: number, cmpOp: number, value: number): this {
        this.addInst(OP.FILTER, [], cmpExprBytes(field, cmpOp, i32LE(value)));
        return this;
    }

    /** filterStr：按字符串字段过滤 */
    filterStr(field: number, cmpOp: number, value: string): this {
        const strBytes = Buffer.from(value, 'utf-8');
        // 字符串值需要包裹 [长度: u32 LE][UTF-8字节] 格式
        const valBytes = Buffer.concat([u32LE(strBytes.length), strBytes]);
        this.addInst(OP.FILTER, [], cmpExprBytes(field, cmpOp, valBytes));
        return this;
    }

    /**
     * sort：排序
     * operand = [field: u32 LE][ascending: u8]（1=升序, 0=降序）
     */
    sort(field: number, ascending: boolean): this {
        const operand = Buffer.allocUnsafe(4 + 1);
        operand.writeUInt32LE(field, 0);
        operand.writeUInt8(ascending ? 1 : 0, 4);
        this.addInst(OP.SORT, [], operand);
        return this;
    }

    /** reverse：反转 */
    reverse(): this {
        this.addInst(OP.REVERSE, [], Buffer.alloc(0));
        return this;
    }

    /** take：取前 N 个，operand = [n: u32 LE] */
    take(n: number): this {
        this.addInst(OP.TAKE, [], u32LE(n));
        return this;
    }

    /** drop：跳过前 N 个，operand = [n: u32 LE] */
    drop(n: number): this {
        this.addInst(OP.DROP, [], u32LE(n));
        return this;
    }

    /** count：计数 */
    count(): this {
        this.addInst(OP.COUNT, [], Buffer.alloc(0));
        return this;
    }

    /** any：存在性检查 */
    any(field: number, cmpOp: number, value: number): this {
        this.addInst(OP.ANY, [], cmpExprBytes(field, cmpOp, i32LE(value)));
        return this;
    }

    /** find：查找第一个匹配 */
    find(field: number, cmpOp: number, value: number): this {
        this.addInst(OP.FIND, [], cmpExprBytes(field, cmpOp, i32LE(value)));
        return this;
    }

    /** findStr：按字符串查找 */
    findStr(field: number, value: string): this {
        const strBytes = Buffer.from(value, 'utf-8');
        // 字符串值需要包裹 [长度: u32 LE][UTF-8字节] 格式
        const valBytes = Buffer.concat([u32LE(strBytes.length), strBytes]);
        this.addInst(OP.FIND, [], cmpExprBytes(field, CMP.EQ, valBytes));
        return this;
    }

    /** set：写操作，operand = [value: i32 LE] */
    set(path: number[], value: number): this {
        this.addInst(OP.SET, path, i32LE(value));
        return this;
    }

    /** add：写操作，operand = [delta: i32 LE] */
    add(path: number[], delta: number): this {
        this.addInst(OP.ADD, path, i32LE(delta));
        return this;
    }

    /**
     * 构建完整的 SPOI 查询二进制数据
     * SpoiStream 格式：[指令数: u32 LE][指令1][指令2]...
     * 末尾自动追加 EXEC 指令
     */
    build(): Buffer {
        this.addInst(OP.EXEC, [], Buffer.alloc(0));
        const countBuf = u32LE(this.instructions.length);
        return Buffer.concat([countBuf, ...this.instructions]);
    }
}

// =============================== 结果解析 ===============================

/**
 * 解析单个 CrossPlayer：
 * [name长度: u32 LE][name UTF-8][hp: i32 LE][level: i32 LE][gold: i32 LE]
 */
function parsePlayer(buf: Buffer, offset: number): { player: Player; nextOffset: number } {
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

/** 格式化单个玩家为字符串 */
function formatPlayer(player: Player): string {
    return `Player{name='${player.name}', hp=${player.hp}, level=${player.level}, gold=${player.gold}}`;
}

/**
 * 解析并打印 SpoiResult：
 * [resultType: u8][data长度: u32 LE][data字节]
 */
function printResult(data: Buffer): void {
    if (data.length === 0) {
        console.log('(空结果)');
        return;
    }

    let offset = 0;
    const resultType = data.readUInt8(offset) as ResultType;
    offset += 1;
    const dataLen = data.readUInt32LE(offset);
    offset += 4;
    const innerData = data.slice(offset, offset + dataLen);

    switch (resultType) {
        case ResultType.COUNT: {
            // count 内部：[count: u32 LE] 4 字节
            if (innerData.length >= 4) {
                const count = innerData.readUInt32LE(0);
                console.log('计数结果: ' + count);
            }
            break;
        }
        case ResultType.BOOL: {
            // bool 内部：[0x00 或 0x01] 1 字节
            console.log('布尔结果: ' + (innerData[0] ? 'true' : 'false'));
            break;
        }
        case ResultType.VECTOR: {
            // vector 内部：[元素数: varint][元素1][元素2]...
            let innerOffset = 0;
            const { value: count, nextOffset } = readVarint(innerData, innerOffset);
            innerOffset = nextOffset;
            console.log('向量结果: ' + count + ' 个元素');
            for (let i = 0; i < count; i++) {
                const { player, nextOffset: no } = parsePlayer(innerData, innerOffset);
                innerOffset = no;
                console.log('    [' + i + '] ' + formatPlayer(player));
            }
            break;
        }
        case ResultType.SINGLE: {
            // single 内部：[元素字节]
            const { player } = parsePlayer(innerData, 0);
            console.log('单个结果: ' + formatPlayer(player));
            break;
        }
        case ResultType.OPTIONAL: {
            // optional 内部：[has_value: u8][元素字节 若有值]
            if (innerData.length > 0 && innerData[0] !== 0) {
                const { player } = parsePlayer(innerData, 1);
                console.log('可选结果: 有值 → ' + formatPlayer(player));
            } else {
                console.log('可选结果: 空');
            }
            break;
        }
        case ResultType.ERROR: {
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

/**
 * 发送带长度前缀的数据：[4字节 u32 LE 数据长度] + [数据]
 */
function sendWithLength(socket: net.Socket, data: Buffer): void {
    const lenBuf = u32LE(data.length);
    socket.write(Buffer.concat([lenBuf, data]));
}

/**
 * 接收带长度前缀的数据：[4字节 u32 LE 数据长度] + [数据]
 */
function recvWithLength(socket: net.Socket): Promise<Buffer> {
    return new Promise((resolve, reject) => {
        let lengthBuffer: Buffer | null = null;
        let dataBuffer: Buffer | null = null;
        let dataOffset = 0;
        let expectedLength = 0;

        function onData(chunk: Buffer): void {
            if (lengthBuffer === null) {
                lengthBuffer = chunk;
            } else {
                lengthBuffer = Buffer.concat([lengthBuffer, chunk]);
            }

            if (lengthBuffer.length >= 4) {
                expectedLength = lengthBuffer.readUInt32LE(0);
                const remaining = lengthBuffer.slice(4);
                lengthBuffer = null;
                dataBuffer = Buffer.alloc(expectedLength);
                dataOffset = 0;
                if (remaining.length > 0) {
                    const copyLen = Math.min(remaining.length, expectedLength);
                    remaining.copy(dataBuffer, 0, 0, copyLen);
                    dataOffset = copyLen;
                }
            } else {
                return;
            }

            if (dataBuffer && dataOffset >= expectedLength) {
                cleanup();
                resolve(dataBuffer);
            }
        }

        function onDataAfterLength(chunk: Buffer): void {
            if (dataBuffer && dataOffset < expectedLength) {
                const copyLen = Math.min(chunk.length, expectedLength - dataOffset);
                chunk.copy(dataBuffer, dataOffset, 0, copyLen);
                dataOffset += copyLen;
            }

            if (dataBuffer && dataOffset >= expectedLength) {
                cleanup();
                resolve(dataBuffer);
            }
        }

        let currentHandler = onData;

        function dataHandler(chunk: Buffer): void {
            currentHandler(chunk);
            if (lengthBuffer === null && dataBuffer && dataOffset < expectedLength) {
                currentHandler = onDataAfterLength;
            }
        }

        function onError(err: Error): void {
            cleanup();
            reject(err);
        }

        function onClose(): void {
            cleanup();
            resolve(Buffer.alloc(0));
        }

        function cleanup(): void {
            socket.removeListener('data', dataHandler);
            socket.removeListener('error', onError);
            socket.removeListener('close', onClose);
        }

        socket.on('data', dataHandler);
        socket.on('error', onError);
        socket.on('close', onClose);
    });
}

// =============================== 主程序 ===============================

async function main(): Promise<void> {
    console.log('=== SPOI 跨语言数据互查 — TypeScript/Node.js 客户端 ===');
    console.log('');

    const HOST = '127.0.0.1';
    const PORT = 9999;

    const socket = new net.Socket();

    await new Promise<void>((resolve, reject) => {
        socket.connect(PORT, HOST, () => {
            console.log('已连接到服务器 ' + HOST + ':' + PORT);
            console.log('');
            resolve();
        });
        socket.on('error', (err: Error) => {
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

    // 查询 2: 过滤 hp > 50 的玩家
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

    // 查询 8: 复杂链式查询（filter + sort + reverse + take）
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

    // 查询 9: 写操作 — 将玩家[0]的 hp 设置为 99
    console.log('--- 查询 ' + (++testNum) + ': 写操作 — 将玩家[0]的 hp 设置为 99 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.set([0, 0, PLAYER_HP], 99).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 10: 验证写操作 — 查找 Alice 的 hp 是否变为 99
    console.log('--- 查询 ' + (++testNum) + ': 验证写操作 — 查找 Alice 的 hp 是否变为 99 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().findStr(PLAYER_NAME, 'Alice').build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 11: 写操作 — 给玩家[0]增加 100 金币
    console.log('--- 查询 ' + (++testNum) + ': 写操作 — 给玩家[0]增加 100 金币 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.add([0, 0, PLAYER_GOLD], 100).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 12: 验证写操作 — 查找 Alice 的金币是否变为 600
    console.log('--- 查询 ' + (++testNum) + ': 验证写操作 — 查找 Alice 的金币是否变为 600 ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().findStr(PLAYER_NAME, 'Alice').build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 13: filter(hp > 20) + drop(2)
    console.log('--- 查询 ' + (++testNum) + ': filter(hp > 20) + drop(2) ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 20).drop(2).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // ================================================================
    // 进阶查询 Q14-Q38
    // ================================================================

    // ---- L1: 多条件组合过滤 ----

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

    // ---- L2: 边界条件 ----

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
        const query = q.find(PLAYER_HP, CMP.EQ, 0).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 22: 空管道ANY
    console.log('--- 查询 ' + (++testNum) + ': 【L2】空管道ANY ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.any(PLAYER_HP, CMP.EQ, 0).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // ---- L3: 复杂管道 ----

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

    // ---- L4: 字符串操作 ----

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

    // ---- L5: 写后查询 ----

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

    // ---- L6: 全比较运算符 ----

    // 查询 32: hp=60 EQ
    console.log('--- 查询 ' + (++testNum) + ': 【L6】hp=60 EQ ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.EQ, 60).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 33: hp=60 NE
    console.log('--- 查询 ' + (++testNum) + ': 【L6】hp=60 NE ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.NE, 60).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 34: hp=60 LT
    console.log('--- 查询 ' + (++testNum) + ': 【L6】hp=60 LT ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.LT, 60).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 35: hp=60 GT
    console.log('--- 查询 ' + (++testNum) + ': 【L6】hp=60 GT ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GT, 60).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 36: hp=60 LE
    console.log('--- 查询 ' + (++testNum) + ': 【L6】hp=60 LE ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.LE, 60).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // 查询 37: hp=60 GE
    console.log('--- 查询 ' + (++testNum) + ': 【L6】hp=60 GE ---');
    {
        const q = new SpoiQueryBuilder();
        const query = q.fromPlayers().filter(PLAYER_HP, CMP.GE, 60).build();
        sendWithLength(socket, query);
        printResult(await recvWithLength(socket));
        console.log('');
    }

    // ---- L7: 极限链 ----

    // 查询 38: fromPlayers + sort(level,true) + reverse() + drop(1) + take(4) + filter(hp>20) + sort(hp,false) + reverse() + take(2) + count()
    console.log('--- 查询 ' + (++testNum) + ': 【L7】极限链式查询 ---');
    console.log('    (sort(level,asc) → reverse → drop(1) → take(4) → filter(hp>20) → sort(hp,desc) → reverse → take(2) → count)');
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

main().catch((err: Error) => {
    console.error('程序异常退出:', err.message);
    process.exit(1);
});