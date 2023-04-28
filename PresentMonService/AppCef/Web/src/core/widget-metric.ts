// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import { RgbaColor } from './color'

export enum AxisAffinity {
    Left,
    Right,
}

export interface WidgetMetric {
    key: number;
    metricId: number;
    lineColor: RgbaColor;
    fillColor: RgbaColor;
    axisAffinity: AxisAffinity;
}

let nextKey = 0;

function GenerateKey(): number {
    return nextKey++;
}

export function ResetKeySequence(): void {
    nextKey = 0;
}

export function RegenerateKeys(m: WidgetMetric): void {
    m.key = GenerateKey();
}

export function makeDefaultWidgetMetric(metricId: number = 0): WidgetMetric {
    return {
        key: GenerateKey(),
        metricId,
        lineColor: {
          r: 100,
          g: 255,
          b: 255,
          a: 220 / 255,
        },
        fillColor: {
          r: 57,
          g: 210,
          b: 250,
          a: 25 / 255,
        },
        axisAffinity: AxisAffinity.Left,
    };
}
