// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import { Preferences } from "./preferences";
import { Widget } from "./widget";

export interface Spec {
  pid: number|null;
  preferences: Preferences;
  widgets: Widget[];
}
