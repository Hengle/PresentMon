// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import { Module, VuexModule, Mutation, Action, getModule  } from 'vuex-module-decorators'
import store from './index'
import { Process } from '@/core/process'
import { Api } from '@/core/api'

@Module({name: 'processes', dynamic: true, store, namespaced: true})
export class ProcessesModule extends VuexModule {
  processes: Process[] = [];

  @Mutation
  replaceAll(processes: Process[]) {
    this.processes = processes;
  }

  @Action({ commit: 'replaceAll' })
  async refresh() {
    return await Api.enumerateProcesses();
  }
}

export const Processes = getModule(ProcessesModule);