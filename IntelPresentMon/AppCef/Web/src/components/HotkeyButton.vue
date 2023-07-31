<!-- Copyright (C) 2023 Intel Corporation -->
<!-- SPDX-License-Identifier: MIT -->

<template>
<div class="hot-border" @click="openHotkeyDialog">
    <div v-if="hotkeyCombination != null" class="hot-combo">
        <div v-for="m in hotkeyCombination.modifiers" :key="m" class="hot-mod">
            <div class="hot-key">{{ getHotkeyModifierName(m) }}</div>
            <v-icon color="secondary" small>mdi-plus</v-icon>
        </div>
        <div class="hot-key">
            {{ getHotkeyKeyName(hotkeyCombination.key) }}
        </div>
    </div>
    <div v-else class="hot-combo-message">
        Select hotkey chord
    </div>

    <hotkey-dialog
        ref="dialog"
        :name="actionName"
        v-model="hotkeyCombination"
    ></hotkey-dialog>
</div>
</template>

<script lang="ts">
import Vue from 'vue'
import { Action, Binding, Combination, KeyCode, ModifierCode } from '@/core/hotkey';
import { Hotkey } from '@/store/hotkey';
import HotkeyDialog from '@/components/HotkeyDialog.vue'


export default Vue.extend({
  name: 'HotkeyButton',

  props: {
    action: Number,
  },

  components: {
    HotkeyDialog,
  },

  methods: {
    async openHotkeyDialog() {
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
    hotkeyCombination: {
      get(): Combination | null {
        return Hotkey.bindings[Action[this.action]]?.combination;
      },
      async set(updated: Combination | null) {
        if (updated === null) {
          await Hotkey.clearHotkey(this.action);
        } else {
          await Hotkey.bindHotkey({
            action: this.action,
            combination: updated,
          });
        }
      },
    },
    actionName(): string {
      return this.getHotkeyActionName(this.action);
    },
  },
  watch: {
  }
});
</script>

<style lang="scss" scoped>
.hot-border {
  display: flex;
  justify-content: center;
  width: 300px;
  height: 50px;
  padding: 10px 10px;
  border-radius: 5px;
  border: solid var(--v-secondary-lighten1) 2px;
  margin: 0;
  user-select: none;
  cursor: pointer;
  transition: border-color .4s;
  &:hover {
    background-color: hsl(240, 1%, 18%);
    border-color: var(--v-secondary-lighten5);
  }
}
.hot-combo-message {
    color: var(--v-secondary-lighten2)
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
