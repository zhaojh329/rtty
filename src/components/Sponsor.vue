<template>
  <el-dialog v-model="model" :title="$t('sponsor.title')" width="500px">
    <el-tabs>
      <el-tab-pane v-for="pay in pays" :key="pay.name" :label="pay.name === 'PayPal' ? 'PayPal' : $t('sponsor.' + pay.name)">
        <div class="tab-content">
          <el-space v-if="pay.name === 'wallet'" direction="vertical">
            <p>{{ $t('sponsor.walletnote') }}</p>
            <el-select v-model="currentWallet" style="width: 240px" @change="copied = false">
              <el-option v-for="w in wallets" :key="w.name" :label="w.name" :value="w.name"/>
            </el-select>
            <el-input v-model="currentWalletAddress" readonly>
              <template #append>
                <el-button :icon="copied ? Check : DocumentCopy" @click="copyAddress" />
              </template>
            </el-input>
            <div style="padding: 10px;background-color: white; width: 220px;">
              <qrcode-vue :value="currentWalletAddress" size="220" level="H" render-as="svg" />
            </div>
          </el-space>
          <el-space v-else-if="pay.name === 'PayPal'">
            <el-icon class="paypal-icon" :size="40" color="#009cde"><PaypalIcon/></el-icon>
            <el-link class="paypal-link" href="https://paypal.me/zjh329?country.x=C2&locale.x=zh_XC" target="_blank">https://paypal.me/zjh329</el-link>
          </el-space>
          <div v-else style="padding-top: 25px;">
            <div class="qr-container">
              <div class="qr-frame">
                <div class="qr-code">
                  <img :src="pay.img" width="95%"/>
                </div>
              </div>
            </div>
          </div>
        </div>
      </el-tab-pane>
    </el-tabs>
    <p>{{ $t('sponsor.acknowledgments') }}</p>
    <p>{{ $t('sponsor.contact') }}<el-link href="mailto:zhaojh329@gmail.com">{{ 'zhaojh329@gmail.com' }}</el-link></p>
  </el-dialog>
</template>

<script setup>
import { CcPaypal as PaypalIcon } from '@vicons/fa'
import wxpayImg from '@/assets/wxpay.png'
import alipayImg from '@/assets/alipay.png'
import { computed, ref } from 'vue'
import { DocumentCopy, Check } from '@element-plus/icons-vue'
import useClipboard from 'vue-clipboard3'
import QrcodeVue from 'qrcode.vue'

const model = defineModel()

const { toClipboard } = useClipboard()

const pays = [
  {
    name: 'wxpay',
    img: wxpayImg
  },
  {
    name: 'alipay',
    img: alipayImg
  },
  {
    name: 'PayPal'
  },
  {
    name: 'wallet'
  }
]

const wallets = [
  {
    name: 'BTC',
    address: 'bc1p2kxxj07kmu8u2wjmrsjpcphpz45mtxg024m6u42nk5lyzc5rgxmqztenej'
  },
  {
    name: 'ETH',
    address: '0x76aD2bd4C018331549d8851b8C4AfDB6569D0e48'
  },
  {
    name: 'BNB',
    address: '0x76aD2bd4C018331549d8851b8C4AfDB6569D0e48'
  },
  {
    name: 'DOGE',
    address: '0x76aD2bd4C018331549d8851b8C4AfDB6569D0e48'
  },
  {
    name: 'USTD',
    address: '0x76aD2bd4C018331549d8851b8C4AfDB6569D0e48'
  }
]

const currentWallet = ref('BTC')

const currentWalletAddress = computed(() => {
  const wallet = wallets.find(w => w.name === currentWallet.value)
  return wallet ? wallet.address : ''
})

const copied = ref(false)

const copyAddress = () => {
  toClipboard(currentWalletAddress.value)
  copied.value = true
}
</script>

<style scoped>
.tab-content {
  display: flex;
  flex-direction: column;
  align-items: center;
}

.qr-container {
  position: relative;
  margin-bottom: 25px;
  transition: all 0.4s ease;
}

.qr-container:hover {
  transform: translateY(-10px);
}

.qr-frame {
  width: 260px;
  height: 260px;
  background: linear-gradient(45deg, #ff00e5, #00e5ff);
  border-radius: 20px;
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 10px;
  position: relative;
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3);
  animation: glow 2s infinite alternate;
}

@keyframes glow {
  from {
    box-shadow: 0 0 15px rgba(0, 229, 255, 0.6);
  }
  to {
    box-shadow: 0 0 25px rgba(255, 0, 229, 0.8), 0 0 35px rgba(0, 229, 255, 0.6);
  }
}

.qr-code {
  width: 240px;
  height: 240px;
  background: white;
  border-radius: 15px;
  display: flex;
  justify-content: center;
  align-items: center;
  font-size: 8rem;
  color: #333;
  overflow: hidden;
}

.paypal-icon {
  filter: drop-shadow(0 0 10px rgba(0, 156, 222, 0.5));
}

.paypal-link {
  font-size: 1.3rem;
  color: #00e5ff !important;
  font-weight: 600;
  transition: all 0.3s ease;
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 10px 25px;
  background: rgba(30, 35, 60, 0.5);
  border-radius: 12px;
  border: 1px solid rgba(255, 255, 255, 0.1);
}

.paypal-link:hover {
  color: #ff00e5 !important;
  transform: translateY(-3px);
  background: rgba(40, 45, 70, 0.7);
  box-shadow: 0 5px 15px rgba(0, 0, 0, 0.3);
}
</style>
