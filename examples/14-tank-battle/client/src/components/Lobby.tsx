import { useState, useCallback } from 'react';
import { GameWS } from '../ws';

interface Props {
  onJoin: (ws: GameWS, playerId: number) => void;
}

export default function Lobby({ onJoin }: Props) {
  const [status, setStatus] = useState('');
  const [joined, setJoined] = useState(false);

  const handleJoin = useCallback(() => {
    setStatus('连接中...');
    const wsUrl = 'ws://localhost:9002';
    const gameWs = new GameWS(wsUrl, {
      onGameState: () => {},
      onPlayerId: (pid: number) => {
        setJoined(true);
        setStatus(`已加入 - 玩家 ${pid}`);
        onJoin(gameWs, pid);
      },
      onStatusChange: setStatus,
    });
    gameWs.connect();
  }, [onJoin]);

  if (joined) return null;

  return (
    <div style={{ textAlign: 'center' }}>
      <h1 style={{ fontSize: '3rem', marginBottom: '0.5rem', color: '#e94560' }}>Tank Battle</h1>
      <p style={{ color: '#aaa', marginBottom: '2rem' }}>双人坦克大战</p>
      <button
        onClick={handleJoin}
        style={{
          padding: '16px 48px',
          fontSize: '1.2rem',
          background: '#e94560',
          color: '#fff',
          border: 'none',
          borderRadius: '8px',
          cursor: 'pointer',
          fontWeight: 'bold',
        }}
      >
        加入游戏
      </button>
      {status && <p style={{ marginTop: '1rem', color: '#aaa' }}>{status}</p>}
      <div style={{ marginTop: '2rem', color: '#666', fontSize: '0.9rem' }}>
        <p>玩家1: WASD 移动, 空格 射击</p>
        <p>玩家2: 方向键 移动, 回车 射击</p>
      </div>
    </div>
  );
}