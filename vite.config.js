import { fileURLToPath, URL } from 'node:url'

import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import vueDevTools from 'vite-plugin-vue-devtools'
import eslint from '@nabla/vite-plugin-eslint'
import { plugin as markdown} from 'vite-plugin-markdown'
import markdownIt from 'markdown-it'
import markdownItAnchor from 'markdown-it-anchor'

const md = markdownIt()
md.use(markdownItAnchor)

// https://vite.dev/config/
export default defineConfig({
  base: '/rtty/',
  plugins: [
    vue(),
    vueDevTools(),
    eslint(),
    markdown({
      mode: 'html',
      markdownIt: md
    })
  ],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url))
    }
  }
})
