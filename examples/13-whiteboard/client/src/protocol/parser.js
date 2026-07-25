// ===== 画板二进制协议解析器 =====
// 解析 C++ 端 UseData 序列化的 Stroke / WhiteboardState
// 格式与 WhiteboardData.hpp 中 Xt_Stroke / Xt_WhiteboardState 定义严格对应
// 字段顺序：points → color → width → tool

const TOOL = { PEN: 0, RECT: 1, CIRCLE: 2, LINE: 3, ERASER: 4 }

class BinaryReader {
  constructor(data) {
    this.view = new DataView(data.buffer, data.byteOffset, data.byteLength)
    this.offset = 0
  }

  readU8()   { const v = this.view.getUint8(this.offset);  this.offset += 1; return v }
  readU32()  { const v = this.view.getUint32(this.offset, true); this.offset += 4; return v }
  readF64()  { const v = this.view.getFloat64(this.offset, true); this.offset += 8; return v }
  readString() {
    const len = this.readU32()
    const bytes = new Uint8Array(this.view.buffer, this.view.byteOffset + this.offset, len)
    this.offset += len
    return new TextDecoder().decode(bytes)
  }
}

// 解析单个 Stroke（对应 C++ Xt_Stroke 字段顺序）
export function parseStroke(data) {
  const r = new BinaryReader(data)
  const stroke = {}

  // vector<StrokePoint> points
  const pointCount = r.readU32()
  stroke.points = []
  for (let i = 0; i < pointCount; i++) {
    stroke.points.push({ x: r.readF64(), y: r.readF64() })
  }

  // u32 color, f64 width, u8 tool
  stroke.color = r.readU32()
  stroke.width = r.readF64()
  stroke.tool  = r.readU8()

  return stroke
}

// 解析全量 WhiteboardState（对应 C++ Xt_WhiteboardState）
export function parseWhiteboardState(data) {
  const r = new BinaryReader(data)

  // vector<Stroke> strokes
  const strokeCount = r.readU32()
  const strokes = []
  for (let i = 0; i < strokeCount; i++) {
    const pointCount = r.readU32()
    const points = []
    for (let j = 0; j < pointCount; j++) {
      points.push({ x: r.readF64(), y: r.readF64() })
    }
    strokes.push({
      points,
      color: r.readU32(),
      width: r.readF64(),
      tool:  r.readU8(),
    })
  }

  return { strokes }
}

// 序列化 Stroke 为二进制（对应 C++ Xt_Stroke 字段顺序）
export function serializeStroke(stroke) {
  // 预估大小：4 + points*16 + 4 + 8 + 1
  const pointBytes = 4 + stroke.points.length * 16
  const total = pointBytes + 4 + 8 + 1
  const buf = new ArrayBuffer(total)
  const view = new DataView(buf)
  let off = 0

  // vector<StrokePoint> points
  view.setUint32(off, stroke.points.length, true); off += 4
  for (const p of stroke.points) {
    view.setFloat64(off, p.x, true); off += 8
    view.setFloat64(off, p.y, true); off += 8
  }

  // u32 color, f64 width, u8 tool
  view.setUint32(off, stroke.color || 0xFF000000, true); off += 4
  view.setFloat64(off, stroke.width || 2.0, true); off += 8
  view.setUint8(off, stroke.tool || 0); off += 1

  return new Uint8Array(buf)
}

// ===== SPOI Delta 解析器 =====
// 解析 C++ SPOI Shadow 生成的增量指令流
// 操作码与 StreamPunkSPOIShadow.hpp 中定义一致

const SPOI_OP = {
  SET:    0x04,
  ADD:    0x05,
  APPEND: 0x06,
  REMOVE: 0x07,
  INSERT: 0x08,
}

// 解析 SPOI 指令流
export function parseSpoiDelta(payload) {
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength)
  let offset = 0
  const instructions = []

  try {
    const instCount = view.getUint32(offset, true); offset += 4

    for (let i = 0; i < instCount; i++) {
      if (offset >= payload.length) break

      const op = payload[offset]; offset += 1

      // path
      const pathLen = view.getUint32(offset, true); offset += 4
      const path = []
      for (let j = 0; j < pathLen; j++) {
        path.push(view.getUint32(offset, true)); offset += 4
      }

      // typeDesc (skip)
      const tdLen = view.getUint32(offset, true); offset += 4 + tdLen * 4

      // operand
      const operandLen = view.getUint32(offset, true); offset += 4
      const operand = payload.slice(offset, offset + operandLen)
      offset += operandLen

      instructions.push({ op, path, operand })
    }
  } catch (e) {
    console.error('解析 SPOI delta 失败:', e)
  }

  return instructions
}

// 将 SPOI 指令应用到画板状态
// fieldIdx 0 = strokes（WhiteboardState 的唯一字段）
export function applySpoiDelta(prevState, instructions) {
  const next = { ...prevState, strokes: [...prevState.strokes] }

  for (const inst of instructions) {
    // strokes 字段
    if (inst.path[0] !== 0) continue

    switch (inst.op) {
      case SPOI_OP.SET: {
        // 全量替换 strokes
        const strokes = parseStrokesFromOperand(inst.operand)
        next.strokes = strokes
        break
      }
      case SPOI_OP.APPEND: {
        // 追加单个 Stroke
        const stroke = parseStroke(inst.operand)
        next.strokes.push(stroke)
        break
      }
      case SPOI_OP.REMOVE: {
        // operand 是 u32 index
        if (inst.operand.length >= 4) {
          const idx = new DataView(inst.operand.buffer, inst.operand.byteOffset, 4).getUint32(0, true)
          if (idx < next.strokes.length) {
            next.strokes.splice(idx, 1)
          }
        }
        break
      }
      default:
        break
    }
  }

  return next
}

function parseStrokesFromOperand(operand) {
  const r = new BinaryReader(operand)
  const count = r.readU32()
  const strokes = []
  for (let i = 0; i < count; i++) {
    const pointCount = r.readU32()
    const points = []
    for (let j = 0; j < pointCount; j++) {
      points.push({ x: r.readF64(), y: r.readF64() })
    }
    strokes.push({
      points,
      color: r.readU32(),
      width: r.readF64(),
      tool:  r.readU8(),
    })
  }
  return strokes
}