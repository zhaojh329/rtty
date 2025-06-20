import { createApp } from 'vue'
import App from './App.vue'
import router from './router'
import ElementPlus from 'element-plus'
import 'element-plus/dist/index.css'
import 'element-plus/theme-chalk/dark/css-vars.css'
import { createI18n } from 'vue-i18n'
import { createPinia } from 'pinia'

import zhCN from './locales/zh-CN.json'
import en from './locales/en.json'

const app = createApp(App)

app.use(router)
app.use(ElementPlus)

const messages = {
  'zh-CN': zhCN,
  en
}

const i18n = createI18n({
  legacy: false,
  locale: navigator.language === 'zh-CN' ? 'zh-CN' : 'en',
  fallbackLocale: 'en',
  messages
})

app.use(i18n)

const pinia = createPinia()
app.use(pinia)

app.mount('#app')
