const TOOLS = [
  { id: 'pen',    label: '画笔', icon: '✏️' },
  { id: 'rect',   label: '矩形', icon: '▭' },
  { id: 'circle', label: '圆',   icon: '○' },
  { id: 'line',   label: '直线', icon: '╱' },
  { id: 'eraser', label: '橡皮', icon: '🧹' },
]

const COLORS = [
  '#333333', '#E53E3E', '#3182CE', '#38A169',
  '#D69E2E', '#805AD5', '#DD6B20', '#E53E8E',
  '#000000', '#FFFFFF',
]

const WIDTHS = [1, 2, 4, 6, 8, 12]

export default function Toolbar({ tool, setTool, color, setColor, width, setWidth, onClear, connected }) {
  return (
    <div className="toolbar">
      <div className="toolbar-section">
        <span className="toolbar-label">工具</span>
        <div className="toolbar-tools">
          {TOOLS.map(t => (
            <button
              key={t.id}
              className={`tool-btn ${tool === t.id ? 'active' : ''}`}
              onClick={() => setTool(t.id)}
              title={t.label}
            >
              {t.icon}
            </button>
          ))}
        </div>
      </div>

      <div className="toolbar-section">
        <span className="toolbar-label">颜色</span>
        <div className="toolbar-colors">
          {COLORS.map(c => (
            <button
              key={c}
              className={`color-btn ${color === c ? 'active' : ''}`}
              style={{ backgroundColor: c }}
              onClick={() => setColor(c)}
            />
          ))}
        </div>
      </div>

      <div className="toolbar-section">
        <span className="toolbar-label">粗细</span>
        <div className="toolbar-widths">
          {WIDTHS.map(w => (
            <button
              key={w}
              className={`width-btn ${width === w ? 'active' : ''}`}
              onClick={() => setWidth(w)}
            >
              <span className="width-dot" style={{ width: w, height: w }} />
            </button>
          ))}
        </div>
      </div>

      <div className="toolbar-section toolbar-right">
        <button className="clear-btn" onClick={onClear} title="清空画板">
          🗑️ 清空
        </button>
        <span className={`status-dot ${connected ? 'connected' : 'disconnected'}`} />
        <span className="status-text">{connected ? '已连接' : '未连接'}</span>
      </div>
    </div>
  )
}