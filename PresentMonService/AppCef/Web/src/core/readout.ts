// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import { Widget, WidgetType, GenerateKey } from './widget'
import { makeDefaultWidgetMetric } from './widget-metric';

export interface Readout extends Widget {
}

export function makeDefaultReadout(metricId: number): Readout {
    return {
        key: GenerateKey(),
        metrics: [makeDefaultWidgetMetric(metricId)],
        widgetType: WidgetType.Readout
    };
}