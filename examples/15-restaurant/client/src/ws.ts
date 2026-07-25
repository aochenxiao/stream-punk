import { ServerState, PlaceOrderRequest, UpdateStatusRequest, PaymentRequest, ChangeTableRequest, RoleLoginRequest, UrgeDishRequest, MergeOrdersRequest, Notification, RoleAssigned, OrderItem, I, O } from './stream-punk-data';

export type StateHandler = (state: ServerState) => void;
export type NotificationHandler = (msg: string, type: number, targetRole: number) => void;
export type RoleHandler = (role: number, name: string, tableId: number) => void;
export type StatusHandler = (status: string) => void;

const DBG = (tag: string, ...args: unknown[]) => console.log(`[WS ${tag}]`, ...args);

export class RestaurantWS {
  private ws: WebSocket | null = null;
  private url: string;
  private onState: StateHandler;
  private onNotification: NotificationHandler;
  private onRole: RoleHandler;
  private onStatus: StatusHandler;

  constructor(url: string, handlers: {
    onState: StateHandler;
    onNotification: NotificationHandler;
    onRole: RoleHandler;
    onStatus: StatusHandler;
  }) {
    this.url = url;
    this.onState = handlers.onState;
    this.onNotification = handlers.onNotification;
    this.onRole = handlers.onRole;
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
    this.ws.onopen = () => { DBG('open'); this.onStatus('Connected'); };
    this.ws.onmessage = (event) => {
      if (event.data instanceof ArrayBuffer) {
        this.handleBinary(new Uint8Array(event.data));
      }
    };
    this.ws.onclose = (e) => { DBG('close', e.code); this.onStatus('Disconnected'); };
    this.ws.onerror = () => this.onStatus('Error');
  }

  private handleBinary(data: Uint8Array): void {
    if (data.length === 0) return;
    const msgType = data[0];
    const payload = data.slice(1);
    try {
      switch (msgType) {
        case 0x10: { // ServerState
          const buf = payload.buffer.slice(payload.byteOffset, payload.byteOffset + payload.length);
          const i = new I(buf);
          const state = new ServerState().from(i);
          this.onState(state);
          break;
        }
        case 0x11: { // Notification
          const buf = payload.buffer.slice(payload.byteOffset, payload.byteOffset + payload.length);
          const n = new Notification().from(new I(buf));
          this.onNotification(n.message, n.type, n.targetRole);
          break;
        }
        case 0x12: { // RoleAssigned
          const buf = payload.buffer.slice(payload.byteOffset, payload.byteOffset + payload.length);
          const ra = new RoleAssigned().from(new I(buf));
          this.onRole(ra.role, ra.name, ra.tableId);
          break;
        }
      }
    } catch (e) {
      DBG('parse error', e);
    }
  }

  private send(msgType: number, data: Uint8Array): void {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return;
    const msg = new Uint8Array(1 + data.length);
    msg[0] = msgType;
    msg.set(data, 1);
    this.ws.send(msg.buffer);
  }

  login(role: number, name: string): void {
    const req = new RoleLoginRequest();
    req.role = role;
    req.name = name;
    const o = new O();
    req.to(o);
    this.send(0x04, o.toBytes());
  }

  placeOrder(tableId: number, items: Array<{ menuItemId: number; menuItemName: string; quantity: number }>): void {
    const req = new PlaceOrderRequest();
    req.tableId = tableId;
    req.items = items.map(it => {
      const oi = new OrderItem();
      oi.menuItemId = it.menuItemId;
      oi.menuItemName = it.menuItemName;
      oi.quantity = it.quantity;
      oi.status = 0;
      oi.note = '';
      return oi;
    });
    const o = new O();
    req.to(o);
    this.send(0x01, o.toBytes());
  }

  updateStatus(orderId: number, itemIndex: number, newStatus: number, note: string = ''): void {
    const req = new UpdateStatusRequest();
    req.orderId = orderId;
    req.itemIndex = itemIndex;
    req.newStatus = newStatus;
    req.note = note;
    const o = new O();
    req.to(o);
    this.send(0x02, o.toBytes());
  }

  pay(tableId: number, paymentMethod: string, discount: number = 1.0): void {
    const req = new PaymentRequest();
    req.tableId = tableId;
    req.paymentMethod = paymentMethod;
    req.discount = discount;
    req.authCode = '';
    const o = new O();
    req.to(o);
    this.send(0x03, o.toBytes());
  }

  changeTable(fromTableId: number, toTableId: number): void {
    const req = new ChangeTableRequest();
    req.fromTableId = fromTableId;
    req.toTableId = toTableId;
    const o = new O();
    req.to(o);
    this.send(0x05, o.toBytes());
  }

  urgeDish(orderId: number): void {
    const req = new UrgeDishRequest();
    req.orderId = orderId;
    const o = new O();
    req.to(o);
    this.send(0x08, o.toBytes());
  }

  mergeOrders(tableId1: number, tableId2: number): void {
    const req = new MergeOrdersRequest();
    req.tableId1 = tableId1;
    req.tableId2 = tableId2;
    const o = new O();
    req.to(o);
    this.send(0x06, o.toBytes());
  }

  disconnect(): void {
    this.ws?.close();
    this.ws = null;
  }
}