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
        <p class="text--secondary text-sm-caption mb-0">Time between metric polling calls (ms)</p>
      </v-col>
      <v-col cols="9">
        <v-slider
          v-model="samplingPeriod"
          :max="250"
          :min="2"
          thumb-label
          hide-details
        ></v-slider>
      </v-col>
    </v-row>

    <v-row class="mt-8">
      <v-col cols="3">
        Offset
        <p class="text--secondary text-sm-caption mb-0">Delay buffer size (enables smooth graphing) (ms)</p>
      </v-col>
      <v-col cols="9">
        <v-slider
          v-model="offset"
          :max="4000"
          :min="0"
          :step="10"
          thumb-label
          hide-details
        ></v-slider>
      </v-col>
    </v-row>

    <v-row class="mt-8">
      <v-col cols="3">
        Window Size
        <p class="text--secondary text-sm-caption mb-0">Size of window used for calculating statistics such as average (ms)</p>
      </v-col>
      <v-col cols="9">
        <v-slider
          v-model="window"
          :max="5000"
          :min="10"
          :step="10"
          thumb-label
          hide-details
        ></v-slider>
      </v-col>
    </v-row>

    <v-row class="mt-8">
      <v-col cols="3">
        Time Range
        <p class="text--secondary text-sm-caption mb-0">Amount of time displayed on graphs (s)</p>
      </v-col>
      <v-col cols="9">
        <v-slider
          v-model="timeRange"
          :max="10"
          :min="0.1"
          :step="0.1"
          thumb-label
          hide-details
        ></v-slider>
      </v-col>
    </v-row>

    <v-row class="mt-8">
      <v-col cols="3">
        Adapter
        <p class="text--secondary text-sm-caption mb-0">Adapter used to source telemetry data such as power</p>
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

    <v-row class="mt-5">
      <v-col cols="3">
        Telemetry Period
        <p class="text--secondary text-sm-caption mb-0">Time between service-side power telemetry polling calls (ms)</p>
      </v-col>
      <v-col cols="9">
        <v-slider
          v-model="telemetrySamplingPeriod"
          :max="500"
          :min="1"
          thumb-label
          hide-details
        ></v-slider>
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
    adapterId: null as number|null,
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
    timeRange: {
      get(): number { return Preferences.preferences.timeRange; },
      set(timeRange: number) {
        Preferences.writeAttribute({ attr: 'timeRange', val: timeRange });
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
  },
  watch: {
    async adapterId(newId: number|null) {
      if (newId !== null) {
        await Api.setAdapter(newId);
      }
    }
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
