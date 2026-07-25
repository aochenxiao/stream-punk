<template>
  <section class="data-section">
    <h2>数据查看器</h2>
    <div class="connection-status">
      <p>Connection Status: {{ connectionStatus }}</p>
      <button @click="$emit('toggle-connection')">{{ isConnected ? '断开连接' : '连接' }}</button>
      <button @click="$emit('send-data')">点击发送</button>
    </div>
    <div class="data-content">
      <section class="hex-display" v-if="hexData">
        <h3>Hex Data</h3>
        <pre>{{ hexData }}</pre>
      </section>
      <section class="data-display" v-if="parsedData">
        <h3>Parsed Data</h3>
        <pre>{{ safeStringify(parsedData) }}</pre>
      </section>
    </div>
  </section>
</template>

<script setup lang="ts">
interface Props {
  connectionStatus: string;
  isConnected: boolean;
  hexData: string;
  parsedData: any[];
}

interface Emits {
  (e: 'toggle-connection'): void;
  (e: 'send-data'): void;
}

defineProps<Props>();
defineEmits<Emits>();

// Safely stringify data containing BigInt values
function safeStringify(value: any): string {
  try {
    return JSON.stringify(value, (_key, v) => (typeof v === 'bigint' ? `${v}n` : v), 2);
  } catch (_) {
    // Fallback with circular reference handling
    const seen = new WeakSet();
    try {
      return JSON.stringify(
        value,
        (_key, v) => {
          if (typeof v === 'bigint') return `${v}n`;
          if (v && typeof v === 'object') {
            if (seen.has(v as object)) return '[Circular]';
            seen.add(v as object);
          }
          return v;
        },
        2
      );
    } catch {
      return String(value);
    }
  }
}
</script>

<style scoped>
.data-section {
  background-color: #161b22;
  padding: 1.5rem;
  border-radius: 8px;
  border: 1px solid #30363d;
}

.data-section h2 {
  margin-top: 0;
  color: #c9d1d9;
  margin-bottom: 1rem;
  padding-bottom: 0.5rem;
  border-bottom: 1px solid #30363d;
}

.connection-status {
  display: flex;
  align-items: center;
  gap: 1rem;
  margin-bottom: 1rem;
  flex-wrap: wrap;
}

.data-content {
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.hex-display,
.data-display {
  background-color: #0d1117;
  padding: 1rem;
  border-radius: 6px;
  border: 1px solid #30363d;
}

.hex-display h3,
.data-display h3 {
  margin-top: 0;
  color: #c9d1d9;
  font-size: 1rem;
  margin-bottom: 0.5rem;
}

.hex-display pre,
.data-display pre {
  margin: 0;
  font-size: 0.75rem;
  line-height: 1.4;
  color: #c9d1d9;
  white-space: pre-wrap;
  overflow-x: auto;
}

button {
  padding: 0.5rem 1rem;
  background-color: #238636;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.2s;
}

button:hover {
  background-color: #2ea043;
}

button:disabled {
  background-color: #6e7681;
  cursor: not-allowed;
}

/* 响应式设计 */
@media (max-width: 768px) {
  .data-section {
    padding: 1rem;
  }

  .connection-status {
    flex-direction: column;
    align-items: flex-start;
  }
}
</style>