// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import { Api, FileLocation } from "./api";

var targetBlocklist:Set<string> = new Set();

const targetBlocklistPath = 'TargetBlockList.txt';

function constructSetFromString(text: string): Set<string> {
    const lines: string[] = text.split("\n");
    const lineSet: Set<string> = new Set();
  
    lines.forEach((line) => {
      const trimmedLine = line.trim();
      if (trimmedLine !== "") {
        lineSet.add(trimmedLine);
      }
    });
  
    return lineSet;
  }

export async function LoadBlocklists(): Promise<void> {
    // try loading custom block list from appData
    try {
        const procs = (await Api.loadFile(FileLocation.Data, targetBlocklistPath)).payload;
        targetBlocklist = constructSetFromString(procs);
        return;
    } catch (e) {}
    // try loading default block list from install directory
    try {
        const procs = (await Api.loadFile(FileLocation.Install, targetBlocklistPath)).payload;
        targetBlocklist = constructSetFromString(procs);
        return;
    } catch (e) {}
    // if all else fails, clear the blocklist
    targetBlocklist.clear();
}

export function IsBlocked(process: string): boolean {
    const s = targetBlocklist;
    return targetBlocklist.has(process.toLowerCase());
}

export function GetBlocklist(): string[] {
    return Array.from(targetBlocklist);
}