// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import Vue from 'vue'
import Vuex from 'vuex'
import { MetricsModule } from './metrics'
import { ProcessesModule } from './processes'
import { NotificationsModule } from './notifications'
import { HotkeyModule } from './hotkey'
import { AdaptersModule } from './adapters'
import { PreferencesModule } from './preferences'
import { LoadoutModule } from './loadout'

Vue.use(Vuex);

interface IRootState {
  metrics: MetricsModule,
  processes: ProcessesModule,
  notifications: NotificationsModule,
  hotkey: HotkeyModule,
  adapters: AdaptersModule,
  preferences: PreferencesModule,
  loadout: LoadoutModule,
}

export default new Vuex.Store<IRootState>({});
