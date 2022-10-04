// Copyright (C) 2019-2021 Intel Corporation
// SPDX-License-Identifier: MIT

#pragma once

#define DEBUG_VERBOSE 0
#if DEBUG_VERBOSE

// Time relative to first event
#define DEBUG_START_TIME_NS     0ull
#define DEBUG_STOP_TIME_NS      UINT64_MAX

#include <stdint.h>

struct PresentEvent; // Can't include PresentMonTraceConsumer.hpp because it includes Debug.hpp (before defining PresentEvent)
struct PMTraceConsumer;
struct EventMetadata;
struct _EVENT_RECORD;
union _LARGE_INTEGER;

// Initialize debug system
void DebugInitialize(_LARGE_INTEGER* firstTimestamp, _LARGE_INTEGER const& timestampFrequency);

// Check if debug is complete
bool DebugDone();

// Print debug information about the handled event
void DebugEvent(PMTraceConsumer* pmConsumer, _EVENT_RECORD* eventRecord, EventMetadata* metadata);

// Call when a new present is created
void DebugCreatePresent(PresentEvent const& p);

// Call before modifying any PresentEvent member
void DebugModifyPresent(PresentEvent const* p);

// Print a time or time range
int DebugPrintTime(uint64_t value);
int DebugPrintTimeDelta(uint64_t value);

#define DebugAssert(condition) while (!(condition)) { printf("ASSERTION FAILED: %s(%d): %s\n", __FILE__, __LINE__, #condition); break; }

#else

#define DebugInitialize(firstTimestamp, timestampFrequency) (void) firstTimestamp, timestampFrequency
#define DebugDone()                                         false
#define DebugEvent(pmConsumer, eventRecord, metadata)       (void) pmConsumer, eventRecord, metadata
#define DebugCreatePresent(p)                               (void) p
#define DebugModifyPresent(p)                               (void) p
#define DebugAssert(condition)                              assert(condition)

#endif
