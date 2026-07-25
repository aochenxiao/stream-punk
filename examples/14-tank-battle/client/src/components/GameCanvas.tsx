import { useEffect, useRef, useCallback } from 'react';
import { GameWS } from '../ws';
import { GameState, PlayerInput } from '../stream-punk-data';

const CANVAS_W = 800;
const CANVAS_H = 600;
const INPUT_INTERVAL = 50; // ms

interface Props {
  ws: GameWS;
  playerId: number;
  onBack: () => void;
}

export default function GameCanvas({ ws, playerId, onBack }: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const gameStateRef = useRef<GameState | null>(null);
  const keysRef = useRef<Set<string>>(new Set());
  const inputTimerRef = useRef<number>(0);

  // 发送输入
  const sendInput = useCallback(() => {
    const keys = keysRef.current;
    const input = new PlayerInput();
    if (playerId === 1) {
      input.up = keys.has('w') || keys.has('W');
      input.down = keys.has('s') || keys.has('S');
      input.left = keys.has('a') || keys.has('A');
      input.right = keys.has('d') || keys.has('D');
      input.fire = keys.has(' ');
    } else {
      input.up = keys.has('ArrowUp');
      input.down = keys.has('ArrowDown');
      input.left = keys.has('ArrowLeft');
      input.right = keys.has('ArrowRight');
      input.fire = keys.has('Enter');
    }
    ws.sendInput(input);
  }, [ws, playerId]);

  // 键盘事件
  useEffect(() => {
    const handleDown = (e: KeyboardEvent) => {
      keysRef.current.add(e.key);
      if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', ' '].includes(e.key)) {
        e.preventDefault();
      }
    };
    const handleUp = (e: KeyboardEvent) => {
      keysRef.current.delete(e.key);
    };
    window.addEventListener('keydown', handleDown);
    window.addEventListener('keyup', handleUp);
    return () => {
      window.removeEventListener('keydown', handleDown);
      window.removeEventListener('keyup', handleUp);
    };
  }, []);

  // 定时发送输入
  useEffect(() => {
    inputTimerRef.current = window.setInterval(sendInput, INPUT_INTERVAL);
    return () => clearInterval(inputTimerRef.current);
  }, [sendInput]);

  // 接收游戏状态
  useEffect(() => {
    const onGameState = (state: GameState) => {
      gameStateRef.current = state;
    };
    const onPlayerId = () => {};
    const onStatusChange = () => {};
    // 替换 ws 的回调
    const origWs = ws as any;
    origWs.onGameState = onGameState;
    origWs.onPlayerId = onPlayerId;
    origWs.onStatusChange = onStatusChange;
  }, [ws]);

  // 渲染循环
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let animId = 0;
    const render = () => {
      const state = gameStateRef.current;
      if (!state) {
        animId = requestAnimationFrame(render);
        return;
      }

      // 清屏
      ctx.fillStyle = '#1a1a2e';
      ctx.fillRect(0, 0, CANVAS_W, CANVAS_H);

      // 绘制网格
      ctx.strokeStyle = '#2a2a4a';
      ctx.lineWidth = 0.5;
      for (let x = 0; x < CANVAS_W; x += 40) {
        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, CANVAS_H); ctx.stroke();
      }
      for (let y = 0; y < CANVAS_H; y += 40) {
        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(CANVAS_W, y); ctx.stroke();
      }

      // 绘制子弹
      for (const bullet of state.bullets) {
        ctx.fillStyle = '#ffdd57';
        ctx.beginPath();
        ctx.arc(bullet.x, bullet.y, 4, 0, Math.PI * 2);
        ctx.fill();
      }

      // 绘制玩家
      const colors = ['#00d2ff', '#e94560'];
      for (const player of state.players) {
        const colorIdx = player.id === 1 ? 0 : 1;
        const color = colors[colorIdx];
        const isMe = player.id === playerId;

        ctx.save();
        ctx.translate(player.x, player.y);
        ctx.rotate(player.rotation);

        // 坦克车身
        ctx.fillStyle = player.hp > 0 ? color : '#444';
        ctx.fillRect(-16, -12, 32, 24);
        // 炮管
        ctx.fillRect(0, -3, 22, 6);
        // 履带
        ctx.fillStyle = '#333';
        ctx.fillRect(-18, -14, 4, 28);
        ctx.fillRect(14, -14, 4, 28);

        ctx.restore();

        // 血条
        if (player.hp > 0) {
          const barW = 40;
          const barH = 5;
          const barX = player.x - barW / 2;
          const barY = player.y - 28;
          ctx.fillStyle = '#333';
          ctx.fillRect(barX, barY, barW, barH);
          ctx.fillStyle = player.hp > 50 ? '#00d2ff' : player.hp > 25 ? '#ffdd57' : '#e94560';
          ctx.fillRect(barX, barY, barW * (player.hp / 100), barH);
        }

        // 玩家标签
        ctx.fillStyle = '#fff';
        ctx.font = '12px monospace';
        ctx.textAlign = 'center';
        const label = `P${player.id}${isMe ? ' (你)' : ''}`;
        ctx.fillText(label, player.x, player.y - 32);
      }

      // 检查游戏结束
      const alivePlayers = state.players.filter(p => p.hp > 0);
      if (alivePlayers.length <= 1 && state.players.length >= 2) {
        ctx.fillStyle = 'rgba(0,0,0,0.6)';
        ctx.fillRect(0, 0, CANVAS_W, CANVAS_H);
        ctx.fillStyle = '#ffdd57';
        ctx.font = 'bold 36px monospace';
        ctx.textAlign = 'center';
        if (alivePlayers.length === 1) {
          ctx.fillText(`P${alivePlayers[0].id} 获胜!`, CANVAS_W / 2, CANVAS_H / 2);
        } else {
          ctx.fillText('平局!', CANVAS_W / 2, CANVAS_H / 2);
        }
      }

      animId = requestAnimationFrame(render);
    };
    animId = requestAnimationFrame(render);
    return () => cancelAnimationFrame(animId);
  }, [playerId]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '12px' }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
        <span style={{ color: '#00d2ff', fontWeight: 'bold' }}>P1: WASD + 空格</span>
        <span style={{ color: '#e94560', fontWeight: 'bold' }}>P2: 方向键 + 回车</span>
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
        width={CANVAS_W}
        height={CANVAS_H}
        style={{ border: '2px solid #333', borderRadius: '4px', background: '#1a1a2e' }}
      />
    </div>
  );
}