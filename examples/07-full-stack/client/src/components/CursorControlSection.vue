<template>
  <section class="cursor-section">
    <h2>浮动鼠标控制 <span class="position-info">X: {{ mouseX.toFixed(0) }}, Y: {{ mouseY.toFixed(0) }}</span></h2>
    <div class="cursor-controls-compact">
      <div class="controls-row">
        <div class="button-group-horizontal">
          <button @click="$emit('move-to-center')">中心</button>
          <button @click="$emit('move-to-random')">随机</button>
          <button
            @click="$emit('toggle-animation')"
            :class="{ 'animation-active': isAnimating }"
          >
            {{ isAnimating ? '停止' : '动画' }}
          </button>
          <button
            @click="$emit('toggle-follow')"
            :class="{ 'follow-active': cursorFollow }"
          >
            {{ cursorFollow ? '跟随开' : '跟随关' }}
          </button>
        </div>
        <div class="appearance-controls">
          <div class="color-control">
            <label>颜色:</label>
            <select :value="cursorColor" @change="$emit('update:cursor-color', $event.target.value)">
              <option value="#00d4ff">蓝色</option>
              <option value="#ff6b6b">红色</option>
              <option value="#51cf66">绿色</option>
              <option value="#ffd43b">黄色</option>
              <option value="#be4bdb">紫色</option>
              <option value="#ff8cc8">粉色</option>
            </select>
          </div>
          <div class="size-control">
            <label>大小:</label>
            <input 
              type="range" 
              :value="cursorSize" 
              @input="$emit('update:cursor-size', parseInt($event.target.value))"
              min="10" 
              max="40" 
              step="2"
            >
            <span>{{ cursorSize }}px</span>
          </div>
        </div>
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
interface Props {
  mouseX: number;
  mouseY: number;
  cursorColor: string;
  cursorSize: number;
  isAnimating: boolean;
  cursorFollow: boolean;
}

interface Emits {
  (e: 'move-to-center'): void;
  (e: 'move-to-random'): void;
  (e: 'toggle-animation'): void;
  (e: 'toggle-follow'): void;
  (e: 'update:cursor-color', value: string): void;
  (e: 'update:cursor-size', value: number): void;
}

defineProps<Props>();
defineEmits<Emits>();
</script>

<style scoped>
.cursor-section {
  background-color: #161b22;
  padding: 1.5rem;
  border-radius: 8px;
  border: 1px solid #30363d;
}

.cursor-section h2 {
  margin-top: 0;
  color: #c9d1d9;
  margin-bottom: 1rem;
  padding-bottom: 0.5rem;
  border-bottom: 1px solid #30363d;
}

.cursor-controls-compact {
  display: flex;
  flex-direction: column;
  justify-content: flex-start;
  gap: 0.75rem;
}

.controls-row {
  display: flex;
  justify-content: flex-start;
  gap: 1rem;
  flex-wrap: wrap;
}

.button-group-horizontal {
  display: flex;
  gap: 0.25rem;
  flex-wrap: nowrap;
  justify-content: flex-start;
}

.button-group-horizontal button {
  padding: 0.25rem 0.5rem;
  background-color: #238636;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 0.75rem;
  transition: background-color 0.2s;
  white-space: nowrap;
  min-width: 50px;
}

.button-group-horizontal button:hover {
  background-color: #2ea043;
}

.button-group-horizontal button:nth-child(2) {
  background-color: #da3633;
}

.button-group-horizontal button:nth-child(2):hover {
  background-color: #f85149;
}

.button-group-horizontal button:nth-child(3) {
  background-color: #fb8c00;
}

.button-group-horizontal button:nth-child(3):hover {
  background-color: #ff9f43;
}

.button-group-horizontal button.animation-active {
  background-color: #6f42c1;
}

.button-group-horizontal button.animation-active:hover {
  background-color: #8e44ad;
}

.button-group-horizontal button.follow-active {
  background-color: #17a2b8;
}

.button-group-horizontal button.follow-active:hover {
  background-color: #138496;
}

.button-group-horizontal button:disabled {
  background-color: #6e7681;
  cursor: not-allowed;
}

.appearance-controls {
  display: flex;
  align-items: center;
  gap: 1rem;
}

.color-control {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.color-control label {
  color: #8b949e;
  font-size: 0.875rem;
}

.color-control select {
  padding: 0.25rem;
  background-color: #0d1117;
  color: #c9d1d9;
  border: 1px solid #30363d;
  border-radius: 4px;
  font-size: 0.875rem;
}

.size-control {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.size-control label {
  color: #8b949e;
  font-size: 0.875rem;
}

.size-control input[type="range"] {
  width: 80px;
}

.size-control span {
  color: #8b949e;
  font-size: 0.875rem;
  min-width: 40px;
}

.position-info {
  font-family: 'Courier New', monospace;
  background-color: #0d1117;
  padding: 0.25rem 0.5rem;
  border-radius: 4px;
  color: #00d4ff;
  font-size: 0.75rem;
}

/* 响应式设计 */
@media (max-width: 768px) {
  .controls-row {
    flex-direction: column;
    align-items: stretch;
    gap: 0.5rem;
  }

  .button-group-horizontal {
    justify-content: center;
    flex-wrap: wrap;
  }

  .appearance-controls {
    justify-content: flex-start;
    flex-wrap: wrap;
  }

  .color-control,
  .size-control {
    justify-content: space-between;
  }
}
</style>