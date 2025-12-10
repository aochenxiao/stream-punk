<script setup lang="ts">
import { ref } from 'vue';
const props = defineProps<{
  data: any;
  metadata: any;
  path: (number | string)[];
  highlightHex: (field: string, subpath?: (number | string)[]) => void;
  clearHighlight: () => void;
}>();
</script>

<template>
  <div v-if="Array.isArray(props.data) || props.data instanceof Set">
    <table>
      <thead>
        <tr>
          <th>Index</th>
          <th>Value</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="(item, index) in (Array.isArray(props.data) ? props.data : Array.from(props.data))" :key="index">
          <td @mouseover="props.highlightHex(String(props.path[0]), [...props.path.slice(1), index])" @mouseleave="props.clearHighlight">{{ index }}</td>
          <td>
            <NestedDataDisplay :data="item" :metadata="props.metadata?.nested?.elements?.[index]" :path="[...props.path, index]" :highlightHex="props.highlightHex" :clearHighlight="props.clearHighlight" />
          </td>
        </tr>
      </tbody>
    </table>
  </div>
  <div v-else-if="props.data instanceof Map">
    <table>
      <thead>
        <tr>
          <th>Key</th>
          <th>Value</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="([key, value], index) in Array.from(props.data)" :key="index">
          <td @mouseover="props.highlightHex(String(props.path[0]), [...props.path.slice(1), index, 'key'])" @mouseleave="props.clearHighlight">{{ key }}</td>
          <td>
            <NestedDataDisplay :data="value" :metadata="props.metadata?.nested?.entries?.[index]?.value" :path="[...props.path, index, 'value']" :highlightHex="props.highlightHex" :clearHighlight="props.clearHighlight" />
          </td>
        </tr>
      </tbody>
    </table>
  </div>
  
  <span v-else @mouseover="props.highlightHex(String(props.path[0]), props.path.slice(1))" @mouseleave="props.clearHighlight">{{ JSON.stringify(props.data) }}</span>
</template>