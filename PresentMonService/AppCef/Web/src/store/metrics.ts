// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import { Module, VuexModule, Mutation, Action, getModule  } from 'vuex-module-decorators'
import store from './index'
import { Metric } from '@/core/metric'
import { Api } from '@/core/api'

@Module({name: 'metrics', dynamic: true, store, namespaced: true})
export class MetricsModule extends VuexModule {
  metrics: Metric[] = [];

  get categories() {
    return Array.from(new Set(this.metrics.map(m => m.category)));
  }

  @Mutation
  replaceAll(metrics: Metric[]) {
    this.metrics = metrics;
  }

  @Action
  async load() {
    if (this.metrics.length === 0) {
      this.context.commit('replaceAll', await Api.enumerateMetrics());
    }
  }
}

export const Metrics = getModule(MetricsModule);