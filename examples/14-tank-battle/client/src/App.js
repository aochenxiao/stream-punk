import { jsx as _jsx, jsxs as _jsxs } from "react/jsx-runtime";
import { useState, useCallback } from 'react';
import Lobby from './components/Lobby';
import GameCanvas from './components/GameCanvas';
export default function App() {
    const [page, setPage] = useState('lobby');
    const [ws, setWs] = useState(null);
    const [playerId, setPlayerId] = useState(0);
    const handleJoin = useCallback((gameWs, pid) => {
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
    return (_jsxs("div", { style: { width: '100vw', height: '100vh', display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center' }, children: [page === 'lobby' && _jsx(Lobby, { onJoin: handleJoin }), page === 'game' && ws && _jsx(GameCanvas, { ws: ws, playerId: playerId, onBack: handleBack })] }));
}
