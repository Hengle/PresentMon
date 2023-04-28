<!-- Copyright (C) 2022 Intel Corporation -->
<!-- SPDX-License-Identifier: MIT -->

<template>
  <v-row>       
    <v-col cols="6">
      <v-select
        :items="categories"
        v-model="components.category"
        label="Category"
        @change="resetNameSelection"
        outlined
        dense
        hide-details
      ></v-select>
    </v-col>
        
    <v-col cols="6">
      <v-select
        :items="names"
        v-model="components.name"
        label="Metric"
        @change="resetStatTypeSelection"
        outlined
        dense
        hide-details
      ></v-select>
    </v-col>
    
    <v-col cols="6">
      <v-select
        :disabled="!selectedNameHasStats"
        :items="statTypes"
        v-model="components.statType"
        label="Statistic"
        outlined
        dense
        hide-details
      ></v-select>
    </v-col>
  </v-row>
</template>

<script lang="ts">
import Vue from 'vue'
import { Metrics } from '@/store/metrics'

interface MetricSelectionComponents {
  category: string;
  name: string;
  statType: string;
}

export default Vue.extend({
  name: 'MetricSelect',
  
  model: {
    prop: 'metricIndex',
    event: 'update:metricIndex',
  },
  props: {
    metricIndex: {required: true, type: Number},
    isNumericOnly: {default: false, type: Boolean}
  },
  data: () => ({
    components: {
      category: '',
      name: '',
      statType: '',
    } as MetricSelectionComponents,
  }),
  async created() {
    this.updateComponents(this.metricIndex);
  },
  methods: {
    // called when category is changed, resetting name and statType selections to available defaults
    resetNameSelection() {
        this.components.name = this.names[0];
        this.resetStatTypeSelection();
    },
    // called when category or name is changed, resetting statType selection to available default
    resetStatTypeSelection() {
      if (this.statTypes.length > 1) {
        this.components.statType = this.statTypes[0];
      } else {
        this.components.statType = '';
      }
    },
    // update components based on metric index from parent
    updateComponents(index: number) {
      this.components.category = Metrics.metrics[index].category;
      this.components.name = Metrics.metrics[index].name;
      this.components.statType = Metrics.metrics[index].statType;
    },
  },  
  computed: {
    // all metric categories 
    categories(): string[] {
      return Metrics.categories;
    },
    // all metric names under selected category
    names(): string[] {
      if (this.isNumericOnly) {
        return Array.from(new Set(Metrics.metrics.filter(m => m.category === this.components.category && m.className == 'Numeric').map(m => m.name)));
      } else {
        return Array.from(new Set(Metrics.metrics.filter(m => m.category === this.components.category).map(m => m.name)));
      }
    },
    // all stat types available for selected metric name (either full set or empty)
    statTypes(): string[] {
      return Array.from(new Set(Metrics.metrics.filter(m => m.category === this.components.category && m.name === this.components.name).map(m => m.statType)));
    },
    // flag showing if selected metric (name) does not have stats
    selectedNameHasStats(): boolean {
      return this.statTypes[0] !== '';
    }
  },
  watch: {
    metricIndex(index: number) {
      this.updateComponents(index);
    },
    components: {
      handler(selection: MetricSelectionComponents) {
        const newIndex = Metrics.metrics.findIndex(m =>
          m.category === selection.category &&
          m.name === selection.name &&
          m.statType === selection.statType
        );
        this.$emit('update:metricIndex', newIndex);
      },
      deep: true
    }
  }
});
</script>

<style lang="scss" scoped>
</style>

