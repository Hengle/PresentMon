<!-- Copyright (C) 2022 Intel Corporation -->
<!-- SPDX-License-Identifier: MIT -->

<template>
<div class="widget-row sortable-row">
  <div class="grip-wrap">
    <v-icon v-show="!locked" class="sortable-handle">mdi-drag-horizontal-variant</v-icon>
  </div>
  <div class="line-wrap">
    <loadout-line
      v-for="(m, i) in metricIds" :key="lineKeys[i]"
      :widgetIdx="widgetIdx" :lineIdx="i" :widgets="widgets"
      :metrics="metrics" :metricOptions="metricOptions" :locked="locked"
      @delete="handleDelete" @add="handleAdd" @clearMulti="handleClearMulti"
    ></loadout-line>
  </div>
</div>
</template>

<script lang="ts">
import Vue from 'vue'
import { Widget } from '@/core/widget'
import { Metric, MetricOption } from '@/core/metric'
import LoadoutLine from './LoadoutLine.vue'
import { Loadout } from '@/store/loadout'

export default Vue.extend({
    name: 'LoadoutRow',

    components: {
      LoadoutLine,
    },

    props: {
      widgetIdx: {required: true, type: Number},
      widgets: {required: true, type: Array as () => Widget[]},
      metrics: {required: true, type: Array as () => Metric[]},
      metricOptions: {required: true, type: Array as () => MetricOption[]},
      locked: {default: false, type: Boolean}
    },

    data: () => ({
      lineKeys: [] as number[],
      nextKey: 0,
    }),

    mounted() {
      this.lineKeys = Array(this.metricIds.length).fill(0).map((n, i) => i);
      this.nextKey = this.metricIds.length;
    },

    methods: {
      deleteLine(lineIdx: number) {
          Loadout.removeWidgetMetric({index: this.widgetIdx, metricIdIdx: lineIdx});
          this.lineKeys.splice(lineIdx, 1);
      },
      handleDelete(lineIdx: number) {
        if (lineIdx === 0) {
          this.$emit('delete', this.widgetIdx);
        } else {
          this.deleteLine(lineIdx);
        }
      },
      handleAdd() {
        try {
          Loadout.addWidgetMetric({index: this.widgetIdx, metricId: 0});
          this.lineKeys.push(this.nextKey++);
        } catch (e) {}
      },
      handleClearMulti() {
        while (this.lineKeys.length > 1) {
          this.deleteLine(1);
        }
      }
    },

    computed: {
      widget(): Widget {
        return this.widgets[this.widgetIdx];
      },
      metricIds(): Number[] {
        return this.widget.metrics.map(m => m.metricId);
      },
    }
});
</script>

<style scoped lang="scss">
</style>
