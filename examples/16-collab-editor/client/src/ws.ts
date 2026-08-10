import { TextOp, CursorInfo, JoinRequest, JoinResponse, UserListUpdate, I, O } from './stream-punk-data';

export type JoinHandler = (resp: JoinResponse) => void;
export type TextOpHandler = (op: TextOp) => void;
export type CursorHandler = (cursor: CursorInfo) => void;
export type UserListHandler = (users: CursorInfo[]) => void;
export type StatusHandler = (status: string) => void;

const DBG = (tag: string, ...args: unknown[]) => console.log(`[WS ${tag}]`, ...args);

export class CollabWS {
  private ws: WebSocket | null = null;
  private url: string;
  private onJoin: JoinHandler;
  private onTextOp: TextOpHandler;
  private onCursor: CursorHandler;
  private onUserList: UserListHandler;
  private onStatus: StatusHandler;
  private pendingMessages: Array<{ type: number; data: Uint8Array }> = [];

  constructor(url: string, handlers: {
    onJoin: JoinHandler;
    onTextOp: TextOpHandler;
    onCursor: CursorHandler;
    onUserList: UserListHandler;
    onStatus: StatusHandler;
  }) {
    this.url = url;
    this.onJoin = handlers.onJoin;
    this.onTextOp = handlers.onTextOp;
    this.onCursor = handlers.onCursor;
    this.onUserList = handlers.onUserList;
    this.onStatus = handlers.onStatus;
  }

  connect(): void {
    DBG('connect', this.url);
    this.onStatus('Connecting...');
    try {
      this.ws = new WebSocket(this.url);
      this.ws.binaryType = 'arraybuffer';
    } catch (e) {
      DBG('error', e);
      this.onStatus('Connection failed');
      return;
    }
    this.ws.onopen = () => {
      DBG('open');
      this.onStatus('Connected');
      // Flush pending messages
      for (const msg of this.pendingMessages) {
        this.send(msg.type, msg.data);
      }
      this.pendingMessages = [];
    };
    this.ws.onmessage = (event) => {
      if (event.data instanceof ArrayBuffer) {
        this.handleBinary(new Uint8Array(event.data));
      }
    };
    this.ws.onclose = (e) => {
      DBG('close', e.code);
      this.onStatus('Disconnected');
    };
    this.ws.onerror = () => this.onStatus('Error');
  }

  private handleBinary(data: Uint8Array): void {
    if (data.length === 0) return;
    const msgType = data[0];
    const payload = data.slice(1);
    try {
      const buf = payload.buffer.slice(payload.byteOffset, payload.byteOffset + payload.length);
      switch (msgType) {
        case 0x02: { // JoinResponse
          const resp = new JoinResponse().from(new I(buf));
          this.onJoin(resp);
          break;
        }
        case 0x03: { // TextOp
          const op = new TextOp().from(new I(buf));
          this.onTextOp(op);
          break;
        }
        case 0x04: { // CursorUpdate
          const cursor = new CursorInfo().from(new I(buf));
          this.onCursor(cursor);
          break;
        }
        case 0x05: { // UserList
          const ul = new UserListUpdate().from(new I(buf));
          this.onUserList(ul.users);
          break;
        }
      }
    } catch (e) {
      DBG('parse error', e);
    }
  }

  private send(msgType: number, data: Uint8Array): void {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      this.pendingMessages.push({ type: msgType, data });
      return;
    }
    const msg = new Uint8Array(1 + data.length);
    msg[0] = msgType;
    msg.set(data, 1);
    this.ws.send(msg.buffer);
  }

  join(userName: string): void {
    const req = new JoinRequest();
    req.userName = userName;
    const o = new O();
    req.to(o);
    this.send(0x01, o.toBytes());
  }

  sendTextOp(opType: number, position: number, text: string, userId: number, version: number): void {
    const op = new TextOp();
    op.opType = opType;
    op.position = position;
    op.text = text;
    op.userId = userId;
    op.version = version;
    const o = new O();
    op.to(o);
    this.send(0x03, o.toBytes());
  }

  sendCursorUpdate(userId: number, userName: string, position: number, color: string): void {
    const cursor = new CursorInfo();
    cursor.userId = userId;
    cursor.userName = userName;
    cursor.position = position;
    cursor.color = color;
    const o = new O();
    cursor.to(o);
    this.send(0x04, o.toBytes());
  }

  disconnect(): void {
    this.ws?.close();
    this.ws = null;
  }
}