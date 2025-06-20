<template>
  <div class="features-container">
    <div class="tabs-container">
      <el-tabs v-model="activeCategory" type="card" class="features-tabs" :stretch="true">
        <el-tab-pane v-for="(category,index) in categories" :key="index" :name="index">
          <template #label>
            <div class="tab-label">
              <span>{{ $t(`features.categories[${index}].title`) }}</span>
            </div>
          </template>

          <div class="features-grid">
            <div v-for="(feature, idx) in category.features" :key="idx" class="feature-item" :style="{ '--delay': idx * 0.1 + 's' }">
              <el-card class="feature-card" shadow="hover">
                <div class="card-header">
                  <div class="feature-icon">
                    <el-icon :size="35" color="#647eff">
                      <component :is="featuresIcon[index][idx]" />
                    </el-icon>
                  </div>
                  <h3 class="feature-title">{{ $t(`features.categories[${index}].features[${idx}].title`) }}</h3>
                </div>

                <div class="feature-content">
                  <div class="feature-description">
                    <p>{{ $t(`features.categories[${index}].features[${idx}].description`) }}</p>
                  </div>
                </div>
              </el-card>
            </div>
          </div>
        </el-tab-pane>
      </el-tabs>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import {
  Terminal as TerminalIcon,
  InternetExplorer as IEIcon,
  Expeditedssl as SSLIcon,
  Linux as LinuxIcon,
  Google as GoogleIcon,
  Server as ServerIcon,
  User as UserIcon
} from '@vicons/fa'
import {
  Identification as IdentificationIcon,
  RetryFailed as RetryFailedIcon,
  Debug as DebugIcon,
  Carbon3DPrintMesh as MeshIcon
} from '@vicons/carbon'
import {
  UploadFileSharp as UploadFileIcon,
  GroupSharp as GroupIcon
} from '@vicons/material'
import {
  SettingsAutomation as SettingsIcon,
  DeviceDesktop as DesktopIcon
} from '@vicons/tabler'
import { MdKey as KeyIcon } from '@vicons/ionicons4'
import { TerminalSharp as TerminalSharpIcon } from '@vicons/ionicons5'
import { useI18n } from 'vue-i18n'

const i18n = useI18n()

const categories = i18n.messages.value.en.features.categories
const activeCategory = ref(0)

const featuresIcon = [
  [ TerminalIcon, IdentificationIcon, GroupIcon, UploadFileIcon, SettingsIcon, IEIcon],
  [ SSLIcon, LinuxIcon, GoogleIcon, DesktopIcon, KeyIcon, RetryFailedIcon],
  [ ServerIcon, TerminalSharpIcon, UserIcon],
  [ MeshIcon, DebugIcon, SettingsIcon ]
]
</script>

<style scoped>
.features-container {
  max-width: 1400px;
  margin: 0 auto;
  padding: 2rem 1.5rem;
  position: relative;
}

.tabs-container {
  margin-bottom: 3rem;
}

.features-tabs {
  --el-color-primary: #647eff;
  --el-border-color-light: rgba(100, 100, 255, 0.2);
  --el-bg-color: rgba(20, 20, 35, 0.5);
  --el-text-color-primary: #e0e0ff;
  --el-fill-color-light: rgba(40, 40, 60, 0.7);
}

:deep(.features-tabs) .el-tabs__header {
  margin: 0;
  border-bottom: none;
}

:deep(.features-tabs) .el-tabs__nav {
  border: none !important;
  border-radius: 12px;
}

:deep(.features-tabs) .el-tabs__item {
  padding: 1rem 1.5rem;
  height: auto;
  border: 1px solid rgba(100, 100, 255, 0.15) !important;
  border-radius: 12px;
  margin: 0 0.5rem 1rem;
  background: rgba(30, 30, 45, 0.7);
  backdrop-filter: blur(5px);
  color: #c0c0f0;
  font-weight: 600;
  transition: all 0.3s ease;
  position: relative;
  z-index: 1;
}

:deep(.features-tabs) .el-tabs__item:hover {
  background: rgba(50, 50, 80, 0.8);
  box-shadow: 0 10px 20px rgba(80, 120, 255, 0.3);
  border-color: rgba(100, 150, 255, 0.5) !important;
  color: #fff;
  z-index: 2;
}

:deep(.features-tabs) .el-tabs__item.is-active {
  background: linear-gradient(135deg, #42d392, #647eff);
  color: white;
  border-color: rgba(100, 150, 255, 0.8) !important;
  box-shadow: 0 5px 15px rgba(80, 120, 255, 0.4);
}

.tab-label {
  display: flex;
  align-items: center;
  gap: 0.8rem;
}

.features-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(350px, 1fr));
  gap: 2rem;
  margin-top: 1.5rem;
}

.feature-item {
  opacity: 0;
  transform: translateY(30px);
  animation: fadeUp 0.6s forwards;
  animation-delay: var(--delay);
}

@keyframes fadeUp {
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

.feature-card {
  height: 100%;
  background: rgba(30, 30, 45, 0.7);
  border: 1px solid rgba(100, 100, 255, 0.15);
  border-radius: 16px;
  transition: all 0.3s ease;
  backdrop-filter: blur(5px);
  position: relative;
  overflow: hidden;
}

.feature-card:hover {
  transform: translateY(-10px);
  box-shadow: 0 15px 30px rgba(80, 120, 255, 0.2);
  border-color: rgba(100, 150, 255, 0.4);
}

.feature-card:before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 4px;
  background: linear-gradient(90deg, #42d392, #647eff);
  transform: scaleX(0);
  transform-origin: left;
  transition: transform 0.5s ease;
}

.feature-card:hover:before {
  transform: scaleX(1);
}

.card-header {
  display: flex;
  align-items: center;
  gap: 1rem;
  padding: 1.5rem 1.5rem 1rem;
}

.feature-icon {
  width: 50px;
  height: 50px;
  border-radius: 12px;
  background: linear-gradient(135deg, rgba(66, 211, 146, 0.15), rgba(100, 126, 255, 0.15));
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
}

.feature-title {
  font-size: 1.4rem;
  font-weight: 700;
  color: #e0e0ff;
  margin: 0;
}

.feature-content {
  padding: 0 1.5rem 1.5rem;
}

.feature-description {
  color: #b0b0e0;
  line-height: 1.6;
  margin-bottom: 1.2rem;
}

@media (max-width: 992px) {
  .features-grid {
    grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
  }
}

@media (max-width: 768px) {
  :deep(.features-tabs) .el-tabs__item {
    padding: 0.8rem 1rem;
    font-size: 0.9rem;
  }

  .tab-icon {
    font-size: 1.2rem;
  }
}

@media (max-width: 480px) {
  .features-grid {
    grid-template-columns: 1fr;
  }

  :deep(.features-tabs) .el-tabs__nav-wrap {
    overflow-x: auto;
    scrollbar-width: none;
  }

  :deep(.features-tabs) .el-tabs__nav-wrap::-webkit-scrollbar {
    display: none;
  }

  :deep(.features-tabs) .el-tabs__item {
    min-width: 120px;
    margin: 0 0.3rem 0.8rem;
    padding: 0.7rem 0.8rem;
    font-size: 0.85rem;
  }

  .tab-icon {
    font-size: 1.1rem;
  }

  .feature-title {
    font-size: 1.2rem;
  }
}
</style>
