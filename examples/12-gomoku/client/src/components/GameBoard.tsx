import { useEffect, useRef, useCallback, useState } from 'react';
import { GameWS } from '../ws';
import { GameState } from '../stream-punk-data';

const BOARD_SIZE = 15;
const CELL_SIZE = 36;
const PADDING = 30;
const CANVAS_SIZE = PADDING * 2 + CELL_SIZE * (BOARD_SIZE - 1);
const STONE_RADIUS = CELL_SIZE * 0.42;

interface Props {
  ws: GameWS;
  playerId: number;
  onBack: () => void;
}

function drawBoard(ctx: CanvasRenderingContext2D, state: GameState | null, playerId: number) {
  const dpr = window.devicePixelRatio || 1;
  ctx.canvas.width = CANVAS_SIZE * dpr;
  ctx.canvas.height = CANVAS_SIZE * dpr;
  ctx.canvas.style.width = CANVAS_SIZE + 'px';
  ctx.canvas.style.height = CANVAS_SIZE + 'px';
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

  // 背景
  ctx.fillStyle = '#dcb468';
  ctx.fillRect(0, 0, CANVAS_SIZE, CANVAS_SIZE);

  // 网格线
  ctx.strokeStyle = '#333';
  ctx.lineWidth = 1;
  for (let i = 0; i < BOARD_SIZE; i++) {
    const pos = PADDING + i * CELL_SIZE;
    ctx.beginPath();
    ctx.moveTo(PADDING, pos);
    ctx.lineTo(PADDING + (BOARD_SIZE - 1) * CELL_SIZE, pos);
    ctx.stroke();

    ctx.beginPath();
    ctx.moveTo(pos, PADDING);
    ctx.lineTo(pos, PADDING + (BOARD_SIZE - 1) * CELL_SIZE);
    ctx.stroke();
  }

  // 星位点
  const starPoints = [
    [3, 3], [3, 7], [3, 11],
    [7, 3], [7, 7], [7, 11],
    [11, 3], [11, 7], [11, 11],
  ];
  ctx.fillStyle = '#333';
  for (const [r, c] of starPoints) {
    ctx.beginPath();
    ctx.arc(PADDING + c * CELL_SIZE, PADDING + r * CELL_SIZE, 3, 0, Math.PI * 2);
    ctx.fill();
  }

  if (!state) return;

  // 绘制棋子
  for (const stone of state.stones) {
    const cx = PADDING + stone.col * CELL_SIZE;
    const cy = PADDING + stone.row * CELL_SIZE;

    // 阴影
    ctx.fillStyle = 'rgba(0,0,0,0.3)';
    ctx.beginPath();
    ctx.arc(cx + 2, cy + 2, STONE_RADIUS, 0, Math.PI * 2);
    ctx.fill();

    // 棋子
    if (stone.player === 1) {
      const grad = ctx.createRadialGradient(cx - 3, cy - 3, 2, cx, cy, STONE_RADIUS);
      grad.addColorStop(0, '#555');
      grad.addColorStop(1, '#111');
      ctx.fillStyle = grad;
    } else {
      const grad = ctx.createRadialGradient(cx - 3, cy - 3, 2, cx, cy, STONE_RADIUS);
      grad.addColorStop(0, '#fff');
      grad.addColorStop(1, '#ccc');
      ctx.fillStyle = grad;
    }
    ctx.beginPath();
    ctx.arc(cx, cy, STONE_RADIUS, 0, Math.PI * 2);
    ctx.fill();
  }

  // 最后落子标记
  if (state.stones.length > 0) {
    const last = state.stones[state.stones.length - 1];
    const lx = PADDING + last.col * CELL_SIZE;
    const ly = PADDING + last.row * CELL_SIZE;
    ctx.strokeStyle = '#e94560';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(lx, ly, 5, 0, Math.PI * 2);
    ctx.stroke();
  }

  // 赢家覆盖层
  if (state.winner !== 0) {
    ctx.fillStyle = 'rgba(0,0,0,0.6)';
    ctx.fillRect(0, 0, CANVAS_SIZE, CANVAS_SIZE);
    ctx.fillStyle = '#ffdd57';
    ctx.font = 'bold 32px sans-serif';
    ctx.textAlign = 'center';
    const winnerLabel = state.winner === playerId ? '你赢了!' : `玩家${state.winner} 获胜`;
    ctx.fillText(winnerLabel, CANVAS_SIZE / 2, CANVAS_SIZE / 2 - 10);
  }
}

export default function GameBoard({ ws, playerId, onBack }: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [gameState, setGameState] = useState<GameState | null>(null);
  const [restarting, setRestarting] = useState(false);

  const handleRestart = useCallback(() => {
    setRestarting(true);
    ws.sendRestart();
  }, [ws]);

  // 接收游戏状态 — 服务端重置后会广播新的 GameState
  useEffect(() => {
    const onGameState = (state: GameState) => {
      setGameState(state);
      if (state.winner === 0) setRestarting(false);
    };
    const onPlayerId = () => {};
    const onStatusChange = () => {};
    const origWs = ws as any;
    origWs.onGameState = onGameState;
    origWs.onPlayerId = onPlayerId;
    origWs.onStatusChange = onStatusChange;
  }, [ws]);

  // 渲染棋盘 — 依赖 gameState 变化
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    drawBoard(ctx, gameState, playerId);
  }, [gameState, playerId]);

  // 棋盘点击
  const handleCanvasClick = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    if (!gameState) return;
    if (gameState.winner !== 0) return;
    if (gameState.currentPlayer !== playerId) return;

    const canvas = canvasRef.current;
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const scaleX = CANVAS_SIZE / rect.width;
    const scaleY = CANVAS_SIZE / rect.height;
    const mx = (e.clientX - rect.left) * scaleX;
    const my = (e.clientY - rect.top) * scaleY;

    const col = Math.round((mx - PADDING) / CELL_SIZE);
    const row = Math.round((my - PADDING) / CELL_SIZE);

    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) return;

    // 检查是否已有棋子
    for (const stone of gameState.stones) {
      if (stone.row === row && stone.col === col) return;
    }

    ws.sendMove(row, col);
  }, [ws, playerId, gameState]);

  const isMyTurn = gameState && gameState.winner === 0 && gameState.currentPlayer === playerId;
  const isBlack = playerId === 1;
  const turnLabel = gameState
    ? gameState.winner !== 0
      ? '游戏结束'
      : gameState.currentPlayer === playerId
        ? '轮到你落子'
        : `等待玩家${gameState.currentPlayer}落子...`
    : '等待对手加入...';

  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '12px' }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
        <span style={{
          color: isBlack ? '#333' : '#ccc',
          fontWeight: isMyTurn ? 'bold' : 'normal',
          fontSize: '1rem',
          background: isBlack ? '#dcb468' : '#555',
          padding: '4px 12px',
          borderRadius: '4px',
        }}>
          {isBlack ? '黑子' : '白子'}
        </span>
        <span style={{ color: '#aaa' }}>{turnLabel}</span>
        {gameState && gameState.winner !== 0 && (
          <button
            onClick={handleRestart}
            disabled={restarting}
            style={{
              padding: '6px 16px',
              background: '#e94560',
              color: '#fff',
              border: 'none',
              borderRadius: '4px',
              cursor: restarting ? 'not-allowed' : 'pointer',
              fontWeight: 'bold',
              opacity: restarting ? 0.6 : 1,
            }}
          >
            {restarting ? '等待中...' : '再来一盘'}
          </button>
        )}
        <button
          onClick={onBack}
          style={{
            padding: '6px 16px',
            background: '#333',
            color: '#fff',
            border: '1px solid #555',
            borderRadius: '4px',
            cursor: 'pointer',
          }}
        >
          返回
        </button>
      </div>
      <canvas
        ref={canvasRef}
        width={CANVAS_SIZE}
        height={CANVAS_SIZE}
        onClick={handleCanvasClick}
        style={{
          border: '2px solid #333',
          borderRadius: '4px',
          cursor: isMyTurn ? 'pointer' : 'default',
        }}
      />
    </div>
  );
}