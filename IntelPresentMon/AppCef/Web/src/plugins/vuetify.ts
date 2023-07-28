// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import Vue from 'vue';
import Vuetify from 'vuetify/lib/framework';

Vue.use(Vuetify);

export default new Vuetify({
    theme: {
        dark: true,
        options: { customProperties: true },
    }
});
