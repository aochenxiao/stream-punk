import { useState, useCallback } from 'react'
import Whiteboard from './components/Whiteboard'
import Toolbar from './components/Toolbar'
import UserList from './components/UserList'
import { useWhiteboardSync } from './hooks/useWhiteboardSync'

export default function App() {
  const [screen, setScreen] = useState('lobby')
  const [tool, setTool] = useState('pen')
  const [color, setColor] = useState('#333333')
  const [width, setWidth] = useState(2)

  const {
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
  } = useWhiteboardSync()

  const handleJoin = useCallback((name, room) => {
    connect(name, room)
    setScreen('whiteboard')
  }, [connect])

  const handleClear = useCallback(() => {
    clearBoard()
  }, [clearBoard])

  if (screen === 'lobby') {
    return <LobbyScreen onJoin={handleJoin} />
  }

  return (
    <div className="app-container">
      <Toolbar
        tool={tool}
        setTool={setTool}
        color={color}
        setColor={setColor}
        width={width}
        setWidth={setWidth}
        onClear={handleClear}
        connected={connected}
      />
      <div className="main-area">
        <Whiteboard
          strokes={strokes}
          tool={tool}
          color={color}
          width={width}
          myColor={myColor}
          cursors={cursors}
          onDrawStroke={sendStroke}
          onDeleteStroke={deleteStroke}
          onCursorMove={sendCursor}
        />
        <UserList users={users} />
      </div>
    </div>
  )
}

function LobbyScreen({ onJoin }) {
  const [name, setName] = useState('')
  const [roomId, setRoomId] = useState('')

  function handleSubmit(e) {
    e.preventDefault()
    if (!name.trim()) return
    onJoin(name.trim(), roomId.trim() || 'default')
  }

  return (
    <div className="lobby">
      <div className="lobby-card">
        <h1>多人协同画板</h1>
        <p className="lobby-subtitle">
          StreamPunk 跨语言二进制增量同步
        </p>
        <form onSubmit={handleSubmit}>
          <input
            type="text"
            placeholder="你的昵称"
            value={name}
            onChange={e => setName(e.target.value)}
            maxLength={16}
            autoFocus
          />
          <input
            type="text"
            placeholder="房间号（留空默认）"
            value={roomId}
            onChange={e => setRoomId(e.target.value)}
            maxLength={16}
          />
          <button type="submit">进入画板</button>
        </form>
        <div className="lobby-features">
          <span>二进制序列化</span>
          <span>增量同步</span>
          <span>多人实时协作</span>
        </div>
      </div>
    </div>
  )
}