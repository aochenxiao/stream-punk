export default function UserList({ users }) {
  if (users.length === 0) return null

  return (
    <div className="user-list">
      <div className="user-list-header">在线用户 ({users.length})</div>
      <div className="user-list-items">
        {users.map((user, i) => (
          <div key={i} className="user-item">
            <span
              className="user-color-dot"
              style={{ backgroundColor: '#' + (user.color >>> 0).toString(16).padStart(6, '0').toUpperCase() }}
            />
            <span className="user-name">{user.name}</span>
          </div>
        ))}
      </div>
    </div>
  )
}