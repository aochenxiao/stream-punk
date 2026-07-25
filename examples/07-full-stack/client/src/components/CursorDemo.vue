<template>
  <div class="cursor-demo">
    <h2>浮动鼠标箭头演示</h2>
    
    <!-- 浮动鼠标箭头组件 -->
    <FloatingCursor
      ref="cursorRef"
      :enabled="cursorEnabled"
      :color="cursorColor"
      :size="cursorSize"
      @position-change="onPositionChange"
    />
    
    <!-- 控制面板 -->
    <div class="controls-panel">
      <h3>控制面板</h3>
      
      <div class="control-group">
        <label>
          <input type="checkbox" v-model="cursorEnabled">
          启用浮动鼠标
        </label>
      </div>
      
      <div class="control-group">
        <label>颜色:</label>
        <select v-model="cursorColor">
          <option value="#00d4ff">蓝色</option>
          <option value="#ff6b6b">红色</option>
          <option value="#51cf66">绿色</option>
          <option value="#ffd43b">黄色</option>
          <option value="#be4bdb">紫色</option>
          <option value="#ff8cc8">粉色</option>
        </select>
      </div>
      
      <div class="control-group">
        <label>大小:</label>
        <input type="range" v-model.number="cursorSize" min="10" max="40" step="2">
        <span>{{ cursorSize }}px</span>
      </div>
      
      <div class="control-group">
        <label>程序化控制:</label>
        <div class="button-group">
          <button @click="moveToCenter">移动到中心</button>
          <button @click="moveToRandom">随机移动</button>
          <button @click="startAnimation">开始动画</button>
          <button @click="stopAnimation">停止动画</button>
        </div>
      </div>
      
      <div class="control-group">
        <label>当前坐标:</label>
        <span class="position-info">X: {{ currentPosition.x.toFixed(0) }}, Y: {{ currentPosition.y.toFixed(0) }}</span>
      </div>
      
      <div class="control-group">
        <label>交互测试区域:</label>
        <div 
          class="hover-area"
          @mouseenter="onAreaHover(true)"
          @mouseleave="onAreaHover(false)"
        >
          将鼠标移到这里测试悬停效果
        </div>
      </div>
    </div>
    
    <!-- 演示区域 -->
    <div class="demo-area">
      <h3>演示区域</h3>
      <p>移动鼠标查看浮动箭头效果</p>
      <p>箭头会根据鼠标移动方向自动旋转</p>
      <p>支持代码控制位置和属性</p>
      
      <div class="feature-list">
        <div class="feature-item" @mouseenter="highlightFeature(1)" @mouseleave="highlightFeature(0)">
          <i class="pi pi-star"></i>
          <span>平滑跟随</span>
        </div>
        <div class="feature-item" @mouseenter="highlightFeature(2)" @mouseleave="highlightFeature(0)">
          <i class="pi pi-sync"></i>
          <span>方向旋转</span>
        </div>
        <div class="feature-item" @mouseenter="highlightFeature(3)" @mouseleave="highlightFeature(0)">
          <i class="pi pi-palette"></i>
          <span>自定义样式</span>
        </div>
        <div class="feature-item" @mouseenter="highlightFeature(4)" @mouseleave="highlightFeature(0)">
          <i class="pi pi-code"></i>
          <span>代码控制</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive } from 'vue'
import FloatingCursor from './FloatingCursor.vue'

const cursorRef = ref()
const cursorEnabled = ref(true)
const cursorColor = ref('#00d4ff')
const cursorSize = ref(20)
const isAnimating = ref(false)
let animationInterval: NodeJS.Timeout | null = null

const currentPosition = reactive({ x: 0, y: 0 })

// 位置变化处理
const onPositionChange = (position: { x: number; y: number }) => {
  currentPosition.x = position.x
  currentPosition.y = position.y
}

// 移动到中心
const moveToCenter = () => {
  if (cursorRef.value) {
    const centerX = window.innerWidth / 2
    const centerY = window.innerHeight / 2
    cursorRef.value.setPosition(centerX, centerY)
  }
}

// 随机移动
const moveToRandom = () => {
  if (cursorRef.value) {
    const randomX = Math.random() * window.innerWidth
    const randomY = Math.random() * window.innerHeight
    cursorRef.value.setPosition(randomX, randomY)
  }
}

// 开始动画
const startAnimation = () => {
  if (isAnimating.value) return
  
  isAnimating.value = true
  let angle = 0
  
  animationInterval = setInterval(() => {
    if (cursorRef.value && isAnimating.value) {
      const centerX = window.innerWidth / 2
      const centerY = window.innerHeight / 2
      const radius = 150
      const x = centerX + Math.cos(angle) * radius
      const y = centerY + Math.sin(angle) * radius
      
      cursorRef.value.setPosition(x, y)
      cursorRef.value.setRotation(angle * 180 / Math.PI)
      
      angle += 0.05
    }
  }, 50)
}

// 停止动画
const stopAnimation = () => {
  isAnimating.value = false
  if (animationInterval) {
    clearInterval(animationInterval)
    animationInterval = null
  }
}

// 悬停效果
const onAreaHover = (hover: boolean) => {
  if (cursorRef.value) {
    cursorRef.value.setHover(hover)
  }
}

// 高亮功能
const highlightFeature = (level: number) => {
  if (cursorRef.value) {
    if (level > 0) {
      cursorRef.value.setHover(true)
      cursorSize.value = 25
    } else {
      cursorRef.value.setHover(false)
      cursorSize.value = 20
    }
  }
}
</script>

<style scoped>
.cursor-demo {
  padding: 2rem;
  max-width: 1200px;
  margin: 0 auto;
}

.controls-panel {
  background-color: #161b22;
  padding: 1.5rem;
  border-radius: 8px;
  margin-bottom: 2rem;
  border: 1px solid #30363d;
}

.controls-panel h3 {
  margin-top: 0;
  color: #c9d1d9;
}

.control-group {
  margin-bottom: 1rem;
  display: flex;
  align-items: center;
  gap: 1rem;
}

.control-group label {
  min-width: 100px;
  color: #8b949e;
}

.control-group input[type="checkbox"] {
  margin-right: 0.5rem;
}

.control-group select,
.control-group input[type="range"] {
  padding: 0.25rem;
  background-color: #0d1117;
  color: #c9d1d9;
  border: 1px solid #30363d;
  border-radius: 4px;
}

.control-group input[type="range"] {
  width: 120px;
}

.button-group {
  display: flex;
  gap: 0.5rem;
  flex-wrap: wrap;
}

.button-group button {
  padding: 0.5rem 1rem;
  background-color: #238636;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 0.875rem;
  transition: background-color 0.2s;
}

.button-group button:hover {
  background-color: #2ea043;
}

.button-group button:nth-child(2) {
  background-color: #da3633;
}

.button-group button:nth-child(2):hover {
  background-color: #f85149;
}

.button-group button:nth-child(3) {
  background-color: #fb8c00;
}

.button-group button:nth-child(3):hover {
  background-color: #ff9f43;
}

.button-group button:nth-child(4) {
  background-color: #6f42c1;
}

.button-group button:nth-child(4):hover {
  background-color: #8e44ad;
}

.position-info {
  font-family: 'Courier New', monospace;
  background-color: #0d1117;
  padding: 0.25rem 0.5rem;
  border-radius: 4px;
  color: #00d4ff;
}

.hover-area {
  padding: 1rem;
  background-color: #0d1117;
  border: 2px dashed #30363d;
  border-radius: 8px;
  text-align: center;
  color: #8b949e;
  cursor: pointer;
  transition: all 0.3s ease;
}

.hover-area:hover {
  border-color: #00d4ff;
  background-color: #161b22;
  color: #00d4ff;
}

.demo-area {
  background-color: #161b22;
  padding: 2rem;
  border-radius: 8px;
  border: 1px solid #30363d;
}

.demo-area h3 {
  margin-top: 0;
  color: #c9d1d9;
}

.feature-list {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
  gap: 1rem;
  margin-top: 2rem;
}

.feature-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 1rem;
  background-color: #0d1117;
  border-radius: 8px;
  border: 1px solid #30363d;
  transition: all 0.3s ease;
  cursor: pointer;
}

.feature-item i {
  font-size: 2rem;
  margin-bottom: 0.5rem;
  color: #8b949e;
}

.feature-item span {
  font-size: 0.875rem;
  color: #8b949e;
  text-align: center;
}

.feature-item:hover {
  border-color: #00d4ff;
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(0, 212, 255, 0.2);
}

.feature-item:hover i,
.feature-item:hover span {
  color: #00d4ff;
}
</style>