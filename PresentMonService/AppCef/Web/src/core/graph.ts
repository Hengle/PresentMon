// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import { RgbaColor } from "./color"
import { Widget, WidgetType, GenerateKey } from './widget'
import { makeDefaultWidgetMetric } from "./widget-metric";

export interface Graph extends Widget {
    height: number;
    vDivs: number;
    hDivs: number;
    showBottomAxis: boolean;
    graphType: {
        name: string,
        range: [number, number],
        rangeRight: [number, number],
        binCount: number,
        countRange: [number, number],
        autoLeft: boolean,
        autoRight: boolean,
        autoCount: boolean,
    },
    gridColor: RgbaColor;
    valueTextColor: RgbaColor;
    dividerColor: RgbaColor;
    backgroundColor: RgbaColor;
    borderColor: RgbaColor;
}

export function makeDefaultGraph(metricId: number): Graph {
    return {
        key: GenerateKey(),
        metrics: [makeDefaultWidgetMetric()],
        widgetType: WidgetType.Graph,
        height: 80,
        vDivs: 4,
        hDivs: 40,
        showBottomAxis: false,
        graphType: {
          name: 'Line',
          range: [0, 150],
          rangeRight: [0, 150],
          binCount: 40,
          countRange: [0, 1000],
          autoLeft: false,
          autoRight: false,
          autoCount: false,
        },
        gridColor: {
          r: 47,
          g: 120,
          b: 190,
          a: 40 / 255,
        },
        valueTextColor: {
          r: 34,
          g: 210,
          b: 250,
          a: 220 / 255,
        },
        dividerColor: {
          r: 57,
          g: 126,
          b: 150,
          a: 220 / 255,
        },
        backgroundColor: {
          r: 0,
          g: 0,
          b: 0,
          a: 0,
        },
        borderColor: {
          r: 0,
          g: 0,
          b: 0,
          a: 0,
        },
    };
}