<template>
  <el-container class="container">
    <div id="particles-js"></div>
    <el-header height="auto" class="header">
      <router-link to="/" class="logo">
        <el-icon :size="35" color="#ff00e5"><TerminalIcon/></el-icon>
        <span>RTTY</span>
      </router-link>

      <el-menu mode="horizontal" router :ellipsis="false" :default-active="selectedMenu">
        <el-menu-item index="/">{{ $t('navbar.home') }}</el-menu-item>
        <el-menu-item index="/features">{{ $t('navbar.features') }}</el-menu-item>
        <el-menu-item index="/docs">{{ $t('navbar.docs') }}</el-menu-item>
      </el-menu>

      <el-space>
        <el-button plain type="primary" @click="sponsor = true">
          <el-icon :size="20" color="red"><HeartIcon/></el-icon>
          <span>{{ $t('navbar.sponsor') }}</span>
        </el-button>
        <el-button plain type="primary" @click="openRttyRelease">
          <span>rtty: {{ version.rtty }}</span>
        </el-button>
        <el-button plain type="primary" @click="openRttyGoRelease">
          <span>rtty-go: {{ version.rtty_go }}</span>
        </el-button>
        <el-button plain type="primary" @click="openRttysRelease">
          <span>rttys: {{ version.rttys }}</span>
        </el-button>
        <el-button plain type="primary" @click="openRttyGithub">
          <el-icon :size="20" color="white"><GithubIcon/></el-icon>
          <span>Star {{ stars }}</span>
        </el-button>
        <el-dropdown @command="lang => i18n.locale.value = lang">
          <el-button plain type="primary">{{ $t('navbar.language') }}</el-button>
          <template #dropdown>
            <el-dropdown-menu>
              <el-dropdown-item command="en" :class="{selected: i18n.locale.value === 'en'}">English</el-dropdown-item>
              <el-dropdown-item command="zh-CN" :class="{selected: i18n.locale.value === 'zh-CN'}">简体中文</el-dropdown-item>
            </el-dropdown-menu>
          </template>
        </el-dropdown>
      </el-space>
    </el-header>

    <el-main>
      <el-scrollbar>
        <router-view/>
      </el-scrollbar>
    </el-main>

    <el-footer height="auto" class="footer">
      <el-divider/>
      <el-space spacer="|">
        <el-text>MIT Licensed</el-text>
        <div>
        <el-text>Copyright © 2019 Jianhui Zhao </el-text>
        <el-link href="mailto:zhaojh329@gmail.com">{{ '<zhaojh329@gmail.com>' }}</el-link>
        </div>
      </el-space>
      <br/>
      <el-space spacer="|">
        <el-link href="https://github.com/zhaojh329/rtty" target="_blank">rtty</el-link>
        <el-link href="https://github.com/zhaojh329/rtty-go" target="_blank">rtty-go</el-link>
        <el-link href="https://github.com/zhaojh329/rttys" target="_blank">rttys</el-link>
        <el-link href="https://github.com/zhaojh329/oui" target="_blank">oui</el-link>
      </el-space>
    </el-footer>
  </el-container>
  <Sponsor v-model="sponsor"/>
</template>

<script setup>
import { Terminal as TerminalIcon, Github as GithubIcon, Heart as HeartIcon } from '@vicons/fa'
import { useRttyVersionStore } from '@/stores/rtty-version'
import Sponsor from './Sponsor.vue'
import particlesOptions from './particles'
import { watch, ref, onMounted } from 'vue'
import { useRoute } from 'vue-router'
import { useI18n } from 'vue-i18n'
import particlesJS from '@/particles.js'
import axios from 'axios'


const i18n = useI18n()
const route = useRoute()

const selectedMenu = ref('')

watch(route, newRoute => selectedMenu.value = newRoute.path)

const version = useRttyVersionStore()

const sponsor = ref(false)

const stars = ref('0')

function openRttyRelease() {
  window.open('https://github.com/zhaojh329/rtty/releases')
}

function openRttyGoRelease() {
  window.open('https://github.com/zhaojh329/rtty-go/releases')
}

function openRttysRelease() {
  window.open('https://github.com/zhaojh329/rttys/releases')
}

function openRttyGithub() {
  window.open('https://github.com/zhaojh329/rtty')
}

onMounted(() => {
  particlesJS('particles-js', particlesOptions)

  axios.get('https://api.github.com/repos/zhaojh329/rtty/releases/latest').then(response => {
    version.rtty = response.data.tag_name.substr(1)
  })

  axios.get('https://api.github.com/repos/zhaojh329/rtty-go/releases/latest').then(response => {
    version.rtty_go = response.data.tag_name.substr(1)
  })

  axios.get('https://api.github.com/repos/zhaojh329/rttys/releases/latest').then(response => {
    version.rttys = response.data.tag_name.substr(1)
  })

  axios.get('https://api.github.com/repos/zhaojh329/rtty').then(response => {
    let stargazers_count = response.data.stargazers_count

    if (stargazers_count < 1000) {
      stargazers_count = stargazers_count.toString()
    } else if (stargazers_count < 1_000_000) {
      const k = stargazers_count / 1000
      stargazers_count = (k % 1 === 0 ? k.toFixed(0) : k.toFixed(1)) + 'K'
    } else {
      const m = stargazers_count / 1_000_000
      stargazers_count = (m % 1 === 0 ? m.toFixed(0) : m.toFixed(1)) + 'M'
    }

    stars.value = stargazers_count
  })
})
</script>

<style scoped>
.container {
  height: calc(100vh - 16px);
  padding: 0 5%;
  font-weight: 700;
}

.header {
  display: flex;
  justify-content: space-between;
}

:deep(.el-dropdown-menu__item).selected {
  background-color: var(--el-dropdown-menuItem-hover-fill);
  color: var(--el-dropdown-menuItem-hover-color);
}

.logo {
  font-size: 2.2rem;
  color: #00e5ff;
  display: flex;
  align-items: center;
  text-decoration: none;
  text-shadow: 0 0 15px rgba(0, 229, 255, 0.7);
  transition: all 0.3s ease;
  background: rgba(10, 15, 30, 0.6);
  padding: 8px 20px;
  border-radius: 50px;
  border: 1px solid rgba(0, 229, 255, 0.3);
  box-shadow: 0 0 15px rgba(0, 229, 255, 0.2);
}

.logo:hover {
  transform: scale(1.05);
  text-shadow: 0 0 25px rgba(0, 229, 255, 1);
}

.logo span {
  padding-left: 10px;
  letter-spacing: 1px;
  font-weight: 800;
}

#particles-js {
  position: fixed;
  width: 100%;
  height: 100%;
  top: 0;
  left: 0;
  z-index: -1;
}

.el-menu {
  background: transparent !important;
  border: none !important;
}

.el-menu--horizontal > .el-menu-item {
  font-size: 18px !important;
  font-weight: 600 !important;
  margin: 0 10px !important;
  height: 50px !important;
  line-height: 50px !important;
  background: rgba(20, 25, 45, 0.5) !important;
  border-radius: 12px !important;
  transition: all 0.3s ease !important;
  border: 1px solid rgba(255, 255, 255, 0.1) !important;
  position: relative;
  overflow: hidden;
}

.el-menu--horizontal > .el-menu-item:before {
  content: '';
  position: absolute;
  bottom: 0;
  left: 0;
  width: 0;
  height: 3px;
  background: linear-gradient(90deg, #ff00e5, #00e5ff);
  transition: width 0.3s ease;
}

.el-menu--horizontal > .el-menu-item:hover,
.el-menu--horizontal > .el-menu-item.is-active {
  color: #fff !important;
  background: rgba(30, 35, 60, 0.7) !important;
  transform: translateY(-3px);
  box-shadow: 0 5px 15px rgba(0, 0, 0, 0.3);
}

.el-menu--horizontal > .el-menu-item:hover:before,
.el-menu--horizontal > .el-menu-item.is-active:before {
  width: 100%;
}

.el-button {
  color: #fff !important;
  transition: all 0.3s ease !important;
  border-radius: 12px !important;
  font-weight: 600 !important;
  height: 45px !important;
  padding: 0 20px !important;
  box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1) !important;
  border: 1px solid rgba(255, 255, 255, 0.15) !important;
}

.el-button:hover {
  transform: translateY(-3px) scale(1.03);
  box-shadow: 0 7px 15px rgba(0, 0, 0, 0.2) !important;
}

.el-button--primary {
  background: linear-gradient(135deg, #6a11cb 0%, #2575fc 100%) !important;
  border: none !important;
}

.el-button--primary:hover {
  background: linear-gradient(135deg, #7d2ae8 0%, #3a8cff 100%) !important;
}

.footer {
  text-align: center;
}
</style>
