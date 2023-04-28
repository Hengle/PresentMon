<!-- Copyright (C) 2022 Intel Corporation -->
<!-- SPDX-License-Identifier: MIT -->

<template>
  <v-card color="#242426">
    <v-card-text class="d-flex flex-column align-center" :class="{'pa-0': minimal}">
      <label v-if="label.length > 0" class="v-label pb-3">{{ label }}</label>
      <v-menu v-model="menuActive" top :close-on-content-click="false">
        <template v-slot:activator="{ on }">
          <div class="swatch" :style="swatchStyle" v-on="on"></div>
        </template>
        <v-card>
          <v-card-text class="pa-0">
            <v-color-picker @input="$emit('update', $event)" :value="color" flat></v-color-picker>
          </v-card-text>
        </v-card>
      </v-menu>
    </v-card-text>
  </v-card>
</template>

<script lang="ts">
import Vue from 'vue'
import { RgbaColor, makeCssString } from '@/core/color'

export default Vue.extend({
    name: 'ColorPicker',

    model: {
      prop: 'color',
      event: 'update',
    },
    props: {
        label: { type: String, default: '' },
        color: Object as () => RgbaColor,
        minimal: { type: Boolean, default: false },
    },
    data: () => ({
        menuActive: false,
    }),
    computed: {
        swatchStyle(): { backgroundColor: string } {
            const c = this.color;
            return {
                backgroundColor: makeCssString(c),
            };
        }
    }
});
</script>

<style lang="scss" scoped>
.swatch {
  transition: border-color .1s;
  border: 1px solid #333;
  cursor: pointer;
  min-width: 30px;
  min-height: 18px;
  align-self: stretch;
  border-radius: 4px;
}
.swatch:hover {
  border: 1.5px solid #fff;
}
</style>