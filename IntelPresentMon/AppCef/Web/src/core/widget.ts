// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import { Graph } from './graph'
import { Readout } from './readout'
import { WidgetMetric, RegenerateKeys as RegenerateMetricKeys } from './widget-metric';

export enum WidgetType {
    Graph,
    Readout,
}

export interface Widget {
    key: number;
    metrics: WidgetMetric[];
    widgetType: WidgetType;
}

let nextKey = 0;

export function GenerateKey(): number {
    return nextKey++;
}

export function ResetKeySequence(): void {
    nextKey = 0;
}

export function RegenerateKeys(w: Widget): void {
    for (const m of w.metrics) {
        RegenerateMetricKeys(m);
    }
    w.key = GenerateKey();
}

export function AsGraph(w: Widget): Graph {    
    if (w.widgetType !== WidgetType.Graph) {
        throw new Error(`Widget type [${WidgetType[w.widgetType]}] accessed as Graph`);
    }
    return w as Graph;
}

export function AsReadout(w: Widget): Readout {    
    if (w.widgetType !== WidgetType.Readout) {
        throw new Error(`Widget type [${WidgetType[w.widgetType]}] accessed as Readout`);
    }
    return w as Readout;
}