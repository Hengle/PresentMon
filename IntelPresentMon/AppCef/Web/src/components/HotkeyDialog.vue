<!-- Copyright (C) 2022 Intel Corporation -->
<!-- SPDX-License-Identifier: MIT -->

<template>
  <v-dialog
    v-model="active"
    max-width="680"
    overlay-color="white"
    persistent
  >
    <v-card>
      <v-card-title>
        <span class="text-h6 grey--text">Hotkey for <span class="blue--text text--lighten-3">{{ name }}</span></span>
      </v-card-title>
      <v-card-text>
        <v-row>
          <v-col :cols="6">
            <v-select
              :items="modifierOptions"
              v-model="modifiers_"
              label="Modifiers"
              item-text="text"
              item-value="code"
              outlined
              multiple
              hide-details
            >
            </v-select>
          </v-col>
          <v-col :cols="6">
            <v-select
              :items="keyOptions"
              v-model="key_"
              label="Key"
              item-text="text"
              item-value="code"
              outlined
              clearable
              hide-details
            >
            </v-select>
          </v-col>
        </v-row>
      </v-card-text>      
        <v-card-actions>
          <v-spacer></v-spacer>
          <v-btn
            color="red darken-2"
            text
            @click="clear"
            :disabled="!combinationSet"
          >
            Clear
          </v-btn>
          <v-btn
            color="white darken-3"
            text
            @click="cancel"
          >
            Cancel
          </v-btn>
          <v-btn
            color="primary"
            text
            @click="submit"
            :disabled="!combinationValid"
          >
            OK
          </v-btn>
        </v-card-actions>
    </v-card>
  </v-dialog>
</template>

<script lang="ts">
import Vue from "vue";
import {
  Combination,
  KeyOption,
  ModifierOption,
  KeyCode,
  ModifierCode,
} from "@/core/hotkey";
import { Hotkey } from "@/store/hotkey";

export default Vue.extend({
  name: "HotkeyDialog",

  props: {
    value: Object as () => Combination|null,
    name: String,
  },
  data: () => ({
    active: false,
    key_: null as KeyCode|null,
    modifiers_: [] as ModifierCode[],
  }),
  methods: {
    show() {
      if (this.value !== null) {
        this.key_ = this.value.key;
        this.modifiers_ = [...this.value.modifiers];
      } else {
        this.key_ = null;
        this.modifiers_ = [];
      }
      this.active = true;
    },
    cancel() {
      this.active = false;
    },
    submit() {
      const combination: Combination = {key: <KeyCode>this.key_, modifiers: this.modifiers_};
      this.$emit('input', {key: this.key_, modifiers: this.modifiers_});
      this.active = false;
    },
    clear() {
      this.$emit('input', null);
      this.active = false;
    }
  },
  computed: {
    keyOptions(): KeyOption[] {
      return Hotkey.keyOptions;
    },
    modifierOptions(): ModifierOption[] {
      return Hotkey.modifierOptions;
    },
    // current (temporary) combination is valid
    combinationValid(): boolean {
      return this.key_ !== null;
    },
    // combination is set in Vuex state
    combinationSet(): boolean {
      return this.value !== null;
    },
  },
});
</script>

<style lang="scss" scoped>
</style>

