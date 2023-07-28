// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import { Module, VuexModule, Mutation, Action, getModule  } from 'vuex-module-decorators'
import store from './index'
import { KeyOption, ModifierOption, Binding, Action as HotkeyAction, ModifierCode, KeyCode } from '@/core/hotkey'
import { Api } from '@/core/api'
import Vue from 'vue'
import { getEnumValues } from '@/core/meta'
import { Notifications } from './notifications'
import { Preferences } from './preferences'

@Module({name: 'hotkey', dynamic: true, store, namespaced: true})
export class HotkeyModule extends VuexModule {
  keyOptions: KeyOption[] = [];
  modifierOptions: ModifierOption[] = [];
  bindings: {[key: string]: Binding} = {};
  defaultBindings: Binding[] = [
    {action: HotkeyAction.ToggleCapture, combination: {key: <KeyCode>42, modifiers: [<ModifierCode>2, <ModifierCode>4]} },
    {action: HotkeyAction.CyclePreset, combination: {key: <KeyCode>47, modifiers: [<ModifierCode>2, <ModifierCode>4]} },
    {action: HotkeyAction.ToggleOverlay, combination: {key: <KeyCode>46, modifiers: [<ModifierCode>2, <ModifierCode>4]} },
  ];

  @Mutation
  replaceAllKeyOptions(keyOptions: KeyOption[]) {
    this.keyOptions = keyOptions;
  }

  @Mutation
  replaceAllModifierOptions(modifierOptions: ModifierOption[]) {
    this.modifierOptions = modifierOptions;
  }

  @Mutation
  setBinding(payload: Binding) {
    Vue.set(this.bindings, HotkeyAction[payload.action], payload);
  }

  @Action
  async bindHotkey(payload: Binding) {
    await this.bindHotkeyUnserialized(payload);
    try {
      await Preferences.serialize();
    }
    catch (e) {
      const actionName = HotkeyAction[payload.action];
      Notifications.notify({text: `Failed to serialize preferences when binding hotkey [${actionName}]`});
      console.error([`Failed to serialize preferences when binding hotkey: [${actionName}]`, e]);
    }
  }

  @Action
  async clearHotkey(action: HotkeyAction) {
    await this.clearHotkeyUnserialized(action);
    try {
      await Preferences.serialize();
    }
    catch (e) {
      const actionName = HotkeyAction[action];
      Notifications.notify({text: `Failed to serialize preferences when clearing hotkey [${actionName}]`});
      console.error([`Failed to serialize preferences when clearing hotkey: [${actionName}]`, e]);
    }
  }

  @Action
  async bindHotkeyUnserialized(payload: Binding) {
    try {
      await Api.bindHotkey(payload);
      this.context.commit('setBinding', payload);
    } catch (e) {
      const actionName = HotkeyAction[payload.action];
      Notifications.notify({text: `Failed to bind hotkey for [${actionName}]`});
      console.error([`Failed to bind hotkey; Action: [${actionName}]`, e]);
    }
  }

  @Action
  async clearHotkeyUnserialized(action: HotkeyAction) {
    try {
      await Api.clearHotkey(action);
      const binding: Binding = {action: action, combination: null};
      this.context.commit('setBinding', binding);
    } catch (e) {
      const actionName = HotkeyAction[action];
      Notifications.notify({text: `Failed to clear hotkey for [${actionName}]`});
      console.error([`Failed to clear hotkey; Action: [${actionName}]`, e]);
    }
  }

  @Action
  async refreshOptions() {
    this.context.commit('replaceAllKeyOptions', await Api.enumerateKeys());
    this.context.commit('replaceAllModifierOptions', await Api.enumerateModifiers());
  }

  @Action
  async initBindings() {
    for (let action of getEnumValues(HotkeyAction)) {
      const binding: Binding = {action, combination: null};
      this.context.commit('setBinding', binding);
    }
  }

  @Action
  async bindDefaults() {
    for (let binding of this.defaultBindings) {
      try {
        await this.bindHotkeyUnserialized(binding);
      }
      catch (e) {
        await Notifications.notify({text: `Unable to bind default hotkey for ${HotkeyAction[binding.action]}`})
      }
    }
  }
}

export const Hotkey = getModule(HotkeyModule);