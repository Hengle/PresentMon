// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
type Timeout = ReturnType<typeof setTimeout>;

export type DelayToken = {
    cancel: () => void;
};

export interface DelayedTask<T> {
    promise: Promise<T>;
    token: DelayToken;
};

export class CancelationToken {};

export async function awaitDelayedPromise<T>(promise: Promise<T|null>): Promise<T|null> {
    try {
        return await promise;
    }
    catch (e) {
        if (e instanceof CancelationToken) {
            return null;
        }
        throw e;
    }
}

export function dispatchDelayedTask<T>(
    fn: () => T,
    delay: number
): DelayedTask<T> {
    let timeoutId: Timeout;
    let rejectFn: ((reason: CancelationToken) => void) | null = null;
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
            if (rejectFn !== null) {
                rejectFn(new CancelationToken);
            }
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
  