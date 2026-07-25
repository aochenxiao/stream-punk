// WebSocket 客户端：连接坦克大战服务端
import { GameState, I, O } from './stream-punk-data';
const DBG = (tag, ...args) => {
    console.log(`[WS ${tag}]`, ...args);
};
export class GameWS {
    constructor(url, handlers) {
        this.ws = null;
        this.url = url;
        this.onGameState = handlers.onGameState;
        this.onPlayerId = handlers.onPlayerId;
        this.onStatusChange = handlers.onStatusChange;
    }
    connect() {
        DBG('connect', `connecting to ${this.url}...`);
        this.onStatusChange('连接中...');
        try {
            this.ws = new WebSocket(this.url);
            this.ws.binaryType = 'arraybuffer';
        }
        catch (e) {
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
    handleBinary(data) {
        if (data.length === 0)
            return;
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
        }
        catch (e) {
            DBG('handleBinary', 'parse error:', e);
        }
    }
    sendInput(input) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN)
            return;
        const o = new O();
        input.to(o);
        const bytes = o.toBytes();
        const msg = new Uint8Array(1 + bytes.length);
        msg[0] = 0x01;
        msg.set(bytes, 1);
        this.ws.send(msg.buffer);
    }
    disconnect() {
        this.ws?.close();
        this.ws = null;
    }
}
