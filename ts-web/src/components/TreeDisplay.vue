<script setup lang="ts">
import { ref, computed } from 'vue';
import Tree from 'primevue/tree';

const props = defineProps<{
  item: {
    name: string;
    data: any;
    hex: string[][][];
    metadata?: { [field: string]: { start: number; end: number } };
  };
}>();

const highlightedBytes = ref<{ start: number; end: number } | null>(null);
const expandedKeys = ref({});
const selectionKeys = ref({});

// 递归构建树节点
type PrimeVueTreeNode = import('primevue/treenode').TreeNode;

interface TreeNode {
  key: string;
  label: string;
  data: {
    value: any;
    path: (string | number)[];
    metadata?: { start: number; end: number } | { [field: string]: { start: number; end: number } };
  };
  children?: TreeNode[];
  selectable?: boolean;
  expanded?: boolean;
}




function buildTreeNodes(data: any, metadata: any, path: (number | string)[] = []): TreeNode[] {
  if (!data) return [];

  if (typeof data === 'object' && !(data instanceof Date)) {
    const nodes: TreeNode[] = [];

    // 处理数组或Set
    if (Array.isArray(data) || data instanceof Set) {
      const items = Array.isArray(data) ? data : Array.from(data);
      items.forEach((item, index) => {
        const childPath = [...path, index];
        // 处理数组元素的元数据，支持新的元数据结构
        let meta = null;
        if (metadata?.elements && Array.isArray(metadata.elements) && index < metadata.elements.length) {
          meta = metadata.elements[index];
        } else if (metadata?.nested?.elements && Array.isArray(metadata.nested.elements) && index < metadata.nested.elements.length) {
          meta = metadata.nested.elements[index];
        }

        const node = {
    key: childPath.join('-'),
    label: `${index}`,
    data: { 
      value: item, 
      path: childPath, 
      metadata: meta
    },
    children: buildTreeNodes(item, meta, childPath),
    selectable: true
  };
        nodes.push(node);
      });
      return nodes as TreeNode[];
    }

    // 处理Map
    if (data instanceof Map) {
      const nodes: TreeNode[] = [];
      const entries = Array.from(data.entries());
      entries.forEach(([key, value], index) => {
        const keyPath = [...path, index, 'key'];
        const valuePath = [...path, index, 'value'];

        // 处理Map条目的元数据，支持新的元数据结构
        let entryMeta = null;
        if (metadata?.entries && Array.isArray(metadata.entries) && index < metadata.entries.length) {
          entryMeta = metadata.entries[index];
        } else if (metadata?.nested?.entries && Array.isArray(metadata.nested.entries) && index < metadata.nested.entries.length) {
          entryMeta = metadata.nested.entries[index];
        }

        const keyNode = {
          key: keyPath.join('-'),
          label: 'key',
          data: { value: key, path: keyPath, metadata: entryMeta?.key },
          selectable: true
        };

        const valueNode = {
          key: valuePath.join('-'),
          label: 'value',
          data: { value, path: valuePath, metadata: entryMeta?.value },
          children: buildTreeNodes(value, entryMeta?.value, valuePath),
          selectable: true
        };

        const node = {
          key: [...path, index].join('-'),
          label: `Entry ${index}`,
          data: { value: entryMeta, path: [...path, index], metadata: entryMeta },
          children: [keyNode, valueNode],
          selectable: false
        };

        nodes.push(node);
      });
      return nodes as TreeNode[];
    }

    // 处理普通对象
    for (const [key, value] of Object.entries(data)) {
      if (key === 'constructor') continue;

      const childPath = [...path, key];
      // 处理对象属性的元数据，支持新的元数据结构
      let childMeta = null;
      if (metadata?.[key]) {
        childMeta = metadata[key];
      } else if (metadata?.nested?.[key]) {
        childMeta = metadata.nested[key];
      }

      const node = {
        key: childPath.join('-'),
        label: key,
        data: { value, path: childPath, metadata: childMeta },
        children: buildTreeNodes(value, childMeta, childPath),
        selectable: true
      };
      nodes.push(node);
    }

    return nodes;
  }

  return [];
}

const treeNodes = computed(() => {
  const rootNode = {
    key: props.item.name,
    label: props.item.name,
    data: { value: props.item.data, path: [], metadata: props.item.metadata },
    children: buildTreeNodes(props.item.data, props.item.metadata),
    selectable: true,
    expanded: true
  };

  return [rootNode];
});

// 初始化展开所有节点
function expandAll() {
  const keys: Record<string, boolean> = {};
  const expandNode = (nodes: any[]) => {
    if (!nodes) return;

    for (const node of nodes) {
      if (node.key) {
        keys[node.key] = true;
      }
      if (node.children) {
        expandNode(node.children);
      }
    }
  };

  expandNode(treeNodes.value);
  expandedKeys.value = keys;
}

function collapseAll() {
  expandedKeys.value = {};
}

function getRange(n: any): { minStart: number; maxEnd: number } | null {
  let minStart = Infinity;
  let maxEnd = -Infinity;
  let hasRange = false;

  // 检查节点自身的元数据
  if (n.data && n.data.metadata) {
    if ('start' in n.data.metadata && 'end' in n.data.metadata &&
        typeof n.data.metadata.start === 'number' && typeof n.data.metadata.end === 'number') {
      minStart = Math.min(minStart, n.data.metadata.start);
      maxEnd = Math.max(maxEnd, n.data.metadata.end);
      hasRange = true;
    }
  }

  // 递归检查子节点
  if (n.children) {
    n.children.forEach((child: any) => {
      const childRange = getRange(child);
      if (childRange) {
        minStart = Math.min(minStart, childRange.minStart);
        maxEnd = Math.max(maxEnd, childRange.maxEnd);
        hasRange = true;
      }
    });
  }

  return hasRange ? { minStart, maxEnd } : null;
}

function highlightHex(node: any) {
  if (!node || !node.data) return;

  const range = getRange(node);
  console.log('Highlighting for node:', node.label, 'range:', range);
  highlightedBytes.value = range ? { start: range.minStart, end: range.maxEnd } : null;
}

const getNodeLabel = (node: import('primevue/treenode').TreeNode) => node.label?.toString() || '';
const getNodeRange = (node: import('primevue/treenode').TreeNode) => `${node.data?.metadata?.start ?? ''}-${node.data?.metadata?.end ?? ''}`;

// Extend PrimeVue's TreeNode type
declare module 'primevue/treenode' {
  interface TreeNode {
    data?: any & {
      value?: any;
      path?: (string | number)[];
      metadata?: { start: number; end: number } | { [field: string]: { start: number; end: number } };
    };
  }
}

function clearHighlight() {
  console.log('Clearing highlight');
  highlightedBytes.value = null;
}

function isByteHighlighted(rowIndex: number, chunkIndex: number, byteIndex: number): boolean {
  if (!highlightedBytes.value) return false;
  const byteOffset = rowIndex * 16 + chunkIndex * 4 + byteIndex;
  return byteOffset >= highlightedBytes.value.start && byteOffset < highlightedBytes.value.end;
}

function findNodeByOffset(node: TreeNode, offset: number): TreeNode | null {
  let candidate = null;

  // 检查节点自身的元数据
  if (node.data && node.data.metadata &&
      'start' in node.data.metadata && typeof node.data.metadata.start === 'number' &&
      'end' in node.data.metadata && typeof node.data.metadata.end === 'number') {
    if (offset >= node.data.metadata.start && offset < node.data.metadata.end) {
      candidate = node;
    }
  }

  // 递归检查子节点，寻找更精确的匹配
  if (node.children) {
    for (const child of node.children) {
      const found = findNodeByOffset(child, offset);
      if (found) {
        // 如果找到子节点匹配，优先返回子节点（更精确的匹配）
        return found;
      }
    }
  }

  return candidate;
}

function highlightTreeNode(rowIndex: number, chunkIndex: number, byteIndex: number) {
  const offset = rowIndex * 16 + chunkIndex * 4 + byteIndex;
  const node = findNodeByOffset(treeNodes.value[0], offset);
  if (node) {
    selectionKeys.value = { [node.key]: true };
    highlightHex(node);
  }
}

function clearTreeHighlight() {
  selectionKeys.value = {};
  clearHighlight();
}

// 初始化时展开所有节点
expandAll();
</script>

<template>
  <div class="data-container-group" :class="{ 'has-highlight': highlightedBytes !== null }">
    <div class="data-container">
      <h2>Received Data (Hex) - {{ item.name }}</h2>
      <div class="data-display">
        <div v-if="item.hex.length === 0">No data received.</div>
        <div v-else @mouseleave="clearTreeHighlight">
          <div class="hex-header hex-row">
            <span class="address-header"></span>
            <div class="hex-chunk">
              <span v-for="i in 4" :key="i" class="col-header">{{ (i - 1).toString(16).toUpperCase().padStart(2, '0') }}</span>
            </div>
            <div class="hex-chunk">
              <span v-for="i in 4" :key="i" class="col-header">{{ (i + 3).toString(16).toUpperCase().padStart(2, '0') }}</span>
            </div>
            <div class="hex-chunk">
              <span v-for="i in 4" :key="i" class="col-header">{{ (i + 7).toString(16).toUpperCase().padStart(2, '0') }}</span>
            </div>
            <div class="hex-chunk">
              <span v-for="i in 4" :key="i" class="col-header">{{ (i + 11).toString(16).toUpperCase().padStart(2, '0') }}</span>
            </div>
          </div>
          <div v-for="(row, rowIndex) in item.hex" :key="rowIndex" class="hex-row">
            <span class="address">{{ (rowIndex * 16).toString(16).padStart(8, '0') }}</span>
            <div v-for="(chunk, chunkIndex) in row" :key="chunkIndex" class="hex-chunk">
              <span v-for="(byte, byteIndex) in chunk" :key="byteIndex" :class="['hex-byte', { highlight: isByteHighlighted(rowIndex, chunkIndex, byteIndex) }]" @mouseenter="highlightTreeNode(rowIndex, chunkIndex, byteIndex)">
                {{ byte }}
              </span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div class="data-container deserialized-data">
      <h2>Deserialized Data ({{ item.name }}) - Tree View</h2>
      <div class="tree-controls">
        <button @click="expandAll">展开全部</button>
        <button @click="collapseAll">折叠全部</button>
      </div>
      <Tree
        v-model:expandedKeys="expandedKeys"
        v-model:selectionKeys="selectionKeys"
        :value="treeNodes"
        selectionMode="single"
        @node-select="highlightHex"
        @node-unselect="clearHighlight"
        class="data-tree"
      >
        <template #default="{ node }">
          <div class="tree-node-content" @mouseenter="highlightHex(node)" @mouseleave="clearHighlight()">
            <span class="node-indicator" v-if="node.data && node.data.metadata && 'start' in node.data.metadata && 'end' in node.data.metadata">🔍</span>
            <span>{{ getNodeLabel(node) }}</span>
            <span v-if="node.data && getNodeRange(node)" class="node-range"> ({{ getNodeRange(node) }}) </span>
            <span v-if="node.data && node.data.value !== undefined && node.data.value !== null && typeof node.data.value !== 'object'" class="node-value">
              : {{ typeof node.data.value === 'bigint' ? node.data.value.toString() : JSON.stringify(node.data.value) }}
            </span>
          </div>
        </template>
      </Tree>
    </div>
  </div>
</template>

<style scoped>
.hex-byte {
  padding-right: 0.5em;
  border-radius: 2px;
  display: inline-block;
}

.highlight {
  background-color: rgba(52, 152, 219, 0.6);
  border-radius: 3px;
  font-weight: bold;
  box-shadow: 0 0 4px rgba(52, 152, 219, 0.8);
  color: #ffffff;
  transition: all 0.2s ease;
}

.tree-controls {
  margin-bottom: 10px;
}

.tree-controls button {
  margin-right: 10px;
  padding: 6px 12px;
  background-color: #34495e;
  border: 1px solid #2c3e50;
  border-radius: 4px;
  cursor: pointer;
  color: #dcdcdc;
  font-weight: 500;
}



.data-tree {
  max-height: 500px;
  overflow: auto;
  border: 1px solid #2c3e50;
  border-radius: 4px;
  padding: 12px;
  background-color: #1e1e1e;
}

.tree-node-content {
  display: flex;
  align-items: center;
  padding: 4px 6px;
  border-radius: 4px;
  transition: background-color 0.2s ease;
}

:deep(.p-treenode-content.p-highlight) .tree-node-content {
  background-color: rgba(52, 152, 219, 0.3);
  box-shadow: 0 0 4px rgba(52, 152, 219, 0.4);
}



.node-indicator {
  margin-right: 5px;
  font-size: 0.9em;
  color: #3498db;
  cursor: pointer;
}

.node-value {
  margin-left: 5px;
  color: #bdc3c7;
  font-style: italic;
}

.data-container-group {
  display: flex;
  flex-direction: column;
  gap: 0;
}

.has-highlight {
  background-color: rgba(52, 152, 219, 0.15);
  border-radius: 8px;
  transition: background-color 0.3s ease;
}

.data-container {
  border: 1px solid #2c3e50;
  border-radius: 5px;
  padding: 15px;
  background-color: #2d2d2d;
}

.data-container:first-child {
  border-bottom: none;
  border-bottom-left-radius: 0;
  border-bottom-right-radius: 0;
}

.data-container:last-child {
  border-top: none;
  border-top-left-radius: 0;
  border-top-right-radius: 0;
}

.hex-row {
  display: flex;
  align-items: center;
  margin-bottom: 4px;
}

.address {
  width: 80px;
  color: #bdc3c7;
  font-family: monospace;
  font-weight: 500;
}

.hex-chunk {
  display: flex;
  margin-right: 10px;
  background-color: rgba(255, 255, 255, 0.05);
  border-radius: 3px;
  padding: 0 4px;
}

.hex-header {
  margin-bottom: 8px;
  border-bottom: 1px solid #dee2e6;
  padding-bottom: 6px;
}

.col-header {
  color: #95a5a6;
  font-weight: 500;
  padding-right: 0.5em;
}

h2 {
  color: #e0e0e0;
  margin-bottom: 15px;
  font-size: 1.4rem;
  border-bottom: 2px solid #34495e;
  padding-bottom: 8px;
}
</style>

