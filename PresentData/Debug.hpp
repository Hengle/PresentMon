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
struct EventMetadata;
struct _EVENT_RECORD;

// Initialize debug system
void DebugInitialize(LARGE_INTEGER* firstTimestamp, LARGE_INTEGER timestampFrequency);

// Check if debug is complete
bool DebugDone();

// Print debug information about the handled event
void DebugEvent(_EVENT_RECORD* eventRecord, EventMetadata* metadata);

// Call when a new present is created
void DebugCreatePresent(PresentEvent const& p);

// Call before modifying any PresentEvent member
void DebugModifyPresent(PresentEvent const& p);

// Call when a present is lost
void DebugLostPresent(PresentEvent const& p);

// Call when 
void DebugFirstDmaStart();

// Call when 
void DebugDmaAccumulated(uint64_t currentTime, uint64_t addedTime);

#else

#define DebugInitialize(firstTimestamp, timestampFrequency) (void) firstTimestamp, timestampFrequency
#define DebugDone()                                         false
#define DebugEvent(eventRecord, metadata)                   (void) eventRecord, metadata
#define DebugCreatePresent(p)                               (void) p
#define DebugModifyPresent(p)                               (void) p
#define DebugLostPresent(p)                                 (void) p
#define DebugFirstDmaStart()
#define DebugDmaAccumulated(currentTime, addedTime)         (void) currentTime, addedTime

#endif
