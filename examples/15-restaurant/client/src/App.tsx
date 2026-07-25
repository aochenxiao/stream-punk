import { useState, useCallback, useRef, useEffect } from 'react';
import { RestaurantWS, StateHandler, NotificationHandler, RoleHandler, StatusHandler } from './ws';
import { ServerState } from './stream-punk-data';
import LoginView from './components/LoginView';
import WaiterView from './components/WaiterView';
import KitchenView from './components/KitchenView';
import RunnerView from './components/RunnerView';
import CashierView from './components/CashierView';
import ManagerView from './components/ManagerView';
import BossView from './components/BossView';
import CustomerView from './components/CustomerView';
import WarehouseView from './components/WarehouseView';

const WS_URL = 'ws://localhost:9004';

const ROLE_NAMES = ['老板','大堂经理','收银员','主厨','厨师','切配员','传菜员','服务员','仓库管理员','顾客'];

export default function App() {
  const [state, setState] = useState<ServerState | null>(null);
  const [role, setRole] = useState<number | null>(null);
  const [name, setName] = useState('');
  const [tableId, setTableId] = useState(0);
  const [wsStatus, setWsStatus] = useState('未连接');
  const [notification, setNotification] = useState<{ msg: string; type: number } | null>(null);
  const wsRef = useRef<RestaurantWS | null>(null);
  const lastNoteRef = useRef('');

  useEffect(() => {
    const ws = new RestaurantWS(WS_URL, {
      onState: ((s: ServerState) => setState(s)) as StateHandler,
      onNotification: ((msg: string, type: number) => {
        const noteKey = msg + type;
        if (noteKey !== lastNoteRef.current) {
          lastNoteRef.current = noteKey;
          setNotification({ msg, type });
          setTimeout(() => {
            setNotification(prev => {
              if (prev && prev.msg + prev.type === noteKey) return null;
              return prev;
            });
          }, 3000);
        }
      }) as NotificationHandler,
      onRole: ((r: number, n: string, tid: number) => { setRole(r); setName(n); setTableId(tid); }) as RoleHandler,
      onStatus: ((s: string) => {
        if (s === 'Connected') setWsStatus('已连接');
        else if (s === 'Disconnected') setWsStatus('未连接');
        else setWsStatus(s);
      }) as StatusHandler,
    });
    wsRef.current = ws;
    ws.connect();
    return () => ws.disconnect();
  }, []);

  const handleLogin = useCallback((r: number, n: string) => {
    wsRef.current?.login(r, n);
  }, []);

  const handleLogout = useCallback(() => {
    setRole(null);
    setName('');
    wsRef.current?.disconnect();
    wsRef.current?.connect();
  }, []);

  const noteClass = notification ? (notification.type === 0 ? 'info' : notification.type === 2 ? 'error' : notification.type === 3 ? 'urgent' : 'warn') : 'info';

  if (role === null) {
    return (
      <div>
        <div className={`notification-bar ${noteClass}${notification ? ' visible' : ''}`}>
          {notification?.msg}
        </div>
        <LoginView onLogin={handleLogin} status={wsStatus} />
      </div>
    );
  }

  const renderView = () => {
    if (!state) return <div className="flex items-center justify-center h-screen text-gray-400">加载中...</div>;
    switch (role) {
      case 7: return <WaiterView state={state} ws={wsRef.current!} />;
      case 3: case 4: case 5: return <KitchenView state={state} ws={wsRef.current!} role={role} />;
      case 6: return <RunnerView state={state} ws={wsRef.current!} />;
      case 2: return <CashierView state={state} ws={wsRef.current!} />;
      case 1: return <ManagerView state={state} ws={wsRef.current!} />;
      case 0: return <BossView state={state} />;
      case 8: return <WarehouseView state={state} />;
      case 9: return <CustomerView state={state} ws={wsRef.current!} tableId={tableId} />;
      default: return <div className="p-8">未知角色</div>;
    }
  };

  return (
    <div>
      <div className={`notification-bar ${noteClass}${notification ? ' visible' : ''}`}>
        {notification?.msg}
      </div>
      <header className="bg-white shadow-sm px-6 py-3 flex items-center justify-between">
        <div className="flex items-center gap-3">
          <h1 className="text-lg font-bold text-gray-800">餐厅管理系统</h1>
          <span className={`inline-block w-2 h-2 rounded-full ${wsStatus === '已连接' ? 'bg-green-500' : 'bg-red-500'}`} />
          <span className="text-xs text-gray-500">{wsStatus}</span>
        </div>
        <div className="flex items-center gap-3">
          <span className="text-sm text-gray-600">{ROLE_NAMES[role]} - {name}</span>
          <button onClick={handleLogout} className="btn btn-sm btn-danger">退出</button>
        </div>
      </header>
      {renderView()}
    </div>
  );
}