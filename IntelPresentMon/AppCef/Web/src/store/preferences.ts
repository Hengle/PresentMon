// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import { Module, VuexModule, Mutation, Action, getModule } from 'vuex-module-decorators'
import store from './index'
import { Preferences as PreferencesType, PreferenceFile, makeDefaultPreferences, Preset } from '@/core/preferences'
import { combinationsAreSame } from '@/core/hotkey'
import { signature } from '@/core/preferences'
import { Hotkey } from './hotkey'
import { Api } from '@/core/api'
import { DelayToken, dispatchDelayedTask } from '@/core/timing'
import { Adapters } from './adapters'

@Module({name: 'preferences', dynamic: true, store, namespaced: true})
export class PreferencesModule extends VuexModule {
  preferences: PreferencesType = makeDefaultPreferences();
  
  // non-persistent
  capturing = false; // capture status as requested
  captureDurationToken:DelayToken|null = null;
  capturingActive = false; // capture status considering delay
  pid:number|null = null;
  // front-end only section
  desiredOverlayDrawRate = 10; // derives samplesPerFrame
  // debouncing
  debounceToken: number|null = null;

  @Mutation
  setDesiredOverlayDrawRate(rate: number) {
      this.desiredOverlayDrawRate = rate;
  }

  @Mutation
  setCapture(active: boolean) {
      this.capturing = active;
  }
 
  @Mutation 
  setCaptureDurationToken(token: DelayToken|null) { 
      this.captureDurationToken = token; 
  } 

  @Mutation
  setPid(pid: number|null) {
      this.pid = pid;
  }

  @Mutation
  setAttribute<K extends keyof PreferencesType>(payload: {attr: K, val: PreferencesType[K]}) {
    this.preferences[payload.attr] = payload.val;
  }

  @Mutation
  setDebounceToken(token: number|null) {
    this.debounceToken = token;
  }

  @Mutation
  setAllPreferences(prefs: PreferencesType) {
    this.preferences = Object.assign({}, this.preferences, prefs);
    this.desiredOverlayDrawRate = 10;
    this.capturing = false;
    this.captureDurationToken = null;
    this.capturingActive = false;
    this.pid = null;
    this.debounceToken = null;
  }

  @Action
  async writeAdapterId(id: number) {    
    await Api.setAdapter(id);
    this.setAttribute({attr: 'adapterId', val: id});
    this.serialize();
  }

  @Action
  serialize() {
    if (this.debounceToken !== null) {
      clearTimeout(this.debounceToken);
    }
    const token = setTimeout(() => {
      this.setDebounceToken(null);
      const file: PreferenceFile = {
          signature,
          preferences: this.preferences,
          hotkeyBindings: Hotkey.bindings,
      };
      Api.storePreferences(JSON.stringify(file, null, 3));
    }, 400);
    this.setDebounceToken(token);
  }

  @Action
  resetPreferences() {    
    this.setAllPreferences(makeDefaultPreferences());
    // start in slot 1
    this.setAttribute({attr:'selectedPreset', val: Preset.Slot1});
    this.serialize();
  }

  @Action
  async writeAttribute<K extends keyof PreferencesType>(payload: {attr: K, val: PreferencesType[K]}) {
    this.setAttribute(payload);
    this.serialize();
  }

  @Action
  async writeCapture(active: boolean) {
    if (active) {
      if (this.preferences.enableCaptureDuration) {
        this.setCaptureDurationToken(dispatchDelayedTask(
          () => this.setCapture(false),
          this.preferences.captureDuration * 1000).token
        );
      }
      this.setCapture(true);
    }
    else {
      if (this.captureDurationToken) {
        this.captureDurationToken.cancel();
        this.setCaptureDurationToken(null);
      }
      this.setCapture(false);
    }
  }

  @Action
  async parseAndReplaceRawPreferenceString(payload: {payload: string}) {
      const config = JSON.parse(payload.payload) as PreferenceFile;
      if (config.signature.code !== signature.code) throw new Error('Bad file format');
      if (config.signature.version !== signature.version) throw new Error('Bad config file version');
      
      Object.assign(this.preferences, config.preferences);
      
      // set hotkey bindings and synchronize with overlay thread
      for (const key in config.hotkeyBindings) {
        const newBinding = config.hotkeyBindings[key];
        const oldBinding = Hotkey.bindings[key];
        if (newBinding.combination === null) {
          if (oldBinding.combination !== null) {
            // clear hotkey if currently set but cleared in loaded config
            await Hotkey.clearHotkey(newBinding.action);
          }
        } else if (oldBinding.combination === null || !combinationsAreSame(newBinding.combination, oldBinding.combination)) {
          // set hotkey if new combination is set and not identical to old
          await Hotkey.bindHotkey(newBinding);
        }
      }

      // handle adapter selection
      if (this.preferences.adapterId !== null) {
        if (this.preferences.adapterId >= Adapters.adapters.length) {
          this.setAttribute({attr: 'adapterId', val: null});
        }
        else {
          await Api.setAdapter(this.preferences.adapterId);
        }
      }
  }
}

export const Preferences = getModule(PreferencesModule);