// 示例 09：SPOI 全语言跨语言数据互查（Kotlin 服务端）
// 展示：Kotlin 服务端托管游戏状态数据，通过 TCP 接收各语言客户端发送的 SPOI 查询指令，
//       执行查询后将结果序列化返回。

import java.io.*
import java.net.*
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.StandardCharsets

// ===== 操作码 =====
const val OP_SET     = 0x04
const val OP_ADD     = 0x05
const val OP_FILTER  = 0x0C
const val OP_SORT    = 0x0E
const val OP_REVERSE = 0x0F
const val OP_TAKE    = 0x10
const val OP_DROP    = 0x11
const val OP_COUNT   = 0x15
const val OP_ANY     = 0x16
const val OP_FIND    = 0x18
const val OP_EXEC    = 0x21

// ===== 比较运算符 =====
const val CMP_EQ = 0
const val CMP_NE = 1
const val CMP_LT = 2
const val CMP_GT = 3
const val CMP_LE = 4
const val CMP_GE = 5

// ===== 字段索引 =====
const val FIELD_PLAYER_NAME  = 0
const val FIELD_PLAYER_HP    = 1
const val FIELD_PLAYER_LEVEL = 2
const val FIELD_PLAYER_GOLD  = 3
const val FIELD_STATE_PLAYERS = 0

// ===== 结果类型 =====
const val RT_UNDEF    = 0
const val RT_SINGLE   = 1
const val RT_VECTOR   = 2
const val RT_COUNT    = 3
const val RT_BOOL     = 4
const val RT_OPTIONAL = 5
const val RT_ERROR    = 6

// ===== 二进制辅助 =====

fun u32le(v: Int): ByteArray =
    ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array()

fun i32le(v: Int): ByteArray = u32le(v)

fun writeVarint(v: Int): ByteArray {
    val bos = ByteArrayOutputStream()
    var x = v
    while (x >= 0x80) {
        bos.write((x and 0x7F) or 0x80)
        x = x ushr 7
    }
    bos.write(x and 0x7F)
    return bos.toByteArray()
}

fun readU32le(data: ByteArray, off: Int): Int =
    ByteBuffer.wrap(data, off, 4).order(ByteOrder.LITTLE_ENDIAN).int

fun readI32le(data: ByteArray, off: Int): Int =
    ByteBuffer.wrap(data, off, 4).order(ByteOrder.LITTLE_ENDIAN).int

fun applyCmpInt(a: Int, cmpOp: Int, b: Int): Boolean = when (cmpOp) {
    CMP_EQ -> a == b
    CMP_NE -> a != b
    CMP_LT -> a < b
    CMP_GT -> a > b
    CMP_LE -> a <= b
    CMP_GE -> a >= b
    else   -> false
}

fun applyCmpStr(a: String, cmpOp: Int, b: String): Boolean = when (cmpOp) {
    CMP_EQ -> a == b
    CMP_NE -> a != b
    CMP_LT -> a < b
    CMP_GT -> a > b
    CMP_LE -> a <= b
    CMP_GE -> a >= b
    else   -> false
}

// ===== 比较表达式 =====

data class CmpExpr(val memberIdx: Int, val cmpOp: Int, val value: ByteArray) {
    companion object {
        fun parse(data: ByteArray): CmpExpr {
            val memberIdx = readU32le(data, 0)
            val cmpOp = data[4].toInt() and 0xFF
            val valueLen = readU32le(data, 5)
            val value = data.copyOfRange(9, 9 + valueLen)
            return CmpExpr(memberIdx, cmpOp, value)
        }
    }

    fun getStrValue(): String? {
        if (value.size >= 4) {
            val slen = readU32le(value, 0)
            if (4 + slen == value.size) {
                return String(value, 4, slen, StandardCharsets.UTF_8)
            }
        }
        return null
    }

    fun getI32Value(): Int? {
        return if (value.size == 4) readI32le(value, 0) else null
    }
}

// ===== 玩家 =====

data class Player(var name: String, var hp: Int, var level: Int, var gold: Int) {
    fun getField(idx: Int): Any? = when (idx) {
        FIELD_PLAYER_NAME  -> name
        FIELD_PLAYER_HP    -> hp
        FIELD_PLAYER_LEVEL -> level
        FIELD_PLAYER_GOLD  -> gold
        else               -> null
    }

    fun setField(idx: Int, value: Any) {
        when (idx) {
            FIELD_PLAYER_NAME  -> name = value as String
            FIELD_PLAYER_HP    -> hp = value as Int
            FIELD_PLAYER_LEVEL -> level = value as Int
            FIELD_PLAYER_GOLD  -> gold = value as Int
        }
    }

    fun addField(idx: Int, delta: Int) {
        when (idx) {
            FIELD_PLAYER_HP    -> hp += delta
            FIELD_PLAYER_LEVEL -> level += delta
            FIELD_PLAYER_GOLD  -> gold += delta
        }
    }

    fun serialize(): ByteArray {
        val nameBytes = name.toByteArray(StandardCharsets.UTF_8)
        val result = ByteArray(4 + nameBytes.size + 12)
        System.arraycopy(u32le(nameBytes.size), 0, result, 0, 4)
        System.arraycopy(nameBytes, 0, result, 4, nameBytes.size)
        var off = 4 + nameBytes.size
        System.arraycopy(i32le(hp), 0, result, off, 4)
        System.arraycopy(i32le(level), 0, result, off + 4, 4)
        System.arraycopy(i32le(gold), 0, result, off + 8, 4)
        return result
    }
}

// ===== 游戏状态 =====

class GameState {
    val players = mutableListOf<Player>()
    var tick = 42
    var serverName = "KotlinServer"

    fun reset() {
        players.clear()
        players.add(Player("Alice", 80, 10, 500))
        players.add(Player("Bob",   30, 5,  200))
        players.add(Player("Carol", 60, 8,  300))
        players.add(Player("Dave",  90, 12, 400))
        players.add(Player("Eve",   15, 3,  100))
        tick = 42
        serverName = "KotlinServer"
    }

    fun comparePlayer(p: Player, expr: CmpExpr): Boolean {
        val value = p.getField(expr.memberIdx) ?: return false
        return when (value) {
            is String -> {
                val cmpVal = expr.getStrValue() ?: return false
                applyCmpStr(value, expr.cmpOp, cmpVal)
            }
            is Int -> {
                val cmpVal = expr.getI32Value() ?: return false
                applyCmpInt(value, expr.cmpOp, cmpVal)
            }
            else -> false
        }
    }
}

// ===== SPOI 执行器 =====

object SpoiExecutor {
    fun execute(state: GameState, queryData: ByteArray): ByteArray {
        try {
            if (queryData.size < 4) return makeError("查询数据太短")

            var offset = 0
            val instCount = readU32le(queryData, offset)
            offset += 4

            val pipeline = mutableListOf<Player>()
            var pipelineActive = false

            for (i in 0 until instCount) {
                if (offset >= queryData.size) break

                val op = queryData[offset++].toInt() and 0xFF
                val pathLen = readU32le(queryData, offset); offset += 4
                val path = IntArray(pathLen)
                for (j in 0 until pathLen) {
                    path[j] = readU32le(queryData, offset)
                    offset += 4
                }
                val operandLen = readU32le(queryData, offset); offset += 4
                val operand = queryData.copyOfRange(offset, offset + operandLen)
                offset += operandLen

                when (op) {
                    OP_FILTER -> {
                        val expr = CmpExpr.parse(operand)
                        if (!pipelineActive && pathLen == 1 && path[0] == FIELD_STATE_PLAYERS) {
                            state.players.filterTo(pipeline) { state.comparePlayer(it, expr) }
                            pipelineActive = true
                        } else if (pipelineActive) {
                            val filtered = pipeline.filter { state.comparePlayer(it, expr) }
                            pipeline.clear()
                            pipeline.addAll(filtered)
                        } else {
                            state.players.filterTo(pipeline) { state.comparePlayer(it, expr) }
                            pipelineActive = true
                        }
                    }

                    OP_SORT -> {
                        if (pipelineActive && operand.size >= 5) {
                            val field = readU32le(operand, 0)
                            val ascending = operand[4].toInt() and 0xFF != 0
                            pipeline.sortBy { (it.getField(field) as? Int) ?: 0 }
                            if (!ascending) pipeline.reverse()
                        }
                    }

                    OP_REVERSE -> {
                        if (pipelineActive) pipeline.reverse()
                    }

                    OP_TAKE -> {
                        if (pipelineActive && operand.size >= 4) {
                            val n = readU32le(operand, 0)
                            if (n < pipeline.size) {
                                val taken = pipeline.take(n)
                                pipeline.clear()
                                pipeline.addAll(taken)
                            }
                        }
                    }

                    OP_DROP -> {
                        if (pipelineActive && operand.size >= 4) {
                            val n = readU32le(operand, 0)
                            if (n < pipeline.size) {
                                val dropped = pipeline.drop(n)
                                pipeline.clear()
                                pipeline.addAll(dropped)
                            } else {
                                pipeline.clear()
                            }
                        }
                    }

                    OP_COUNT -> {
                        if (pipelineActive) return makeCount(pipeline.size)
                    }

                    OP_ANY -> {
                        if (pipelineActive) {
                            val expr = CmpExpr.parse(operand)
                            for (p in pipeline) {
                                if (state.comparePlayer(p, expr)) return makeBool(true)
                            }
                            return makeBool(false)
                        }
                    }

                    OP_FIND -> {
                        if (pipelineActive) {
                            val expr = CmpExpr.parse(operand)
                            for (p in pipeline) {
                                if (state.comparePlayer(p, expr)) return makeOptional(p)
                            }
                            return makeOptional(null)
                        }
                    }

                    OP_SET -> {
                        if (pathLen >= 3 && path[0] == FIELD_STATE_PLAYERS) {
                            val idx = path[1]
                            val field = path[2]
                            if (idx in 0 until state.players.size && operand.size >= 4) {
                                val value = readI32le(operand, 0)
                                state.players[idx].setField(field, value)
                            }
                        }
                        // 修改后继续处理后续指令
                    }

                    OP_ADD -> {
                        if (pathLen >= 3 && path[0] == FIELD_STATE_PLAYERS) {
                            val idx = path[1]
                            val field = path[2]
                            if (idx in 0 until state.players.size && operand.size >= 4) {
                                val delta = readI32le(operand, 0)
                                state.players[idx].addField(field, delta)
                            }
                        }
                        // 修改后继续处理后续指令
                    }

                    OP_EXEC -> {
                        if (pipelineActive) {
                            val countBuf = writeVarint(pipeline.size)
                            val bos = ByteArrayOutputStream()
                            bos.write(countBuf)
                            for (p in pipeline) {
                                bos.write(p.serialize())
                            }
                            return makeResult(RT_VECTOR, bos.toByteArray())
                        }
                        return makeResult(RT_UNDEF, ByteArray(0))
                    }
                }
            }
            return makeResult(RT_UNDEF, ByteArray(0))
        } catch (e: Exception) {
            return makeError("执行错误: ${e.message}")
        }
    }

    fun makeResult(type: Int, data: ByteArray): ByteArray {
        val result = ByteArray(5 + data.size)
        result[0] = type.toByte()
        System.arraycopy(u32le(data.size), 0, result, 1, 4)
        System.arraycopy(data, 0, result, 5, data.size)
        return result
    }

    fun makeCount(count: Int) = makeResult(RT_COUNT, i32le(count))
    fun makeBool(v: Boolean) = makeResult(RT_BOOL, byteArrayOf(if (v) 1 else 0))

    fun makeOptional(player: Player?): ByteArray {
        if (player == null) return makeResult(RT_OPTIONAL, byteArrayOf(0))
        val playerBytes = player.serialize()
        val data = ByteArray(1 + playerBytes.size)
        data[0] = 1
        System.arraycopy(playerBytes, 0, data, 1, playerBytes.size)
        return makeResult(RT_OPTIONAL, data)
    }

    fun makeError(msg: String) =
        makeResult(RT_ERROR, msg.toByteArray(StandardCharsets.UTF_8))
}

// ===== TCP 通信 =====

object Tcp {
    fun sendWithLength(out: OutputStream, data: ByteArray) {
        out.write(u32le(data.size))
        out.write(data)
        out.flush()
    }

    fun recvWithLength(input: InputStream): ByteArray {
        val lenBuf = ByteArray(4)
        var read = 0
        while (read < 4) {
            val n = input.read(lenBuf, read, 4 - read)
            if (n == -1) throw EOFException()
            read += n
        }
        val len = readU32le(lenBuf, 0)
        val data = ByteArray(len)
        read = 0
        while (read < len) {
            val n = input.read(data, read, len - read)
            if (n == -1) throw EOFException()
            read += n
        }
        return data
    }
}

// ===== 主程序 =====

fun main() {
    println("=== SPOI 全语言跨语言数据互查 — Kotlin 服务端 ===\n")

    val state = GameState()
    state.reset()

    println("游戏状态已初始化：")
    println("  服务器名称: ${state.serverName}")
    println("  tick: ${state.tick}")
    println("  玩家数: ${state.players.size}")
    for (p in state.players) {
        println("    ${p.name}: hp=${p.hp} level=${p.level} gold=${p.gold}")
    }

    val serverSocket = ServerSocket(9999, 50, InetAddress.getByName("127.0.0.1"))
    println("\n服务器正在监听 127.0.0.1:9999，等待客户端连接...")

    var clientNum = 0
    while (true) {
        try {
            val client = serverSocket.accept()
            clientNum++
            println("\n[客户端 #$clientNum] 已连接 (${client.remoteSocketAddress})")
            handleClient(client, state)
            println("[客户端 #$clientNum] 已断开连接")
        } catch (e: IOException) {
            System.err.println("服务器错误: ${e.message}")
        }
    }
}

fun handleClient(client: Socket, state: GameState) {
    try {
        state.reset()
        val input = client.getInputStream()
        val output = client.getOutputStream()

        while (true) {
            val queryData = Tcp.recvWithLength(input)
            val result = SpoiExecutor.execute(state, queryData)
            Tcp.sendWithLength(output, result)
        }
    } catch (e: EOFException) {
        // 正常断开
    } catch (e: IOException) {
        // 连接错误
    } finally {
        try { client.close() } catch (_: IOException) {}
    }
}