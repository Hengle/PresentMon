<!-- Copyright (C) 2022 Intel Corporation -->
<!-- SPDX-License-Identifier: MIT -->

<template>
  <div class="position-wrapper">
    <label v-if="label.length > 0" class="v-label mb-2">{{label}}</label>
    <div class="position-container">
      <div v-for="key in positionKeys" :key="key"
        @click="$emit('input', key)"
        class="position-panel"
        :class="{'position-panel-selected': value == key}"
        v-ripple>
      </div>
    </div>
  </div>
</template>

<script lang="ts">
import Vue from 'vue'
import { OverlayPosition } from '@/core/overlay-position'

export default Vue.extend({
    name: 'OverlayPositioner',

    props: {
        label: { type: String, default: '' },
        value: Number,
    },

    data: () => ({
    }),

    computed: {
        positionKeys(): {readonly tl: number, readonly tr: number, readonly bl: number, readonly br: number} {
            return Object.freeze({
                tl: OverlayPosition.TopLeft,
                tr: OverlayPosition.TopRight,
                bl: OverlayPosition.BottomLeft,
                br: OverlayPosition.BottomRight,
            });
        }
    }
});
</script>

<style scoped>
.position-wrapper {
  display: flex;
  flex-direction: column;
  align-items: center;
  width: 150px;
}
.position-container {
  display: flex;
  flex-direction: row;
  flex-flow: wrap;
  align-self: stretch;
}
.position-panel {
  width: 73px;
  height: 73px;
  margin: 1px;
  background-color: grey;
  cursor: pointer;
  transition: filter .5s;
  filter: brightness(100%);
}
.position-panel-selected {
  background-color: lightskyblue;
}
.position-panel:hover {
  filter: brightness(120%);
}
</style>
