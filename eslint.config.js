import globals from 'globals'
import pluginJs from '@eslint/js'
import pluginVue from 'eslint-plugin-vue'

export default [
  {files: ['**/*.{js,mjs,cjs,vue}']},
  {languageOptions: { globals: globals.browser }},
  pluginJs.configs.recommended,
  ...pluginVue.configs['flat/essential'],
  {
    rules: {
      'vue/no-multiple-template-root': 'off',
      'vue/multi-word-component-names': 'off',
      'linebreak-style': ['error', 'unix'],
      'quotes': ['error', 'single'],
      'brace-style': 'error',
      'comma-dangle': 'error',
      'comma-spacing': 'error',
      'keyword-spacing': 'error',
      'no-trailing-spaces': 'error',
      'no-unneeded-ternary': 'error',
      'space-before-function-paren': ['error', 'never'],
      'space-infix-ops': ['error', {'int32Hint': false}],
      'arrow-spacing': 'error',
      'no-var': 'error',
      'no-duplicate-imports': 'error',
      'space-before-blocks': 'error',
      'space-in-parens': ['error', 'never'],
      'no-multi-spaces': 'error',
      'eqeqeq': 'error',
      'indent': ['error', 2],
      'semi': ['error', 'never']
    }
  }
]
