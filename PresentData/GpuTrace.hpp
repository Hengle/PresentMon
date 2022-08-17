// Copyright (C) 2021-2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once

#include <stdint.h>
#include <unordered_map>

#include "etw/Microsoft_Windows_DxgKrnl.h"

struct PresentEvent;
struct PMTraceConsumer;

enum INTCGPUTimer {
    INTC_GPU_TIMER_SYNC_TYPE_WAIT_SYNC_OBJECT_CPU,
    INTC_GPU_TIMER_SYNC_TYPE_POLL_ON_QUERY_GET_DATA,
    INTC_GPU_TIMER_WAIT_FOR_COMPILATION_ON_DRAW,
    INTC_GPU_TIMER_WAIT_FOR_COMPILATION_ON_CREATE,
    INTC_GPU_TIMER_COUNT
};

enum ResidencyEventTypes {
    DXGK_RESIDENCY_EVENT_MAKE_RESIDENT,
    DXGK_RESIDENCY_EVENT_PAGING_QUEUE_PACKET,
    DXGK_RESIDENCY_EVENT_COUNT
};

class GpuTrace {
    // PacketTrace is the execution information for each process' frame.
    struct PacketTrace {
        uint64_t mFirstPacketTime;         // QPC when the first packet started for the current frame
        uint64_t mLastPacketTime;          // QPC when the last packet completed for the current frame
        uint64_t mAccumulatedPacketTime;   // QPC duration while at least one packet was running during the current frame
        uint64_t mRunningPacketStartTime;  // QPC when the oldest, currently-running packet started on any node
        uint32_t mRunningPacketCount;      // Number of currently-running packets on any node
    };

    // Node is information about a particular GPU parallel node, including any
    // packets currently running/queued to it.
    struct Node {
        enum { MAX_QUEUE_SIZE = 9 };               // MAX_QUEUE_SIZE=9 for 2 full cachelines (one is not enough).
        PacketTrace* mPacketTrace[MAX_QUEUE_SIZE]; // Frame trace for enqueued packets
        uint32_t mSequenceId[MAX_QUEUE_SIZE];      // Sequence IDs for enqueued packets
        uint32_t mQueueIndex;                      // Index into mPacketTrace and mSequenceId for currently-running packet
        uint32_t mQueueCount;                      // Number of enqueued packets
        bool mIsVideo;
        bool mIsVideoDecode;
    };

    // Context is a process' gpu context, mapping a PacketTrace to a
    // particular Node.
    struct Context {
        PacketTrace* mPacketTrace;
        Node* mNode;
        uint64_t mParentDxgHwQueue;
        bool mIsVideoEncoderForCloudStreamingApp;
    };

    // State for tracking GPU execution per-frame, per-process
    struct ProcessFrameInfo {
        // Depending on mTrackGPUVideo, we may track video engines separately
        PacketTrace mVideoEngines;
        PacketTrace mOtherEngines;

        // INTC-internal state:
        struct {
            uint64_t mStartTime;            // QPC of the start event for this timer, or 0 if no start event
            uint64_t mAccumulatedTime;      // QPC duration of all processed timer durations
            uint32_t mStartCount;           // The number of timers started
        } mINTCTimers[INTC_GPU_TIMER_COUNT];

        struct {
            uint64_t mStartTime;            // QPC of the start of the latest operation.
            uint64_t mAccumulatedTime;      // QPC duration of all completed operations.
        } mResidencyTimers[DXGK_RESIDENCY_EVENT_COUNT];
    };

    std::unordered_map<uint64_t, std::unordered_map<uint32_t, Node> > mNodes;   // pDxgAdapter -> NodeOrdinal -> Node
    std::unordered_map<uint64_t, uint64_t> mDevices;                            // hDevice -> pDxgAdapter
    std::unordered_map<uint64_t, Context> mContexts;                            // hContext -> Context
    std::unordered_map<uint32_t, ProcessFrameInfo> mProcessFrameInfo;           // ProcessID -> ProcessFrameInfo
    std::unordered_map<uint64_t, uint32_t> mPagingSequenceIds;                  // SequenceID -> ProcessID

    // The parent trace consumer
    PMTraceConsumer* mPMConsumer;

    // The process id of the first identified cloud streaming process.
    uint32_t mCloudStreamingProcessId;

    void SetContextProcessId(Context* context, uint32_t processId);

    void StartPacket(PacketTrace* packetTrace, uint64_t timestamp) const;
    void CompletePacket(PacketTrace* packetTrace, uint64_t timestamp) const;

    void EnqueueWork(Context* context, uint32_t sequenceId, uint64_t timestamp);
    bool CompleteWork(Context* context, uint32_t sequenceId, uint64_t timestamp);

    #if DEBUG_VERBOSE
    uint32_t LookupPacketTraceProcessId(PacketTrace* packetTrace) const;
    void DebugPrintRunningContexts() const;
    #endif

public:
    explicit GpuTrace(PMTraceConsumer* pmConsumer);

    void RegisterDevice(uint64_t hDevice, uint64_t pDxgAdapter);
    void UnregisterDevice(uint64_t hDevice);

    void RegisterContext(uint64_t hContext, uint64_t hDevice, uint32_t nodeOrdinal, uint32_t processId);
    void RegisterHwQueueContext(uint64_t hContext, uint64_t parentDxgHwQueue);
    void UnregisterContext(uint64_t hContext);

    void SetEngineType(uint64_t pDxgAdapter, uint32_t nodeOrdinal, Microsoft_Windows_DxgKrnl::DXGK_ENGINE engineType);

    void EnqueueQueuePacket(uint32_t processId, uint64_t hContext, uint32_t sequenceId, uint64_t timestamp);
    void CompleteQueuePacket(uint64_t hContext, uint32_t sequenceId, uint64_t timestamp);

    void EnqueueDmaPacket(uint64_t hContext, uint32_t sequenceId, uint64_t timestamp);
    uint32_t CompleteDmaPacket(uint64_t hContext, uint32_t sequenceId, uint64_t timestamp);

    void StartINTCTimer(INTCGPUTimer timer, uint32_t processId, uint64_t timestamp);
    void StopINTCTimer(INTCGPUTimer timer, uint32_t processId, uint64_t timestamp);

    void StartMakeResident(uint32_t processId, uint64_t timestamp);
    void StopMakeResident(uint32_t processId, uint64_t timestamp);

    void RegisterPagingQueuePacket(uint32_t processId, uint64_t sequenceId);
    void StartPagingQueuePacket(uint64_t sequenceId, uint64_t timestamp);
    void StopPagingQueuePacket(uint64_t sequenceId, uint64_t timestamp);

    void CompleteFrame(PresentEvent* pEvent, uint64_t timestamp);
};
