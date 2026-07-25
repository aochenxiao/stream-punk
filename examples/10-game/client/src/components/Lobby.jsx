import { useState } from 'react'

export default function Lobby({ onJoinGame }) {
  const [name, setName] = useState('')
  const [roomId, setRoomId] = useState('')

  function handleSubmit(e) {
    e.preventDefault()
    if (!name.trim()) return
    const room = roomId.trim() || 'default'
    onJoinGame(name.trim(), room)
  }

  return (
    <div className="lobby">
      <h1>StreamWorms</h1>
      <p className="subtitle">跨语言同步对战游戏</p>

      <form className="lobby-form" onSubmit={handleSubmit}>
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
          placeholder="房间号（留空自动加入）"
          value={roomId}
          onChange={e => setRoomId(e.target.value)}
          maxLength={16}
        />
        <button type="submit">加入游戏</button>
      </form>
    </div>
  )
}