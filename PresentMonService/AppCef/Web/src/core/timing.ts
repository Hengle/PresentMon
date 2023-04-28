// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
type Timeout = ReturnType<typeof setTimeout>;

export type DelayToken = {
    cancel: () => void;
};

export function dispatchDelayedTask<T>(
    fn: () => T,
    delay: number
): { promise: Promise<T>; token: DelayToken } {
    let timeoutId: Timeout;
    let rejectFn: ((reason?: any) => void) | null = null;
    const promise = new Promise<T>((resolve, reject) => {
        rejectFn = reject;
        timeoutId = setTimeout(() => {
            try {
                resolve(fn());
            } catch (e) {
                reject(e);
            }
        }, delay);
    });

    const token: DelayToken = {
        cancel: () => {
            clearTimeout(timeoutId);
        },
    };

    return { promise, token };
}

export async function delayTask<T>(
    fn: () => T,
    delay: number
) : Promise<T> {
    return dispatchDelayedTask(fn, delay).promise;
}

export async function delayFor(ms: number): Promise<void> {
    return delayTask(() => {}, ms);
}
  