<!-- Copyright (C) 2022 Intel Corporation -->
<!-- SPDX-License-Identifier: MIT -->

<template>
  <div class="page-wrap">
    
  <h2 class="mt-5 ml-5 link-head">
      Data Processing Configuration
  </h2>

  <v-card class="page-card">

    <v-row class="mt-5">
      <v-col cols="3">
        Sampling Period
        <p class="text--secondary text-sm-caption mb-0">Time between polls to API for metric data (ms). Directly affects temporal resolution of graphs and readouts.</p>
      </v-col>
      <v-col cols="9">
        <v-slider
          v-model="samplingPeriod"
          :max="250"
          :min="2"
          thumb-label="always"
          hide-details
        ></v-slider>
      </v-col>
    </v-row>

    <v-row class="mt-5">
      <v-col cols="3">
        Telemetry Period
        <p class="text--secondary text-sm-caption mb-0">Time between service-side power telemetry polling calls (ms). Indirectly affects temporal resolution of a subset of metrics, such as GPU power and temperature.</p>
      </v-col>
      <v-col cols="9">
        <v-slider
          v-model="telemetrySamplingPeriod"
          :max="500"
          :min="1"
          thumb-label="always"
          hide-details
        ></v-slider>
      </v-col>
    </v-row>

    <v-row class="mt-8">
      <v-col cols="3">
        Window Size
        <p class="text--secondary text-sm-caption mb-0">Size of sample window used for calculating statistics such as average or 99% (ms)</p>
      </v-col>
      <v-col cols="9">
        <v-slider
          v-model="window"
          :max="5000"
          :min="10"
          :step="10"
          thumb-label="always"
          hide-details
        ></v-slider>
      </v-col>
    </v-row>

    <v-row class="mt-8">
      <v-col cols="3">
        Adapter
        <p class="text--secondary text-sm-caption mb-0">Adapter used to source GPU telemetry data such as power</p>
      </v-col>
      <v-col cols="9">
        <v-select
          v-model="adapterId"
          :items="adapters"
          item-value="id"
          item-text="name"
          placeholder="Default adapter"
          outlined
          dense
          hide-details
        ></v-select>
      </v-col>
    </v-row>
  
  </v-card>

  </div>
</template>

<script lang="ts">
import Vue from 'vue'
import { Preferences } from '@/store/preferences'
import { Adapter } from '@/core/adapter'
import { Adapters } from '@/store/adapters'
import { Api } from '@/core/api'


export default Vue.extend({
  name: 'MetricProcessing',

  data: () => ({
  }),
  methods: {
  },  
  computed: {
    // v-model enablers
    samplingPeriod: {
      get(): number { return Preferences.preferences.samplingPeriodMs; },
      set(period: number) {
        Preferences.writeAttribute({ attr: 'samplingPeriodMs', val: period });
      },
    },
    offset: {
      get(): number { return Preferences.preferences.metricsOffset; },
      set(metricsOffset: number) {
        Preferences.writeAttribute({ attr: 'metricsOffset', val: metricsOffset });
      },
    },
    window: {
      get(): number { return Preferences.preferences.metricsWindow; },
      set(metricsWindow: number) {
        Preferences.writeAttribute({ attr: 'metricsWindow', val: metricsWindow });
      },
    },
    adapters(): Adapter[] {
      return Adapters.adapters;
    },
    telemetrySamplingPeriod: {
      get(): number { return Preferences.preferences.telemetrySamplingPeriodMs; },
      set(period: number) {
        Preferences.writeAttribute({ attr: 'telemetrySamplingPeriodMs', val: period });
      },
    },
    adapterId: {
      get(): number|null { return Preferences.preferences.adapterId; },
      set(id: number) {
        Preferences.writeAdapterId(id);
      },
    },
  }
});
</script>

<style scoped>
.top-label {
    margin: 0;
    padding: 0;
    height: auto;
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
