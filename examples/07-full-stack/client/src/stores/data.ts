import { defineStore } from 'pinia'
import { ref } from 'vue'
import { MousePosition } from '../stream-punk-data'

export const useDataStore = defineStore('data', () => {
  const data = ref<ArrayBuffer | null>(null)
  const connectionStatus = ref('Disconnected')
  const mouse = ref(new MousePosition())

  function setData(newData: ArrayBuffer) {
    data.value = newData
  }

  function setConnectionStatus(status: string) {
    connectionStatus.value = status
  }

  return { data, setData, connectionStatus, setConnectionStatus, mouse }
})
