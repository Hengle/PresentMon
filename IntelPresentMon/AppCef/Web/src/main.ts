// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
//import devtools from '@vue/devtools'
import Vue from 'vue'
import App from './App.vue'
import vuetify from './plugins/vuetify'

import './assets/css/global.css'
import store from './store'
import router from './router'

Vue.config.productionTip = false;
Vue.config.devtools = false;

new Vue({
  vuetify,
  store,
  router,
  render: h => h(App)
}).$mount('#app');

// if (process.env.NODE_ENV === 'development') {
//   devtools.connect();
// }
