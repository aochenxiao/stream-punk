import React, { useState, useRef, useEffect, useCallback } from 'react';
import type { CursorInfo } from '../stream-punk-data';

interface EditorProps {
  document: string;
  users: CursorInfo[];
  myUserId: number;
  onTextOp: (opType: number, position: number, text: string) => void;
  onCursorMove: (position: number) => void;
}

// Generate a color from a string
function getCursorColor(userId: number, color: string): string {
  return color || '#e94560';
}

const Editor: React.FC<EditorProps> = ({
  document,
  users,
  myUserId,
  onTextOp,
  onCursorMove,
}) => {
  const [localText, setLocalText] = useState(document);
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const prevDocRef = useRef(document);
  const isRemoteRef = useRef(false);

  // Sync from server
  useEffect(() => {
    if (document !== prevDocRef.current) {
      isRemoteRef.current = true;
      setLocalText(document);
      prevDocRef.current = document;
    }
  }, [document]);

  const handleChange = useCallback((e: React.ChangeEvent<HTMLTextAreaElement>) => {
    if (isRemoteRef.current) {
      isRemoteRef.current = false;
      return;
    }

    const newText = e.target.value;
    const oldText = localText;
    const cursorPos = e.target.selectionStart;

    if (newText.length > oldText.length) {
      // Insert
      const insertLen = newText.length - oldText.length;
      const insertPos = cursorPos - insertLen;
      const inserted = newText.slice(insertPos, cursorPos);
      onTextOp(0, insertPos, inserted);
    } else if (newText.length < oldText.length) {
      // Delete
      const deleteLen = oldText.length - newText.length;
      const deletePos = cursorPos;
      onTextOp(1, deletePos, String(deleteLen));
    }

    setLocalText(newText);
    prevDocRef.current = newText;
  }, [localText, onTextOp]);

  const handleSelect = useCallback(() => {
    const pos = textareaRef.current?.selectionStart ?? 0;
    onCursorMove(pos);
  }, [onCursorMove]);

  // Render remote cursors
  const otherUsers = users.filter((u) => u.userId !== myUserId && u.position >= 0);

  return (
    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', backgroundColor: '#1a1a2e' }}>
      <div style={{
        padding: '8px 16px',
        backgroundColor: '#16213e',
        borderBottom: '1px solid #333',
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        flexWrap: 'wrap',
      }}>
        {otherUsers.map((user) => (
          <div
            key={user.userId}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontSize: '12px',
              color: user.color,
            }}
          >
            <div style={{
              width: '8px',
              height: '8px',
              borderRadius: '50%',
              backgroundColor: user.color,
            }} />
            {user.userName}
          </div>
        ))}
        {otherUsers.length === 0 && (
          <span style={{ fontSize: '12px', color: '#666' }}>No other users</span>
        )}
      </div>
      <div style={{ flex: 1, position: 'relative', overflow: 'hidden' }}>
        <textarea
          ref={textareaRef}
          value={localText}
          onChange={handleChange}
          onSelect={handleSelect}
          onClick={handleSelect}
          onKeyUp={handleSelect}
          style={{
            width: '100%',
            height: '100%',
            backgroundColor: '#1a1a2e',
            color: '#e0e0e0',
            border: 'none',
            padding: '20px',
            fontSize: '15px',
            fontFamily: "'Cascadia Code', 'Fira Code', 'Consolas', monospace",
            lineHeight: '1.6',
            resize: 'none',
            outline: 'none',
            boxSizing: 'border-box',
            tabSize: 2,
          }}
          placeholder="Start typing..."
          spellCheck={false}
        />
      </div>
    </div>
  );
};

export default Editor;