<!---
[![](https://img.shields.io/github/license/GameTechDev/PresentMon)]()
[![](https://img.shields.io/github/v/release/GameTechDev/PresentMon)](https://github.com/GameTechDev/PresentMon/releases/latest)
[![](https://img.shields.io/github/commits-since/GameTechDev/PresentMon/latest/main)]()
[![](https://img.shields.io/github/issues/GameTechDev/PresentMon)]()
[![](https://img.shields.io/github/last-commit/GameTechDev/PresentMon)]()
-->

# Intel Internal PresentMon

PresentMon is a tool to capture and analyze [ETW](https://msdn.microsoft.com/en-us/library/windows/desktop/bb968803%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396) events related to swap chain presentation on Windows.  It can be used to trace key performance metrics for graphics applications (e.g., CPU and Display frame durations and latencies) and works across different graphics APIs, different hardware configurations, and for both desktop and UWP applications.

While PresentMon itself is focused on lightweight collection and analysis, there are several other programs that build on its functionality and/or helps visualize the resulting data.  For example, see

- [CapFrameX](https://github.com/DevTechProfile/CapFrameX)
- [FrameView](https://www.nvidia.com/en-us/geforce/technologies/frameview/)
- [OCAT](https://github.com/GPUOpen-Tools/OCAT)
- [PIX](https://devblogs.microsoft.com/pix/download/) (used as part of its [system monitor UI](https://devblogs.microsoft.com/pix/system-monitor/))

## License

Copyright (C) 2017-2022 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Releases

Binaries for main release versions of PresentMon are provided on GitHub:

- [Latest release](https://github.com/intel-innersource/drivers.gpu.tools.presentmon/releases/latest)
- [List of all releases](https://github.com/intel-innersource/drivers.gpu.tools.presentmon/releases)

See [CONTRIBUTING](https://github.com/intel-innersource/drivers.gpu.tools.presentmon/blob/main/CONTRIBUTING.md) for information on how to request features, report issues, or contribute code changes.

## Command line options

PresentMon needs to be run by a user who is a member of the "Performance Log Users" user group, or to be run with administrator privilege. If neither of these are true, you will get an error "failed to start trace session (access denied)".

To add a user to the "Performance Log Users" user group:

1. Run `compmgmt.msc` as administrator.
2. In the "Computer Management" window, expand "System Tools", expand "Local Users and Groups", and then click "Groups".
3. Double-click "Performance Log Users", and then click "Add".
4. In the "Enter the object names to select" text box, type the name of the user account or group account that you want to add, and then click "OK".
5. Sign out and log back in for the changes to take effect.

If PresentMon is not run with administrator privilege, it will not have complete process information for processes running on different user accounts.  Such processes will be listed in the console and CSV as "<error>", and they cannot be targeted by name.

### Capture Target Options

|                      |                                                                                                                  |
| -------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `-captureall`        | Record all processes (default).                                                                                  |
| `-process_name name` | Record only processes with the provided exe name.  This argument can be repeated to capture multiple processes.  |
| `-exclude name`      | Don't record processes with the provided exe name.  This argument can be repeated to exclude multiple processes. |
| `-process_id id`     | Record only the process specified by ID.                                                                         |
| `-etl_file path`     | Consume events from an ETW log file instead of running processes.                                                |

### Output Options

|                     |                                                                          |
| ------------------- | ------------------------------------------------------------------------ |
| `-output_file path` | Write CSV output to the provided path.                                   |
| `-output_stdout`    | Write CSV output to STDOUT.                                              |
| `-multi_csv`        | Create a separate CSV file for each captured process.                    |
| `-no_csv`           | Do not create any output file.                                           |
| `-no_top`           | Don't display active swap chains in the console                          |
| `-qpc_time`         | Output present time as a performance counter value.                      |
| `-qpc_time_s`       | Output present time as a performance counter value converted to seconds. |

### Recording Options

|                     |                                                                                                                                               |
| ------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| `-hotkey key`       | Use provided key to start and stop recording, writing to a unique CSV file each time. 'key' is of the form MODIFIER+KEY, e.g., alt+shift+f11. |
| `-delay seconds`    | Wait for provided time before starting to record. If using -hotkey, the delay occurs each time recording is started.                          |
| `-timed seconds`    | Stop recording after the provided amount of time.                                                                                             |
| `-exclude_dropped`  | Exclude dropped presents from the csv output.                                                                                                 |
| `-scroll_indicator` | Enable scroll lock while recording.                                                                                                           |
| `-no_track_display` | Disable tracking through GPU and display.                                                                                                     |
| `-track_debug`      | Adds additional data to output not relevant to normal usage.                                                                                  |

### Execution Options

|                           |                                                                                                                                                                                                                                                                                                                   |
| ------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `-session_name name`      | Use the provided name to start a new realtime ETW session, instead of the default "PresentMon". This can be used to start multiple realtime captures at the same time (using distinct, case-insensitive names). A realtime PresentMon capture cannot start if there are any existing sessions with the same name. |
| `-stop_existing_session`  | If a trace session with the same name is already running, stop the existing session (to allow this one to proceed).                                                                                                                                                                                               |
| `-terminate_existing`     | Terminate any existing PresentMon realtime trace sessions, then exit. Use with `-session_name` to target particular sessions.                                                                                                                                                                                     |
| `-restart_as_admin`       | If not running with elevated privilege, restart and request to be run as administrator. (See discussion above).                                                                                                                                                                                                   |
| `-terminate_on_proc_exit` | Terminate PresentMon when all the target processes have exited.                                                                                                                                                                                                                                                   |
| `-terminate_after_timed`  | When using `-timed`, terminate PresentMon after the timed capture completes.                                                                                                                                                                                                                                      |

### Beta Options

|                        |                                                                                                         |
| ---------------------- | ------------------------------------------------------------------------------------------------------- |
| `-date_time`           | Output present time as a date and time with nanosecond precision.                                       |
| `-track_gpu`           | Tracks the duration of each process' GPU work performed between presents.  Not supported on Win7.       |
| `-track_gpu_video`     | Track the video encode/decode portion of GPU work separately from other engines. Not supported on Win7. |
| `-track_input`         | Tracks the time of keyboard/mouse clicks that were used by each frame.                                  |
| `-track_power`         | Writes PCAT metrics to presentmon_pcat.csv in the working directory.                                    |
| `-track_mixed_reality` | Capture Windows Mixed Reality data to a CSV file with "_WMR" suffix.                                    |

### Internal Options

|                       |                                                           |
| --------------------- | --------------------------------------------------------- |
| `-track_queue_timers` | Capture Intel D3D11 driver producer/consumer queue timers |
| `-track_cpu_gpu_sync` | Capture Intel D3D11 driver CPU/GPU syncs                  |
| `-debug_frame_pacing` | Report extra driver metrics realted to frame pacing.      |

## Comma-separated value (CSV) file output

### CSV file names

By default, PresentMon creates a CSV file named `PresentMon-TIME.csv`, where `TIME` is the creation time in ISO 8601 format.  To specify your own output location, use the `-output_file PATH` command line argument.

If `-multi_csv` is used, then one CSV is created for each process captured and `-PROCESSNAME` appended to the file name.

If `-hotkey` is used, then one CSV is created for each time recording is started and `-INDEX` appended to the file name.

### CSV columns

| Column Header          | Data Description                                                                                                                                                                                                                                                          | Required argument            |
| ---------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------- |
| Application            | The name of the process that called Present().                                                                                                                                                                                                                            |                              |
| ProcessID              | The process ID of the process that called Present().                                                                                                                                                                                                                      |                              |
| SwapChainAddress       | The address of the swap chain that was presented into.                                                                                                                                                                                                                    |                              |
| Runtime                | The runtime used to present (e.g., D3D9 or DXGI).                                                                                                                                                                                                                         |                              |
| SyncInterval           | The sync interval provided by the application in the Present() call. This value may be modified later by the driver, e.g., based on control panel overrides.                                                                                                              |                              |
| PresentFlags           | Flags used in the Present() call.                                                                                                                                                                                                                                         |                              |
| PresentMode            | The presentation mode used by the system for this Present().  See the table below for more details.                                                                                                                                                                       | not `-no_track_display`      |
| AllowsTearing          | Whether tearing is possible (1) or not (0).                                                                                                                                                                                                                               | not `-no_track_display`      |
| TimeInSeconds          | The time of the Present() call, in seconds, relative to when the PresentMon started recording.                                                                                                                                                                            | not `-date_time`             |
| PresentTime            | The time of the Present() call, with date and time of day, relative to when the PresentMon started recording.                                                                                                                                                             | `-date_time`                 |
| QPCTime                | The time of the Present() call, as a [performance counter value](https://docs.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter).  If `-qpc_time_s` is used, the value is converted to seconds by dividing by the counter frequency. | `-qpc_time` or `-qpc_time_s` |
| msInPresentAPI         | The time spent inside the Present() call, in milliseconds.                                                                                                                                                                                                                |                              |
| msSinceInput           | The time from the earliest keyboard/mouse click impacting this frame, and the Present() call.  Is zero if no input was used for this frame.                                                                                                                               | `-track_input`               |
| msUntilRenderStart     | The time between the Present() call and when the GPU work started, in milliseconds.                                                                                                                                                                                       | `-track_gpu`                 |
| msUntilRenderComplete  | The time between the Present() call and when the GPU work completed, in milliseconds. Note that the value recorded will be slightly different if `-track_gpu` is or isn't used.  The value with `-track_gpu` is more accurate, but its collection has higher overhead.    | not `-no_track_display`      |
| msUntilDisplayed       | The time between the Present() call and when the frame was displayed, in milliseconds.                                                                                                                                                                                    | not `-no_track_display`      |
| Dropped                | Whether the frame was dropped (1) or displayed (0).  Note, if dropped, msUntilDisplayed will be 0.                                                                                                                                                                        |                              |
| msBetweenPresents      | The time between this Present() call and the previous one, in milliseconds.                                                                                                                                                                                               |                              |
| msBetweenDisplayChange | How long the previous frame was displayed before this Present() was displayed, in milliseconds.                                                                                                                                                                           | not `-no_track_display`      |
| msGPUActive            | The time that any GPU engine was active working on this frame, in milliseconds. If `--track_gpu_video` this only includes non-video engines.                                                                                                                              | `-track_gpu`                 |
| msGPUVideoActive       | The time the GPU spent encoding/decoding video during this frame, in milliseconds.                                                                                                                                                                                        | `-track_gpu_video`           |
| WasBatched             | Whether the frame was submitted by the driver on a different thread than the app (1) or not (0).                                                                                                                                                                          | `-track_debug`               |
| DwmNotified            | Whether the desktop compositor was notified about the frame (1) or not (0).                                                                                                                                                                                               | `-track_debug`               |
| WaitIfFullTime          | How long the producer thread was stalled on a full queue.                                                                                                                                                                                                                 | `-track_queue_timers`        |
| WaitIfEmptyTime         | How long the consumer thread was stalled on an empty queue.                                                                                                                                                                                                               | `-track_queue_timers`        |
| WaitUntilEmptySyncTime  | How long the driver waited for the queue to empty due to synchronization.                                                                                                                                                                                                 | `-track_queue_timers`        |
| WaitUntilEmptyDrainTime | How long the driver waited for the queue to drain.                                                                                                                                                                                                                        | `-track_queue_timers`        |
| WaitForFence            | How long the driver waited for consumer fences.                                                                                                                                                                                                                           | `-track_queue_timers`        |
| WaitUntilFenceSubmitted | How long the driver waited for fences submitted to the GPU.                                                                                                                                                                                                               | `-track_queue_timers`        |
| FrameTimeApp            | The time between present operations on the producer thread.                                                                                                                                                                                                               | `-track_queue_timers`        |
| FrameTimeDrv            | The time between present operations on the consumer thread.                                                                                                                                                                                                               | `-track_queue_timers`        |
| WaitSyncObjFromCpu      | How much time was spent waiting for a sync object from the CPU.                                                                                                                                                                                                           | `-track_cpu_gpu_sync`        |
| PollOnQueryGetData      | How much time the driver spent polling for query data.                                                                                                                                                                                                                    | `-track_cpu_gpu_sync`        |

The following values are used in the PresentMode column:

| PresentMode                           | Description                                                                                                                                                                                          |
| ------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Hardware: Legacy Flip                 | Indicates the app took ownership of the screen, and is swapping the displayed surface every frame.                                                                                                   |
| Hardware: Legacy Copy to front buffer | Indicates the app took ownership of the screen, and is copying new contents to an already-on-screen surface every frame.                                                                             |
| Hardware: Independent Flip            | Indicates the app does not have ownership of the screen, but is still swapping the displayed surface every frame.                                                                                    |
| Composed: Flip                        | Indicates the app is windowed, is using ["flip model" swapchains](https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model), and is sharing its surfaces with DWM to be composed. |
| Hardware Composed: Independent Flip   | Indicates the app is using ["flip model" swapchains](https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model), and has been granted a hardware overlay plane.                    |
| Composed: Copy with GPU GDI           | Indicates the app is windowed, and is copying contents into a surface that's shared with GDI.                                                                                                        |
| Composed: Copy with CPU GDI           | Indicates the app is windowed, and is copying contents into a dedicated DirectX window surface. GDI contents are stored separately, and are composed together with DX contents by the DWM.           |
| Composed: Composition Atlas           | Indicates use of DirectComposition.                                                                                                                                                                  |

For more information on the performance implications of these, see:

- https://www.youtube.com/watch?v=E3wTajGZOsA
- https://software.intel.com/content/www/us/en/develop/articles/sample-application-for-direct3d-12-flip-model-swap-chains.html

### Windows Mixed Reality

*Note: Windows Mixed Reality support is in beta, with limited OS support and maintenance.*

If `-track_mixed_reality` is used, a second CSV file will be generated with `_WMR` appended to the filename with the following columns:

| Column Header                                | Data Description                                                                                                | Required argument                                  |
| -------------------------------------------- | --------------------------------------------------------------------------------------------------------------- | -------------------------------------------------- |
| Application                                  | Process name (if known)                                                                                         | `-track_mixed_reality`                             |
| ProcessID                                    | Process ID                                                                                                      | `-track_mixed_reality`                             |
| DwmProcessID                                 | Compositor Process ID                                                                                           | `-track_mixed_reality`                             |
| TimeInSeconds                                | Time since PresentMon recording started                                                                         | `-track_mixed_reality`                             |
| msBetweenLsrs                                | Time between this Lsr CPU start and the previous one                                                            | `-track_mixed_reality`                             |
| AppMissed                                    | Whether Lsr is reprojecting a new (0) or old (1) App frame (App GPU work must complete before Lsr CPU start)    | `-track_mixed_reality`                             |
| LsrMissed                                    | Whether Lsr displayed a new frame (0) or not (1+) at the intended V-Sync (Count V-Syncs with no display change) | `-track_mixed_reality`                             |
| msAppPoseLatency                             | Time between App's pose sample and the intended mid-photon frame display                                        | `-track_mixed_reality`                             |
| msLsrPoseLatency                             | Time between Lsr's pose sample and the intended mid-photon frame display                                        | `-track_mixed_reality`                             |
| msActualLsrPoseLatency                       | Time between Lsr's pose sample and mid-photon frame display                                                     | `-track_mixed_reality`                             |
| msTimeUntilVsync                             | Time between Lsr CPU start and the intended V-Sync                                                              | `-track_mixed_reality`                             |
| msLsrThreadWakeupToGpuEnd                    | Time between Lsr CPU start and GPU work completion                                                              | `-track_mixed_reality`                             |
| msLsrThreadWakeupError                       | Time between intended Lsr CPU start and Lsr CPU start                                                           | `-track_mixed_reality`                             |
| msLsrPreemption                              | Time spent preempting the GPU with Lsr GPU work                                                                 | `-track_mixed_reality`                             |
| msLsrExecution                               | Time spent executing the Lsr GPU work                                                                           | `-track_mixed_reality`                             |
| msCopyPreemption                             | Time spent preempting the GPU with Lsr GPU cross-adapter copy work (if required)                                | `-track_mixed_reality`                             |
| msCopyExecution                              | Time spent executing the Lsr GPU cross-adapter copy work (if required)                                          | `-track_mixed_reality`                             |
| msGpuEndToVsync                              | Time between Lsr GPU work completion and V-Sync                                                                 | `-track_mixed_reality`                             |
| msBetweenAppPresents                         | Time between App's present and the previous one                                                                 | `-track_mixed_reality` and not `-no_track_display` |
| msAppPresentToLsr                            | Time between App's present and Lsr CPU start                                                                    | `-track_mixed_reality` and not `-no_track_display` |
| HolographicFrameID                           | App's Holographic Frame ID                                                                                      | `-track_mixed_reality` `-track_debug`              |
| msSourceReleaseFromRenderingToLsrAcquire     | Time between composition end and Lsr acquire                                                                    | `-track_mixed_reality` `-track_debug`              |
| msAppCpuRenderFrame                          | Time between App's CreateNextFrame() API call and PresentWithCurrentPrediction() API call                       | `-track_mixed_reality` `-track_debug`              |
| msAppMisprediction                           | Time between App's intended pose time and the intended mid-photon frame display                                 | `-track_mixed_reality` `-track_debug`              |
| msLsrCpuRenderFrame                          | Time between Lsr CPU render start and GPU work submit                                                           | `-track_mixed_reality` `-track_debug`              |
| msLsrThreadWakeupToCpuRenderFrameStart       | Time between Lsr CPU start and CPU render start                                                                 | `-track_mixed_reality` `-track_debug`              |
| msCpuRenderFrameStartToHeadPoseCallbackStart | Time between Lsr CPU render start and pose sample                                                               | `-track_mixed_reality` `-track_debug`              |
| msGetHeadPose                                | Time between Lsr pose sample start and pose sample end                                                          | `-track_mixed_reality` `-track_debug`              |
| msHeadPoseCallbackStopToInputLatch           | Time between Lsr pose sample end and input latch                                                                | `-track_mixed_reality` `-track_debug`              |
| msInputLatchToGpuSubmission                  | Time between Lsr input latch and GPU work submit                                                                | `-track_mixed_reality` `-track_debug`              |

## Known issues

See [public GitHub Issues](https://github.com/GameTechDev/PresentMon/issues) or [internal GitHub Issues](https://github.com/GameTechDev/PresentMon/issues) for a current list of reported issues.

### Analyzing OpenGL and Vulkan applications

Applications that do not use D3D9 or DXGI APIs for presenting frames (e.g., as is typical with OpenGL or Vulkan applications) will report the following:

- Runtime = Other.
- SwapChainAddress = 0
- SyncInterval = -1
- PresentFlags = 0
- msInPresentAPI = 0

In this case, PresentTime/TimeInSeconds will represent the first time the present is observed in the kernel, as opposed to the runtime, and therefore will be sometime after the application presented the frame (typically ~0.5ms).  Since msUntilRenderComplete and msUntilDisplayed are deltas from PresentTime/TimeInSeconds, they will be correspondingly smaller then they would have been if measured from application present.  msBetweenDisplayChange will still be correct, and msBetweenPresents should be correct on average.

### Measuring input latency

When using `-track_input`, PresentMon will track when keyboard/mouse events are read by the OS and the target application.  Then, for frames where `msSinceInput` is non-zero, `msSinceInput + msUntilDisplayed` can be used to better-understand the latency between user input and the display of the resulting rendered frame.  Note, however, that this is just the OS-visible subset of the full  latency: it does not include any latency incurred within the input/display hardware and drivers.

### Shutting down PresentMon on Windows 7

Some users have observed system stability issues when forcibly shutting down PresentMon on Windows 7.  If you are having similar issues, they can be avoided by using Ctrl+C in the PresentMon window to shut it down.
