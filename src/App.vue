<template>
  <el-config-provider :locale="locale">
    <layout></layout>
  </el-config-provider>
</template>

<script setup>
import zhCn from 'element-plus/es/locale/lang/zh-cn'
import en from 'element-plus/es/locale/lang/en'
import { useDark } from '@vueuse/core'
import { useI18n } from 'vue-i18n'
import { computed } from 'vue'
import Layout from '@/components/Layout.vue'

const i18n = useI18n()

const locale = computed(() => {
  document.title = i18n.t('app.title')
  return i18n.locale.value === 'en' ? en : zhCn
})

const isDark = useDark()
isDark.value = true
</script>
