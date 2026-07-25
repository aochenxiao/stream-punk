import { useState, useCallback } from 'react';
import Lobby from './components/Lobby';
import GameBoard from './components/GameBoard';
import { GameWS } from './ws';

type Page = 'lobby' | 'game';

export default function App() {
  const [page, setPage] = useState<Page>('lobby');
  const [ws, setWs] = useState<GameWS | null>(null);
  const [playerId, setPlayerId] = useState(0);

  const handleJoin = useCallback((gameWs: GameWS, pid: number) => {
    setWs(gameWs);
    setPlayerId(pid);
    setPage('game');
  }, []);

  const handleBack = useCallback(() => {
    ws?.disconnect();
    setWs(null);
    setPlayerId(0);
    setPage('lobby');
  }, [ws]);

  return (
    <div style={{ width: '100vw', height: '100vh', display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center' }}>
      {page === 'lobby' && <Lobby onJoin={handleJoin} />}
      {page === 'game' && ws && <GameBoard ws={ws} playerId={playerId} onBack={handleBack} />}
    </div>
  );
}