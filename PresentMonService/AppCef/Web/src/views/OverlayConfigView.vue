<!-- Copyright (C) 2022 Intel Corporation -->
<!-- SPDX-License-Identifier: MIT -->

<template>
  <div class="page-wrap">
    
  <h2 class="mt-5 ml-5 link-head">
      Overlay Configuration
  </h2>

  <v-card class="page-card">

    <v-row class="mt-5">
      <v-col cols="3">
        Mode
        <p class="text--secondary text-sm-caption mb-0">Choose between an auto-tracking overlay or an independent window</p>
      </v-col>
      <v-col cols="9">
        <v-row>
            <v-col cols="6">
                <v-switch v-model="independent" label="Independent"></v-switch>
            </v-col>
        </v-row>
      </v-col>
    </v-row>

    <v-row class="mt-8">
      <v-col cols="3">
        Visibility
        <p class="text--secondary text-sm-caption mb-0">Toggle overlay manually or automatically hide only during capture</p>
      </v-col>
      <v-col cols="9">
        <v-row>
            <v-col cols="6">
                <v-switch v-model="visible" label="Visible"></v-switch>
            </v-col>
            <v-col cols="6">
                <v-switch v-model="hideDuringCapture" label="Hide During Capture" :disabled="!visible"></v-switch>
            </v-col>
        </v-row>
      </v-col>
    </v-row>

    <v-row class="mt-8">       
      <v-col cols="3">
        Position
        <p class="text--secondary text-sm-caption mb-0">Where the overlay appears on the target window</p>
      </v-col>
      <v-col cols="9">
        <overlay-positioner v-model="position">           
        </overlay-positioner>
      </v-col>
    </v-row>
  
    <v-row class="mt-8">
      <v-col cols="3">
        Width
        <p class="text--secondary text-sm-caption mb-0">Width of the overlay window (height determined by content)</p>
      </v-col>
      <v-col cols="9">
        <v-row>
            <v-col cols="12">
                <v-slider
                    v-model="width"
                    :max="1920"
                    :min="120"
                    thumb-label
                    hide-details
                ></v-slider>
            </v-col>
        </v-row>
      </v-col>
    </v-row>

    <v-row class="mt-8">       
      <v-col cols="3">
        Draw Rate
        <p class="text--secondary text-sm-caption mb-0">Closest valid rate will be targeted (depends on metric sample period)</p>
      </v-col>
      <v-col cols="9">
        <v-slider
            class="overlay-draw-rate"
            v-model="desiredDrawRate"
            :max="120"
            :min="1"
            :messages="[drawRateMessage]"
            thumb-label
        ></v-slider>
      </v-col>
    </v-row>

  </v-card>

  </div>
</template>

<script lang="ts">
import Vue from 'vue'
import { Preferences } from '@/store/preferences'
import { Processes } from '@/store/processes'
import { Process } from '@/core/process'
import OverlayPositioner from '@/components/OverlayPositioner.vue'


export default Vue.extend({
  name: 'OverlayConfig',

  components: {
    OverlayPositioner,
  },
  data: () => ({
    processRefreshTimestamp: null as number|null,
  }),
  methods: {
    async refresh() {
      // @click gets triggered twice for some reason, suppress 2nd invocation
      const stamp = window.performance.now();
      const debounceThresholdMs = 150;
      if (this.processRefreshTimestamp === null || stamp - this.processRefreshTimestamp > debounceThresholdMs) {
        this.processRefreshTimestamp = stamp;
        await Processes.refresh();
      }
    }
  },  
  computed: {
    processes(): Process[] {
      return Processes.processes;
    },
    position: {
      get(): number { return Preferences.preferences.overlayPosition; },
      set(position: number) {
        Preferences.writeAttribute({ attr: 'overlayPosition', val: position });
      },
    },
    drawRateMessage(): string {
      const actual = 1000 / (Preferences.preferences.samplingPeriodMs * Preferences.preferences.samplesPerFrame);
      return `Actual target overlay FPS: ${actual.toFixed(1)}`;
    },

    // v-model enablers
    width: {
      get(): number { return Preferences.preferences.overlayWidth; },
      set(width: number) {
        Preferences.writeAttribute({ attr: 'overlayWidth', val: width });
      },
    },
    hideDuringCapture: {
      get(): boolean { return Preferences.preferences.hideDuringCapture; },
      set(hide: boolean) {
        Preferences.writeAttribute({ attr: 'hideDuringCapture', val: hide });
      },
    },
    visible: {
      get(): boolean { return !Preferences.preferences.hideAlways; },
      set(visible: boolean) {
        Preferences.writeAttribute({ attr: 'hideAlways', val: !visible });
      },
    },
    independent: {
      get(): boolean { return Preferences.preferences.independentWindow; },
      set(independent: boolean) {
        Preferences.writeAttribute({ attr: 'independentWindow', val: independent });
      },
    },
    // this is used to calculate spec.samplesPerFrame in App.vue
    desiredDrawRate: {
      get(): number { return Preferences.desiredOverlayDrawRate; },
      set(drawRate: number) {
        Preferences.setDesiredOverlayDrawRate(drawRate);
      },
    },
  },
  watch: {
  }
});
</script>

<style scoped>
.top-label {
    margin: 0;
    padding: 0;
    height: auto;
}
.overlay-draw-rate >>> .v-messages__message {
  color: blueviolet;
  padding-left: 10px;
}
.link-head {
  color: white;
  user-select: none;
}
.page-card {
  margin: 15px 0;
  padding: 0 15px 15px;
}
.page-wrap {
  max-width: 750px;
  flex-grow: 1;
}
</style>
