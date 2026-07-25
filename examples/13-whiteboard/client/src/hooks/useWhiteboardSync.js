import { useState, useRef, useCallback } from 'react'
import { parseWhiteboardState, parseSpoiDelta, applySpoiDelta, serializeStroke } from '../protocol/parser'

const MSG = {
  FullState: 0x01,
  SpoiDelta: 0x02,
  SystemMsg: 0x04,
  DrawStroke: 0x10,
  ClearBoard: 0x11,
  DeleteStroke: 0x12,
  CursorMove: 0x13,
}

function sendBinary(ws, msgType, payload) {
  if (!ws || ws.readyState !== WebSocket.OPEN) return
  const totalLen = 1 + payload.length
  const packet = new Uint8Array(4 + totalLen)
  const view = new DataView(packet.buffer)
  view.setUint32(0, totalLen, true)
  packet[4] = msgType
  packet.set(payload, 5)
  ws.send(packet.buffer)
}

function sendJson(ws, msgType, obj) {
  const enc = new TextEncoder()
  sendBinary(ws, msgType, enc.encode(JSON.stringify(obj)))
}

export function useWhiteboardSync() {
  const [strokes, setStrokes] = useState([])
  const [users, setUsers] = useState([])
  const [connected, setConnected] = useState(false)
  const [myColor, setMyColor] = useState('#333333')
  const [cursors, setCursors] = useState({})
  const wsRef = useRef(null)
  const userIndexRef = useRef(-1)

  function connect(userName, roomId) {
    if (wsRef.current) wsRef.current.close()

    const ws = new WebSocket('ws://localhost:9998')
    ws.binaryType = 'arraybuffer'
    wsRef.current = ws

    ws.onopen = () => {
      setConnected(true)
      console.log('[WS] 已连接')
    }

    ws.onmessage = (event) => {
      if (typeof event.data === 'string') {
        try {
          const json = JSON.parse(event.data)
          console.log('[系统消息]', json)
        } catch (e) {}
        return
      }

      const data = new Uint8Array(event.data)
      if (data.length < 5) return

      const view = new DataView(data.buffer, data.byteOffset, data.byteLength)
      const totalLen = view.getUint32(0, true)
      const msgType = data[4]
      const payload = data.slice(5, 5 + totalLen - 1)

      switch (msgType) {
        case MSG.FullState: {
          const state = parseWhiteboardState(payload)
          console.log('[全量状态] strokes:', state.strokes.length)
          setStrokes(state.strokes)
          break
        }
        case MSG.SpoiDelta: {
          const instructions = parseSpoiDelta(payload)
          console.log('[SPOI 增量]', instructions.length, '条指令')
          setStrokes(prev => applySpoiDelta({ strokes: prev }, instructions).strokes)
          break
        }
        case MSG.SystemMsg: {
          const text = new TextDecoder().decode(payload)
          try {
            const json = JSON.parse(text)
            if (json.type === 'welcome') {
              sendJson(ws, MSG.SystemMsg, {
                type: 'join',
                roomId: roomId || 'default',
                userName: userName || 'Player'
              })
            } else if (json.type === 'joined') {
              userIndexRef.current = json.userIndex ?? -1
              const colorHex = '#' + (json.color >>> 0).toString(16).padStart(6, '0').toUpperCase()
              setMyColor(colorHex)
            } else if (json.type === 'userList') {
              setUsers(json.users || [])
            } else if (json.type === 'cursor') {
              setCursors(prev => ({
                ...prev,
                [json.userIndex]: { x: json.x, y: json.y }
              }))
              // 清除旧的远程光标（3秒后）
              setTimeout(() => {
                setCursors(prev => {
                  if (prev[json.userIndex]?.x === json.x && prev[json.userIndex]?.y === json.y) {
                    const next = { ...prev }
                    delete next[json.userIndex]
                    return next
                  }
                  return prev
                })
              }, 3000)
            }
          } catch (e) {}
          break
        }
      }
    }

    ws.onclose = () => {
      setConnected(false)
      console.log('[WS] 断开')
    }

    ws.onerror = (err) => {
      console.error('[WS] 错误:', err)
    }
  }

  const sendStroke = useCallback((stroke) => {
    const data = serializeStroke(stroke)
    sendBinary(wsRef.current, MSG.DrawStroke, data)
  }, [])

  const clearBoard = useCallback(() => {
    sendBinary(wsRef.current, MSG.ClearBoard, new Uint8Array(0))
  }, [])

  const deleteStroke = useCallback((index) => {
    const buf = new ArrayBuffer(4)
    new DataView(buf).setUint32(0, index, true)
    sendBinary(wsRef.current, MSG.DeleteStroke, new Uint8Array(buf))
  }, [])

  const sendCursor = useCallback((x, y) => {
    const buf = new ArrayBuffer(16)
    const view = new DataView(buf)
    view.setFloat64(0, x, true)
    view.setFloat64(8, y, true)
    sendBinary(wsRef.current, MSG.CursorMove, new Uint8Array(buf))
  }, [])

  return {
    strokes,
    users,
    connected,
    myColor,
    cursors,
    connect,
    sendStroke,
    clearBoard,
    deleteStroke,
    sendCursor,
  }
}