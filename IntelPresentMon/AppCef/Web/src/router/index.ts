// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import Vue from 'vue'
import VueRouter, { RouteConfig } from 'vue-router'
import GraphConfigView from '@/views/GraphConfigView.vue'
import ReadoutConfigView from '@/views/ReadoutConfigView.vue'
import OverlayConfigView from '@/views/OverlayConfigView.vue'
import MetricProcessingView from '@/views/MetricProcessingView.vue'
import LoadoutConfigView from '@/views/LoadoutConfigView.vue'
import SimpleView from '@/views/SimpleView.vue'
import CaptureConfigView from '@/views/CaptureConfigView.vue'
import OtherConfigView from '@/views/OtherConfigView.vue'

Vue.use(VueRouter);

const routes: RouteConfig[] = [
  {
    path: '/',
    redirect: { name: 'simple' },
  },
  {
    path: '/graphs/:index',
    name: 'graph-config',
    component: GraphConfigView,
    props: true,
  },
  {
    path: '/readouts/:index',
    name: 'readout-config',
    component: ReadoutConfigView,
    props: true,
  },
  {
    path: '/simple',
    name: 'simple',
    component: SimpleView,
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
  {
    path: '/capture',
    name: 'capture-config',
    component: CaptureConfigView,
  },
  {
    path: '/other',
    name: 'other-config',
    component: OtherConfigView,
  },
];

const router = new VueRouter({
  mode: 'hash',
  routes
});

export default router;
