<template>
  <div
    ref="cursorRef"
    class="floating-cursor"
    :class="{ 'cursor-hover': isHovering }"
    :style="{
      left: x + 'px',
      top: y + 'px',
      transform: `translate(-50%, -50%) rotate(${rotation}deg)`
    }"
  >
    <div class="cursor-core"></div>
    <div class="cursor-ring"></div>
    <div class="cursor-glow"></div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted, nextTick, watch } from 'vue'

interface Position {
  x: number
  y: number
}

const props = withDefaults(defineProps<{
  enabled?: boolean
  follow?: boolean
  color?: string
  size?: number
  rotation?: number
  mouse?: Position
}>(), {
  enabled: true,
  follow: true,
  color: '#00d4ff',
  size: 20,
  rotation: 0,
  mouse: () => ({ x: 0, y: 0 })
})

// 数据驱动模式，无需事件发射

const cursorRef = ref<HTMLElement>()
const x = ref(0)
const y = ref(0)
const rotation = ref(props.rotation)
const isHovering = ref(false)
let animationFrame: number | null = null

// 目标位置和当前位置的插值变量
let targetX = 0
let targetY = 0
let currentX = 0
let currentY = 0

// 平滑插值函数
const smoothLerp = (current: number, target: number, factor: number = 0.15) => {
  return current + (target - current) * factor
}

// 鼠标移动处理函数
const handleMouseMove = (event: MouseEvent) => {
  if (!props.enabled || !props.follow) return

  targetX = event.clientX
  targetY = event.clientY
}

// 动画循环
const animate = () => {
  if (!props.enabled) return

  currentX = smoothLerp(currentX, targetX)
  currentY = smoothLerp(currentY, targetY)

  x.value = currentX
  y.value = currentY

  // 计算旋转角度
  const deltaX = targetX - currentX
  const deltaY = targetY - currentY
  rotation.value = Math.atan2(deltaY, deltaX) * (180 / Math.PI)

  animationFrame = requestAnimationFrame(animate)
}

// 监听 props.mouse 变化，直接更新目标位置
watch(() => props.mouse, (newMouse) => {
  targetX = newMouse.x
  targetY = newMouse.y
}, { deep: true })

// 设置旋转角度
const setRotation = (angle: number) => {
  rotation.value = angle
}

// 显示/隐藏光标
const show = () => {
  if (cursorRef.value) {
    cursorRef.value.style.opacity = '1'
  }
}

const hide = () => {
  if (cursorRef.value) {
    cursorRef.value.style.opacity = '0'
  }
}

// 悬停效果
const setHover = (hover: boolean) => {
  isHovering.value = hover
}

onMounted(() => {
  if (!props.enabled) return

  // 初始化位置到屏幕中心
  const centerX = window.innerWidth / 2
  const centerY = window.innerHeight / 2
  targetX = currentX = centerX
  targetY = currentY = centerY
  x.value = centerX
  y.value = centerY

  // 添加鼠标移动监听
  document.addEventListener('mousemove', handleMouseMove, { passive: true })

  // 开始动画循环
  animate()
})

onUnmounted(() => {
  document.removeEventListener('mousemove', handleMouseMove)

  if (animationFrame) {
    cancelAnimationFrame(animationFrame)
  }
})

// 简化暴露，仅保留必要方法
defineExpose({
  setRotation,
  show,
  hide,
  setHover
})
</script>

<style scoped>
.floating-cursor {
  position: fixed;
  left: 0;
  top: 0;
  width: v-bind(size + 'px');
  height: v-bind(size + 'px');
  pointer-events: none;
  z-index: 9999;
  opacity: 1;
  transition: opacity 0.3s ease;
  will-change: transform;
}

.cursor-core {
  position: absolute;
  width: 100%;
  height: 100%;
  background: linear-gradient(45deg, v-bind(color), #ffffff);
  clip-path: polygon(50% 0%, 0% 100%, 100% 100%);
  filter: drop-shadow(0 0 8px v-bind(color));
  transform-origin: 50% 50%;
}

.cursor-ring {
  position: absolute;
  width: 120%;
  height: 120%;
  left: -10%;
  top: -10%;
  border: 2px solid v-bind(color);
  border-radius: 50%;
  opacity: 0.3;
  animation: pulse 2s infinite;
}

.cursor-glow {
  position: absolute;
  width: 150%;
  height: 150%;
  left: -25%;
  top: -25%;
  background: radial-gradient(circle, v-bind(color) 0%, transparent 70%);
  opacity: 0.1;
  border-radius: 50%;
  animation: glow 3s infinite alternate;
}

.cursor-hover .cursor-core {
  filter: drop-shadow(0 0 12px v-bind(color)) brightness(1.2);
  transform: scale(1.1);
}

.cursor-hover .cursor-ring {
  border-color: #ffffff;
  opacity: 0.5;
  animation-duration: 1s;
}

@keyframes pulse {
  0% {
    transform: scale(1);
    opacity: 0.3;
  }
  50% {
    transform: scale(1.1);
    opacity: 0.1;
  }
  100% {
    transform: scale(1);
    opacity: 0.3;
  }
}

@keyframes glow {
  0% {
    opacity: 0.1;
    transform: scale(1);
  }
  100% {
    opacity: 0.2;
    transform: scale(1.1);
  }
}

/* 鼠标悬停交互效果 */
.cursor-hover {
  filter: drop-shadow(0 0 20px v-bind(color));
}
</style>
