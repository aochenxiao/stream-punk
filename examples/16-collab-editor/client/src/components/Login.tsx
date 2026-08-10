import React, { useState } from 'react';

interface LoginProps {
  onJoin: (userName: string) => void;
}

const Login: React.FC<LoginProps> = ({ onJoin }) => {
  const [userName, setUserName] = useState('');

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    const name = userName.trim();
    if (name) {
      onJoin(name);
    }
  };

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'center',
      justifyContent: 'center',
      height: '100vh',
      backgroundColor: '#1a1a2e',
      color: '#eee',
      fontFamily: 'system-ui, sans-serif',
    }}>
      <div style={{
        backgroundColor: '#16213e',
        padding: '40px',
        borderRadius: '12px',
        boxShadow: '0 4px 20px rgba(0,0,0,0.3)',
        minWidth: '320px',
        textAlign: 'center',
      }}>
        <h1 style={{ margin: '0 0 8px 0', fontSize: '24px', color: '#e94560' }}>
          Collaborative Editor
        </h1>
        <p style={{ margin: '0 0 24px 0', color: '#888', fontSize: '14px' }}>
          Real-time text collaboration
        </p>
        <form onSubmit={handleSubmit}>
          <input
            type="text"
            value={userName}
            onChange={(e) => setUserName(e.target.value)}
            placeholder="Enter your name"
            autoFocus
            style={{
              width: '100%',
              padding: '12px',
              border: '1px solid #333',
              borderRadius: '8px',
              backgroundColor: '#0f3460',
              color: '#eee',
              fontSize: '16px',
              outline: 'none',
              boxSizing: 'border-box',
              marginBottom: '16px',
            }}
          />
          <button
            type="submit"
            disabled={!userName.trim()}
            style={{
              width: '100%',
              padding: '12px',
              border: 'none',
              borderRadius: '8px',
              backgroundColor: userName.trim() ? '#e94560' : '#444',
              color: '#fff',
              fontSize: '16px',
              cursor: userName.trim() ? 'pointer' : 'not-allowed',
              fontWeight: 'bold',
            }}
          >
            Join
          </button>
        </form>
      </div>
    </div>
  );
};

export default Login;