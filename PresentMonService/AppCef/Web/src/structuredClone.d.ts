// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
interface WindowOrWorkerGlobalScope {
    structuredClone(value: any, options?: StructuredSerializeOptions): any;
}
declare function structuredClone( value: any, options?: StructuredSerializeOptions): any;