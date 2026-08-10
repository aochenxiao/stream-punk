import React from 'react';
import type { CursorInfo } from '../stream-punk-data';

interface UserListProps {
  users: CursorInfo[];
  myUserId: number;
}

const UserList: React.FC<UserListProps> = ({ users, myUserId }) => {
  return (
    <div style={{
      width: '200px',
      backgroundColor: '#16213e',
      borderRight: '1px solid #333',
      padding: '16px',
      overflowY: 'auto',
      fontFamily: 'system-ui, sans-serif',
    }}>
      <h3 style={{
        margin: '0 0 12px 0',
        fontSize: '14px',
        color: '#888',
        textTransform: 'uppercase',
        letterSpacing: '1px',
      }}>
        Users ({users.length})
      </h3>
      <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
        {users.map((user) => (
          <div
            key={user.userId}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              padding: '8px',
              borderRadius: '6px',
              backgroundColor: user.userId === myUserId ? '#0f3460' : 'transparent',
              fontSize: '13px',
              color: '#ccc',
            }}
          >
            <div style={{
              width: '10px',
              height: '10px',
              borderRadius: '50%',
              backgroundColor: user.color,
              flexShrink: 0,
            }} />
            <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
              {user.userName}
              {user.userId === myUserId ? ' (you)' : ''}
            </span>
            <span style={{
              marginLeft: 'auto',
              fontSize: '11px',
              color: '#666',
              flexShrink: 0,
            }}>
              pos:{user.position}
            </span>
          </div>
        ))}
      </div>
    </div>
  );
};

export default UserList;