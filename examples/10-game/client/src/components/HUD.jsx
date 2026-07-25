const WORM_COLORS = ['#ff4444', '#44aaff', '#ffaa00', '#44ff44']
const MAX_MOVE_DISTANCE = 60

export default function HUD({ gameState, playerIndex, connected, angle, power, charging, weapon, weaponName, powerBarRef }) {
  const worms = gameState?.worms || []
  const wind = gameState?.wind || { direction: 0, strength: 0 }
  const phase = gameState?.phase || 'waiting'
  const currentTurn = gameState?.currentTurn ?? -1
  const turnTimeLeft = gameState?.turnTimeLeft ?? 0
  const winner = gameState?.winner ?? -1

  const isMyTurn = currentTurn === playerIndex && phase === 'aiming'
  const currentWorm = worms[playerIndex]
  const movedThisTurn = currentWorm?.movedThisTurn ?? 0
  const moveLeft = Math.max(0, MAX_MOVE_DISTANCE - movedThisTurn)

  const windDir = wind.direction > 0.1 ? '→' : wind.direction < -0.1 ? '←' : '·'

  return (
    <>
      {/* 顶部状态栏 */}
      <div className="hud-top">
        <span className={`connection-status ${connected ? 'connected' : 'disconnected'}`} />
        {phase === 'gameover' ? (
          <span>
            {winner >= 0 && worms[winner]
              ? `${worms[winner].name} 获胜！`
              : '游戏结束'}
          </span>
        ) : phase === 'aiming' ? (
          <span>
            轮到 {worms[currentTurn]?.name || '...'} 操作
            {' · '}
            剩余 {turnTimeLeft}s
            {' · '}
            风向 {windDir} {Math.abs(wind.strength).toFixed(1)}
          </span>
        ) : phase === 'firing' ? (
          <span>发射中...</span>
        ) : phase === 'waiting' ? (
          <span>等待玩家加入...</span>
        ) : (
          <span>{phase}</span>
        )}
      </div>

      {/* 底部 HUD */}
      <div className="hud">
        {/* 玩家列表 */}
        <div className="hud-players">
          {worms.map((worm, i) => (
            <div key={i} className={`hud-player ${i === currentTurn ? 'active' : ''}`}>
              <div className="name" style={{ color: WORM_COLORS[worm.color % WORM_COLORS.length] }}>
                {worm.name}
                {!worm.alive && ' 💀'}
              </div>
              <div className="hp-bar">
                <div
                  className="hp-fill"
                  style={{ width: `${Math.max(0, worm.hp)}%` }}
                />
              </div>
              <div className="hp-text">{Math.max(0, Math.round(worm.hp))} HP</div>
            </div>
          ))}
        </div>

        {/* 操作面板 */}
        {isMyTurn && (
          <div className="hud-controls">
            <div className="hud-readout">
              <span>角度: {angle.toFixed(0)}°</span>
              <span>移动: {moveLeft.toFixed(0)}px</span>
            </div>

            <div className="power-charge">
              <span>力度</span>
              <div className="power-bar">
                <div
                  ref={powerBarRef}
                  className="power-fill"
                  style={{ width: `${power}%` }}
                />
              </div>
              <span>{charging ? power.toFixed(0) : '0'}%</span>
            </div>

            <div className="hud-weapon">
              <span>武器: {weaponName}</span>
              <span className="weapon-stats">
                {weapon === 0 ? '⚫ 中等' : weapon === 1 ? '🔴 大范围' : '🟡 小范围×3'}
              </span>
            </div>

            <div className="hud-hint">
              W/S 瞄准 · A/D 移动 · Q/E 切换武器 · 空格蓄力发射
            </div>
          </div>
        )}

        {/* 风力指示 */}
        <div className="hud-wind">
          <span className="arrow" style={{ transform: `rotate(${wind.direction > 0 ? 0 : 180}deg)` }}>
            {windDir}
          </span>
          <span>风力 {wind.strength.toFixed(1)}</span>
        </div>
      </div>
    </>
  )
}
