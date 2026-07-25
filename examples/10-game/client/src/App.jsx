import { useState, useCallback, useMemo } from 'react'
import Lobby from './components/Lobby'
import GameCanvas from './components/GameCanvas'
import HUD from './components/HUD'
import { useGameSync } from './hooks/useGameSync'
import { useKeyboardControls } from './hooks/useKeyboardControls'

export default function App() {
  const [screen, setScreen] = useState('lobby')

  const {
    gameState,
    connected,
    playerIndex,
    connect,
    sendAim,
    sendMove,
    sendFire,
    sendSwitchWeapon,
    trajectory
  } = useGameSync()

  const handleJoinGame = useCallback((name, room) => {
    connect(name, room)
    setScreen('game')
  }, [connect])

  const isMyTurn = useMemo(() => {
    return gameState?.currentTurn === playerIndex && gameState?.phase === 'aiming'
  }, [gameState, playerIndex])

  const currentWorm = useMemo(() => {
    return gameState?.worms?.[playerIndex]
  }, [gameState, playerIndex])

  const { angle, power, charging, weapon, weaponName, powerBarRef } = useKeyboardControls({
    isMyTurn,
    currentWorm,
    onAim: sendAim,
    onMove: sendMove,
    onFire: sendFire,
    onSwitchWeapon: sendSwitchWeapon,
  })

  return (
    <div className="app">
      {screen === 'lobby' ? (
        <Lobby onJoinGame={handleJoinGame} />
      ) : (
        <div className="game-container">
          <GameCanvas
            gameState={gameState}
            trajectory={trajectory}
          />
          <HUD
            gameState={gameState}
            playerIndex={playerIndex}
            connected={connected}
            angle={angle}
            power={power}
            charging={charging}
            weapon={weapon}
            weaponName={weaponName}
            powerBarRef={powerBarRef}
          />
        </div>
      )}
    </div>
  )
}
