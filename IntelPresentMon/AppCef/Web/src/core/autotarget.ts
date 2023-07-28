// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import { dispatchDelayedTask, DelayedTask, awaitDelayedPromise } from "./timing";
import { Process } from "./process";
import { Api } from "./api"
import { GetBlocklist } from "./block-list";
import { Preferences } from "@/store/preferences";

var utilizationPollTask: DelayedTask<Promise<Process|null>>|null = null;

export async function launchAutotargetting(): Promise<void> {
    const top = await doGpuUtilizationTopPolling(250);
    if (top !== null) {
        Preferences.setPid(top.pid);
    }
}

export async function doGpuUtilizationTopPolling(specifiedDelayMs: number): Promise<Process|null> {
    // initial poll without any delay
    let delayMs = 0;
    // loop polling while result not received
    while (true) {
        utilizationPollTask = dispatchDelayedTask(async () => {
            return await Api.getTopGpuProcess(GetBlocklist());        
        }, delayMs);
        const result = await awaitDelayedPromise(utilizationPollTask.promise);
        if (result !== null) {
            utilizationPollTask = null;
            return result;
        }
        if (utilizationPollTask === null) {
            return result;
        }
        // subsequent polls use the passed in delay
        delayMs = specifiedDelayMs
    }
}

export function cancelTopPolling(): void {
    if (utilizationPollTask !== null) {
        utilizationPollTask.token.cancel();
        utilizationPollTask = null;
    }
}