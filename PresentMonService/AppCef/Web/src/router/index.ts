// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import Vue from 'vue'
import VueRouter, { RouteConfig } from 'vue-router'
import WidgetConfigView from '@/views/WidgetConfigView.vue'
import OverlayConfigView from '@/views/OverlayConfigView.vue'
import MetricProcessingView from '@/views/MetricProcessingView.vue'
import LoadoutConfigView from '@/views/LoadoutConfigView.vue'
import SimpleView from '@/views/SimpleView.vue'
import HotkeyConfigView from '@/views/HotkeyConfigView.vue'

Vue.use(VueRouter);

const routes: RouteConfig[] = [
  {
    path: '/',
    redirect: { name: 'simple' },
  },
  {
    path: '/widgets/:index',
    name: 'widget-config',
    component: WidgetConfigView,
    props: true,
  },
  {
    path: '/simple',
    name: 'simple',
    component: SimpleView,
  },
  {
    path: '/hotkeys',
    name: 'hotkey-config',
    component: HotkeyConfigView,
  },
  {
    path: '/overlay',
    name: 'overlay-config',
    component: OverlayConfigView,
  },
  {
    path: '/metrics',
    name: 'metric-processing',
    component: MetricProcessingView,
  },
  {
    path: '/loadout',
    name: 'loadout-config',
    component: LoadoutConfigView,
  },
];

const router = new VueRouter({
  mode: 'hash',
  base: process.env.BASE_URL,
  routes
});

export default router;
