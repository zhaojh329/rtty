import { createRouter, createWebHashHistory } from 'vue-router'

const routes = [
  {
    path: '/',
    name: 'home',
    component: () => import('../views/HomeView.vue')
  },
  {
    path: '/features',
    name: 'features',
    component: () => import('../views/FeaturesView.vue')
  },
  {
    path: '/docs',
    name: 'docs',
    component: () => import('../views/DocsView.vue')
  }
]

const router = createRouter({
  history: createWebHashHistory('/rtty/'),
  routes,
  scrollBehavior(to, from, savedPosition) {
    if (savedPosition) {
      return savedPosition
    } else {
      return { top: 0 }
    }
  }
})

export default router
