// 示例 09：SPOI 全语言跨语言数据互查（JavaScript/Node.js 服务端）
// 展示：Node.js 服务端托管游戏状态数据，通过 TCP 接收各语言客户端发送的 SPOI 查询指令，
//       执行查询后将结果序列化返回。

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
    EXEC:    0x21,
};

// 比较运算符
const CMP = {
    EQ: 0, NE: 1, LT: 2, GT: 3, LE: 4, GE: 5,
};

// 字段索引
const PLAYER_NAME  = 0;
const PLAYER_HP    = 1;
const PLAYER_LEVEL = 2;
const PLAYER_GOLD  = 3;
const STATE_PLAYERS = 0;

// 结果类型
const RESULT = {
    UNDEF: 0, SINGLE: 1, VECTOR: 2, COUNT: 3, BOOL: 4, OPTIONAL: 5, ERROR: 6,
};

// =============================== 二进制辅助函数 ===============================

function u32LE(v) {
    const buf = Buffer.allocUnsafe(4);
    buf.writeUInt32LE(v, 0);
    return buf;
}

function i32LE(v) {
    const buf = Buffer.allocUnsafe(4);
    buf.writeInt32LE(v, 0);
    return buf;
}

function readU32LE(buf, offset) {
    return { value: buf.readUInt32LE(offset), nextOffset: offset + 4 };
}

function readI32LE(buf, offset) {
    return { value: buf.readInt32LE(offset), nextOffset: offset + 4 };
}

function readU8(buf, offset) {
    return { value: buf.readUInt8(offset), nextOffset: offset + 1 };
}

function writeVarint(v) {
    const parts = [];
    while (v >= 0x80) {
        parts.push((v & 0x7F) | 0x80);
        v >>>= 7;
    }
    parts.push(v & 0x7F);
    return Buffer.from(parts);
}

function applyCmp(a, op, b) {
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

// =============================== 游戏状态 ===============================

class Player {
    constructor(name, hp, level, gold) {
        this.name = name;
        this.hp = hp;
        this.level = level;
        this.gold = gold;
    }

    getField(idx) {
        switch (idx) {
            case PLAYER_NAME:  return this.name;
            case PLAYER_HP:    return this.hp;
            case PLAYER_LEVEL: return this.level;
            case PLAYER_GOLD:  return this.gold;
            default: return null;
        }
    }

    setField(idx, value) {
        switch (idx) {
            case PLAYER_NAME:  this.name = value; break;
            case PLAYER_HP:    this.hp = value; break;
            case PLAYER_LEVEL: this.level = value; break;
            case PLAYER_GOLD:  this.gold = value; break;
        }
    }

    addField(idx, delta) {
        switch (idx) {
            case PLAYER_HP:    this.hp += delta; break;
            case PLAYER_LEVEL: this.level += delta; break;
            case PLAYER_GOLD:  this.gold += delta; break;
        }
    }

    serialize() {
        const nameBytes = Buffer.from(this.name, 'utf-8');
        return Buffer.concat([
            u32LE(nameBytes.length), nameBytes,
            i32LE(this.hp), i32LE(this.level), i32LE(this.gold),
        ]);
    }
}

class GameState {
    constructor() {
        this.players = [];
        this.tick = 42;
        this.serverName = 'JSServer';
    }

    reset() {
        this.players = [
            new Player('Alice', 80, 10, 500),
            new Player('Bob',   30, 5,  200),
            new Player('Carol', 60, 8,  300),
            new Player('Dave',  90, 12, 400),
            new Player('Eve',   15, 3,  100),
        ];
        this.tick = 42;
        this.serverName = 'JSServer';
    }
}

// =============================== SPOI 指令解析与执行 ===============================

function parseCmpExpr(data, offset) {
    const memberIdx = data.readUInt32LE(offset);
    offset += 4;
    const cmpOp = data.readUInt8(offset);
    offset += 1;
    const valueLen = data.readUInt32LE(offset);
    offset += 4;
    const value = data.slice(offset, offset + valueLen);
    offset += valueLen;
    return { expr: { memberIdx, cmpOp, value }, nextOffset: offset };
}

function parseCmpValueStr(expr) {
    const v = expr.value;
    if (v.length >= 4) {
        const slen = v.readUInt32LE(0);
        if (4 + slen === v.length) {
            return v.toString('utf-8', 4, 4 + slen);
        }
    }
    return null;
}

function parseCmpValueI32(expr) {
    if (expr.value.length === 4) {
        return expr.value.readInt32LE(0);
    }
    return null;
}

function comparePlayer(player, expr) {
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

function executeQuery(state, queryData) {
    try {
        if (queryData.length < 4) {
            return makeErrorResult('查询数据太短');
        }

        let offset = 0;
        const instCount = queryData.readUInt32LE(offset);
        offset += 4;

        let pipeline = [];
        let pipelineActive = false;

        for (let i = 0; i < instCount; i++) {
            if (offset >= queryData.length) break;

            const op = queryData.readUInt8(offset);
            offset += 1;
            const pathLen = queryData.readUInt32LE(offset);
            offset += 4;
            const path = [];
            for (let j = 0; j < pathLen; j++) {
                path.push(queryData.readUInt32LE(offset));
                offset += 4;
            }
            const operandLen = queryData.readUInt32LE(offset);
            offset += 4;
            const operand = queryData.slice(offset, offset + operandLen);
            offset += operandLen;

            if (op === OP.FILTER) {
                if (!pipelineActive && path.length === 1 && path[0] === STATE_PLAYERS) {
                    const { expr } = parseCmpExpr(operand, 0);
                    pipeline = state.players.filter(p => comparePlayer(p, expr));
                    pipelineActive = true;
                } else if (pipelineActive) {
                    const { expr } = parseCmpExpr(operand, 0);
                    pipeline = pipeline.filter(p => comparePlayer(p, expr));
                } else {
                    const { expr } = parseCmpExpr(operand, 0);
                    pipeline = state.players.filter(p => comparePlayer(p, expr));
                    pipelineActive = true;
                }
            }
            else if (op === OP.SORT) {
                if (pipelineActive && operand.length >= 5) {
                    const field = operand.readUInt32LE(0);
                    const ascending = operand[4] !== 0;
                    pipeline.sort((a, b) => {
                        const va = a.getField(field) || 0;
                        const vb = b.getField(field) || 0;
                        return ascending ? va - vb : vb - va;
                    });
                }
            }
            else if (op === OP.REVERSE) {
                if (pipelineActive) pipeline.reverse();
            }
            else if (op === OP.TAKE) {
                if (pipelineActive && operand.length >= 4) {
                    const n = operand.readUInt32LE(0);
                    pipeline = pipeline.slice(0, n);
                }
            }
            else if (op === OP.DROP) {
                if (pipelineActive && operand.length >= 4) {
                    const n = operand.readUInt32LE(0);
                    pipeline = pipeline.slice(n);
                }
            }
            else if (op === OP.COUNT) {
                if (pipelineActive) return makeCountResult(pipeline.length);
            }
            else if (op === OP.ANY) {
                if (pipelineActive) {
                    const { expr } = parseCmpExpr(operand, 0);
                    const result = pipeline.some(p => comparePlayer(p, expr));
                    return makeBoolResult(result);
                }
            }
            else if (op === OP.FIND) {
                if (pipelineActive) {
                    const { expr } = parseCmpExpr(operand, 0);
                    const found = pipeline.find(p => comparePlayer(p, expr));
                    return makeOptionalResult(found || null);
                }
            }
            else if (op === OP.SET) {
                if (path.length >= 3 && path[0] === STATE_PLAYERS) {
                    const idx = path[1];
                    const field = path[2];
                    if (idx >= 0 && idx < state.players.length && operand.length >= 4) {
                        const val = operand.readInt32LE(0);
                        state.players[idx].setField(field, val);
                    }
                }
                // 修改后继续处理后续指令
            }
            else if (op === OP.ADD) {
                if (path.length >= 3 && path[0] === STATE_PLAYERS) {
                    const idx = path[1];
                    const field = path[2];
                    if (idx >= 0 && idx < state.players.length && operand.length >= 4) {
                        const delta = operand.readInt32LE(0);
                        state.players[idx].addField(field, delta);
                    }
                }
                // 修改后继续处理后续指令
            }
            else if (op === OP.EXEC) {
                if (pipelineActive) {
                    const countBuf = writeVarint(pipeline.length);
                    const playersBuf = Buffer.concat(pipeline.map(p => p.serialize()));
                    return makeResult(RESULT.VECTOR, Buffer.concat([countBuf, playersBuf]));
                } else {
                    return makeResult(RESULT.UNDEF, Buffer.alloc(0));
                }
            }
        }

        return makeResult(RESULT.UNDEF, Buffer.alloc(0));
    } catch (e) {
        return makeErrorResult('执行错误: ' + e.message);
    }
}

// =============================== 结果构建 ===============================

function makeResult(resultType, data) {
    const header = Buffer.allocUnsafe(5);
    header.writeUInt8(resultType, 0);
    header.writeUInt32LE(data.length, 1);
    return Buffer.concat([header, data]);
}

function makeCountResult(count) {
    return makeResult(RESULT.COUNT, i32LE(count));
}

function makeBoolResult(val) {
    return makeResult(RESULT.BOOL, Buffer.from([val ? 1 : 0]));
}

function makeOptionalResult(player) {
    if (player === null) {
        return makeResult(RESULT.OPTIONAL, Buffer.from([0]));
    }
    return makeResult(RESULT.OPTIONAL, Buffer.concat([Buffer.from([1]), player.serialize()]));
}

function makeErrorResult(msg) {
    return makeResult(RESULT.ERROR, Buffer.from(msg, 'utf-8'));
}

// =============================== TCP 通信 ===============================

function sendWithLength(socket, data) {
    const lenBuf = u32LE(data.length);
    socket.write(Buffer.concat([lenBuf, data]));
}

/**
 * 从 socket 读取一条长度前缀帧，使用持久化缓冲区
 */
function recvWithLength(socket) {
    return new Promise((resolve, reject) => {
        if (!socket._recvBuffer) {
            socket._recvBuffer = Buffer.alloc(0);
            socket._recvPending = [];

            socket.on('data', (chunk) => {
                socket._recvBuffer = Buffer.concat([socket._recvBuffer, chunk]);

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

        socket._recvPending.push({ resolve, reject });
    });
}

// =============================== 主程序 ===============================

const HOST = '127.0.0.1';
const PORT = 9999;

const state = new GameState();
state.reset();

console.log('=== SPOI 全语言跨语言数据互查 — JavaScript/Node.js 服务端 ===\n');
console.log('游戏状态已初始化：');
console.log('  服务器名称: ' + state.serverName);
console.log('  tick: ' + state.tick);
console.log('  玩家数: ' + state.players.length);
state.players.forEach(p => {
    console.log('    ' + p.name + ': hp=' + p.hp + ' level=' + p.level + ' gold=' + p.gold);
});

const server = net.createServer((socket) => {
    state.reset();
    let clientNum = ++server._clientCount;
    console.log('\n[客户端 #' + clientNum + '] 已连接 (' + socket.remoteAddress + ':' + socket.remotePort + ')');

    (async () => {
        try {
            while (true) {
                const queryData = await recvWithLength(socket);
                if (queryData.length === 0) break;

                const result = executeQuery(state, queryData);
                sendWithLength(socket, result);
            }
        } catch (e) {
            // 客户端断开
        }
        console.log('[客户端 #' + clientNum + '] 已断开连接');
    })();
});

server._clientCount = 0;

server.listen(PORT, HOST, () => {
    console.log('\n服务器正在监听 ' + HOST + ':' + PORT + '，等待客户端连接...');
});

server.on('error', (err) => {
    console.error('服务器错误:', err.message);
    process.exit(1);
});