// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
export interface Metric {
    category: string,
    name: string,
    statType: string,
    units: string,
    index: number,
    className: string
}

// combined category + name w/ array of metricId for each stat variation
export interface MetricOption {
    name: string,
    metricIds: number[],
    className: string,
}