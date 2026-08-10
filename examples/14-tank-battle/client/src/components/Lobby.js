import { jsx as _jsx, jsxs as _jsxs } from "react/jsx-runtime";
import { useState, useCallback } from 'react';
import { GameWS } from '../ws';
export default function Lobby({ onJoin }) {
    const [status, setStatus] = useState('');
    const [joined, setJoined] = useState(false);
    const handleJoin = useCallback(() => {
        setStatus('连接中...');
        const wsUrl = `ws://${window.location.hostname}:9002`;
        const gameWs = new GameWS(wsUrl, {
            onGameState: () => { },
            onPlayerId: (pid) => {
                setJoined(true);
                setStatus(`已加入 - 玩家 ${pid}`);
                onJoin(gameWs, pid);
            },
            onStatusChange: setStatus,
        });
        gameWs.connect();
    }, [onJoin]);
    if (joined)
        return null;
    return (_jsxs("div", { style: { textAlign: 'center' }, children: [_jsx("h1", { style: { fontSize: '3rem', marginBottom: '0.5rem', color: '#e94560' }, children: "Tank Battle" }), _jsx("p", { style: { color: '#aaa', marginBottom: '2rem' }, children: "\u53CC\u4EBA\u5766\u514B\u5927\u6218" }), _jsx("button", { onClick: handleJoin, style: {
                    padding: '16px 48px',
                    fontSize: '1.2rem',
                    background: '#e94560',
                    color: '#fff',
                    border: 'none',
                    borderRadius: '8px',
                    cursor: 'pointer',
                    fontWeight: 'bold',
                }, children: "\u52A0\u5165\u6E38\u620F" }), status && _jsx("p", { style: { marginTop: '1rem', color: '#aaa' }, children: status }), _jsxs("div", { style: { marginTop: '2rem', color: '#666', fontSize: '0.9rem' }, children: [_jsx("p", { children: "\u73A9\u5BB61: WASD \u79FB\u52A8, \u7A7A\u683C \u5C04\u51FB" }), _jsx("p", { children: "\u73A9\u5BB62: \u65B9\u5411\u952E \u79FB\u52A8, \u56DE\u8F66 \u5C04\u51FB" })] })] }));
}
