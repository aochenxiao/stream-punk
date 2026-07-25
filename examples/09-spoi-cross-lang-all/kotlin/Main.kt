// SPOI 跨语言数据互查 — Kotlin 客户端
// 展示：Kotlin 客户端通过 TCP 向 C++ 服务器发送 SPOI 查询指令，接收并展示查询结果。
// 使用 Kotlin 标准库（java.net.Socket, java.nio.ByteBuffer）手工构建 SPOI 二进制协议。

import java.io.ByteArrayOutputStream
import java.net.Socket
import java.nio.ByteBuffer
import java.nio.ByteOrder

// ===== 协议常量 =====

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

const val CMP_EQ = 0
const val CMP_NE = 1
const val CMP_LT = 2
const val CMP_GT = 3
const val CMP_LE = 4
const val CMP_GE = 5

const val PLAYER_NAME  = 0
const val PLAYER_HP    = 1
const val PLAYER_LEVEL = 2
const val PLAYER_GOLD  = 3
const val STATE_PLAYERS = 0

// 结果类型
const val RT_UNDEF    = 0
const val RT_SINGLE   = 1
const val RT_VECTOR   = 2
const val RT_COUNT    = 3
const val RT_BOOL     = 4
const val RT_OPTIONAL = 5
const val RT_ERROR    = 6

// ===== 序列化工具函数 =====

/** 将 Int 序列化为 4 字节小端字节数组 */
fun i32le(v: Int): ByteArray =
    ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array()

/** 将 Int 序列化为 4 字节小端字节数组（无符号语义，仅序列化） */
fun u32le(v: Int): ByteArray = i32le(v)

/** 序列化字符串：4 字节长度前缀（u32 LE） + UTF-8 字节 */
fun strBytes(s: String): ByteArray {
    val utf8 = s.toByteArray(Charsets.UTF_8)
    return u32le(utf8.size) + utf8
}

/** 构建 SpoiCmpExpr 的字节表示 */
fun cmpExprBytes(memberIdx: Int, cmpOp: Int, value: ByteArray): ByteArray {
    val bos = ByteArrayOutputStream()
    bos.write(u32le(memberIdx))   // memberIdx: u32 LE
    bos.write(cmpOp)              // cmpOp: u8
    bos.write(u32le(value.size))  // value 长度: u32 LE
    bos.write(value)              // value 字节
    return bos.toByteArray()
}

// ===== SPOI 查询构建器 =====

class SpoiQueryBuilder {
    data class Instruction(val op: Int, val path: List<Int>, val operand: ByteArray)

    private val instructions = mutableListOf<Instruction>()

    private fun addInst(op: Int, path: List<Int>, operand: ByteArray) {
        instructions.add(Instruction(op, path, operand))
    }

    /** fromPlayers — 发送 FILTER 指令，路径=[STATE_PLAYERS]，操作数=cmpExpr(hp >= 0) */
    fun fromPlayers(): SpoiQueryBuilder {
        addInst(OP_FILTER, listOf(STATE_PLAYERS), cmpExprBytes(PLAYER_HP, CMP_GE, i32le(0)))
        return this
    }

    /** filter — 按整数字段过滤 */
    fun filter(field: Int, cmpOp: Int, value: Int): SpoiQueryBuilder {
        addInst(OP_FILTER, emptyList(), cmpExprBytes(field, cmpOp, i32le(value)))
        return this
    }

    /** filterStr — 按字符串字段过滤（用于 find 的字符串比较） */
    fun filterStr(field: Int, cmpOp: Int, value: String): SpoiQueryBuilder {
        addInst(OP_FILTER, emptyList(), cmpExprBytes(field, cmpOp, strBytes(value)))
        return this
    }

    /** sort — 排序 */
    fun sort(field: Int, ascending: Boolean): SpoiQueryBuilder {
        val operand = u32le(field) + byteArrayOf(if (ascending) 1 else 0)
        addInst(OP_SORT, emptyList(), operand)
        return this
    }

    /** reverse — 反转 */
    fun reverse(): SpoiQueryBuilder {
        addInst(OP_REVERSE, emptyList(), ByteArray(0))
        return this
    }

    /** take — 取前 n 个 */
    fun take(n: Int): SpoiQueryBuilder {
        addInst(OP_TAKE, emptyList(), u32le(n))
        return this
    }

    /** drop — 丢弃前 n 个 */
    fun drop(n: Int): SpoiQueryBuilder {
        addInst(OP_DROP, emptyList(), u32le(n))
        return this
    }

    /** count — 统计数量 */
    fun count(): SpoiQueryBuilder {
        addInst(OP_COUNT, emptyList(), ByteArray(0))
        return this
    }

    /** any — 是否存在满足条件的元素 */
    fun any(field: Int, cmpOp: Int, value: Int): SpoiQueryBuilder {
        addInst(OP_ANY, emptyList(), cmpExprBytes(field, cmpOp, i32le(value)))
        return this
    }

    /** find — 查找满足条件的第一个元素 */
    fun find(field: Int, cmpOp: Int, value: Int): SpoiQueryBuilder {
        addInst(OP_FIND, emptyList(), cmpExprBytes(field, cmpOp, i32le(value)))
        return this
    }

    /** findStr — 按字符串字段查找 */
    fun findStr(field: Int, value: String): SpoiQueryBuilder {
        addInst(OP_FIND, emptyList(), cmpExprBytes(field, CMP_EQ, strBytes(value)))
        return this
    }

    /** set — 写入值 */
    fun set(path: List<Int>, value: Int): SpoiQueryBuilder {
        addInst(OP_SET, path, i32le(value))
        return this
    }

    /** add — 增加值 */
    fun add(path: List<Int>, delta: Int): SpoiQueryBuilder {
        addInst(OP_ADD, path, i32le(delta))
        return this
    }

    /** 构建完整的 SpoiStream 二进制（自动追加 EXEC 指令） */
    fun build(): ByteArray {
        addInst(OP_EXEC, emptyList(), ByteArray(0))

        val bos = ByteArrayOutputStream()
        val tmp = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)

        fun writeU32(v: Int) {
            tmp.clear()
            tmp.putInt(v)
            bos.write(tmp.array())
        }

        fun writeU8(v: Int) {
            bos.write(v)
        }

        fun writeBytes(b: ByteArray) {
            if (b.isNotEmpty()) {
                bos.write(b)
            }
        }

        // SpoiStream: [指令数: u32 LE] + [指令...]
        writeU32(instructions.size)
        for (inst in instructions) {
            // SpoiInstruction: [op: u8] + [路径长度: u32 LE] + [路径段: u32 LE × N] + [操作数长度: u32 LE] + [操作数字节]
            writeU8(inst.op)
            writeU32(inst.path.size)
            for (seg in inst.path) {
                writeU32(seg)
            }
            writeU32(inst.operand.size)
            writeBytes(inst.operand)
        }
        return bos.toByteArray()
    }
}

// ===== TCP 通信 =====

/** 发送带长度前缀的数据：[4字节 u32 LE 数据长度] + [数据] */
fun Socket.sendWithLength(data: ByteArray) {
    val lenBuf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(data.size)
    getOutputStream().write(lenBuf.array())
    if (data.isNotEmpty()) {
        getOutputStream().write(data)
    }
    getOutputStream().flush()
}

/** 接收带长度前缀的数据 */
fun Socket.recvWithLength(): ByteArray {
    val lenBuf = ByteArray(4)
    var read = 0
    while (read < 4) {
        val n = getInputStream().read(lenBuf, read, 4 - read)
        if (n == -1) return ByteArray(0)
        read += n
    }
    val len = ByteBuffer.wrap(lenBuf).order(ByteOrder.LITTLE_ENDIAN).int
    if (len == 0) return ByteArray(0)
    val data = ByteArray(len)
    read = 0
    while (read < len) {
        val n = getInputStream().read(data, read, len - read)
        if (n == -1) return ByteArray(0)
        read += n
    }
    return data
}

// ===== varint 解码 =====

/** 读取无符号 LEB128 varint */
fun ByteBuffer.readVarint(): Int {
    var result = 0
    var shift = 0
    while (true) {
        val b = get().toInt() and 0xFF
        result = result or ((b and 0x7F) shl shift)
        if ((b and 0x80) == 0) break
        shift += 7
    }
    return result
}

// ===== CrossPlayer 反序列化 =====

/** 从 ByteBuffer 读取一个 CrossPlayer */
fun ByteBuffer.readPlayer(): String {
    val nameLen = int  // u32 LE
    val nameBytes = ByteArray(nameLen)
    get(nameBytes)
    val name = String(nameBytes, Charsets.UTF_8)
    val hp = int
    val level = int
    val gold = int
    return "Player{name='$name', hp=$hp, level=$level, gold=$gold}"
}

// ===== 结果解析与打印 =====

/** 解析并打印 SpoiResult */
fun printResult(data: ByteArray) {
    if (data.isEmpty()) {
        println("(空结果)")
        return
    }

    val buf = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN)

    // SpoiResult: [resultType: u8] + [data长度: u32 LE] + [data字节]
    val resultType = buf.get().toInt() and 0xFF
    val dataLen = buf.int
    val innerData = ByteArray(dataLen)
    buf.get(innerData)

    when (resultType) {
        RT_COUNT -> {
            if (innerData.size >= 4) {
                val count = ByteBuffer.wrap(innerData).order(ByteOrder.LITTLE_ENDIAN).int
                println("计数结果: $count")
            }
        }

        RT_BOOL -> {
            val value = innerData[0].toInt() and 0xFF
            println("布尔结果: ${if (value != 0) "true" else "false"}")
        }

        RT_VECTOR -> {
            val innerBuf = ByteBuffer.wrap(innerData).order(ByteOrder.LITTLE_ENDIAN)
            val count = innerBuf.readVarint()
            println("向量结果: $count 个元素")
            for (idx in 0 until count) {
                val playerStr = innerBuf.readPlayer()
                println("    [$idx] $playerStr")
            }
        }

        RT_SINGLE -> {
            val innerBuf = ByteBuffer.wrap(innerData).order(ByteOrder.LITTLE_ENDIAN)
            val playerStr = innerBuf.readPlayer()
            println("单个结果: $playerStr")
        }

        RT_OPTIONAL -> {
            if (innerData.isNotEmpty() && innerData[0].toInt() and 0xFF != 0) {
                val innerBuf = ByteBuffer.wrap(innerData, 1, innerData.size - 1).order(ByteOrder.LITTLE_ENDIAN)
                val playerStr = innerBuf.readPlayer()
                println("可选结果: 有值 → $playerStr")
            } else {
                println("可选结果: 空")
            }
        }

        RT_ERROR -> {
            val errMsg = String(innerData, Charsets.UTF_8)
            println("错误: $errMsg")
        }

        else -> println("未知结果类型: $resultType")
    }
}

// ===== 主程序 =====

fun main() {
    // 显式设置标准输出为 UTF-8，避免 Windows 下 GBK 编码导致中文乱码
    System.setOut(java.io.PrintStream(System.out, true, "UTF-8"))
    println("=== SPOI 跨语言数据互查 — Kotlin 客户端 ===\n")

    val host = "127.0.0.1"
    val port = 9999

    val socket = try {
        Socket(host, port).also {
            println("已连接到服务器 $host:$port\n")
        }
    } catch (e: Exception) {
        System.err.println("无法连接到服务器 $host:$port\n请确保 C++ 服务器已启动！")
        return
    }

    socket.use { sock ->
        var testNum = 0

        // 查询 1: 统计玩家总数
        println("--- 查询 ${++testNum}: 统计玩家总数 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().count().build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 2: 过滤 hp > 50
        println("--- 查询 ${++testNum}: 过滤 hp > 50 的玩家 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_GT, 50).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 3: 过滤 level >= 8，取前 2 个
        println("--- 查询 ${++testNum}: 过滤 level >= 8，取前 2 个 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_LEVEL, CMP_GE, 8).take(2).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 4: 查找名为 "Alice" 的玩家
        println("--- 查询 ${++testNum}: 查找名为 \"Alice\" 的玩家 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().findStr(PLAYER_NAME, "Alice").build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 5: 按 hp 降序排列，取前 3 个
        println("--- 查询 ${++testNum}: 按 hp 降序排列，取前 3 个 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().sort(PLAYER_HP, false).take(3).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 6: 检查是否有 hp < 20 的玩家
        println("--- 查询 ${++testNum}: 检查是否有 hp < 20 的玩家 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().any(PLAYER_HP, CMP_LT, 20).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 7: 统计 hp > 0 的玩家数
        println("--- 查询 ${++testNum}: 统计 hp > 0 的玩家数 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_GT, 0).count().build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 8: 复杂链式查询
        println("--- 查询 ${++testNum}: 复杂链式查询（filter + sort + reverse + take） ---")
        println("    (hp > 30 → 按 level 排序 → 反转 → 取前 2)")
        run {
            val query = SpoiQueryBuilder()
                .fromPlayers()
                .filter(PLAYER_HP, CMP_GT, 30)
                .sort(PLAYER_LEVEL, true)
                .reverse()
                .take(2)
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 9: 写操作 — 设置 hp
        println("--- 查询 ${++testNum}: 写操作 — 将玩家[0]的 hp 设置为 99 ---")
        run {
            val query = SpoiQueryBuilder().set(listOf(0, 0, PLAYER_HP), 99).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 10: 验证写操作
        println("--- 查询 ${++testNum}: 验证写操作 — 查找 Alice 的 hp 是否变为 99 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().findStr(PLAYER_NAME, "Alice").build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 11: 写操作 — 增加金币
        println("--- 查询 ${++testNum}: 写操作 — 给玩家[0]增加 100 金币 ---")
        run {
            val query = SpoiQueryBuilder().add(listOf(0, 0, PLAYER_GOLD), 100).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 12: 验证金币增加
        println("--- 查询 ${++testNum}: 验证写操作 — 查找 Alice 的金币是否变为 600 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().findStr(PLAYER_NAME, "Alice").build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 13: filter + drop
        println("--- 查询 ${++testNum}: filter(hp > 20) + drop(2) ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_GT, 20).drop(2).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // ===== L1: 多条件组合过滤 (3个) =====

        // 查询 14: hp>30 AND level>5
        println("--- 查询 ${++testNum}: 【L1】hp>30 AND level>5 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_GT, 30).filter(PLAYER_LEVEL, CMP_GT, 5).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 15: hp>30 AND level<=5
        println("--- 查询 ${++testNum}: 【L1】hp>30 AND level<=5 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_GT, 30).filter(PLAYER_LEVEL, CMP_LE, 5).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 16: hp>20 AND level>3 AND gold>200
        println("--- 查询 ${++testNum}: 【L1】hp>20 AND level>3 AND gold>200 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_GT, 20).filter(PLAYER_LEVEL, CMP_GT, 3).filter(PLAYER_GOLD, CMP_GT, 200).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // ===== L2: 边界条件 (6个) =====

        // 查询 17: hp>9000 (空结果)
        println("--- 查询 ${++testNum}: 【L2】hp>9000 (空结果) ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_GT, 9000).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 18: TAKE(100)
        println("--- 查询 ${++testNum}: 【L2】TAKE(100) ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().take(100).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 19: DROP(100)
        println("--- 查询 ${++testNum}: 【L2】DROP(100) ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().drop(100).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 20: DROP(100)+COUNT
        println("--- 查询 ${++testNum}: 【L2】DROP(100)+COUNT ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().drop(100).count().build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 21: 空管道FIND
        println("--- 查询 ${++testNum}: 【L2】空管道FIND ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_GT, 9000).find(PLAYER_HP, CMP_GT, 100).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 22: 空管道ANY
        println("--- 查询 ${++testNum}: 【L2】空管道ANY ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_GT, 9000).any(PLAYER_HP, CMP_GT, 100).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // ===== L3: 复杂管道 (3个) =====

        // 查询 23: SORT(level,asc)+DROP(2)+TAKE(2)
        println("--- 查询 ${++testNum}: 【L3】SORT(level,asc)+DROP(2)+TAKE(2) ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().sort(PLAYER_LEVEL, true).drop(2).take(2).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 24: SORT(hp,desc)+REVERSE+DROP(1)+TAKE(2)+COUNT
        println("--- 查询 ${++testNum}: 【L3】SORT(hp,desc)+REVERSE+DROP(1)+TAKE(2)+COUNT ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().sort(PLAYER_HP, false).reverse().drop(1).take(2).count().build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 25: SORT(level,asc)+REVERSE+DROP(1)+TAKE(3)+FILTER(hp>40)
        println("--- 查询 ${++testNum}: 【L3】SORT(level,asc)+REVERSE+DROP(1)+TAKE(3)+FILTER(hp>40) ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().sort(PLAYER_LEVEL, true).reverse().drop(1).take(3).filter(PLAYER_HP, CMP_GT, 40).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // ===== L4: 字符串操作 (3个) =====

        // 查询 26: name NE "Alice"
        println("--- 查询 ${++testNum}: 【L4】name NE \"Alice\" ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filterStr(PLAYER_NAME, CMP_NE, "Alice").build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 27: name LT "Carol"
        println("--- 查询 ${++testNum}: 【L4】name LT \"Carol\" ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filterStr(PLAYER_NAME, CMP_LT, "Carol").build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 28: FIND "Zoe"
        println("--- 查询 ${++testNum}: 【L4】FIND \"Zoe\" ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().findStr(PLAYER_NAME, "Zoe").build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // ===== L5: 写后查询 (3个) =====

        // 查询 29: SET hp=50, ADD hp=30, FIND Alice
        println("--- 查询 ${++testNum}: 【L5】SET hp=50, ADD hp=30, FIND Alice ---")
        run {
            val query = SpoiQueryBuilder().set(listOf(0, 0, PLAYER_HP), 50).add(listOf(0, 0, PLAYER_HP), 30).fromPlayers().findStr(PLAYER_NAME, "Alice").build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 30: SET Alice hp=999, SET Bob gold=9999, FILTER gold>9000
        println("--- 查询 ${++testNum}: 【L5】SET Alice hp=999, SET Bob gold=9999, FILTER gold>9000 ---")
        run {
            val query = SpoiQueryBuilder().set(listOf(0, 0, PLAYER_HP), 999).set(listOf(0, 1, PLAYER_GOLD), 9999).fromPlayers().filter(PLAYER_GOLD, CMP_GT, 9000).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 31: ADD gold=-300, FIND Alice
        println("--- 查询 ${++testNum}: 【L5】ADD gold=-300, FIND Alice ---")
        run {
            val query = SpoiQueryBuilder().add(listOf(0, 0, PLAYER_GOLD), -300).fromPlayers().findStr(PLAYER_NAME, "Alice").build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // ===== L6: 全比较运算符 (6个) =====

        // 查询 32: hp EQ 60
        println("--- 查询 ${++testNum}: 【L6】hp EQ 60 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_EQ, 60).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 33: hp NE 60
        println("--- 查询 ${++testNum}: 【L6】hp NE 60 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_NE, 60).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 34: hp LT 60
        println("--- 查询 ${++testNum}: 【L6】hp LT 60 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_LT, 60).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 35: hp GT 60
        println("--- 查询 ${++testNum}: 【L6】hp GT 60 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_GT, 60).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 36: hp LE 60
        println("--- 查询 ${++testNum}: 【L6】hp LE 60 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_LE, 60).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 37: hp GE 60
        println("--- 查询 ${++testNum}: 【L6】hp GE 60 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_GE, 60).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // ===== L7: 极限链 (1个) =====

        // 查询 38: fromPlayers + sort(level,true) + reverse() + drop(1) + take(4) + filter(hp>20) + sort(hp,false) + reverse() + take(2) + count()
        println("--- 查询 ${++testNum}: 【L7】极限链：sort(level,asc)+reverse+drop(1)+take(4)+filter(hp>20)+sort(hp,desc)+reverse+take(2)+count ---")
        run {
            val query = SpoiQueryBuilder()
                .fromPlayers()
                .sort(PLAYER_LEVEL, true)
                .reverse()
                .drop(1)
                .take(4)
                .filter(PLAYER_HP, CMP_GT, 20)
                .sort(PLAYER_HP, false)
                .reverse()
                .take(2)
                .count()
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // ===== L8: 管道操作边缘情况 (6个) =====

        // 查询 39: REVERSE x2 → 应与原始顺序相同
        println("--- 查询 ${++testNum}: 【L8】REVERSE x2 → 应与原始顺序相同 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().reverse().reverse().build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 40: TAKE(0) → 取0个元素（空向量）
        println("--- 查询 ${++testNum}: 【L8】TAKE(0) → 取0个元素（空向量） ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().take(0).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 41: DROP(0) → 丢弃0个（应返回全部）
        println("--- 查询 ${++testNum}: 【L8】DROP(0) → 丢弃0个（应返回全部） ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().drop(0).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 42: SORT 覆盖 → SORT(level,asc) + SORT(hp,desc)（以最后一次排序为准）
        println("--- 查询 ${++testNum}: 【L8】SORT覆盖 → SORT(level,asc)+SORT(hp,desc) ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().sort(PLAYER_LEVEL, true).sort(PLAYER_HP, false).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 43: REVERSE x3 → 等同于单次 REVERSE
        println("--- 查询 ${++testNum}: 【L8】REVERSE x3 → 等同于单次 REVERSE ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().reverse().reverse().reverse().build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 44: DROP 到只剩 1 个 + TAKE(1)
        println("--- 查询 ${++testNum}: 【L8】DROP到只剩1个 + TAKE(1) ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().drop(4).take(1).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // ===== L9: 数值边界与极端值 (6个) =====

        // 查询 45: FILTER hp < 0 → 无玩家 hp 为负
        println("--- 查询 ${++testNum}: 【L9】FILTER hp < 0 → 无玩家hp为负 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_LT, 0).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 46: SET hp=0, FILTER hp EQ 0 → 零值精确匹配
        println("--- 查询 ${++testNum}: 【L9】SET hp=0, FILTER hp EQ 0 → 零值精确匹配 ---")
        run {
            val query = SpoiQueryBuilder().set(listOf(0, 0, PLAYER_HP), 0).fromPlayers().filter(PLAYER_HP, CMP_EQ, 0).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 47: ADD 负值使金币变负, FILTER gold < 0
        println("--- 查询 ${++testNum}: 【L9】ADD负值使金币变负, FILTER gold < 0 ---")
        run {
            val query = SpoiQueryBuilder().add(listOf(0, 0, PLAYER_GOLD), -1000).fromPlayers().filter(PLAYER_GOLD, CMP_LT, 0).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 48: 互斥条件 → FILTER hp>0, FILTER hp<=0（必然空）
        println("--- 查询 ${++testNum}: 【L9】互斥条件 → hp>0 AND hp<=0（必然空） ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_GT, 0).filter(PLAYER_HP, CMP_LE, 0).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 49: FILTER level = 0 → 不存在 level=0 的玩家
        println("--- 查询 ${++testNum}: 【L9】FILTER level = 0 → 不存在level=0的玩家 ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_LEVEL, CMP_EQ, 0).build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 50: FILTER hp >= 0（全部通过） + COUNT
        println("--- 查询 ${++testNum}: 【L9】FILTER hp>=0（全部通过）+COUNT ---")
        run {
            val query = SpoiQueryBuilder().fromPlayers().filter(PLAYER_HP, CMP_GE, 0).count().build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // ===== L10: 写操作与管道混合 (5个) =====

        // 查询 51: 多次 SET 后管道查询 → 改3个玩家hp，然后 FILTER + SORT
        println("--- 查询 ${++testNum}: 【L10】多次SET后管道查询 → 改3个玩家hp, FILTER+SORT ---")
        run {
            val query = SpoiQueryBuilder()
                .set(listOf(0, 0, PLAYER_HP), 45)
                .set(listOf(0, 1, PLAYER_HP), 55)
                .set(listOf(0, 2, PLAYER_HP), 65)
                .fromPlayers()
                .filter(PLAYER_HP, CMP_GT, 50)
                .sort(PLAYER_HP, false)
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 52: SET + ADD 同一字段后查询
        println("--- 查询 ${++testNum}: 【L10】SET+ADD同一字段后查询 ---")
        run {
            val query = SpoiQueryBuilder()
                .set(listOf(0, 0, PLAYER_LEVEL), 10)
                .add(listOf(0, 0, PLAYER_LEVEL), -2)
                .fromPlayers()
                .findStr(PLAYER_NAME, "Alice")
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 53: ADD 全部玩家 level+1, 然后 FILTER + COUNT
        println("--- 查询 ${++testNum}: 【L10】ADD全部玩家level+1, FILTER+COUNT ---")
        run {
            val query = SpoiQueryBuilder()
                .add(listOf(0, 0, PLAYER_LEVEL), 1)
                .add(listOf(0, 1, PLAYER_LEVEL), 1)
                .add(listOf(0, 2, PLAYER_LEVEL), 1)
                .add(listOf(0, 3, PLAYER_LEVEL), 1)
                .add(listOf(0, 4, PLAYER_LEVEL), 1)
                .fromPlayers()
                .filter(PLAYER_LEVEL, CMP_GT, 10)
                .count()
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 54: SET 不存在索引 [0,99] → 应静默忽略，无玩家 hp>9000
        println("--- 查询 ${++testNum}: 【L10】SET不存在索引[0,99] → 应静默忽略 ---")
        run {
            val query = SpoiQueryBuilder()
                .set(listOf(0, 99, PLAYER_HP), 9999)
                .fromPlayers()
                .filter(PLAYER_HP, CMP_GT, 9000)
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 55: 写入后管道操作 → SET hp=55, SORT hp, REVERSE, TAKE(2)
        println("--- 查询 ${++testNum}: 【L10】写入后管道操作 → SET hp=55, SORT hp, REVERSE, TAKE(2) ---")
        run {
            val query = SpoiQueryBuilder()
                .set(listOf(0, 0, PLAYER_HP), 55)
                .fromPlayers()
                .sort(PLAYER_HP, true)
                .reverse()
                .take(2)
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // ===== L11: 交叉字段查询 (5个) =====

        // 查询 56: FILTER(hp>30) + SORT(gold) + ANY(level>8)
        println("--- 查询 ${++testNum}: 【L11】FILTER(hp>30)+SORT(gold)+ANY(level>8) ---")
        run {
            val query = SpoiQueryBuilder()
                .fromPlayers()
                .filter(PLAYER_HP, CMP_GT, 30)
                .sort(PLAYER_GOLD, true)
                .any(PLAYER_LEVEL, CMP_GT, 8)
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 57: FILTER(gold>200) + FILTER(hp>50) + SORT(level)
        println("--- 查询 ${++testNum}: 【L11】FILTER(gold>200)+FILTER(hp>50)+SORT(level) ---")
        run {
            val query = SpoiQueryBuilder()
                .fromPlayers()
                .filter(PLAYER_GOLD, CMP_GT, 200)
                .filter(PLAYER_HP, CMP_GT, 50)
                .sort(PLAYER_LEVEL, true)
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 58: FILTER(gold>200) + SORT(hp) + FIND(level=12)
        println("--- 查询 ${++testNum}: 【L11】FILTER(gold>200)+SORT(hp)+FIND(level=12) ---")
        run {
            val query = SpoiQueryBuilder()
                .fromPlayers()
                .filter(PLAYER_GOLD, CMP_GT, 200)
                .sort(PLAYER_HP, true)
                .find(PLAYER_LEVEL, CMP_EQ, 12)
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 59: FILTER(hp>50) + SORT(level) + REVERSE + ANY(gold>300)
        println("--- 查询 ${++testNum}: 【L11】FILTER(hp>50)+SORT(level)+REVERSE+ANY(gold>300) ---")
        run {
            val query = SpoiQueryBuilder()
                .fromPlayers()
                .filter(PLAYER_HP, CMP_GT, 50)
                .sort(PLAYER_LEVEL, true)
                .reverse()
                .any(PLAYER_GOLD, CMP_GT, 300)
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 60: 全字段三条件 → hp>25 AND level>4 AND gold>150
        println("--- 查询 ${++testNum}: 【L11】全字段三条件 → hp>25 AND level>4 AND gold>150 ---")
        run {
            val query = SpoiQueryBuilder()
                .fromPlayers()
                .filter(PLAYER_HP, CMP_GT, 25)
                .filter(PLAYER_LEVEL, CMP_GT, 4)
                .filter(PLAYER_GOLD, CMP_GT, 150)
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // ===== L12: 极限组合压力 (5个) =====

        // 查询 61: 15步极限链
        println("--- 查询 ${++testNum}: 【L12】15步极限链 ---")
        println("    (SORT→REVERSE→DROP→TAKE→FILTER→SORT→REVERSE→TAKE→FILTER→SORT→REVERSE→DROP→TAKE→COUNT)")
        run {
            val query = SpoiQueryBuilder()
                .fromPlayers()
                .sort(PLAYER_LEVEL, true)
                .reverse()
                .drop(1)
                .take(3)
                .filter(PLAYER_HP, CMP_GT, 20)
                .sort(PLAYER_HP, false)
                .reverse()
                .take(2)
                .filter(PLAYER_GOLD, CMP_GT, 100)
                .sort(PLAYER_GOLD, true)
                .reverse()
                .drop(0)
                .take(2)
                .count()
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 62: 写入全部5个玩家, 然后复杂链查询
        println("--- 查询 ${++testNum}: 【L12】写入全部5个玩家, 复杂链查询 ---")
        println("    (SET 5个玩家hp → fromPlayers → FILTER hp>30 → SORT hp → REVERSE → TAKE(3))")
        run {
            val query = SpoiQueryBuilder()
                .set(listOf(0, 0, PLAYER_HP), 60)
                .set(listOf(0, 1, PLAYER_HP), 45)
                .set(listOf(0, 2, PLAYER_HP), 80)
                .set(listOf(0, 3, PLAYER_HP), 33)
                .set(listOf(0, 4, PLAYER_HP), 70)
                .fromPlayers()
                .filter(PLAYER_HP, CMP_GT, 30)
                .sort(PLAYER_HP, true)
                .reverse()
                .take(3)
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 63: SORT+REVERSE 循环3次
        println("--- 查询 ${++testNum}: 【L12】SORT+REVERSE循环3次 ---")
        println("    (SORT(level,asc)→REVERSE→SORT(hp,desc)→REVERSE→SORT(gold,asc)→REVERSE)")
        run {
            val query = SpoiQueryBuilder()
                .fromPlayers()
                .sort(PLAYER_LEVEL, true)
                .reverse()
                .sort(PLAYER_HP, false)
                .reverse()
                .sort(PLAYER_GOLD, true)
                .reverse()
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 64: 过滤到单元素 + 全操作
        println("--- 查询 ${++testNum}: 【L12】过滤到单元素+全操作 → FILTER(hp>85)→SORT→REVERSE→DROP(0)→TAKE(1) ---")
        run {
            val query = SpoiQueryBuilder()
                .fromPlayers()
                .filter(PLAYER_HP, CMP_GT, 85)
                .sort(PLAYER_HP, true)
                .reverse()
                .drop(0)
                .take(1)
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        // 查询 65: 极限混合
        println("--- 查询 ${++testNum}: 【L12】极限混合 ---")
        println("    (SET hp=60→ADD gold=50→fromPlayers→FILTER hp>30→SORT level→REVERSE→DROP(1)→TAKE(3)→FILTER gold>100→SORT hp→REVERSE→TAKE(2)→COUNT)")
        run {
            val query = SpoiQueryBuilder()
                .set(listOf(0, 0, PLAYER_HP), 60)
                .add(listOf(0, 0, PLAYER_GOLD), 50)
                .fromPlayers()
                .filter(PLAYER_HP, CMP_GT, 30)
                .sort(PLAYER_LEVEL, true)
                .reverse()
                .drop(1)
                .take(3)
                .filter(PLAYER_GOLD, CMP_GT, 100)
                .sort(PLAYER_HP, true)
                .reverse()
                .take(2)
                .count()
                .build()
            sock.sendWithLength(query)
            printResult(sock.recvWithLength())
            println()
        }

        println("=== 所有查询完成 ===")
    }
}