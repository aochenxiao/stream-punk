import { useState, useRef, useCallback } from 'react'

// ===== 消息协议常量 =====
const MSG = {
  FullState:   0x01,
  SpoiDelta:   0x02,
  Trajectory:  0x03,
  SystemMsg:   0x04,
  SpoiCommand: 0x10,
  LobbyAction: 0x11,
}

function createDefaultState() {
  return {
    worms: [],
    terrain: [],
    wind: { direction: 0, strength: 0 },
    currentTurn: 0,
    trajectory: [],
    explosions: [],
    phase: 'waiting',
    turnTimeLeft: 30,
    winner: -1,
  }
}

// ===== 发送消息（二进制协议） =====
function sendBinaryMessage(ws, msgType, payload) {
  if (!ws || ws.readyState !== WebSocket.OPEN) return

  const totalLen = 1 + payload.length
  const packet = new Uint8Array(4 + totalLen)
  const view = new DataView(packet.buffer)

  // 4字节长度（小端）
  view.setUint32(0, totalLen, true)
  // 1字节消息类型
  packet[4] = msgType
  // payload
  packet.set(payload, 5)

  ws.send(packet.buffer)
}

function sendJsonMessage(ws, msgType, obj) {
  const encoder = new TextEncoder()
  const payload = encoder.encode(JSON.stringify(obj))
  sendBinaryMessage(ws, msgType, payload)
}

// ===== SPOI 操作码 =====
const SPOI_OP = {
  SET:    0x04,
  ADD:    0x05,
  APPEND: 0x06,
  REMOVE: 0x07,
  INSERT: 0x08,
}

// GameState 字段索引（与 C++ UseSPOI 定义一致）
const GS = {
  worms: 0, terrain: 1, wind: 2, currentTurn: 3,
  trajectory: 4, explosions: 5, phase: 6, turnTimeLeft: 7, winner: 8,
}

// ===== SPOI 增量解析器 =====

// 解析 SPOI 二进制流（C++ UseData 格式），返回指令列表
function parseSpoiDelta(payload) {
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength)
  let offset = 0
  const instructions = []

  try {
    const instCount = view.getUint32(offset, true)
    offset += 4

    for (let i = 0; i < instCount; i++) {
      if (offset >= payload.length) break

      const op = payload[offset]
      offset += 1

      // path
      const pathLen = view.getUint32(offset, true)
      offset += 4
      const path = []
      for (let j = 0; j < pathLen; j++) {
        path.push(view.getUint32(offset, true))
        offset += 4
      }

      // typeDesc (skip) — SpToken = u32, 每个元素 4 字节
      const tdLen = view.getUint32(offset, true)
      offset += 4 + tdLen * 4

      // operand
      const operandLen = view.getUint32(offset, true)
      offset += 4
      const operand = payload.slice(offset, offset + operandLen)
      offset += operandLen

      instructions.push({ op, path, operand })
    }
  } catch (e) {
    console.error('解析 SPOI delta 失败:', e)
  }

  return instructions
}

// 解析 operand 中的 UseData 值
function parseOperandValue(operand, fieldIdx) {
  const view = new DataView(operand.buffer, operand.byteOffset, operand.byteLength)
  let off = 0

  switch (fieldIdx) {
    case GS.worms: {
      // vector<Worm>
      const count = view.getUint32(off, true); off += 4
      const worms = []
      for (let i = 0; i < count; i++) {
        const nameLen = view.getUint32(off, true); off += 4
        const nameBytes = operand.slice(off, off + nameLen)
        const name = new TextDecoder().decode(nameBytes)
        off += nameLen
        const hp = view.getFloat64(off, true); off += 8
        const x = view.getFloat64(off, true); off += 8
        const y = view.getFloat64(off, true); off += 8
        const angle = view.getFloat64(off, true); off += 8
        const power = view.getFloat64(off, true); off += 8
        const weapon = view.getInt32(off, true); off += 4
        const alive = operand[off] !== 0; off += 1
        const color = view.getInt32(off, true); off += 4
        const facingRight = operand[off] !== 0; off += 1
        const movedThisTurn = view.getFloat64(off, true); off += 8
        worms.push({ name, hp, x, y, angle, power, weapon, alive, color, facingRight, movedThisTurn })
      }
      return worms
    }
    case GS.terrain: {
      // vector<f64>
      const count = view.getUint32(off, true); off += 4
      const terrain = []
      for (let i = 0; i < count; i++) {
        terrain.push(view.getFloat64(off, true))
        off += 8
      }
      return terrain
    }
    case GS.wind: {
      const direction = view.getFloat64(off, true); off += 8
      const strength = view.getFloat64(off, true)
      return { direction, strength }
    }
    case GS.currentTurn:
    case GS.turnTimeLeft:
    case GS.winner:
      return view.getInt32(off, true)
    case GS.trajectory: {
      const count = view.getUint32(off, true); off += 4
      const pts = []
      for (let i = 0; i < count; i++) {
        const x = view.getFloat64(off, true); off += 8
        const y = view.getFloat64(off, true); off += 8
        pts.push({ x, y })
      }
      return pts
    }
    case GS.explosions: {
      const count = view.getUint32(off, true); off += 4
      const exps = []
      for (let i = 0; i < count; i++) {
        const cx = view.getFloat64(off, true); off += 8
        const cy = view.getFloat64(off, true); off += 8
        const radius = view.getFloat64(off, true); off += 8
        const damage = view.getFloat64(off, true); off += 8
        exps.push({ cx, cy, radius, damage })
      }
      return exps
    }
    case GS.phase: {
      const len = view.getUint32(off, true); off += 4
      const bytes = operand.slice(off, off + len)
      return new TextDecoder().decode(bytes)
    }
    default:
      return null
  }
}

// 将 SPOI 指令应用到游戏状态（不可变更新）
function applySpoiDelta(prevState, instructions) {
  const next = { ...prevState }

  for (const inst of instructions) {
    if (inst.op === SPOI_OP.SET && inst.path.length >= 1) {
      const fieldIdx = inst.path[0]
      const value = parseOperandValue(inst.operand, fieldIdx)

      if (value !== null) {
        switch (fieldIdx) {
          case GS.worms:        next.worms = value; break
          case GS.terrain:      next.terrain = value; break
          case GS.wind:         next.wind = value; break
          case GS.currentTurn:  next.currentTurn = value; break
          case GS.trajectory:   next.trajectory = value; break
          case GS.explosions:   next.explosions = value; break
          case GS.phase:        next.phase = value; break
          case GS.turnTimeLeft: next.turnTimeLeft = value; break
          case GS.winner:       next.winner = value; break
        }
      }
    }
    // 其他操作码（APPEND/REMOVE等）暂不处理，后续可扩展
  }

  return next
}

// ===== 解析全量状态（二进制 → JS 对象） =====
function parseGameState(data) {
  try {
    const view = new DataView(data.buffer, data.byteOffset, data.byteLength)
    let offset = 0

    // worms: vector<Worm> → [count: u32][worms...]
    const wormCount = view.getUint32(offset, true)
    offset += 4
    const worms = []
    for (let i = 0; i < wormCount; i++) {
      const nameLen = view.getUint32(offset, true)
      offset += 4
      const nameBytes = new Uint8Array(data.buffer, data.byteOffset + offset, nameLen)
      const name = new TextDecoder().decode(nameBytes)
      offset += nameLen

      const hp = view.getFloat64(offset, true); offset += 8
      const x = view.getFloat64(offset, true); offset += 8
      const y = view.getFloat64(offset, true); offset += 8
      const angle = view.getFloat64(offset, true); offset += 8
      const power = view.getFloat64(offset, true); offset += 8
      const weapon = view.getInt32(offset, true); offset += 4
      const alive = view.getUint8(offset) !== 0; offset += 1
      const color = view.getInt32(offset, true); offset += 4
      const facingRight = view.getUint8(offset) !== 0; offset += 1
      const movedThisTurn = view.getFloat64(offset, true); offset += 8

      worms.push({ name, hp, x, y, angle, power, weapon, alive, color, facingRight, movedThisTurn })
    }

    // terrain: vector<f64> → [count: u32][f64...]
    const terrainCount = view.getUint32(offset, true)
    offset += 4
    const terrain = []
    for (let i = 0; i < terrainCount; i++) {
      terrain.push(view.getFloat64(offset, true))
      offset += 8
    }

    // wind: Wind → direction, strength
    const windDir = view.getFloat64(offset, true); offset += 8
    const windStr = view.getFloat64(offset, true); offset += 8
    const wind = { direction: windDir, strength: windStr }

    const currentTurn = view.getInt32(offset, true); offset += 4

    // trajectory: vector<TrajectoryPoint>
    const trajCount = view.getUint32(offset, true)
    offset += 4
    const trajectory = []
    for (let i = 0; i < trajCount; i++) {
      const tx = view.getFloat64(offset, true); offset += 8
      const ty = view.getFloat64(offset, true); offset += 8
      trajectory.push({ x: tx, y: ty })
    }

    // explosions: vector<Explosion>
    const expCount = view.getUint32(offset, true)
    offset += 4
    const explosions = []
    for (let i = 0; i < expCount; i++) {
      const cx = view.getFloat64(offset, true); offset += 8
      const cy = view.getFloat64(offset, true); offset += 8
      const radius = view.getFloat64(offset, true); offset += 8
      const damage = view.getFloat64(offset, true); offset += 8
      explosions.push({ cx, cy, radius, damage })
    }

    // phase: string
    const phaseLen = view.getUint32(offset, true)
    offset += 4
    const phaseBytes = new Uint8Array(data.buffer, data.byteOffset + offset, phaseLen)
    const phase = new TextDecoder().decode(phaseBytes)
    offset += phaseLen

    const turnTimeLeft = view.getInt32(offset, true); offset += 4
    const winner = view.getInt32(offset, true); offset += 4

    return { worms, terrain, wind, currentTurn, trajectory, explosions, phase, turnTimeLeft, winner }
  } catch (e) {
    console.error('解析 GameState 失败:', e)
    return createDefaultState()
  }
}

// ===== WebSocket 同步 Hook =====
export function useGameSync() {
  const [gameState, setGameState] = useState(createDefaultState)
  const [connected, setConnected] = useState(false)
  const [playerIndex, setPlayerIndex] = useState(-1)
  const [trajectory, setTrajectory] = useState(null)
  const wsRef = useRef(null)

  function connect(playerName, roomId) {
    if (wsRef.current) {
      wsRef.current.close()
    }

    const ws = new WebSocket(`ws://${window.location.hostname}:9999`)
    ws.binaryType = 'arraybuffer'
    wsRef.current = ws

    ws.onopen = () => {
      console.log('[WS] 已连接')
      setConnected(true)

      // 发送加入房间（等收到 welcome 后再发）
    }

    ws.onmessage = (event) => {
      const data = new Uint8Array(event.data)
      if (data.length < 5) {
        // 可能是文本欢迎消息
        if (typeof event.data === 'string') {
          try {
            const json = JSON.parse(event.data)
            console.log('[系统消息]', json)
          } catch (e) {}
        }
        return
      }

      const view = new DataView(data.buffer, data.byteOffset, data.byteLength)
      const totalLen = view.getUint32(0, true)
      const msgType = data[4]
      const payload = data.slice(5, 5 + totalLen - 1)

      switch (msgType) {
        case MSG.FullState: {
          const state = parseGameState(payload)
          console.log('[全量状态] worms:', state.worms.length, 'terrain:', state.terrain.length)
          setGameState(state)
          setTrajectory(state.trajectory)
          break
        }
        case MSG.SystemMsg: {
          const text = new TextDecoder().decode(payload)
          try {
            const json = JSON.parse(text)
            console.log('[系统消息]', json)

            if (json.type === 'welcome') {
              // 收到欢迎消息后发送加入房间
              sendJsonMessage(ws, MSG.LobbyAction, {
                type: 'join',
                roomId: roomId || 'default',
                playerName: playerName || 'Player'
              })
            } else if (json.type === 'joined') {
              setPlayerIndex(json.playerIndex)
            } else if (json.type === 'gameStart') {
              console.log('[游戏开始]')
            } else if (json.type === 'playerList') {
              console.log('[玩家列表]', json.players)
            } else if (json.type === 'error') {
              console.warn('[错误]', json.msg)
            }
          } catch (e) {
            console.log('[系统消息(文本)]', text)
          }
          break
        }
        case MSG.Trajectory: {
          try {
            const points = parseTrajectory(payload)
            console.log('[弹道] 收到轨迹, 点数:', points.length,
              '首点:', JSON.stringify(points[0]),
              '尾点:', JSON.stringify(points[points.length - 1]),
              '前3:', JSON.stringify(points.slice(0, 3)))
            setTrajectory(points)
          } catch (e) {
            console.error('解析轨迹失败:', e)
          }
          break
        }
        case MSG.SpoiDelta: {
          try {
            const instructions = parseSpoiDelta(payload)
            console.log('[SPOI 增量]', instructions.length, '条指令')

            setGameState(prev => {
              const next = applySpoiDelta(prev, instructions)
              return next
            })
          } catch (e) {
            console.error('应用 SPOI delta 失败:', e)
          }
          break
        }
        default:
          console.log('[未知消息]', msgType, payload.length, 'bytes')
      }
    }

    ws.onclose = () => {
      console.log('[WS] 断开')
      setConnected(false)
    }

    ws.onerror = (err) => {
      console.error('[WS] 错误:', err)
    }
  }

  const sendAim = useCallback((angle) => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) return
    const buf = new ArrayBuffer(9)
    const view = new DataView(buf)
    view.setUint8(0, 0x01)
    view.setFloat64(1, angle, true)
    sendBinaryMessage(wsRef.current, MSG.SpoiCommand, new Uint8Array(buf))
  }, [])

  const sendMove = useCallback((dir) => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) return
    const buf = new ArrayBuffer(2)
    const view = new DataView(buf)
    view.setUint8(0, 0x02)
    view.setInt8(1, dir)
    sendBinaryMessage(wsRef.current, MSG.SpoiCommand, new Uint8Array(buf))
  }, [])

  const sendFire = useCallback((power) => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) return
    console.log('[Fire] 发送发射指令 power:', power)
    const buf = new ArrayBuffer(9)
    const view = new DataView(buf)
    view.setUint8(0, 0x03)
    view.setFloat64(1, power, true)
    sendBinaryMessage(wsRef.current, MSG.SpoiCommand, new Uint8Array(buf))
  }, [])

  const sendSwitchWeapon = useCallback((weapon) => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) return
    const buf = new ArrayBuffer(5)
    const view = new DataView(buf)
    view.setUint8(0, 0x04)
    view.setInt32(1, weapon, true)
    sendBinaryMessage(wsRef.current, MSG.SpoiCommand, new Uint8Array(buf))
  }, [])

  return {
    gameState,
    connected,
    playerIndex,
    trajectory,
    connect,
    sendAim,
    sendMove,
    sendFire,
    sendSwitchWeapon,
  }
}

// 解析弹道轨迹
function parseTrajectory(data) {
  const view = new DataView(data.buffer, data.byteOffset, data.byteLength)
  let offset = 0
  const count = view.getUint32(offset, true)
  offset += 4
  const points = []
  for (let i = 0; i < count; i++) {
    const x = view.getFloat64(offset, true); offset += 8
    const y = view.getFloat64(offset, true); offset += 8
    points.push({ x, y })
  }
  return points
}