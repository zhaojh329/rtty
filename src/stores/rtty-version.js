import { defineStore } from 'pinia'

export const useRttyVersionStore = defineStore('version', {
  state: () => ({ rtty: '0.0.0', rtty_go: '0.0.0', rttys: '0.0.0' })
})
