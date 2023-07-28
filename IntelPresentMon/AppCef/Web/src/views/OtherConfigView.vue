<!-- Copyright (C) 2022 Intel Corporation -->
<!-- SPDX-License-Identifier: MIT -->

<template>
  <div class="page-wrap">
    
  <h2 class="mt-5 ml-5 link-head">
      Other Configuration
  </h2>

  <v-card class="page-card">

    <v-row class="mt-5">
      <v-col cols="3">
        Reset Preferences
        <p class="text--secondary text-sm-caption mb-0">Reset all preferences to their defaults</p>
      </v-col>
      <v-col cols="9">
        <v-row>
            <v-col cols="6">
              <v-btn class="mt-3" color="primary" @click="dialog = true">Reset</v-btn>
      
              <v-dialog v-model="dialog" max-width="500px">
                <v-card>
                  <v-card-title class="headline">Reset Preferences</v-card-title>

                  <v-card-text>
                    Preferences will be set to their default values. All current settings will be lost. Continue?
                  </v-card-text>

                  <v-card-actions>
                    <v-spacer></v-spacer>

                    <v-btn color="primary" @click="reset">Reset</v-btn>
                    
                    <v-btn color="grey darken-1" text @click="dialog = false">Cancel</v-btn>
                  </v-card-actions>
                </v-card>
              </v-dialog>
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
import { Hotkey } from '@/store/hotkey'

export default Vue.extend({
  name: 'OtherConfig',

  components: {
  },
  data: () => ({
    dialog: false,
  }),
  methods: {
    async reset(): Promise<void> {
      Preferences.resetPreferences();
      await Hotkey.bindDefaults();
      this.dialog = false;
    },
  },
  computed: {
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
