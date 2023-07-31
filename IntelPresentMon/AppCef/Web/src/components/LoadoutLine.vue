<!-- Copyright (C) 2022 Intel Corporation -->
<!-- SPDX-License-Identifier: MIT -->

<template>
  <div class="widget-line">
    <div class="widget-cell col-metric">
      <v-autocomplete
        v-model="metricOption"
        :items="metricOptionsFiltered"
        item-text="name"
        :disabled="locked"
        return-object
        outlined
        :dense="!isMaster"
        hide-details
      ></v-autocomplete>
    </div>
    <div class="widget-cell col-stat"> 
      <v-select
        v-model="metricId"
        :items="statVariationMetricIds"
        :disabled="locked || statVariationMetricIds.length < 2"
        outlined
        :dense="!isMaster"
        hide-details
      >
        <template slot="selection" slot-scope="data">
          {{ metrics[data.item].statType }}
        </template>
        <template slot="item" slot-scope="data">
          {{ metrics[data.item].statType }}
        </template>
      </v-select>
    </div>
    <div class="widget-cell col-type">
      <v-select
        v-if="isMaster"
        v-model="widgetType"
        :items="widgetTypeOptions"
        :disabled="locked || widgetTypeOptions.length < 2"
        outlined
        hide-details
      >    
        <template slot="selection" slot-scope="data">
          {{ widgetTypeToString(data.item) }}
        </template>
        <template slot="item" slot-scope="data">
          {{ widgetTypeToString(data.item) }}
        </template>
      </v-select>
    </div>
    <div class="widget-cell col-subtype">
      <v-select
        v-if="isMaster"
        v-model="widgetSubtype"
        :items="widgetSubtypeOptions"
        :disabled="locked || widgetSubtypeOptions.length < 2"
        outlined
        hide-details
      ></v-select>
      <v-switch v-else v-model="axisAffinityRight" label="Right Axis" hide-details dense class="mt-0"></v-switch>
    </div>
    <div class="widget-cell col-line-color">
      <color-picker
        v-if="isGraphWidget"
        v-model="lineColor"
        :minimal="true"
        class="mb-1"
      ></color-picker>
      <color-picker
        v-if="isGraphWidget"
        v-model="fillColor"
        :minimal="true"
      ></color-picker>
    </div>
    <div class="widget-cell col-controls text-right">
      <v-btn v-if="isMaster && !locked && isGraphWidget" :to="{name: 'graph-config', params: {index: widgetIdx}}" class="widget-btn details-btn" x-large icon>
        <v-icon>mdi-cog</v-icon>
      </v-btn>
      <v-btn v-if="!locked && isReadoutWidget" :to="{name: 'readout-config', params: {index: widgetIdx}}" class="widget-btn details-btn" x-large icon>
        <v-icon>mdi-cog</v-icon>
      </v-btn>
      <v-btn v-if="isMaster && !locked && isLineGraphWidget" @click="$emit('add')" class="widget-btn add-line-btn" x-large icon>
        <v-icon>mdi-plus</v-icon>
      </v-btn>
      <v-btn v-if="isMaster && !locked" @click="$emit('delete', lineIdx)" class="widget-btn remove-btn" x-large icon>
        <v-icon>mdi-close</v-icon>
      </v-btn>
      <v-btn v-if="!isMaster && !locked" @click="$emit('delete', lineIdx)" class="widget-line-btn line-btn mr-2" icon>
        <v-icon>mdi-close</v-icon>
      </v-btn>
    </div>
  </div>
</template>

<script lang="ts">
import Vue from 'vue'
import { AsGraph, Widget, WidgetType } from '@/core/widget'
import { Metric, MetricOption } from '@/core/metric'
import { Loadout } from '@/store/loadout'
import { AxisAffinity, makeDefaultWidgetMetric, WidgetMetric }  from '@/core/widget-metric'
import ColorPicker from './ColorPicker.vue'
import { RgbaColor } from '@/core/color'

export default Vue.extend({
    name: 'LoadoutLine',

    components: {
      ColorPicker,
    },

    props: {
      widgetIdx: {required: true, type: Number},
      lineIdx: {required: true, type: Number},
      widgets: {required: true, type: Array as () => Widget[]},
      metrics: {required: true, type: Array as () => Metric[]},
      metricOptions: {required: true, type: Array as () => MetricOption[]},
      locked: {default: false, type: Boolean},
    },

    data: () => ({
      metricOption: null as MetricOption|null,
    }),

    mounted() {
      this.metricOption = this.metricOptions.find(o => o.metricIds.includes(this.metricId)) ?? null;
    },

    methods: {
      widgetTypeToString(t: WidgetType): string {
        return WidgetType[t];
      },
    },

    computed: {
      widget(): Widget {
        return this.widgets[this.widgetIdx];
      },
      statVariationMetricIds(): number[] {
        return this.metricOption?.metricIds ?? [];
      },
      widgetTypeOptions(): WidgetType[] {
        const opts = [WidgetType.Readout];
        if (this.metric.className === 'Numeric') {
          opts.push(WidgetType.Graph);
        }
        return opts;
      },
      metricOptionsFiltered(): MetricOption[] {
        return this.isMaster ?
          this.metricOptions :
          this.metricOptions.filter(o => o.className === 'Numeric');
      },
      widgetSubtypeOptions(): string[] {
        return this.widget.widgetType === WidgetType.Graph ? ['Line', 'Histogram'] : [];
      },
      metric(): Metric {
        return this.metrics[this.metricId];
      },
      widgetMetric(): WidgetMetric {
        return this.widget.metrics[this.lineIdx];
      },
      widgetType: {        
        set(type: WidgetType) {
          if (type !== this.widget.widgetType) {
            Loadout.resetWidgetAs({index: this.widgetIdx, type});
          }
        },
        get(): WidgetType {
          return this.widget.widgetType;
        }
      },
      widgetSubtype: {        
        set(val: string) {
          if (this.widgetType === WidgetType.Graph) {
            Loadout.setGraphTypeAttribute({index: this.widgetIdx, attr: 'name', val})
          }
        },
        get(): string {
          return this.widgetType === WidgetType.Graph ? AsGraph(this.widget).graphType.name : '';
        }
      },
      isMaster(): boolean {
        return this.lineIdx === 0;
      },
      isGraphWidget(): boolean {
        return this.widgetType == WidgetType.Graph;
      },
      isLineGraphWidget(): boolean {
        return this.isGraphWidget && AsGraph(this.widget).graphType.name === 'Line';
      },
      isReadoutWidget(): boolean {
        return this.widgetType == WidgetType.Readout;
      },
      // spec bindings
      metricId: {
        get(): number {
          return this.widgetMetric.metricId;
        },
        set(metricId: number) {
          const metric: WidgetMetric = {...this.widgetMetric, metricId};
          Loadout.setWidgetMetric({index: this.widgetIdx, metricIdx: this.lineIdx, metric});
        }
      },
      lineColor: {
        get(): RgbaColor {
          return this.widgetMetric.lineColor;
        },
        set(lineColor: RgbaColor) {
          const metric: WidgetMetric = {...this.widgetMetric, lineColor};
          Loadout.setWidgetMetric({index: this.widgetIdx, metricIdx: this.lineIdx, metric});
        }
      },
      fillColor: {
        get(): RgbaColor {
          return this.widgetMetric.fillColor;
        },
        set(fillColor: RgbaColor) {
          const metric: WidgetMetric = {...this.widgetMetric, fillColor};
          Loadout.setWidgetMetric({index: this.widgetIdx, metricIdx: this.lineIdx, metric});
        }
      },
      axisAffinityRight: {
        get(): boolean {
          return this.widgetMetric.axisAffinity === AxisAffinity.Right;
        },
        set(affinityRight: boolean) {
          const axisAffinity = affinityRight ? AxisAffinity.Right : AxisAffinity.Left;
          const metric: WidgetMetric = {...this.widgetMetric, axisAffinity};
          Loadout.setWidgetMetric({index: this.widgetIdx, metricIdx: this.lineIdx, metric});
        }
      }
    },

    watch: {
      metricOption(newOption: MetricOption) {
        // if a new option is selected, reset to first available stat-id
        // but don't reset if current id is in the set (happens when loading file)
        if (!newOption.metricIds.includes(this.metricId)) {
          this.metricId = newOption.metricIds[0];
        }
        if (this.metric.className !== 'Numeric') {
          this.widgetType = WidgetType.Readout;
        }
      },
      widgetSubtype(newSubtype: string) {
        if (newSubtype !== 'Line') {
          this.$emit('clearMulti');
        }
      }
    }
});
</script>

<style scoped lang="scss">
</style>
