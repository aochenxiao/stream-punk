import { useDataStore } from './stores/data'

export function connectWebSocket(url: string) {
  const dataStore = useDataStore()
  const ws = new WebSocket(url)

  ws.binaryType = 'arraybuffer'

  ws.onopen = () => {
    dataStore.setConnectionStatus('Connected')
    console.log('WebSocket connection opened')
    ws.send('1')
  }

  ws.onmessage = (event) => {
    if (event.data instanceof ArrayBuffer) {
      dataStore.setData(event.data)
    }
  }

  ws.onclose = () => {
    dataStore.setConnectionStatus('Disconnected')
    console.log('WebSocket connection closed')
  }

  ws.onerror = (error) => {
    dataStore.setConnectionStatus('Error')
    console.error('WebSocket error:', error)
  }

  return ws
}