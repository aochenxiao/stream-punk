import React, { useState, useCallback, useRef } from 'react';
import Login from './components/Login';
import Editor from './components/Editor';
import UserList from './components/UserList';
import { CollabWS } from './ws';
import { JoinResponse, TextOp, CursorInfo } from './stream-punk-data';

const WS_URL = `ws://${window.location.hostname}:9005`;

const App: React.FC = () => {
  const [joined, setJoined] = useState(false);
  const [userName, setUserName] = useState('');
  const [myUserId, setMyUserId] = useState(0);
  const [myColor, setMyColor] = useState('');
  const [document, setDocument] = useState('');
  const [users, setUsers] = useState<CursorInfo[]>([]);
  const [status, setStatus] = useState('Disconnected');
  const wsRef = useRef<CollabWS | null>(null);

  const handleJoin = useCallback((name: string) => {
    setUserName(name);
    const ws = new CollabWS(WS_URL, {
      onJoin: (resp: JoinResponse) => {
        setMyUserId(resp.userId);
        setDocument(resp.document);
        setUsers(resp.users);
        // Find my color
        const me = resp.users.find((u) => u.userId === resp.userId);
        if (me) setMyColor(me.color);
        setJoined(true);
      },
      onTextOp: (op: TextOp) => {
        if (op.userId === myUserId) return;
        setDocument((prev) => {
          if (op.opType === 0) {
            // Insert
            return prev.slice(0, op.position) + op.text + prev.slice(op.position);
          } else {
            // Delete
            const len = parseInt(op.text, 10);
            return prev.slice(0, op.position) + prev.slice(op.position + len);
          }
        });
      },
      onCursor: (cursor: CursorInfo) => {
        setUsers((prev) =>
          prev.map((u) => {
            if (u.userId === cursor.userId) {
              const updated = new CursorInfo();
              updated.userId = u.userId;
              updated.userName = u.userName;
              updated.position = cursor.position;
              updated.color = u.color;
              return updated;
            }
            return u;
          })
        );
      },
      onUserList: (newUsers: CursorInfo[]) => {
        setUsers(newUsers);
      },
      onStatus: setStatus,
    });
    wsRef.current = ws;
    ws.connect();
    ws.join(name);
  }, [myUserId]);

  const handleTextOp = useCallback((opType: number, position: number, text: string) => {
    wsRef.current?.sendTextOp(opType, position, text, myUserId, 0);
  }, [myUserId]);

  const handleCursorMove = useCallback((position: number) => {
    wsRef.current?.sendCursorUpdate(myUserId, userName, position, myColor);
  }, [myUserId, userName, myColor]);

  if (!joined) {
    return <Login onJoin={handleJoin} />;
  }

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100vh',
      backgroundColor: '#1a1a2e',
      color: '#eee',
      fontFamily: 'system-ui, sans-serif',
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 16px',
        backgroundColor: '#0f3460',
        borderBottom: '1px solid #333',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span style={{ fontWeight: 'bold', color: '#e94560', fontSize: '16px' }}>
            Collab Editor
          </span>
          <span style={{ fontSize: '12px', color: '#888' }}>
            {status}
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px' }}>
          <div style={{
            width: '8px',
            height: '8px',
            borderRadius: '50%',
            backgroundColor: myColor,
          }} />
          <span>{userName}</span>
        </div>
      </div>

      {/* Main area */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        <UserList users={users} myUserId={myUserId} />
        <Editor
          document={document}
          users={users}
          myUserId={myUserId}
          onTextOp={handleTextOp}
          onCursorMove={handleCursorMove}
        />
      </div>
    </div>
  );
};

export default App;