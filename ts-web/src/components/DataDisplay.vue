<script setup lang="ts">
import { ref } from 'vue';
import NestedDataDisplay from './NestedDataDisplay.vue';

const props = defineProps<{
  item: {
    type: string;
    value: any;
    name: string;
    data: any;
    hex: string[][][];
    metadata?: { [field: string]: NestedMetadata };
  };
}>();

const highlightedBytes = ref<{ start: number; end: number } | null>(null);



interface NestedMetadata {
  nested?: {
    elements?: any[];
    entries?: any[];
  };
  [key: string]: any;
}

function highlightHexNested(field: string, subpath: (number | string)[] = []) {
  let meta = props.item.metadata?.[field];
  if (!meta) return;

  if (subpath.length === 0) {
    highlightedBytes.value = { start: meta.start, end: meta.end };
  console.log(`Setting highlight for ${field} with subpath ${JSON.stringify(subpath)} to ${meta.start}-${meta.end}`);
    return;
  }

  // 处理子路径，逐层深入元数据
  for (const part of subpath) {
    if (typeof part === 'number') {
      if (meta.nested?.elements) {
        meta = meta.nested.elements[part];
      } else if (meta.nested?.entries) {
        meta = meta.nested.entries[part];
      } else {
        return;
      }
    } else if (typeof part === 'string' && (part === 'key' || part === 'value') && meta?.[part]) {
      meta = meta[part];
    } else {
      return;
    }
    if (!meta) return;
  }

  highlightedBytes.value = { start: meta.start, end: meta.end };
}

function highlightHex(field: string) {
  highlightHexNested(field);
}

function clearHighlight() {
  highlightedBytes.value = null;
}

function isByteHighlighted(rowIndex: number, chunkIndex: number, byteIndex: number): boolean {
  if (!highlightedBytes.value) return false;
  const byteOffset = rowIndex * 16 + chunkIndex * 4 + byteIndex;
  return byteOffset >= highlightedBytes.value.start && byteOffset < highlightedBytes.value.end;
}

// Add wc property to AllBasicTypes interface
interface AllBasicTypes {
  wc: number;
}
</script>

<template>
  <div class="data-container-group">
    <div class="data-container">
      <h2>Received Data (Hex) - {{ item.name }}</h2>
      <div class="data-display">
        <div v-if="item.hex.length === 0">No data received.</div>
        <div v-else>
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
              <span v-for="(byte, byteIndex) in chunk" :key="byteIndex" :class="['hex-byte', { highlight: isByteHighlighted(rowIndex, chunkIndex, byteIndex) }]">
                {{ byte }}
              </span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div class="data-container deserialized-data">
      <div v-if="item.data instanceof AllBasicTypes">
        <h2>Deserialized Data (AllBasicTypes)</h2>
        <table>
          <tbody>
            <tr><td @mouseover="highlightHex('b')" @mouseleave="clearHighlight">b</td><td>{{ item.data.b }}</td></tr>
            <tr><td @mouseover="highlightHex('i8_v')" @mouseleave="clearHighlight">i8_v</td><td>{{ item.data.i8_v }}</td></tr>
            <tr><td @mouseover="highlightHex('u8_v')" @mouseleave="clearHighlight">u8_v</td><td>{{ item.data.u8_v }}</td></tr>
            <tr><td @mouseover="highlightHex('i16_v')" @mouseleave="clearHighlight">i16_v</td><td>{{ item.data.i16_v }}</td></tr>
            <tr><td @mouseover="highlightHex('u16_v')" @mouseleave="clearHighlight">u16_v</td><td>{{ item.data.u16_v }}</td></tr>
            <tr><td @mouseover="highlightHex('i32_v')" @mouseleave="clearHighlight">i32_v</td><td>{{ item.data.i32_v }}</td></tr>
            <tr><td @mouseover="highlightHex('u32_v')" @mouseleave="clearHighlight">u32_v</td><td>{{ item.data.u32_v }}</td></tr>
            <tr><td @mouseover="highlightHex('i64_v')" @mouseleave="clearHighlight">i64_v</td><td>{{ item.data.i64_v.toString() }}</td></tr>
            <tr><td @mouseover="highlightHex('u64_v')" @mouseleave="clearHighlight">u64_v</td><td>{{ item.data.u64_v.toString() }}</td></tr>
            <tr><td @mouseover="highlightHex('f')" @mouseleave="clearHighlight">f</td><td>{{ item.data.f }}</td></tr>
            <tr><td @mouseover="highlightHex('d')" @mouseleave="clearHighlight">d</td><td>{{ item.data.d }}</td></tr>
            <tr><td @mouseover="highlightHex('c')" @mouseleave="clearHighlight">c</td><td>'{{ item.data.c }}'</td></tr>
            <tr><td @mouseover="highlightHex('wc')" @mouseleave="clearHighlight">wc</td><td>{{ item.data.wc }} (char: '{{ String.fromCodePoint(item.data.wc) }}')</td></tr>
            <tr><td @mouseover="highlightHex('c8')" @mouseleave="clearHighlight">c8</td><td>'{{ item.data.c8 }}'</td></tr>
            <tr><td @mouseover="highlightHex('c16')" @mouseleave="clearHighlight">c16</td><td>'{{ item.data.c16 }}'</td></tr>
            <tr><td @mouseover="highlightHex('c32')" @mouseleave="clearHighlight">c32</td><td>'{{ item.data.c32 }}'</td></tr>
          </tbody>
        </table>
      </div>
      <div v-else-if="item.data instanceof TemplateContainer">
        <h2>Deserialized Data (TemplateContainer)</h2>
        <table>
          <tbody>
            <tr><td @mouseover="highlightHex('s')" @mouseleave="clearHighlight">s</td><td>"{{ item.data.s }}"</td></tr>
            <tr><td @mouseover="highlightHex('u8s')" @mouseleave="clearHighlight">u8s</td><td>"{{ item.data.u8s }}"</td></tr>
            <tr><td @mouseover="highlightHex('vec')" @mouseleave="clearHighlight">vec</td><td><NestedDataDisplay :data="item.data.vec" :metadata="item.metadata.vec" :path="['vec']" :highlightHex="highlightHexNested" :clearHighlight="clearHighlight" /></td></tr>
            <tr><td @mouseover="highlightHex('deq')" @mouseleave="clearHighlight">deq</td><td><NestedDataDisplay :data="item.data.deq" :metadata="item.metadata.deq" :path="['deq']" :highlightHex="highlightHexNested" :clearHighlight="clearHighlight" /></td></tr>
            <tr><td @mouseover="highlightHex('lst')" @mouseleave="clearHighlight">lst</td><td><NestedDataDisplay :data="item.data.lst" :metadata="item.metadata.lst" :path="['lst']" :highlightHex="highlightHexNested" :clearHighlight="clearHighlight" /></td></tr>
            <tr><td @mouseover="highlightHex('shortForwardList')" @mouseleave="clearHighlight">shortForwardList</td><td><NestedDataDisplay :data="item.data.shortForwardList" :metadata="item.metadata.shortForwardList" :path="['shortForwardList']" :highlightHex="highlightHexNested" :clearHighlight="clearHighlight" /></td></tr>
            <tr><td @mouseover="highlightHex('uintSet')" @mouseleave="clearHighlight">uintSet</td><td><NestedDataDisplay :data="item.data.uintSet" :metadata="item.metadata.uintSet" :path="['uintSet']" :highlightHex="highlightHexNested" :clearHighlight="clearHighlight" /></td></tr>
            <tr><td @mouseover="highlightHex('stringHashSet')" @mouseleave="clearHighlight">stringHashSet</td><td><NestedDataDisplay :data="item.data.stringHashSet" :metadata="item.metadata.stringHashSet" :path="['stringHashSet']" :highlightHex="highlightHexNested" :clearHighlight="clearHighlight" /></td></tr>
            <tr><td @mouseover="highlightHex('intStringMap')" @mouseleave="clearHighlight">intStringMap</td><td><NestedDataDisplay :data="item.data.intStringMap" :metadata="item.metadata.intStringMap" :path="['intStringMap']" :highlightHex="highlightHexNested" :clearHighlight="clearHighlight" /></td></tr>
            <tr><td @mouseover="highlightHex('stringFloatHashMap')" @mouseleave="clearHighlight">stringFloatHashMap</td><td><NestedDataDisplay :data="item.data.stringFloatHashMap" :metadata="item.metadata.stringFloatHashMap" :path="['stringFloatHashMap']" :highlightHex="highlightHexNested" :clearHighlight="clearHighlight" /></td></tr>
          </tbody>
        </table>
      </div>
      <div v-else>
        <h2>Deserialized Data (Unknown Type)</h2>
        <p>Unsupported data type.</p>
      </div>
    </div>
  </div>
</template>

<style scoped>
.hex-byte {
  padding-right: 0.5em;
}

.highlight {
  background-color: rgba(255, 255, 0, 0.5);
  border-radius: 3px;
}
/* Add more styles as needed */
</style>

<template>
  <NestedDataDisplay
    v-if="item.type === 'vec'"
    :data="item.value"
    :metadata="item.metadata?.vec"
    :path="['vec']"
    :highlightHex="highlightHexNested"
    :clearHighlight="clearHighlight"
  />
  <NestedDataDisplay
    v-if="item.type === 'deq'"
    :data="item.value"
    :metadata="item.metadata?.deq"
    :path="['deq']"
    :highlightHex="highlightHexNested"
    :clearHighlight="clearHighlight"
  />
  <NestedDataDisplay
    v-if="item.type === 'lst'"
    :data="item.value"
    :metadata="item.metadata?.lst"
    :path="['lst']"
    :highlightHex="highlightHexNested"
    :clearHighlight="clearHighlight"
  />
  <NestedDataDisplay
    v-if="item.type === 'shortForwardList'"
    :data="item.value"
    :metadata="item.metadata?.shortForwardList"
    :path="['shortForwardList']"
    :highlightHex="highlightHexNested"
    :clearHighlight="clearHighlight"
  />
  <NestedDataDisplay
    v-if="item.type === 'uintSet'"
    :data="item.value"
    :metadata="item.metadata?.uintSet"
    :path="['uintSet']"
    :highlightHex="highlightHexNested"
    :clearHighlight="clearHighlight"
  />
  <NestedDataDisplay
    v-if="item.type === 'stringHashSet'"
    :data="item.value"
    :metadata="item.metadata?.stringHashSet"
    :path="['stringHashSet']"
    :highlightHex="highlightHexNested"
    :clearHighlight="clearHighlight"
  />
  <NestedDataDisplay
    v-if="item.type === 'intStringMap'"
    :data="item.value"
    :metadata="item.metadata?.intStringMap"
    :path="['intStringMap']"
    :highlightHex="highlightHexNested"
    :clearHighlight="clearHighlight"
  />
  <NestedDataDisplay
    v-if="item.type === 'stringFloatHashMap'"
    :data="item.value"
    :metadata="item.metadata?.stringFloatHashMap"
    :path="['stringFloatHashMap']"
    :highlightHex="highlightHexNested"
    :clearHighlight="clearHighlight"
  />
</template>
