# 浮动鼠标箭头组件说明

## 概述

这是一个美观、可自定义的浮动鼠标箭头组件，可以替代或增强默认的鼠标光标效果。组件具有平滑跟随、方向旋转、自定义样式和代码控制等功能。

## 功能特性

✨ **平滑跟随**: 鼠标移动时箭头具有平滑的跟随效果  
🎯 **方向旋转**: 箭头会根据移动方向自动旋转  
🌈 **自定义样式**: 支持自定义颜色、大小等属性  
🔧 **代码控制**: 可通过API程序化控制位置和属性  
💫 **悬停效果**: 支持交互式悬停效果  
🚀 **高性能**: 使用requestAnimationFrame优化性能  

## 组件位置

- **主组件**: `src/components/FloatingCursor.vue`
- **演示组件**: `src/components/CursorDemo.vue`

## 基本用法

### 1. 基础导入和使用

```vue
<template>
  <div>
    <!-- 浮动鼠标箭头组件 -->
    <FloatingCursor
      ref="cursorRef"
      :enabled="true"
      color="#00d4ff"
      :size="20"
      @position-change="onPositionChange"
    />
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import FloatingCursor from './components/FloatingCursor.vue'

const cursorRef = ref()

const onPositionChange = (position: { x: number; y: number }) => {
  console.log('鼠标位置:', position.x, position.y)
}
</script>
```

### 2. 组件属性 (Props)

| 属性名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `enabled` | `boolean` | `true` | 是否启用浮动鼠标 |
| `color` | `string` | `'#00d4ff'` | 箭头颜色 |
| `size` | `number` | `20` | 箭头大小(px) |
| `rotation` | `number` | `0` | 初始旋转角度(度) |

### 3. 事件 (Events)

| 事件名 | 参数 | 说明 |
|--------|------|------|
| `position-change` | `{x: number, y: number}` | 鼠标位置变化时触发 |

### 4. 方法 (Methods)

通过 `ref` 可以调用以下方法：

```typescript
const cursorRef = ref()

// 设置位置
cursorRef.value?.setPosition(x: number, y: number)

// 设置旋转角度
cursorRef.value?.setRotation(angle: number)

// 显示/隐藏
cursorRef.value?.show()
cursorRef.value?.hide()

// 设置悬停状态
cursorRef.value?.setHover(hover: boolean)

// 获取当前位置
const position = cursorRef.value?.getPosition()
```

## 高级用法

### 1. 程序化控制示例

```vue
<script setup lang="ts">
import { ref } from 'vue'
import FloatingCursor from './components/FloatingCursor.vue'

const cursorRef = ref()
const isAnimating = ref(false)

// 移动到指定位置
const moveToElement = (element: HTMLElement) => {
  const rect = element.getBoundingClientRect()
  const centerX = rect.left + rect.width / 2
  const centerY = rect.top + rect.height / 2
  cursorRef.value?.setPosition(centerX, centerY)
}

// 圆形动画
const startCircularAnimation = () => {
  if (isAnimating.value) return
  
  isAnimating.value = true
  let angle = 0
  
  const animate = () => {
    if (!isAnimating.value) return
    
    const centerX = window.innerWidth / 2
    const centerY = window.innerHeight / 2
    const radius = 100
    
    const x = centerX + Math.cos(angle) * radius
    const y = centerY + Math.sin(angle) * radius
    
    cursorRef.value?.setPosition(x, y)
    cursorRef.value?.setRotation(angle * 180 / Math.PI)
    
    angle += 0.1
    requestAnimationFrame(animate)
  }
  
  animate()
}

// 停止动画
const stopAnimation = () => {
  isAnimating.value = false
}
</script>
```

### 2. 交互式效果示例

```vue
<template>
  <div>
    <FloatingCursor ref="cursorRef" />
    
    <!-- 悬停区域 -->
    <div 
      class="hover-area"
      @mouseenter="cursorRef.value?.setHover(true)"
      @mouseleave="cursorRef.value?.setHover(false)"
    >
      悬停这里查看效果
    </div>
    
    <!-- 点击区域 -->
    <div 
      class="click-area"
      @click="clickEffect"
    >
      点击查看特效
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import FloatingCursor from './components/FloatingCursor.vue'

const cursorRef = ref()

const clickEffect = () => {
  if (cursorRef.value) {
    // 临时改变颜色和大小
    const originalColor = '#00d4ff'
    const originalSize = 20
    
    // 点击动画效果
    cursorRef.value.setHover(true)
    
    setTimeout(() => {
      cursorRef.value?.setHover(false)
    }, 500)
  }
}
</script>
```

### 3. 多样式配置

```vue
<template>
  <!-- 蓝色科技风格 -->
  <FloatingCursor 
    color="#00d4ff" 
    :size="20"
    @position-change="handlePosition"
  />
  
  <!-- 红色警告风格 -->
  <FloatingCursor 
    color="#ff6b6b" 
    :size="25"
    @position-change="handlePosition"
  />
  
  <!-- 绿色成功风格 -->
  <FloatingCursor 
    color="#51cf66" 
    :size="18"
    @position-change="handlePosition"
  />
</template>
```

## 样式定制

### CSS 变量

组件使用Vue的响应式绑定，可以通过以下方式定制样式：

```css
/* 在父组件中覆盖样式 */
.floating-cursor {
  /* 自定义滤镜效果 */
  filter: drop-shadow(0 0 10px #00d4ff) brightness(1.2);
}

/* 自定义动画 */
.cursor-ring {
  animation: customPulse 1.5s infinite;
}

@keyframes customPulse {
  0%, 100% { transform: scale(1); opacity: 0.3; }
  50% { transform: scale(1.2); opacity: 0.1; }
}
```

## 性能优化

1. **使用will-change**: 组件使用了`will-change: transform`来优化性能
2. **requestAnimationFrame**: 使用动画帧确保流畅的动画效果
3. **插值优化**: 使用平滑插值算法减少计算开销
4. **事件优化**: 使用passive事件监听器

## 浏览器兼容性

- ✅ Chrome 60+
- ✅ Firefox 55+
- ✅ Safari 12+
- ✅ Edge 79+

## 注意事项

1. **层级管理**: 组件默认使用`z-index: 9999`，确保在其他元素之上
2. **事件冲突**: 如果有其他全局鼠标事件监听器，注意事件冲突
3. **性能影响**: 在大量DOM元素的环境中，可能会有轻微性能影响
4. **移动端**: 组件主要针对桌面端设计，移动端可能需要额外适配

## 演示页面

启动应用后，点击导航栏中的"浮动鼠标演示"标签页可以查看完整的功能演示。

## 更新日志

### v1.0.0
- ✨ 初始版本发布
- ✨ 支持平滑跟随效果
- ✨ 支持方向旋转
- ✨ 支持自定义样式
- ✨ 支持程序化控制