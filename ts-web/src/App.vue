<template>
  <div class="app-container">
    <AppHeader />

    <main class="app-main">
      <FloatingCursor
        ref="cursorRef"
        :color="cursorColor"
        :size="cursorSize"
        :follow="cursorFollow"
        :mouse="dataStore.mouse"
      />

      <div class="content-grid">
        <CursorControlSection
          :mouse-x="dataStore.mouse.x"
          :mouse-y="dataStore.mouse.y"
          :cursor-color="cursorColor"
          :cursor-size="cursorSize"
          :is-animating="isAnimating"
          :cursor-follow="cursorFollow"
          @update:cursor-color="cursorColor = $event"
          @update:cursor-size="cursorSize = $event"
          @move-to-center="moveToCenter"
          @move-to-random="moveToRandom"
          @toggle-animation="isAnimating ? stopAnimation() : startAnimation()"
          @toggle-follow="cursorFollow = !cursorFollow"
        />

        <DataViewerSection
          :connection-status="connectionStatus"
          :is-connected="isConnected"
          :hex-data="hexData"
          :parsed-data="parsedData"
          @toggle-connection="toggleConnection"
          @send-data="sendData"
        />
      </div>
    </main>

    <AppFooter />
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch } from 'vue';
import { Base, I, O, E_StreamPunkType } from './stream-punk-data';
import FloatingCursor from './components/FloatingCursor.vue';
import AppHeader from './components/AppHeader.vue';
import AppFooter from './components/AppFooter.vue';
import CursorControlSection from './components/CursorControlSection.vue';
import DataViewerSection from './components/DataViewerSection.vue';
import { useDataStore } from './stores/data';

const connectionStatus = ref('Disconnected');
const isConnected = ref(false);
const parsedData = ref<any[]>([]);
const hexData = ref<string>('');
const cursorRef = ref();
const cursorFollow = ref(false);
const cursorColor = ref('#00d4ff');
const cursorSize = ref(20);
const isAnimating = ref(false);
// 数据驱动模式，位置信息从 store 获取
let animationInterval: NodeJS.Timeout | null = null;
let ws: WebSocket | null = null;

const dataStore = useDataStore();

// 鼠标跟随功能
let mouseMoveHandler: ((event: MouseEvent) => void) | null = null;

const startMouseFollowing = () => {
  mouseMoveHandler = (event: MouseEvent) => {
    dataStore.mouse.x = event.clientX;
    dataStore.mouse.y = event.clientY;
  };
  document.addEventListener('mousemove', mouseMoveHandler);
};

const stopMouseFollowing = () => {
  if (mouseMoveHandler) {
    document.removeEventListener('mousemove', mouseMoveHandler);
    mouseMoveHandler = null;
  }
};

// 监听跟随模式变化
watch(cursorFollow, (newValue) => {
  if (newValue) {
    startMouseFollowing();
  } else {
    stopMouseFollowing();
  }
});

// 组件挂载时的初始化
onMounted(() => {
  if (cursorFollow.value) {
    startMouseFollowing();
  }
});

// 组件卸载时清理
onUnmounted(() => {
  stopMouseFollowing();
});

// 数据驱动模式，无需监听位置变化

// 浮动鼠标控制方法

const startAnimation = () => {
  if (isAnimating.value) return;

  // 如果正在跟随鼠标，先停止跟随
  if (cursorFollow.value) {
    cursorFollow.value = false;
  }

  isAnimating.value = true;
  let angle = 0;

  animationInterval = setInterval(() => {
    if (isAnimating.value) {
      const centerX = window.innerWidth / 2;
      const centerY = window.innerHeight / 2;
      const radius = 150;
      const x = centerX + Math.cos(angle) * radius;
      const y = centerY + Math.sin(angle) * radius;

      dataStore.mouse.x = x;
      dataStore.mouse.y = y;
      if (cursorRef.value) {
        cursorRef.value.setRotation(angle * 180 / Math.PI);
      }

      angle += 0.05;
    }
  }, 50);
};

const stopAnimation = () => {
  isAnimating.value = false;
  if (animationInterval) {
    clearInterval(animationInterval);
    animationInterval = null;
  }
};

// 修改 moveToCenter 和 moveToRandom 函数，确保它们不与跟随模式冲突
const moveToCenter = () => {
  // 如果正在跟随鼠标，先停止跟随
  if (cursorFollow.value) {
    cursorFollow.value = false;
  }
  const centerX = window.innerWidth / 2;
  const centerY = window.innerHeight / 2;
  dataStore.mouse.x = centerX;
  dataStore.mouse.y = centerY;
};

const moveToRandom = () => {
  // 如果正在跟随鼠标，先停止跟随
  if (cursorFollow.value) {
    cursorFollow.value = false;
  }
  const randomX = Math.random() * window.innerWidth;
  const randomY = Math.random() * window.innerHeight;
  dataStore.mouse.x = randomX;
  dataStore.mouse.y = randomY;
};

function arrayBufferToHex(buffer: ArrayBuffer): string {
  return Array.from(new Uint8Array(buffer))
    .map(b => b.toString(16).padStart(2, '0'))
    .join(' ');
}

const objects: SpRef<Base>[] = [];

function printArrayBufferHex(buffer: ArrayBuffer, label: string = ''): string {
  const hex = arrayBufferToHex(buffer);
  console.log(label ? `${label} ${hex}` : hex);
  return hex;
}

function printObj(obj: any) {
  console.log(obj)
  const o = new O();
  obj.to(o)
  const data = o.to_array_buffer();
  printArrayBufferHex(data, "");
}

function testO(){
  console.log('parsedData:', parsedData.value)
  printObj(parsedData.value[0].value)
  const data2 = parsedData.value[1].value
  console.log(data2)
  const o = new O();
  data2.to(o)
  const data2n = o.to_array_buffer();
  printArrayBufferHex(data2n, "");
}

function sendData() {
  if (!ws || !isConnected.value) return;
  testO();
}

function toggleConnection() {
  if (isConnected.value) {
    ws?.close();
    return;
  }
  connectWebSocket();
}
function connectWebSocket() {
  ws = new WebSocket('ws://localhost:12345');
  ws.binaryType = 'arraybuffer';

  ws.onopen = () => {
    connectionStatus.value = 'Connected';
    isConnected.value = true;
    ws?.send(new Uint8Array([E_StreamPunkType.e_unknowType]).buffer);
  };

  ws.onmessage = (event) => {
    const data = event.data as ArrayBuffer;
    hexData.value = printArrayBufferHex(data, 'Received hex data:');
    const i = new I(data);
    while (i.hasMoreData()) {
      const obj = i.read_ptr_with_typeID<Base>();
      if (obj) {
        const typeID = (obj.value.constructor as any).typeID;
        if(typeID == E_StreamPunkType.MousePosition){
          console.log('mousePosition:', obj.value)
          const mousePosition = obj.value as MousePosition;
          dataStore.mouse.x = mousePosition.x;
          dataStore.mouse.y = mousePosition.y;
          continue;
        }else{
          objects.push(obj);
        }
      }
      console.log(obj);
    }
    parsedData.value = objects;
  };

  ws.onclose = () => {
    connectionStatus.value = 'Disconnected';
    isConnected.value = false;
  };

  ws.onerror = (error) => {
    connectionStatus.value = 'Error';
    console.error('WebSocket Error:', error);
  };
}
</script>

<style scoped>
@import '@/assets/app.css';
</style>
