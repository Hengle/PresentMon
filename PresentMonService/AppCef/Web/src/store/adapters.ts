// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import { Module, VuexModule, Mutation, Action, getModule  } from 'vuex-module-decorators'
import store from './index'
import { Adapter } from '@/core/adapter'
import { Api } from '@/core/api'

@Module({name: 'adapters', dynamic: true, store, namespaced: true})
export class AdaptersModule extends VuexModule {
  adapters: Adapter[] = [];

  @Mutation
  replaceAll(adapters: Adapter[]) {
    this.adapters = adapters;
  }

  @Action({ commit: 'replaceAll' })
  async refresh() {
    return await Api.enumerateAdapters();
  }
}

export const Adapters = getModule(AdaptersModule);