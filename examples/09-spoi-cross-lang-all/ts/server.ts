// 示例 09：SPOI 全语言跨语言数据互查（TypeScript/Node.js 服务端）
// 展示：TypeScript 服务端托管游戏状态数据，通过 TCP 接收各语言客户端发送的 SPOI 查询指令，
//       执行查询后将结果序列化返回。

import * as net from 'net';

// =============================== 常量 ===============================

const OP = {
    SET: 0x04, ADD: 0x05, FILTER: 0x0C, SORT: 0x0E, REVERSE: 0x0F,
    TAKE: 0x10, DROP: 0x11, COUNT: 0x15, ANY: 0x16, FIND: 0x18, EXEC: 0x21,
} as const;

const CMP = { EQ: 0, NE: 1, LT: 2, GT: 3, LE: 4, GE: 5 } as const;

const PLAYER_NAME = 0, PLAYER_HP = 1, PLAYER_LEVEL = 2, PLAYER_GOLD = 3;
const STATE_PLAYERS = 0;

const enum ResultType { UNDEF = 0, SINGLE = 1, VECTOR = 2, COUNT = 3, BOOL = 4, OPTIONAL = 5, ERROR = 6 }

// =============================== 二进制辅助函数 ===============================

function u32LE(v: number): Buffer { const b = Buffer.allocUnsafe(4); b.writeUInt32LE(v, 0); return b; }
function i32LE(v: number): Buffer { const b = Buffer.allocUnsafe(4); b.writeInt32LE(v, 0); return b; }

function writeVarint(v: number): Buffer {
    const parts: number[] = [];
    while (v >= 0x80) { parts.push((v & 0x7F) | 0x80); v >>>= 7; }
    parts.push(v & 0x7F);
    return Buffer.from(parts);
}

function applyCmp(a: any, op: number, b: any): boolean {
    switch (op) {
        case CMP.EQ: return a === b;
        case CMP.NE: return a !== b;
        case CMP.LT: return a < b;
        case CMP.GT: return a > b;
        case CMP.LE: return a <= b;
        case CMP.GE: return a >= b;
        default: return false;
    }
}

// =============================== 类型定义 ===============================

interface CmpExpr {
    memberIdx: number;
    cmpOp: number;
    value: Buffer;
}

class Player {
    constructor(
        public name: string,
        public hp: number,
        public level: number,
        public gold: number,
    ) {}

    getField(idx: number): any {
        switch (idx) {
            case PLAYER_NAME:  return this.name;
            case PLAYER_HP:    return this.hp;
            case PLAYER_LEVEL: return this.level;
            case PLAYER_GOLD:  return this.gold;
            default: return null;
        }
    }

    setField(idx: number, value: any): void {
        switch (idx) {
            case PLAYER_NAME:  this.name = value; break;
            case PLAYER_HP:    this.hp = value; break;
            case PLAYER_LEVEL: this.level = value; break;
            case PLAYER_GOLD:  this.gold = value; break;
        }
    }

    addField(idx: number, delta: number): void {
        switch (idx) {
            case PLAYER_HP:    this.hp += delta; break;
            case PLAYER_LEVEL: this.level += delta; break;
            case PLAYER_GOLD:  this.gold += delta; break;
        }
    }

    serialize(): Buffer {
        const nameBytes = Buffer.from(this.name, 'utf-8');
        return Buffer.concat([
            u32LE(nameBytes.length), nameBytes,
            i32LE(this.hp), i32LE(this.level), i32LE(this.gold),
        ]);
    }
}

class GameState {
    players: Player[] = [];
    tick = 42;
    serverName = 'TSServer';

    reset(): void {
        this.players = [
            new Player('Alice', 80, 10, 500),
            new Player('Bob',   30, 5,  200),
            new Player('Carol', 60, 8,  300),
            new Player('Dave',  90, 12, 400),
            new Player('Eve',   15, 3,  100),
        ];
        this.tick = 42;
        this.serverName = 'TSServer';
    }
}

// =============================== SPOI 指令解析与执行 ===============================

function parseCmpExpr(data: Buffer, offset: number): { expr: CmpExpr; nextOffset: number } {
    const memberIdx = data.readUInt32LE(offset); offset += 4;
    const cmpOp = data.readUInt8(offset); offset += 1;
    const valueLen = data.readUInt32LE(offset); offset += 4;
    const value = data.slice(offset, offset + valueLen);
    offset += valueLen;
    return { expr: { memberIdx, cmpOp, value }, nextOffset: offset };
}

function parseCmpValueStr(expr: CmpExpr): string | null {
    const v = expr.value;
    if (v.length >= 4) {
        const slen = v.readUInt32LE(0);
        if (4 + slen === v.length) return v.toString('utf-8', 4, 4 + slen);
    }
    return null;
}

function parseCmpValueI32(expr: CmpExpr): number | null {
    if (expr.value.length === 4) return expr.value.readInt32LE(0);
    return null;
}

function comparePlayer(player: Player, expr: CmpExpr): boolean {
    const val = player.getField(expr.memberIdx);
    if (val === null) return false;

    if (typeof val === 'string') {
        const cmpVal = parseCmpValueStr(expr);
        if (cmpVal === null) return false;
        return applyCmp(val, expr.cmpOp, cmpVal);
    } else if (typeof val === 'number') {
        const cmpVal = parseCmpValueI32(expr);
        if (cmpVal === null) return false;
        return applyCmp(val, expr.cmpOp, cmpVal);
    }
    return false;
}

function executeQuery(state: GameState, queryData: Buffer): Buffer {
    try {
        if (queryData.length < 4) return makeErrorResult('查询数据太短');

        let offset = 0;
        const instCount = queryData.readUInt32LE(offset);
        offset += 4;

        let pipeline: Player[] = [];
        let pipelineActive = false;

        for (let i = 0; i < instCount; i++) {
            if (offset >= queryData.length) break;

            const op = queryData.readUInt8(offset); offset += 1;
            const pathLen = queryData.readUInt32LE(offset); offset += 4;
            const path: number[] = [];
            for (let j = 0; j < pathLen; j++) {
                path.push(queryData.readUInt32LE(offset));
                offset += 4;
            }
            const operandLen = queryData.readUInt32LE(offset); offset += 4;
            const operand = queryData.slice(offset, offset + operandLen);
            offset += operandLen;

            if (op === OP.FILTER) {
                const { expr } = parseCmpExpr(operand, 0);
                if (!pipelineActive && path.length === 1 && path[0] === STATE_PLAYERS) {
                    pipeline = state.players.filter(p => comparePlayer(p, expr));
                    pipelineActive = true;
                } else if (pipelineActive) {
                    pipeline = pipeline.filter(p => comparePlayer(p, expr));
                } else {
                    pipeline = state.players.filter(p => comparePlayer(p, expr));
                    pipelineActive = true;
                }
            } else if (op === OP.SORT && pipelineActive && operand.length >= 5) {
                const field = operand.readUInt32LE(0);
                const ascending = operand[4] !== 0;
                pipeline.sort((a, b) => {
                    const va = a.getField(field) || 0;
                    const vb = b.getField(field) || 0;
                    return ascending ? va - vb : vb - va;
                });
            } else if (op === OP.REVERSE && pipelineActive) {
                pipeline.reverse();
            } else if (op === OP.TAKE && pipelineActive && operand.length >= 4) {
                pipeline = pipeline.slice(0, operand.readUInt32LE(0));
            } else if (op === OP.DROP && pipelineActive && operand.length >= 4) {
                pipeline = pipeline.slice(operand.readUInt32LE(0));
            } else if (op === OP.COUNT && pipelineActive) {
                return makeCountResult(pipeline.length);
            } else if (op === OP.ANY && pipelineActive) {
                const { expr } = parseCmpExpr(operand, 0);
                return makeBoolResult(pipeline.some(p => comparePlayer(p, expr)));
            } else if (op === OP.FIND && pipelineActive) {
                const { expr } = parseCmpExpr(operand, 0);
                const found = pipeline.find(p => comparePlayer(p, expr));
                return makeOptionalResult(found || null);
            } else if (op === OP.SET && path.length >= 3 && path[0] === STATE_PLAYERS) {
                const idx = path[1], field = path[2];
                if (idx >= 0 && idx < state.players.length && operand.length >= 4) {
                    state.players[idx].setField(field, operand.readInt32LE(0));
                }
                // 修改后继续处理后续指令
            } else if (op === OP.ADD && path.length >= 3 && path[0] === STATE_PLAYERS) {
                const idx = path[1], field = path[2];
                if (idx >= 0 && idx < state.players.length && operand.length >= 4) {
                    state.players[idx].addField(field, operand.readInt32LE(0));
                }
                // 修改后继续处理后续指令
            } else if (op === OP.EXEC && pipelineActive) {
                const countBuf = writeVarint(pipeline.length);
                const playersBuf = Buffer.concat(pipeline.map(p => p.serialize()));
                return makeResult(ResultType.VECTOR, Buffer.concat([countBuf, playersBuf]));
            } else if (op === OP.EXEC) {
                return makeResult(ResultType.UNDEF, Buffer.alloc(0));
            }
        }
        return makeResult(ResultType.UNDEF, Buffer.alloc(0));
    } catch (e: any) {
        return makeErrorResult('执行错误: ' + e.message);
    }
}

// =============================== 结果构建 ===============================

function makeResult(resultType: number, data: Buffer): Buffer {
    const header = Buffer.allocUnsafe(5);
    header.writeUInt8(resultType, 0);
    header.writeUInt32LE(data.length, 1);
    return Buffer.concat([header, data]);
}

function makeCountResult(count: number): Buffer { return makeResult(ResultType.COUNT, i32LE(count)); }
function makeBoolResult(val: boolean): Buffer { return makeResult(ResultType.BOOL, Buffer.from([val ? 1 : 0])); }

function makeOptionalResult(player: Player | null): Buffer {
    if (player === null) return makeResult(ResultType.OPTIONAL, Buffer.from([0]));
    return makeResult(ResultType.OPTIONAL, Buffer.concat([Buffer.from([1]), player.serialize()]));
}

function makeErrorResult(msg: string): Buffer { return makeResult(ResultType.ERROR, Buffer.from(msg, 'utf-8')); }

// =============================== TCP 通信 ===============================

function sendWithLength(socket: net.Socket, data: Buffer): void {
    socket.write(Buffer.concat([u32LE(data.length), data]));
}

function recvWithLength(socket: net.Socket): Promise<Buffer> {
    return new Promise((resolve, reject) => {
        let lenBuf: Buffer | null = null;
        let dataBuf: Buffer | null = null;
        let dataOffset = 0;
        let expectedLen = 0;

        const onData = (chunk: Buffer) => {
            lenBuf = lenBuf ? Buffer.concat([lenBuf, chunk]) : chunk;
            if (lenBuf.length >= 4) {
                expectedLen = lenBuf.readUInt32LE(0);
                const remaining = lenBuf.slice(4);
                lenBuf = null;
                dataBuf = Buffer.alloc(expectedLen);
                dataOffset = 0;
                if (remaining.length > 0) {
                    const copyLen = Math.min(remaining.length, expectedLen);
                    remaining.copy(dataBuf, 0, 0, copyLen);
                    dataOffset = copyLen;
                }
                if (dataOffset >= expectedLen) {
                    cleanup(); resolve(dataBuf);
                    return;
                }
                socket.removeListener('data', onData);
                socket.on('data', onRemaining);
            }
        };

        const onRemaining = (chunk: Buffer) => {
            if (dataBuf && dataOffset < expectedLen) {
                const copyLen = Math.min(chunk.length, expectedLen - dataOffset);
                chunk.copy(dataBuf, dataOffset, 0, copyLen);
                dataOffset += copyLen;
                if (dataOffset >= expectedLen) {
                    cleanup(); resolve(dataBuf);
                }
            }
        };

        const onError = (err: Error) => { cleanup(); reject(err); };
        const onClose = () => { cleanup(); resolve(Buffer.alloc(0)); };

        const cleanup = () => {
            socket.removeListener('data', onData);
            socket.removeListener('data', onRemaining);
            socket.removeListener('error', onError);
            socket.removeListener('close', onClose);
        };

        socket.on('data', onData);
        socket.on('error', onError);
        socket.on('close', onClose);
    });
}

// =============================== 主程序 ===============================

const HOST = '127.0.0.1';
const PORT = 9999;

const state = new GameState();
state.reset();

console.log('=== SPOI 全语言跨语言数据互查 — TypeScript/Node.js 服务端 ===\n');
console.log('游戏状态已初始化：');
console.log('  服务器名称: ' + state.serverName);
console.log('  tick: ' + state.tick);
console.log('  玩家数: ' + state.players.length);
state.players.forEach(p => {
    console.log(`    ${p.name}: hp=${p.hp} level=${p.level} gold=${p.gold}`);
});

let clientCount = 0;

const server = net.createServer((socket) => {
    state.reset();
    const clientNum = ++clientCount;
    console.log(`\n[客户端 #${clientNum}] 已连接 (${socket.remoteAddress}:${socket.remotePort})`);

    (async () => {
        try {
            while (true) {
                const queryData = await recvWithLength(socket);
                if (queryData.length === 0) break;
                const result = executeQuery(state, queryData);
                sendWithLength(socket, result);
            }
        } catch (e) { /* 客户端断开 */ }
        console.log(`[客户端 #${clientNum}] 已断开连接`);
    })();
});

server.listen(PORT, HOST, () => {
    console.log(`\n服务器正在监听 ${HOST}:${PORT}，等待客户端连接...`);
});

server.on('error', (err) => {
    console.error('服务器错误:', err.message);
    process.exit(1);
});