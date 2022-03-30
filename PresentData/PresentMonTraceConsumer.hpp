// Copyright (C) 2017-2022 Intel Corporation
// SPDX-License-Identifier: MIT

#pragma once

#define NOMINMAX

#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>
#include <set>
#include <windows.h>
#include <evntcons.h> // must include after windows.h

#include "Debug.hpp"
#include "TraceConsumer.hpp"

enum class PresentMode
{
    Unknown,
    Hardware_Legacy_Flip,
    Hardware_Legacy_Copy_To_Front_Buffer,
    /* Not detected:
    Hardware_Direct_Flip,
    */
    Hardware_Independent_Flip,
    Composed_Flip,
    Composed_Copy_GPU_GDI,
    Composed_Copy_CPU_GDI,
    Composed_Composition_Atlas,
    Hardware_Composed_Independent_Flip,
};

enum class PresentResult
{
    Unknown, Presented, Discarded, Error
};

enum class Runtime
{
    DXGI, D3D9, Other
};

// A ProcessEvent occurs whenever a Process starts or stops.
struct ProcessEvent {
    std::string ImageFileName;
    uint64_t QpcTime;
    uint32_t ProcessId;
    bool IsStartEvent;
};

struct PresentEvent {
    uint64_t QpcTime;       // QPC value of the first event related to the Present (D3D9, DXGI, or DXGK Present_Start)
    uint32_t ProcessId;     // ID of the process that presented
    uint32_t ThreadId;      // ID of the thread that presented
    uint64_t TimeTaken;     // QPC duration between runtime present start and end
    uint64_t GPUStartTime;  // QPC value when the frame's first DMA packet started
    uint64_t ReadyTime;     // QPC value when the frame's last DMA packet completed
    uint64_t GPUDuration;   // QPC duration during which a frame's DMA packet was running on any node
    uint64_t ScreenTime;    // QPC value when the present was displayed on screen

    // Extra present parameters obtained through DXGI or D3D9 present
    uint64_t SwapChainAddress;
    int32_t SyncInterval;
    uint32_t PresentFlags;

    // Keys used to index into PMTraceConsumer's tracking data structures:
    uint64_t DxgKrnlHContext;           // Key for mBltsByDxgContext
    uint64_t Win32KPresentCount;        // Combine with CompositionSurfaceLuid and Win32KBindId as key into mWin32KPresentHistoryTokens
    uint64_t Win32KBindId;              // Combine with CompositionSurfaceLuid and Win32KPresentCount as key into mWin32KPresentHistoryTokens
    uint64_t LegacyBlitTokenData;       // Key for mPresentsByLegacyBlitToken
    uint64_t Hwnd;
    uint64_t TokenPtr;
    uint64_t CompositionSurfaceLuid;
    uint32_t mAllPresentsTrackingIndex; // Index in PMTraceConsumer's mAllPresents.
    uint32_t QueueSubmitSequence;       // Submit sequence for the Present packet

    // Properties deduced by watching events through present pipeline
    uint32_t DestWidth;
    uint32_t DestHeight;
    uint32_t DriverBatchThreadId;
    Runtime Runtime;
    PresentMode PresentMode;
    PresentResult FinalState;
    bool SupportsTearing;
    bool MMIO;
    bool SeenDxgkPresent;
    bool SeenWin32KEvents;
    bool DwmNotified;
    bool SeenInFrameEvent;      // This present has gotten a Win32k TokenStateChanged event into InFrame state
    bool CompletionIsDeferred;  // A FinalState has been determined, but not all expected events have been observed yet
    bool IsCompleted;           // All expected events have been observed
    bool IsLost;                // This PresentEvent was found in an unexpected state or is too old

    // We need a signal to prevent us from looking fruitlessly through the WaitingForDwm list
    bool PresentInDwmWaitingStruct;

    // Additional transient tracking state
    std::deque<std::shared_ptr<PresentEvent>> DependentPresents;

    // Track the path the present took through the PresentMon analysis.
    #ifdef TRACK_PRESENT_PATHS
    uint64_t AnalysisPath;
    #endif

    // Give every present a unique id for debugging.
    #if DEBUG_VERBOSE
    uint64_t Id;
    #endif

    PresentEvent(EVENT_HEADER const& hdr, ::Runtime runtime);

private:
    PresentEvent(PresentEvent const& copy); // dne
};

// A high-level description of the sequence of events for each present type,
// ignoring runtime end:
//
// Hardware Legacy Flip:
//   Runtime PresentStart -> Flip (by thread/process, for classification) -> QueueSubmit (by thread, for submit sequence) ->
//   MMIOFlip (by submit sequence, for ready time and immediate flags) [-> VSyncDPC (by submit sequence, for screen time)]
//
// Composed Flip (FLIP_SEQUENTIAL, FLIP_DISCARD, FlipEx):
//   Runtime PresentStart -> TokenCompositionSurfaceObject (by thread/process, for classification and token key) ->
//   PresentHistoryDetailed (by thread, for token ptr) -> QueueSubmit (by thread, for submit sequence) ->
//   DxgKrnl_PresentHistory (by token ptr, for ready time) and TokenStateChanged (by token key, for discard status and intent to present) ->
//   DWM Present (consumes most recent present per hWnd, marks DWM thread ID) ->
//   A fullscreen present is issued by DWM, and when it completes, this present is on screen
//
// Hardware Direct Flip:
//   N/A, not currently uniquely detectable (follows the same path as composed flip)
//
// Hardware Independent Flip:
//   Follows composed flip, TokenStateChanged indicates IndependentFlip -> MMIOFlip (by submit sequence, for immediate flags)
//   [-> VSyncDPC or HSyncDPC (by submit sequence, for screen time)]
//
// Hardware Composed Independent Flip:
//   Identical to hardware independent flip, but VSyncDPCMPO and HSyncDPCMPO contains more than one valid plane and SubmitSequence.
//
// Composed Copy with GPU GDI (a.k.a. Win7 Blit):
//   Runtime PresentStart -> DxgKrnl_Blit (by thread/process, for classification) ->
//   DxgKrnl_PresentHistoryDetailed (by thread, for token ptr and classification) -> DxgKrnl_Present (by thread, for hWnd) ->
//   DxgKrnl_PresentHistory (by token ptr, for ready time) -> DWM UpdateWindow (by hWnd, marks hWnd active for composition) ->
//   DWM Present (consumes most recent present per hWnd, marks DWM thread ID) ->
//   A fullscreen present is issued by DWM, and when it completes, this present is on screen
//
// Hardware Copy to front buffer:
//   Runtime PresentStart -> DxgKrnl_Blit (by thread/process, for classification) -> QueueSubmit (by thread, for submit sequence) ->
//   QueueComplete (by submit sequence, indicates ready and screen time)
//   Distinction between FS and windowed blt is done by LACK of other events
//
// Composed Copy with CPU GDI (a.k.a. Vista Blit):
//   Runtime PresentStart -> DxgKrnl_Blit (by thread/process, for classification) ->
//   SubmitPresentHistory (by thread, for token ptr, legacy blit token, and classification) ->
//   DxgKrnl_PresentHistory (by token ptr, for ready time) ->
//   DWM FlipChain (by legacy blit token, for hWnd and marks hWnd active for composition) ->
//   Follows the Windowed_Blit path for tracking to screen
//
// Composed Composition Atlas (DirectComposition):
//   SubmitPresentHistory (use model field for classification, get token ptr) -> DxgKrnl_PresentHistory (by token ptr) ->
//   Assume DWM will compose this buffer on next present (missing InFrame event), follow windowed blit paths to screen time

struct PMTraceConsumer
{
    PMTraceConsumer();

    EventMetadata mMetadata;

    bool mFilteredEvents = false;       // Whether the trace session was configured to filter non-PresentMon events
    bool mFilteredProcessIds = false;   // Whether to filter presents to specific processes
    bool mTrackDisplay = true;          // Whether the analysis should track presents to display
    bool mTrackGPU = false;             // Whether the analysis should track GPU work

    // Whether we've completed any presents yet.  This is used to indicate that
    // all the necessary providers have started and it's safe to start tracking
    // presents.
    bool mHasCompletedAPresent = false;

    // Store completed and lost presents until the consumer thread removes them
    // using Dequeue*PresentEvents().
    //
    // Completed presents are those that have seen all their expected events,
    // based on the presentation path used.
    //
    // Lost presents are not yet completed, but have been waiting for
    // completion for a long time or were found in an unexpected state.  This
    // was most likely caused by a missed ETW event.

    std::mutex mPresentEventMutex;
    std::vector<std::shared_ptr<PresentEvent>> mCompletePresentEvents;

    std::mutex mLostPresentEventMutex;
    std::vector<std::shared_ptr<PresentEvent>> mLostPresentEvents;

    // If a present has been determined to be either discarded or displayed,
    // but it has not yet seen all of its expected events, it is removed from
    // the tracking structures and placed into the DeferredCompletions list
    // with CompletionIsDeferred set.  These are not completed until a
    // case-dependent number of Presents() have occurred from the same process.

    // (PresentEvent, NumPresentStopsToWaitFor)
    using DeferredCompletions = std::vector<std::pair<std::shared_ptr<PresentEvent>, uint32_t>>;

    // [Process ID] => DeferredCompletions
    std::unordered_map<uint32_t, DeferredCompletions> mDeferredCompletions;

    // Process events
    std::mutex mProcessEventMutex;
    std::vector<ProcessEvent> mProcessEvents;


    // These data structures store in-progress presents (i.e., ones that are
    // still being processed by the system and are not yet completed).
    //
    // mPresentByThreadId stores the in-progress present that was last operated
    // on by each thread for event sequences that are known to execute on the
    // same thread. Its members' lifetime should track the lifetime of the
    // runtime present API as much as possible. Only one present will be going
    // through this sequence on any particular thread at a time.
    //
    // mPresentsByProcess stores each process' in-progress presents in the
    // order that they were created by PresentMon.  This is used to look up
    // presents across systems running on different threads
    // (DXGI/D3D/DXGK/Win32) including batched presents, and so that we know to
    // discard all older presents when a newer one is completed.
    //
    // mPresentsBySubmitSequence is used to lookup the active present
    // associated with a present queue packet.
    //
    // All flip model presents (windowed flip, dFlip, iFlip) are uniquely
    // identifyed by a Win32K present history token (composition surface,
    // present count, and bind id).  mWin32KPresentHistoryTokens stores the
    // mapping from this token to in-progress present to optimize lookups
    // during Win32K events.

    // Circular buffer of all Presents, older presents will be considered lost if not completed by the next visit.
    unsigned int mAllPresentsNextIndex = 0;
    std::vector<std::shared_ptr<PresentEvent>> mAllPresents;

    // [thread id]
    std::map<uint32_t, std::shared_ptr<PresentEvent>> mPresentByThreadId;

    // [process id][qpc time]
    using OrderedPresents = std::map<uint64_t, std::shared_ptr<PresentEvent>>;
    std::map<uint32_t, OrderedPresents> mPresentsByProcess;

    // Maps from queue packet submit sequence
    // Used for Flip -> MMIOFlip -> VSyncDPC for FS, for PresentHistoryToken -> MMIOFlip -> VSyncDPC for iFlip,
    // and for Blit Submission -> Blit completion for FS Blit

    // [submit sequence]
    std::map<uint32_t, std::shared_ptr<PresentEvent>> mPresentsBySubmitSequence;

    // [(composition surface pointer, present count, bind id)]
    using Win32KPresentHistoryTokenKey = std::tuple<uint64_t, uint64_t, uint64_t>;
    std::map<Win32KPresentHistoryTokenKey, std::shared_ptr<PresentEvent>> mWin32KPresentHistoryTokens;


    // DxgKrnl present history tokens are uniquely identified and used for all
    // types of windowed presents to track a "ready" time.
    //
    // The token is assigned to the last present on the same thread, on
    // non-REDIRECTED_GDI model DxgKrnl_Event_PresentHistoryDetailed or
    // DxgKrnl_Event_SubmitPresentHistory events.
    //
    // We stop tracking the token on a DxgKrnl_Event_PropagatePresentHistory
    // (which signals handing-off to DWM) -- or in CompletePresent() if the
    // hand-off wasn't detected.
    //
    // The following events lookup presents based on this token:
    // Dwm_Event_FlipChain_Pending, Dwm_Event_FlipChain_Complete,
    // Dwm_Event_FlipChain_Dirty,
    std::map<uint64_t, std::shared_ptr<PresentEvent>> mDxgKrnlPresentHistoryTokens;

    // For blt presents on Win7, it's not possible to distinguish between DWM-off or fullscreen blts, and the DWM-on blt to redirection bitmaps.
    // The best we can do is make the distinction based on the next packet submitted to the context. If it's not a PHT, it's not going to DWM.
    std::map<uint64_t, std::shared_ptr<PresentEvent>> mBltsByDxgContext;

    // mLastWindowPresent is used as storage for presents handed off to DWM.
    //
    // For blit (Composed_Copy_GPU_GDI) presents:
    // DxgKrnl_Event_PropagatePresentHistory causes the present to be moved
    // from mDxgKrnlPresentHistoryTokens to mLastWindowPresent.
    //
    // For flip presents: Dwm_Event_FlipChain_Pending,
    // Dwm_Event_FlipChain_Complete, or Dwm_Event_FlipChain_Dirty sets
    // mLastWindowPresent to the present that matches the token from
    // mDxgKrnlPresentHistoryTokens (but doesn't clear mDxgKrnlPresentHistory).
    //
    // Dwm_Event_GetPresentHistory will move all the Composed_Copy_GPU_GDI and
    // Composed_Copy_CPU_GDI mLastWindowPresents to mPresentsWaitingForDWM
    // before clearing mLastWindowPresent.
    //
    // For Win32K-tracked events, Win32K_Event_TokenStateChanged InFrame will
    // set mLastWindowPresent (and set any current present as discarded), and
    // Win32K_Event_TokenStateChanged Confirmed will clear mLastWindowPresent.
    std::map<uint64_t, std::shared_ptr<PresentEvent>> mLastWindowPresent;

    // Presents that will be completed by DWM's next present
    std::deque<std::shared_ptr<PresentEvent>> mPresentsWaitingForDWM;

    // Store the DWM process id, and the last DWM thread id to have started
    // a present.  This is needed to determine if a flip event is coming from
    // DWM, but can also be useful for targetting non-DWM processes.
    uint32_t DwmProcessId = 0;
    uint32_t DwmPresentThreadId = 0;

    // Yet another unique way of tracking present history tokens, this time from DxgKrnl -> DWM, only for legacy blit
    std::map<uint64_t, std::shared_ptr<PresentEvent>> mPresentsByLegacyBlitToken;

    // Limit tracking to specified processes
    std::set<uint32_t> mTrackedProcessFilter;
    std::shared_mutex mTrackedProcessFilterMutex;

    // Storage for passing present path tracking id to Handle...() functions.
    #ifdef TRACK_PRESENT_PATHS
    uint32_t mAnalysisPathID;
    #endif

    // Tracking for GPU work contributing to each frame.  We need to track DMA
    // packet queuing to know how when each DMA packet actually ran.
    struct FrameDmaPackets {
        uint64_t mFirstDmaTime;         // QPC when the first DMA packet started for the current frame
        uint64_t mLastDmaTime;          // QPC when the last DMA packet completed for the current frame
        uint64_t mAccumulatedDmaTime;   // QPC duration while at least one DMA packet was running during the current frame
        uint64_t mRunningDmaStartTime;  // QPC when the oldest currently-running DMA packet started
        uint32_t mRunningDmaCount;      // Number of currently-running DMA packets
    };

    // A queue of up to 10 scheduled dma packets.  Using 10 to fill 2
    // cachelines.  10 should be more than we need, but 4 (fills 1 cacheline)
    // is not enough.
    struct Node {
        FrameDmaPackets* mFrameDmaPackets[10];  // Frame state for enqueued packets
        uint32_t mSequenceId[10];               // Sequence IDs for enqueued packets
        uint32_t mQueueIndex;                   // Index into mFrameDmaPackets and mSequenceId for currently-running packet
        uint32_t mQueueCount;                   // Number of enqueued packets
    };

    struct Context {
        FrameDmaPackets* mFrameDmaPackets;
        Node* mNode;
    };

    std::unordered_map<uint64_t, std::unordered_map<uint32_t, Node> > mNodes; // pDxgAdapter -> NodeOrdinal -> Node
    std::unordered_map<uint64_t, uint64_t> mDevices;                          // hDevice -> pDxgAdapter
    std::unordered_map<uint64_t, Context> mContexts;                          // hContext -> Context
    std::unordered_map<uint32_t, FrameDmaPackets> mProcessFrameDmaPackets;    // ProcessID -> FrameDmaPackets


    void DequeueProcessEvents(std::vector<ProcessEvent>& outProcessEvents)
    {
        std::lock_guard<std::mutex> lock(mProcessEventMutex);
        outProcessEvents.swap(mProcessEvents);
    }

    void DequeuePresentEvents(std::vector<std::shared_ptr<PresentEvent>>& outPresentEvents)
    {
        std::lock_guard<std::mutex> lock(mPresentEventMutex);
        outPresentEvents.swap(mCompletePresentEvents);
    }

    void DequeueLostPresentEvents(std::vector<std::shared_ptr<PresentEvent>>& outPresentEvents)
    {
        std::lock_guard<std::mutex> lock(mLostPresentEventMutex);
        outPresentEvents.swap(mLostPresentEvents);
    }

    void HandleDxgkBlt(EVENT_HEADER const& hdr, uint64_t hwnd, bool redirectedPresent);
    void HandleDxgkBltCancel(EVENT_HEADER const& hdr);
    void HandleDxgkFlip(EVENT_HEADER const& hdr, int32_t flipInterval, bool mmio);
    void HandleDxgkQueueSubmit(EVENT_HEADER const& hdr, uint32_t packetType, uint32_t submitSequence, uint64_t context, bool present, bool supportsDxgkPresentEvent);
    void HandleDxgkQueueComplete(EVENT_HEADER const& hdr, uint32_t submitSequence);
    void HandleDxgkMMIOFlip(EVENT_HEADER const& hdr, uint32_t flipSubmitSequence, uint32_t flags);
    void HandleDxgkMMIOFlipMPO(EVENT_HEADER const& hdr, uint32_t flipSubmitSequence, uint32_t flipEntryStatusAfterFlip, bool flipEntryStatusAfterFlipValid);
    void HandleDxgkSyncDPC(EVENT_HEADER const& hdr, uint32_t flipSubmitSequence, bool isMultiplane);
    void HandleDxgkPresentHistory(EVENT_HEADER const& hdr, uint64_t token, uint64_t tokenData, PresentMode knownPresentMode);
    void HandleDxgkPresentHistoryInfo(EVENT_HEADER const& hdr, uint64_t token);

    void CompletePresent(std::shared_ptr<PresentEvent> const& p);
    void CompletePresentHelper(std::shared_ptr<PresentEvent> const& p, OrderedPresents* completed);
    void CompleteDeferredCompletion(std::shared_ptr<PresentEvent> const& present);
    std::shared_ptr<PresentEvent> FindBySubmitSequence(uint32_t submitSequence);
    std::shared_ptr<PresentEvent> FindOrCreatePresent(EVENT_HEADER const& hdr);
    void TrackPresentOnThread(std::shared_ptr<PresentEvent> present);
    void TrackPresent(std::shared_ptr<PresentEvent> present, OrderedPresents* presentsByThisProcess);
    void RemoveLostPresent(std::shared_ptr<PresentEvent> present);
    void RemovePresentFromTemporaryTrackingCollections(std::shared_ptr<PresentEvent> present, bool waitForPresentStop);
    void RuntimePresentStop(EVENT_HEADER const& hdr, bool AllowPresentBatching, ::Runtime runtime);

    void HandleNTProcessEvent(EVENT_RECORD* pEventRecord);
    void HandleDXGIEvent(EVENT_RECORD* pEventRecord);
    void HandleD3D9Event(EVENT_RECORD* pEventRecord);
    void HandleDXGKEvent(EVENT_RECORD* pEventRecord);
    void HandleWin32kEvent(EVENT_RECORD* pEventRecord);
    void HandleDWMEvent(EVENT_RECORD* pEventRecord);
    void HandleMetadataEvent(EVENT_RECORD* pEventRecord);

    void HandleWin7DxgkBlt(EVENT_RECORD* pEventRecord);
    void HandleWin7DxgkFlip(EVENT_RECORD* pEventRecord);
    void HandleWin7DxgkPresentHistory(EVENT_RECORD* pEventRecord);
    void HandleWin7DxgkQueuePacket(EVENT_RECORD* pEventRecord);
    void HandleWin7DxgkVSyncDPC(EVENT_RECORD* pEventRecord);
    void HandleWin7DxgkMMIOFlip(EVENT_RECORD* pEventRecord);

    void AddTrackedProcessForFiltering(uint32_t processID);
    void RemoveTrackedProcessForFiltering(uint32_t processID);
    bool IsProcessTrackedForFiltering(uint32_t processID);
};
