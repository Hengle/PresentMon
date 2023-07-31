<!-- Copyright (C) 2022 Intel Corporation -->
<!-- SPDX-License-Identifier: MIT -->

<template>
  <div class="page-wrap">
    
  <h2 class="mt-5 ml-5 link-head">
      Capture Configuration
  </h2>

  <v-card class="page-card">

    <v-row class="mt-5">
      <v-col cols="3">
        Summary Stats
        <p class="text--secondary text-sm-caption mb-0">Generate a file that summarizes statistics over the entire capture run</p>
      </v-col>
      <v-col cols="9">
        <v-row>
            <v-col cols="6">
                <v-switch v-model="generateStats" label="Enable"></v-switch>
            </v-col>
        </v-row>
      </v-col>
    </v-row>

    <v-row class="mt-8">
      <v-col cols="3">
        Target Block List
        <p class="text--secondary text-sm-caption mb-0">Filter the target process selection list to remove common non-realtime graphics applications</p>
      </v-col>
      <v-col cols="9">
        <v-row>
            <v-col cols="6">
                <v-switch v-model="enableTargetBlocklist" label="Enable"></v-switch>
            </v-col>
        </v-row>
      </v-col>
    </v-row>

  </v-card>

  </div>
</template>

<script lang="ts">
import Vue from 'vue'
import { Preferences } from '@/store/preferences'

export default Vue.extend({
  name: 'CaptureConfig',

  components: {
  },
  data: () => ({
  }),
  methods: {
  },  
  computed: {
    generateStats: {
      get(): boolean { return Preferences.preferences.generateStats; },
      set(position: boolean) {
        Preferences.writeAttribute({ attr: 'generateStats', val: position });
      },
    },
    enableTargetBlocklist: {
      get(): boolean { return Preferences.preferences.enableTargetBlocklist; },
      set(position: boolean) {
        Preferences.writeAttribute({ attr: 'enableTargetBlocklist', val: position });
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
