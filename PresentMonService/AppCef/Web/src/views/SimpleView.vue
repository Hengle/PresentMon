<!-- Copyright (C) 2022 Intel Corporation -->
<!-- SPDX-License-Identifier: MIT -->

<template>
<div class="page-wrap">

  <v-card class="page-card my-7 pt-3" :class="{'stepper-hilight': wizardStep === 0 && !tutorialFinished}">
    <v-row>
      <v-col cols="3">
        Process
        <p class="text--secondary text-sm-caption mb-0">Application process to track, overlay and capture</p>
      </v-col>
      <v-col cols="9" class="d-flex align-center">
        <v-autocomplete
          :items="processes"
          v-model="pid"
          item-value="pid"
          item-text="name"
          label="Process"
          @click="refreshProcessList"
          hide-details
          outlined
          clearable
          dense
        >
          <template slot="selection" slot-scope="data">
            {{ data.item.name }}
            <span class="pid-node">[{{ data.item.pid }}]</span>
          </template>
          <template slot="item" slot-scope="data">
            {{ data.item.name }}
            <span class="pid-node">[{{ data.item.pid }}]</span>
          </template>
        </v-autocomplete>
      </v-col>
    </v-row>
    <v-expand-transition>
    <div v-if="wizardStep === 0 && !tutorialFinished" class="hilight-info mt-2">
      Get started by selecting a process (running game application) to gather performance information from!
    </div>
    </v-expand-transition>
  </v-card>
  
  <v-card class="page-card my-7 pt-3" :class="{'stepper-hilight': wizardStep === 1}">
    <v-row>       
      <v-col cols="3">
        Preset
        <p class="text--secondary text-sm-caption mb-0">Select a preset configuration for overlay widget loadout etc.</p>
        <p class="hotkey-text mb-0">{{getCombinationText(cyclePresetAction)}}</p>
      </v-col>

      <v-col cols="9" class="d-flex justify-center align-center">        
        <v-btn-toggle v-model="selectedPreset" :mandatory="selectedPreset !== null">
          <v-btn class="px-5" large :disabled="wizardStep < 1">
            Basic FPS
          </v-btn>

          <v-btn class="px-5" large :disabled="wizardStep < 1">
            Graphed FPS
          </v-btn>

          <v-btn class="px-5" large :disabled="wizardStep < 1">
            Power
          </v-btn>

          <v-btn class="px-5" large :value="1000" :disabled="wizardStep < 1">
            Custom
          </v-btn>        
        </v-btn-toggle>
        <v-btn
          :to="{name: 'loadout-config'}"
          color="primary" class="ml-5"
          :disabled="selectedPreset !== 1000 || wizardStep < 1"
        >
          Edit
        </v-btn>
      </v-col>  

      <v-overlay :value="wizardStep < 1" :absolute="true" opacity="0.8"></v-overlay>
    </v-row>

    <v-expand-transition>
    <div v-if="wizardStep === 1" class="hilight-info mt-2">
      Now select what information is overlaid on your chosen process.      
    </div>
    </v-expand-transition>
  </v-card>
  
  <v-card class="page-card my-7 pt-3" :class="{'stepper-hilight': wizardStep === 2 || wizardStep === 3}">
    <v-row>       
      <v-col cols="3">
        Capture
        <p class="text--secondary text-sm-caption mb-0">Capture a trace of per-frame performance data as a CSV file</p>
        <p class="hotkey-text mb-0">{{getCombinationText(toggleCaptureAction)}}</p>
      </v-col>

      <v-col cols="9" class="d-flex justify-center align-center">
        <v-btn 
          x-large
          :color="beginCaptureButtonColor"
          class="pa-10"
          @click="handleCaptureClick"
          :disabled="!hasActiveTarget || wizardStep < 2"
        >{{ beginCaptureButtonText }}</v-btn>
      </v-col>

      <v-overlay :value="wizardStep < 2" :absolute="true" opacity="0.8"></v-overlay>
    </v-row>

    <v-expand-transition>
    <div v-if="wizardStep === 2" class="hilight-info mt-2">
      Click <small class="text--primary">BEGIN CAPTURE</small> to record a trace of performance data.      
    </div>
    </v-expand-transition>

    <v-expand-transition>
    <div v-if="wizardStep === 3" class="hilight-info mt-2">
      Click <small class="text--primary">END CAPTURE</small> to finish recording the trace.   
    </div>
    </v-expand-transition>

    <v-row dense>
      <v-col cols="3">
        Capture Duration
        <p class="text--secondary text-sm-caption mb-0">Automatically stop capture after N seconds</p>
      </v-col>      
      <v-col cols="3">
          <v-switch v-model="enableCaptureDuration" label="Enable" hide-details></v-switch>
      </v-col>
      <v-col cols="6">
        <v-slider
          v-model="captureDuration"
          :max="120"
          :min="0.5"
          :step="0.5"
          :disabled="!enableCaptureDuration"
          class="mt-4"
          thumb-label
          hide-details
        ></v-slider>
      </v-col>
    </v-row>

  </v-card>
  
  <v-card class="page-card my-7 pt-3" :class="{'stepper-hilight': wizardStep === 4}">
    <v-row>       
      <v-col cols="3">
        Trace Storage
        <p class="text--secondary text-sm-caption mb-0">Open the folder containing all captured traces</p>
      </v-col>

      <v-col cols="9" class="d-flex justify-center align-center">
        <v-btn 
          large
          color="secondary"
          class="px-6"
          @click="handleExploreClick"
          :disabled="wizardStep < 4"
        >Open in Explorer</v-btn>
      </v-col>

      <v-overlay :value="wizardStep < 4" :absolute="true" opacity="0.8"></v-overlay>
    </v-row>

    <v-expand-transition>
    <div v-if="wizardStep === 4" class="hilight-info mt-2">
      <span>Click <small class="text--primary">OPEN IN EXPORER</small> to examine the captured performance trace.</span>
    </div>
    </v-expand-transition>
  </v-card>
  <v-row>
    <v-col cols="12" class="text-right">
      <router-link v-show="tutorialFinished" class="settings-link" :to="{name: 'overlay-config'}">
        Settings
        <v-icon large>mdi-cog</v-icon>
      </router-link>
    </v-col>
  </v-row>
</div>
</template>

<script lang="ts">
import Vue from 'vue'
import { Preferences } from '@/store/preferences'
import { Processes } from '@/store/processes'
import { Process } from '@/core/process'
import { Api } from '@/core/api'
import { Preset, WizardStep } from '@/core/preferences'
import { ModifierCode, KeyCode, Action } from '@/core/hotkey'
import { Hotkey } from '@/store/hotkey'


export default Vue.extend({
  name: 'SimpleView',

  data: () => ({
    processRefreshTimestamp: null as number|null,
    toggleCaptureAction: Action.ToggleCapture,
    cyclePresetAction: Action.CyclePreset,
  }),

  methods: {
    async refreshProcessList() {
      // @click gets triggered twice for some reason, suppress 2nd invocation
      const stamp = window.performance.now();
      const debounceThresholdMs = 150;
      if (this.processRefreshTimestamp === null || stamp - this.processRefreshTimestamp > debounceThresholdMs) {
        this.processRefreshTimestamp = stamp;
        await Processes.refresh();
      }
    },    
    handleCaptureClick() {
      Preferences.writeCapture(!this.capturing);
    },
    async handleExploreClick() {
      await Api.exploreCaptures();
      if (this.wizardStep === 4) {
        this.wizardStep = 5;
      }
    },    
    getModifierName(mod: ModifierCode): string {
      return Hotkey.modifierOptions.find(mo => mo.code === mod)?.text ?? '???';
    },
    getKeyName(key: KeyCode): string {
      return Hotkey.keyOptions.find(ko => ko.code === key)?.text ?? '???';
    },
    getCombinationText(action: Action): string {
      const combination = Hotkey.bindings[Action[action]]?.combination;
      if (combination) {
        let text = '';
        for (const m of combination.modifiers) {
          text += this.getModifierName(m) + ' + ';
        }
        return text + this.getKeyName(combination.key);
      }
      else {
        return '';
      }
    },
  },  
  computed: {
    processes(): Process[] {
      return Processes.processes;
    },
    hasActiveTarget(): boolean {
      return Preferences.pid !== null;
    },
    capturing(): boolean {
      return Preferences.capturing;
    },
    beginCaptureButtonColor(): string {
      return this.capturing ? 'warning' : 'primary';
    },
    beginCaptureButtonText(): string {
      return this.capturing ? 'END CAPTURE' : 'BEGIN CAPTURE';
    },

    // v-model enablers
    pid: {
      get(): number|null { return Preferences.pid; },
      set(val: number|null) {
        Preferences.setPid(val);
      },
    },
    selectedPreset: {
      get(): Preset|null { return Preferences.preferences.selectedPreset; },
      set(val: Preset|null) {
        Preferences.writeAttribute({attr: 'selectedPreset', val});
      }
    },
    wizardStep: {
      get(): WizardStep { return Preferences.wizardStep; },
      set(step: WizardStep) {
        Preferences.setWizardStep(step);
      }
    },
    tutorialFinished: {      
      get(): boolean { return Preferences.preferences.tutorialFinished; },
      set(val: boolean) {
        Preferences.writeAttribute({attr: 'tutorialFinished', val});
      }
    },
    captureDuration: {      
      get(): number { return Preferences.preferences.captureDuration; },
      set(val: number) {
        Preferences.writeAttribute({attr: 'captureDuration', val});
      }
    },
    enableCaptureDuration: {      
      get(): boolean { return Preferences.preferences.enableCaptureDuration; },
      set(val: boolean) {
        Preferences.writeAttribute({attr: 'enableCaptureDuration', val});
      }
    }
  },
  watch: {
    async selectedPreset(newPreset: Preset) {
      // custom preset selected, don't clear wizard
      if (newPreset === Preset.Custom) {
        return;
      }    
      if (this.wizardStep === WizardStep.SelectPreset) {
        this.wizardStep = WizardStep.BeginCapture;
      }
    },
    pid() {
      if (this.wizardStep === WizardStep.SelectTarget) {
        this.wizardStep = WizardStep.SelectPreset;
      }
    },
    capturing() {
      if (this.wizardStep === WizardStep.BeginCapture) {
        this.wizardStep = WizardStep.EndCapture;
      } else if (this.wizardStep === WizardStep.EndCapture) {
        this.wizardStep = WizardStep.ExploreTraces;
      }
    },
    wizardStep(newStep: WizardStep) {
      if (newStep === WizardStep.Done) {
        this.tutorialFinished = true;
      }
    }
  }
});
</script>

<style lang="scss" scoped>
.pid-node {
  font-size: 12px;
  color: grey;
  padding-left: 8px;
}
.page-card {
  margin: 15px 0;
  padding: 0 15px 15px;
}
.page-wrap {
  max-width: 1024px;
}
.stepper-hilight.stepper-hilight.stepper-hilight.stepper-hilight {
  border:1px solid white;
  box-shadow:
    0 0 2px 1px hsl(125, 100%, 88%),  /* inner white */
    0 0 6px 4px hsl(84, 100%, 59%), /* middle green */
    0 0 9px 5px hsl(266, 100%, 59%); /* outer cyan */
}
.hilight-info {
  color: greenyellow;
}
.hotkey-text {
  font-size: 10px;
  text-transform: capitalize;
  color: gray;
}
.settings-link {
  font-size: 24px;
  color: #CCC;
  text-decoration: none;
  &:hover {
    color: #FFF
  }
  transition: color .3s;

  i.v-icon.v-icon {
    color: inherit;
  }
}
</style>

