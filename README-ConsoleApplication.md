# PresentMon Console Application

The PresentMon/ directory contains source for a standalone console application that uses the *PresentMon SDK* to capture and analyze graphics applications, outputting data to the console and/or CSV file(s).

A binary of the console application is provided in the release, e.g.: [PresentMon-1.9.0-x64.exe](releases/download/v1.9.0/PresentMon-1.9.0-x64.exe).

## Command line options

| Capture Target Options |                                                                                                                  |
| ---------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `-captureall`          | Record all processes (default).                                                                                  |
| `-process_name name`   | Record only processes with the provided exe name.  This argument can be repeated to capture multiple processes.  |
| `-exclude name`        | Don't record processes with the provided exe name.  This argument can be repeated to exclude multiple processes. |
| `-process_id id`       | Record only the process specified by ID.                                                                         |
| `-etl_file path`       | Consume events from an ETW log file instead of running processes.                                                |

| Output Options      |                                                                          |
| ------------------- | ------------------------------------------------------------------------ |
| `-output_file path` | Write CSV output to the provided path.                                   |
| `-output_stdout`    | Write CSV output to STDOUT.                                              |
| `-multi_csv`        | Create a separate CSV file for each captured process.                    |
| `-no_csv`           | Do not create any output file.                                           |
| `-no_top`           | Don't display active swap chains in the console                          |
| `-qpc_time`         | Output present time as a performance counter value.                      |
| `-qpc_time_s`       | Output present time as a performance counter value converted to seconds. |

| Recording Options   |                                                                                                                                                 |
| ------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| `-hotkey key`       | Use provided key to start and stop recording, writing to a unique CSV file each time. 'key' is of the form MODIFIER+KEY, e.g., "alt+shift+f11". |
| `-delay seconds`    | Wait for provided time before starting to record. If using -hotkey, the delay occurs each time recording is started.                            |
| `-timed seconds`    | Stop recording after the provided amount of time.                                                                                               |
| `-exclude_dropped`  | Exclude dropped presents from the csv output.                                                                                                   |
| `-scroll_indicator` | Enable scroll lock while recording.                                                                                                             |
| `-no_track_display` | Disable tracking through GPU and display.                                                                                                       |
| `-track_debug`      | Adds additional data to output not relevant to normal usage.                                                                                    |

| Execution Options         |                                                                                                                                                                                                                                                                                                                   |
| ------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `-session_name name`      | Use the provided name to start a new realtime ETW session, instead of the default "PresentMon". This can be used to start multiple realtime captures at the same time (using distinct, case-insensitive names). A realtime PresentMon capture cannot start if there are any existing sessions with the same name. |
| `-stop_existing_session`  | If a trace session with the same name is already running, stop the existing session (to allow this one to proceed).                                                                                                                                                                                               |
| `-terminate_existing`     | Terminate any existing PresentMon realtime trace sessions, then exit. Use with `-session_name` to target particular sessions.                                                                                                                                                                                     |
| `-restart_as_admin`       | If not running with elevated privilege, restart and request to be run as administrator. (See discussion above).                                                                                                                                                                                                   |
| `-terminate_on_proc_exit` | Terminate PresentMon when all the target processes have exited.                                                                                                                                                                                                                                                   |
| `-terminate_after_timed`  | When using `-timed`, terminate PresentMon after the timed capture completes.                                                                                                                                                                                                                                      |

| Beta Options              |                                                                                                         |
| ------------------------- | ------------------------------------------------------------------------------------------------------- |
| `-date_time`              | Output present time as a date and time with nanosecond precision.                                       |
| `-track_gpu`              | Tracks the duration of each process' GPU work performed between presents.  Not supported on Win7.       |
| `-track_gpu_video`        | Track the video encode/decode portion of GPU work separately from other engines. Not supported on Win7. |
| `-track_input`            | Tracks the time of keyboard/mouse clicks that were used by each frame.                                  |
| `-track_mixed_reality`    | Capture Windows Mixed Reality data to a CSV file with "_WMR" suffix.                                    |

## Comma-separated value (CSV) file output

### CSV file names

By default, PresentMon creates a CSV file named "PresentMon-\<Time>.csv", where "\<Time>" is the creation time in ISO 8601 format.  To specify your own output location, use the `-output_file PATH` command line argument.

If `-multi_csv` is used, then one CSV is created for each process captured and "-\<ProcessName>-\<ProcessId>" is appended to the file name.

If `-hotkey` is used, then one CSV is created for each time recording is started and "-\<Index>" is appended to the file name.

### CSV columns

| Column Header            | Data Description                                                                                                                                                                                                                                                                                                                                         |
| ------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| *Application*            | The name of the process that called Present().                                                                                                                                                                                                                                                                                                           |
| *ProcessID*              | The process ID of the process that called Present().                                                                                                                                                                                                                                                                                                     |
| *SwapChainAddress*       | The address of the swap chain that was presented into.                                                                                                                                                                                                                                                                                                   |
| *Runtime*                | The runtime used to present (e.g., D3D9 or DXGI).                                                                                                                                                                                                                                                                                                        |
| *SyncInterval*           | The sync interval provided by the application in the Present() call. This value may be modified later by the driver, e.g., based on control panel overrides.                                                                                                                                                                                             |
| *PresentFlags*           | Flags used in the Present() call.                                                                                                                                                                                                                                                                                                                        |
| *PresentMode*            | The presentation mode used by the system for this Present().  See the table below for more details.<br>This column is not available when `-no_track_display` is used.                                                                                                                                                                                    |
| *AllowsTearing*          | Whether tearing is possible (1) or not (0).<br>This column is not available when `-no_track_display` is used.                                                                                                                                                                                                                                            |
| *TimeInSeconds*          | The time of the Present() call, in seconds, relative to when the PresentMon started recording.                                                                                                                                                                                                                                                           |
| *QPCTime*                | The time of the Present() call, as a [performance counter value](https://docs.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter).<br>This column is only available when `-qpc_time` or `-qpc_time_s` are used.  When `-qpc_time_s` is used, the value is converted to seconds by dividing by the counter frequency. |
| *msInPresentAPI*         | The time spent inside the Present() call, in milliseconds.                                                                                                                                                                                                                                                                                               |
| *msUntilRenderComplete*  | The time between the Present() call and when GPU work for this frame completed, in milliseconds.<br>This column is not available when `-no_track_display` is used. |
| *msUntilDisplayed*       | The time between the Present() call and when the frame was displayed, in milliseconds.<br>This column is not available when `-no_track_display` is used.                                                                                                                                                                                                 |
| *Dropped*                | Whether the frame was dropped (1) or displayed (0).  Note, if dropped, *msUntilDisplayed* will be 0.                                                                                                                                                                                                                                                     |
| *msBetweenPresents*      | The time between this Present() call and the previous one, in milliseconds.                                                                                                                                                                                                                                                                              |
| *msBetweenDisplayChange* | How long the previous frame was displayed before this Present() was displayed, in milliseconds.<br>This column is not available when `-no_track_display` is used.                                                                                                                                                                                        |
| *WasBatched*             | Whether the frame was submitted by the driver on a different thread than the app (1) or not (0).<br>This column is only available when `-track_debug` is used.                                                                                                                                                                                           |
| *DwmNotified*            | Whether the desktop compositor was notified about the frame (1) or not (0).<br>This column is only available when `-track_debug` is used.                                                                                                                                                                                                                |

Using `-track_gpu` or `-track_gpu_video` will add the following columns:

| Column Header                     | Data Description                                                                            |
| --------------------------------- | ------------------------------------------------------------------------------------------- |
| *msUntilRenderStart*              | The time between the Present() call and when GPU work for this frame started, in milliseconds.  Note that rendering for a frame can start before the Present() call, so this value can be negative. |
| *msGPUActive*, *msGPUVideoActive* | The total duration the GPU was working on this frame, in milliseconds.  Time is counted whenever at least one engine is executing work from the target process. When `-track_gpu_video` is used, then the *msGPUVideoActive* column is added showing the duration of work on the GPU's video encode and/or decode engines and, in this case, the video encode/decode work is not included in *msGPUActive*. |

Using `-track_input` will add the following columns:

| Column Header  | Data Description                                                                            |
| -------------- | ------------------------------------------------------------------------------------------- |
| *msSinceInput* | The time between the Present() call and the earliest keyboard or mouse interaction that contributed to this frame.  For frames where *msSinceInput* is non-zero, `msSinceInput + msUntilDisplayed` can be used as a measure of the latency between user input and the display of the resulting rendered frame.  Note, however, that this is just the software-visible subset of the full input-to-photon latency and doesn't include:<br>&bull; time spent processing input in the keyboard/controller hardware or drivers (typically a fixed additional overhead),<br>&bull; time spent processing the output in the display hardware or drivers (typically a fixed additional overhead), and<br>&bull; a combination of display blanking interval and scan time (which varies, depending on timing and tearing). |

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

For more information on the performance implications of these, see:

- https://www.youtube.com/watch?v=E3wTajGZOsA
- https://software.intel.com/content/www/us/en/develop/articles/sample-application-for-direct3d-12-flip-model-swap-chains.html

### Windows Mixed Reality

*Note: Windows Mixed Reality support is in beta, with limited OS support and maintenance.*

If `-track_mixed_reality` is used, a second CSV file will be generated with "_WMR" appended to the filename with the following columns:

| Column Header                                  | Data Description                                                                                                                                         |
| ---------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| *Application*                                  | Process name (if known)                                                                                                                                  |
| *ProcessID*                                    | Process ID                                                                                                                                               |
| *DwmProcessID*                                 | Compositor Process ID                                                                                                                                    |
| *TimeInSeconds*                                | Time since PresentMon recording started                                                                                                                  |
| *msBetweenLsrs*                                | Time between this Lsr CPU start and the previous one                                                                                                     |
| *AppMissed*                                    | Whether Lsr is reprojecting a new (0) or old (1) App frame (App GPU work must complete before Lsr CPU start)                                             |
| *LsrMissed*                                    | Whether Lsr displayed a new frame (0) or not (1+) at the intended V-Sync (Count V-Syncs with no display change)                                          |
| *msAppPoseLatency*                             | Time between App's pose sample and the intended mid-photon frame display                                                                                 |
| *msLsrPoseLatency*                             | Time between Lsr's pose sample and the intended mid-photon frame display                                                                                 |
| *msActualLsrPoseLatency*                       | Time between Lsr's pose sample and mid-photon frame display                                                                                              |
| *msTimeUntilVsync*                             | Time between Lsr CPU start and the intended V-Sync                                                                                                       |
| *msLsrThreadWakeupToGpuEnd*                    | Time between Lsr CPU start and GPU work completion                                                                                                       |
| *msLsrThreadWakeupError*                       | Time between intended Lsr CPU start and Lsr CPU start                                                                                                    |
| *msLsrPreemption*                              | Time spent preempting the GPU with Lsr GPU work                                                                                                          |
| *msLsrExecution*                               | Time spent executing the Lsr GPU work                                                                                                                    |
| *msCopyPreemption*                             | Time spent preempting the GPU with Lsr GPU cross-adapter copy work (if required)                                                                         |
| *msCopyExecution*                              | Time spent executing the Lsr GPU cross-adapter copy work (if required)                                                                                   |
| *msGpuEndToVsync*                              | Time between Lsr GPU work completion and V-Sync                                                                                                          |
| *msBetweenAppPresents*                         | Time between App's present and the previous one.                                                                                                         |
| *msAppPresentToLsr*                            | Time between App's present and Lsr CPU start.<br>This column is not available when `-no_track_display` is used.                                          |
| *HolographicFrameID*                           | App's Holographic Frame ID.<br>This column is only available when `-track_debug` is used.                                                                |
| *msSourceReleaseFromRenderingToLsrAcquire*     | Time between composition end and Lsr acquire.<br>This column is only available when `-track_debug` is used.                                              |
| *msAppCpuRenderFrame*                          | Time between App's CreateNextFrame() API call and PresentWithCurrentPrediction() API call.<br>This column is only available when `-track_debug` is used. |
| *msAppMisprediction*                           | Time between App's intended pose time and the intended mid-photon frame display.<br>This column is only available when `-track_debug` is used.           |
| *msLsrCpuRenderFrame*                          | Time between Lsr CPU render start and GPU work submit.<br>This column is only available when `-track_debug` is used.                                     |
| *msLsrThreadWakeupToCpuRenderFrameStart*       | Time between Lsr CPU start and CPU render start.<br>This column is only available when `-track_debug` is used.                                           |
| *msCpuRenderFrameStartToHeadPoseCallbackStart* | Time between Lsr CPU render start and pose sample.<br>This column is only available when `-track_debug` is used.                                         |
| *msGetHeadPose*                                | Time between Lsr pose sample start and pose sample end.<br>This column is only available when `-track_debug` is used.                                    |
| *msHeadPoseCallbackStopToInputLatch*           | Time between Lsr pose sample end and input latch.<br>This column is only available when `-track_debug` is used.                                          |
| *msInputLatchToGpuSubmission*                  | Time between Lsr input latch and GPU work submit.<br>This column is only available when `-track_debug` is used.                                          |
