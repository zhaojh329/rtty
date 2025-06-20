<template>
  <div class="home-container">
    <el-row justify="space-between" align="middle">
      <el-col :xs="24" :sm="24" :md="14" :lg="14" :xl="14" class="hero-content">
        <h1 class="hero-title">{{ $t('hero.title') }}</h1>
        <h2 class="hero-subtitle">{{ $t('hero.subtitle') }}</h2>
        <p class="hero-description">
          {{ $t('hero.description[0]') }}
          <el-link type="primary" href="https://github.com/zhaojh329/rtty">rtty</el-link>
          {{ $t('hero.description[1]') }}
          <el-link type="primary" href="https://github.com/zhaojh329/rtty-go">rtty-go</el-link>
          {{ $t('hero.description[2]') }}
          <el-link type="primary" href="https://github.com/zhaojh329/rttys">rttys</el-link>
        </p>

        <el-button v-if="false" type="primary" size="large" round @click="0">
          {{ $t('hero.demo') }}
        </el-button>
      </el-col>

      <el-col :xs="24" :sm="24" :md="10" :lg="9" :xl="9">
        <div class="terminal-window">
          <div class="terminal-header">
            <div class="terminal-dots">
              <span v-for="c in ['red', 'yellow', 'green']" :key="c" :class="{dot: true, [c]: true}"></span>
            </div>
          </div>
          <div class="terminal-body">
            <div class="terminal-line">$ rtty -I MyDevice -h rttys.example.com <span class="cursor">|</span></div>
          </div>
        </div>
      </el-col>
    </el-row>

    <h1 class="sponsors-title">{{ $t('sponsors') }}</h1>
    <el-space size="large">
      <el-link v-for="(sponsor, index) in sponsors" :key="index" :href="sponsor.url" target="_blank" class="sponsor-link">
        <img :src="sponsor.logo" :alt="sponsor.name"/>
      </el-link>
    </el-space>
  </div>
</template>

<script setup>
const sponsors = [
  {
    name: 'GL.iNet',
    logo: 'https://www.gl-inet.com/logo.svg',
    url: 'https://www.gl-inet.com'
  }
]
</script>

<style scoped>
.home-container {
  max-width: 1200px;
  margin: 0 auto;
}

.hero-content {
  padding-right: 40px;
}

.hero-title {
  font-size: 3.5rem;
  line-height: 1.2;
  margin-bottom: 20px;
  background: linear-gradient(135deg, #42b983, #64c8c8);
  -webkit-background-clip: text;
  background-clip: text;
  -webkit-text-fill-color: transparent;
}

.hero-subtitle {
  font-size: 1.8rem;
  font-weight: 400;
  margin-bottom: 30px;
  color: #e6e6e6;
}

.hero-description {
  font-size: 1.2rem;
  line-height: 1.6;
  margin-bottom: 40px;
  color: #b3b3b3;
}

:deep(.hero-description .el-link) {
  font-size: 1.3rem;
  vertical-align: baseline;
}

.terminal-window {
  background: #0d1117;
  border-radius: 10px;
  box-shadow: 0 20px 50px rgba(0, 0, 0, 0.5);
  max-width: 450px;
}

.terminal-header {
  background: #1a2639;
  padding: 10px 15px;
}

.terminal-dots {
  display: flex;
  gap: 8px;
}

.dot {
  width: 12px;
  height: 12px;
  border-radius: 50%;
}

.red {
  background: #ff5f56;
}

.yellow {
  background: #ffbd2e;
}

.green {
  background: #27c93f;
}

.terminal-body {
  padding: 20px;
  font-family: 'Courier New', monospace;
  color: #e6e6e6;
  min-height: 200px;
}

.terminal-line {
  margin-bottom: 10px;
}

.terminal-line:nth-child(1) {
  animation: typing 3s steps(20, end) forwards;
  width: 0;
  overflow: hidden;
  white-space: nowrap;
}

.cursor {
  animation: blink 1s infinite;
  color: #42b983;
}

@keyframes blink {
  0% { opacity: 1; }
  50% { opacity: 0; }
  100% { opacity: 1; }
}

@keyframes typing {
  from { width: 0; }
  to { width: 100%; }
}

.sponsors-title {
  color: #64c8c8;
  font-size: 2rem;
  margin-bottom: 25px;
  font-weight: 500;
}

.sponsor-link {
  background-color: aliceblue;
  border-radius: 5px;
  padding: 10px;
}

.sponsor-link img {
  width: 200px;
}
</style>
