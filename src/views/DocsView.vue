<template>
  <el-container class="docs-container">
    <el-aside width="300px">
      <el-scrollbar>
        <el-menu unique-opened :default-active="selectedMenu" @select="onMenuSelect">
          <template v-for="(menu, index) in menus" :key="index">
            <el-sub-menu v-if="menu.children.length > 0" :index="menu.index">
              <template #title>
                <span class="menu-item-title">{{ menu.title }}</span>
              </template>
              <el-menu-item v-for="(submenu,sindex) in menu.children" :key="index + sindex" :index="submenu.index">
                <template #title>
                  <span class="menu-item-title">{{ submenu.title }}</span>
                </template>
              </el-menu-item>
            </el-sub-menu>
            <el-menu-item v-else :key="index" :index="menu.index">
              <template #title>
                <span class="menu-item-title">{{ menu.title }}</span>
              </template>
            </el-menu-item>
          </template>
        </el-menu>
      </el-scrollbar>
    </el-aside>
    <el-main>
      <el-scrollbar>
        <div class="docs-content" v-html="DocsHtml"></div>
      </el-scrollbar>
    </el-main>
  </el-container>
</template>

<script setup>
import 'highlight.js/styles/github-dark-dimmed.css'
import hljs from 'highlight.js'
import { html as DocsEnHtml } from '@/md/docs.md'
import { html as DocsZhHtml } from '@/md/docs-zh.md'
import { ElMessage } from 'element-plus'
import useClipboard from 'vue-clipboard3'
import { useI18n } from 'vue-i18n'
import { useRttyVersionStore } from '@/stores/rtty-version'
import { ref, computed, watch, nextTick, onMounted } from 'vue'

const { toClipboard } = useClipboard()
const version = useRttyVersionStore()
const i18n = useI18n()

const selectedMenu = ref('')
const menus = ref([])

const DocsHtml = computed(() => {
  let html = i18n.locale.value === 'en' ? DocsEnHtml : DocsZhHtml
  return html.replace(/RTTY-VERSION/g, version.rtty).replace(/RTTYS-VERSION/g, version.rttys)
})

function onMenuSelect(index) {
  const element = document.getElementById(index)
  if (element) {
    selectedMenu.value = index
    element.scrollIntoView({ behavior: 'smooth', block: 'start' })
  }
}

function setupCopyButtons() {
  document.querySelectorAll('pre').forEach(pre => {
    const button = document.createElement('button')
    button.type = 'button'
    button.innerHTML = `<svg style="width: 15px" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024">
  <path fill="var(--el-color-primary)" d="M128 320v576h576V320zm-32-64h640a32 32 0 0 1 32 32v640a32 32 0 0 1-32 32H96a32 32 0 0 1-32-32V288a32 32 0 0 1 32-32M960 96v704a32 32 0 0 1-32 32h-96v-64h64V128H384v64h-64V96a32 32 0 0 1 32-32h576a32 32 0 0 1 32 32M256 672h320v64H256zm0-192h320v64H256z"></path></svg>
    `

    Object.assign(button.style, {
      position: 'absolute',
      top: '1px',
      right: '-4px',
      background: 'transparent',
      border: 'none',
      cursor: 'pointer'
    })

    button.addEventListener('click', () => {
      toClipboard(pre.textContent)
      ElMessage.success(i18n.t('docs.copied'))
    })

    const wrapper = document.createElement('div')
    wrapper.style.position = 'relative'

    pre.parentNode.insertBefore(wrapper, pre)
    wrapper.appendChild(pre)

    wrapper.appendChild(button)
  })
}

function setupMenus() {
  const newMenus = []

  document.querySelectorAll('h1,h2').forEach(h => {
    if (h.tagName === 'H1') {
      newMenus.push({
        title: h.textContent,
        index: h.id,
        children: []
      })
    } else {
      newMenus[newMenus.length - 1].children.push({
        title: h.textContent,
        index: h.id
      })
    }
  })

  if (newMenus[0].children.length > 0)
    selectedMenu.value = newMenus[0].children[0].index
  else
    selectedMenu.value = newMenus[0].index

  menus.value = newMenus
}

function onDocsHtmlChanged() {
  setupMenus()
  setupCopyButtons()
  hljs.highlightAll()
}

watch(() => DocsHtml.value, () => {
  nextTick(onDocsHtmlChanged)
})

onMounted(onDocsHtmlChanged)
</script>

<style scoped>
.docs-container {
  height: calc(100vh - 230px);
  max-width: 1300px;
  margin: 0 auto;
}

.docs-content {
  padding-right: 10px;
}

:deep(.docs-content h1) {
  font-size: 1.8em;
}

:deep(.docs-content code) {
  font-size: 1.1em;
}

:deep(.el-menu-item),
:deep(.el-sub-menu__title) {
  height: auto;
  line-height: normal;
  padding-top: 10px;
  padding-bottom: 10px;
}

.menu-item-title {
  white-space: normal;
  word-break: break-word;
}
</style>
