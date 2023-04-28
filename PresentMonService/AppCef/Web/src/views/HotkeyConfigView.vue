<!-- Copyright (C) 2022 Intel Corporation -->
<!-- SPDX-License-Identifier: MIT -->

<template>
  <div class="page-wrap">
    
  <h2 class="mt-5 ml-5 link-head">
      Hotkey Configuration
  </h2>
  
  <v-card class="page-card">    
    <div v-for="b in hotkeyBindings" :key="b.action" class="hot-item" @click="openHotkeyDialog(b.action)" v-ripple>
      <div class="hot-label text--primary">{{ getHotkeyActionName(b.action) }}</div>
      <div v-if="b.combination !== null" class="hot-combo">
        <div v-for="m in b.combination.modifiers" :key="m" class="hot-mod">
          <div class="hot-key">{{ getHotkeyModifierName(m) }}</div>
          <v-icon color="secondary" small>mdi-plus</v-icon>
        </div>
        <div class="hot-key">
          {{ getHotkeyKeyName(b.combination.key) }}
        </div>
      </div>
    </div>
  </v-card>

  <hotkey-dialog
    ref="dialog"
    :name="hotkeyActiveName"
    v-model="hotkeyActiveCombination"
  ></hotkey-dialog>

  </div>
</template>

<script lang="ts">
import Vue from 'vue'
import { Action, Binding, Combination, KeyCode, ModifierCode } from '@/core/hotkey';
import { Hotkey } from '@/store/hotkey';
import HotkeyDialog from '@/components/HotkeyDialog.vue'


export default Vue.extend({
  name: 'HotkeyConfig',

  components: {
    HotkeyDialog,
  },

  data: () => ({
    activeAction: 0,
  }),

  methods: {
    async openHotkeyDialog(action: Action) {
      this.activeAction = action;
      // we need to delay showing of the dialog until the computed properties have caught up
      // otherwise the wrong combination will be captured internally by the dialog state
      await this.$nextTick();
      (this.$refs.dialog as any).show();
    },
    getHotkeyActionName(action: Action): string {
      const key = Action[action];
      return key.match(/([A-Z]?[^A-Z]*)/g)?.slice(0, -1).join(" ") ?? "";
    },
    getHotkeyModifierName(mod: ModifierCode): string {
      return Hotkey.modifierOptions.find(mo => mo.code === mod)?.text ?? '???';
    },
    getHotkeyKeyName(key: KeyCode): string {
      return Hotkey.keyOptions.find(ko => ko.code === key)?.text ?? '???';
    },
  },  
  computed: {
    hotkeyBindings(): Binding[] {
      return Object.values(Hotkey.bindings);
    },
    hotkeyActiveCombination: {
      get(): Combination | null {
        return Hotkey.bindings[Action[this.activeAction]].combination;
      },
      async set(updated: Combination | null) {
        if (updated === null) {
          await Hotkey.clearHotkey(this.activeAction);
        } else {
          await Hotkey.bindHotkey({
            action: this.activeAction,
            combination: updated,
          });
        }
      },
    },
    hotkeyActiveName(): string {
      return this.getHotkeyActionName(this.activeAction);
    },
  },
  watch: {
  }
});
</script>

<style lang="scss" scoped>
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

.hot-item {
  display: flex;
  justify-content: space-between;
  padding: 15px 10px;
  cursor: pointer;
  &:hover {
    background-color: hsl(240, 1%, 18%);
  }
}
.hot-label {
  font-size: 16px;
  font-weight: 300;
}
.hot-combo {
  display: flex;
  color: var(--v-primary-base);
  font-size: 12px
}
.hot-mod {
  display: flex;
}
.hot-key {
  border: solid var(--v-primary-base) 1.5px;
  padding: 2px 7px;
  border-radius: 5px;
  min-width: 30px;
  text-align: center;
}
</style>
