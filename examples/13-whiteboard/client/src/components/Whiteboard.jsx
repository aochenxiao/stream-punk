import { useRef, useEffect, useCallback, useState } from 'react'

export default function Whiteboard({
  strokes, tool, color, width, myColor,
  cursors, onDrawStroke, onDeleteStroke, onCursorMove
}) {
  const canvasRef = useRef(null)
  const containerRef = useRef(null)
  const [canvasSize, setCanvasSize] = useState({ w: 1200, h: 800 })
  const isDrawing = useRef(false)
  const currentStroke = useRef(null)
  const lastCursorSend = useRef(0)

  // 响应容器大小变化
  useEffect(() => {
    function resize() {
      if (containerRef.current) {
        setCanvasSize({
          w: containerRef.current.clientWidth,
          h: containerRef.current.clientHeight,
        })
      }
    }
    resize()
    window.addEventListener('resize', resize)
    return () => window.removeEventListener('resize', resize)
  }, [])

  // 重绘所有笔画
  const redraw = useCallback(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext('2d')
    ctx.clearRect(0, 0, canvas.width, canvas.height)

    // 绘制背景网格
    drawGrid(ctx, canvas.width, canvas.height)

    // 绘制所有笔画
    for (const stroke of strokes) {
      drawStroke(ctx, stroke)
    }
  }, [strokes])

  useEffect(() => {
    redraw()
  }, [redraw])

  // 绘制网格背景
  function drawGrid(ctx, w, h) {
    ctx.strokeStyle = '#e8e8e8'
    ctx.lineWidth = 0.5
    const step = 40
    for (let x = 0; x <= w; x += step) {
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke()
    }
    for (let y = 0; y <= h; y += step) {
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke()
    }
  }

  // 绘制单个笔画
  function drawStroke(ctx, stroke) {
    if (!stroke.points || stroke.points.length === 0) return

    ctx.save()
    ctx.lineCap = 'round'
    ctx.lineJoin = 'round'

    const hexColor = '#' + (stroke.color >>> 0).toString(16).padStart(6, '0').toUpperCase()
    // 橡皮擦用 destination-out 合成模式
    if (stroke.tool === 4) {
      ctx.globalCompositeOperation = 'destination-out'
      ctx.strokeStyle = 'rgba(0,0,0,1)'
    } else {
      ctx.globalCompositeOperation = 'source-over'
      ctx.strokeStyle = hexColor
    }
    ctx.lineWidth = stroke.width

    const pts = stroke.points
    if (stroke.tool === 1) {
      // 矩形
      const [p1, p2] = [pts[0], pts[pts.length - 1]]
      ctx.strokeRect(p1.x, p1.y, p2.x - p1.x, p2.y - p1.y)
    } else if (stroke.tool === 2) {
      // 圆
      const [p1, p2] = [pts[0], pts[pts.length - 1]]
      const cx = (p1.x + p2.x) / 2, cy = (p1.y + p2.y) / 2
      const rx = Math.abs(p2.x - p1.x) / 2, ry = Math.abs(p2.y - p1.y) / 2
      ctx.beginPath()
      ctx.ellipse(cx, cy, rx, ry, 0, 0, Math.PI * 2)
      ctx.stroke()
    } else if (stroke.tool === 3) {
      // 直线
      ctx.beginPath()
      ctx.moveTo(pts[0].x, pts[0].y)
      ctx.lineTo(pts[pts.length - 1].x, pts[pts.length - 1].y)
      ctx.stroke()
    } else {
      // 自由绘制 / 橡皮擦
      ctx.beginPath()
      ctx.moveTo(pts[0].x, pts[0].y)
      for (let i = 1; i < pts.length; i++) {
        ctx.lineTo(pts[i].x, pts[i].y)
      }
      ctx.stroke()
    }

    ctx.restore()
  }

  // 获取 canvas 坐标
  function getCanvasPos(e) {
    const canvas = canvasRef.current
    const rect = canvas.getBoundingClientRect()
    const scaleX = canvas.width / rect.width
    const scaleY = canvas.height / rect.height
    return {
      x: (e.clientX - rect.left) * scaleX,
      y: (e.clientY - rect.top) * scaleY,
    }
  }

  function handlePointerDown(e) {
    const pos = getCanvasPos(e)
    isDrawing.current = true
    currentStroke.current = {
      points: [pos],
      color: parseInt(color.replace('#', '0x')),
      width,
      tool: toolMap[tool] ?? 0,
    }
  }

  function handlePointerMove(e) {
    // 发送光标位置（限流 50ms）
    const now = Date.now()
    if (now - lastCursorSend.current > 50) {
      lastCursorSend.current = now
      const pos = getCanvasPos(e)
      onCursorMove?.(pos.x, pos.y)
    }

    if (!isDrawing.current || !currentStroke.current) return
    const pos = getCanvasPos(e)

    const s = currentStroke.current
    if (s.tool === 0 || s.tool === 4) {
      // 自由绘制/橡皮擦：追加所有中间点
      s.points.push(pos)
    } else {
      // 矩形/圆/直线：只保留首尾两点
      s.points[1] = pos
    }

    // 实时预览（本地绘制）
    const canvas = canvasRef.current
    const ctx = canvas.getContext('2d')
    redraw()
    drawStroke(ctx, s)
  }

  function handlePointerUp(e) {
    if (!isDrawing.current || !currentStroke.current) return
    isDrawing.current = false

    const stroke = currentStroke.current
    currentStroke.current = null

    if (stroke.points.length === 0) return

    // 发送笔画到服务器
    onDrawStroke?.(stroke)

    // 重绘以包含新笔画
    redraw()
  }

  // 双击删除笔画
  function handleDoubleClick(e) {
    const pos = getCanvasPos(e)
    // 从后往前查找命中的笔画
    for (let i = strokes.length - 1; i >= 0; i--) {
      const s = strokes[i]
      if (hitTest(s, pos.x, pos.y)) {
        onDeleteStroke?.(i)
        return
      }
    }
  }

  function hitTest(stroke, x, y) {
    const threshold = Math.max(stroke.width + 4, 8)
    for (const p of stroke.points) {
      if (Math.abs(p.x - x) < threshold && Math.abs(p.y - y) < threshold) {
        return true
      }
    }
    return false
  }

  // 光标样式
  const cursorStyle = tool === 'eraser'
    ? 'crosshair'
    : 'crosshair'

  return (
    <div ref={containerRef} className="whiteboard-container">
      <canvas
        ref={canvasRef}
        width={canvasSize.w}
        height={canvasSize.h}
        className="whiteboard-canvas"
        style={{ cursor: cursorStyle }}
        onPointerDown={handlePointerDown}
        onPointerMove={handlePointerMove}
        onPointerUp={handlePointerUp}
        onPointerLeave={handlePointerUp}
        onDoubleClick={handleDoubleClick}
      />
      {/* 远程光标 */}
      {Object.entries(cursors).map(([idx, pos]) => (
        <div
          key={idx}
          className="remote-cursor"
          style={{
            left: (pos.x / canvasSize.w) * 100 + '%',
            top: (pos.y / canvasSize.h) * 100 + '%',
          }}
        >
          <svg width="16" height="16" viewBox="0 0 16 16">
            <path d="M1 1l5 14 2-5 5-2z" fill="#333" />
          </svg>
        </div>
      ))}
    </div>
  )
}

const toolMap = { pen: 0, rect: 1, circle: 2, line: 3, eraser: 4 }