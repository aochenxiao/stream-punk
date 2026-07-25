// WebSocket 客户端：连接五子棋服务端
import { GameState, Move, I, O } from './stream-punk-data';

export type GameStateHandler = (state: GameState) => void;
export type PlayerIdHandler = (playerId: number) => void;
export type StatusHandler = (status: string) => void;

const DBG = (tag: string, ...args: unknown[]) => {
  console.log(`[WS ${tag}]`, ...args);
};

export class GameWS {
  private ws: WebSocket | null = null;
  private url: string;
  private onGameState: GameStateHandler;
  private onPlayerId: PlayerIdHandler;
  private onStatusChange: StatusHandler;

  constructor(
    url: string,
    handlers: {
      onGameState: GameStateHandler;
      onPlayerId: PlayerIdHandler;
      onStatusChange: StatusHandler;
    },
  ) {
    this.url = url;
    this.onGameState = handlers.onGameState;
    this.onPlayerId = handlers.onPlayerId;
    this.onStatusChange = handlers.onStatusChange;
  }

  connect(): void {
    DBG('connect', `connecting to ${this.url}...`);
    this.onStatusChange('连接中...');

    try {
      this.ws = new WebSocket(this.url);
      this.ws.binaryType = 'arraybuffer';
    } catch (e) {
      DBG('connect', 'WebSocket constructor threw:', e);
      this.onStatusChange('创建连接失败');
      return;
    }

    this.ws.onopen = () => {
      DBG('onopen', 'WebSocket connected!');
      this.onStatusChange('已连接');
    };

    this.ws.onmessage = (event) => {
      if (event.data instanceof ArrayBuffer) {
        this.handleBinary(new Uint8Array(event.data));
      }
    };

    this.ws.onclose = (event) => {
      DBG('onclose', `code=${event.code}`);
      this.onStatusChange(`已断开 (code=${event.code})`);
    };

    this.ws.onerror = () => {
      this.onStatusChange('连接错误');
    };
  }

  private handleBinary(data: Uint8Array): void {
    if (data.length === 0) return;
    const msgType = data[0];
    const payload = data.slice(1);

    try {
      switch (msgType) {
        case 0x02: { // GameState
          const buf = payload.buffer.slice(payload.byteOffset, payload.byteOffset + payload.length);
          const state = new GameState().from(new I(buf));
          this.onGameState(state);
          break;
        }
        case 0x04: { // PlayerId
          if (payload.length >= 1) {
            this.onPlayerId(payload[0]);
          }
          break;
        }
      }
    } catch (e) {
      DBG('handleBinary', 'parse error:', e);
    }
  }

  sendMove(row: number, col: number): void {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return;
    const move = new Move();
    move.row = row;
    move.col = col;
    const o = new O();
    move.to(o);
    const bytes = o.toBytes();
    const msg = new Uint8Array(1 + bytes.length);
    msg[0] = 0x01;
    msg.set(bytes, 1);
    this.ws.send(msg.buffer);
  }

  disconnect(): void {
    this.ws?.close();
    this.ws = null;
  }
}