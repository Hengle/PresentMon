// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include <span>
#include <format>
#include "PresentMonClient.h"
#include "../PresentMonUtils/QPCUtils.h"
#include "../PresentMonUtils/PresentDataUtils.h"
#include <iostream>
#include <filesystem>

#define GOOGLE_GLOG_DLL_DECL
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>

static const uint32_t kMaxRespBufferSize = 4096;
static const uint64_t kClientFrameDeltaQPCThreshold = 50000000;

PresentMonClient::PresentMonClient()
    : pipe_(INVALID_HANDLE_VALUE),
      set_metric_offset_in_qpc_ticks_(0),
      client_to_frame_data_delta_(0) {
  LPCTSTR pipe_name = TEXT("\\\\.\\pipe\\presentmonsvcnamedpipe");
  
  // initialize glog based on command line if not already initialized
  // clients using glog should init before initializing pmon api
  // logs go to working directory
  if (!google::IsGoogleLoggingInitialized()) {
      std::filesystem::create_directories("logs");
      google::InitGoogleLogging(__argv[0]);
      google::SetLogDestination(google::GLOG_INFO, ".\\logs\\pm-cli-info-");
      google::SetLogDestination(google::GLOG_WARNING, ".\\logs\\pm-cli-warn-");
      google::SetLogDestination(google::GLOG_ERROR, ".\\logs\\pm-cli-err-");
      google::SetLogDestination(google::GLOG_FATAL, ".\\logs\\pm-cli-fatal-");
      google::SetLogFilenameExtension(".txt");
  }

  // Try to open a named pipe; wait for it, if necessary.
  while (1) {
    pipe_ = CreateFile(pipe_name,      // pipe name
                       GENERIC_READ |  // read and write access
                           GENERIC_WRITE,
                       0,              // no sharing
                       NULL,           // default security attributes
                       OPEN_EXISTING,  // opens existing pipe
                       0,              // default attributes
                       NULL);          // no template file

    // Break if the pipe handle is valid.
    if (pipe_ != INVALID_HANDLE_VALUE) {
      break;
    }

    // Exit if an error other than ERROR_PIPE_BUSY occurs.
    if (GetLastError() != ERROR_PIPE_BUSY) {
      LOG(ERROR) << "Service not found.";
      throw std::runtime_error{"Service not found"};
    }

    // All pipe instances are busy, so wait for 20 seconds.
    if (!WaitNamedPipe(pipe_name, 20000)) {
      LOG(ERROR) << "Pipe sessions full.";
      throw std::runtime_error{"Pipe sessions full"};
    }
  }
  // The pipe connected; change to message-read mode.
  DWORD mode = PIPE_READMODE_MESSAGE;
  BOOL success = SetNamedPipeHandleState(pipe_,  // pipe handle
                                         &mode,  // new pipe mode
                                         NULL,   // don't set maximum bytes
                                         NULL);  // don't set maximum time
  if (!success) {
    LOG(ERROR) << "Pipe error.";
    throw std::runtime_error{"Pipe error"};
  }

  // Store the client process id
  client_process_id_ = GetCurrentProcessId();
}

PresentMonClient::~PresentMonClient() {
  if (pipe_ != INVALID_HANDLE_VALUE) {
    CloseHandle(pipe_);
    pipe_ = INVALID_HANDLE_VALUE;
  }
  return;
}

// Calculate percentile using linear interpolation between the closet ranks
// method
double PresentMonClient::GetPercentile(std::vector<double>& data,
                                       double percentile) {
  double integral_part_as_double;
  double fractpart =
      modf(((percentile * (static_cast<double>(data.size() - 1))) + 1),
           &integral_part_as_double);

  // Subtract off one from the integral_part as we are zero based and the
  // calculation above is based one based
  uint32_t integral_part = static_cast<uint32_t>(integral_part_as_double) - 1;
  uint32_t next_idx = integral_part + 1;
  // Before we access the vector data ensure that our calculated index values
  // are not out of range
  if (integral_part < data.size() || next_idx < data.size()) {
    return data[integral_part] +
           (fractpart * (data[next_idx] - data[integral_part]));
  } else {
    LOG(INFO) << "Invalid percentile calculation inputs detected.";
    return 0.0f;
  }
}

PM_STATUS PresentMonClient::GetGfxLatencyData(
    uint32_t process_id, PM_GFX_LATENCY_DATA* gfx_latency_data,
    double window_size_in_ms, uint32_t* num_swapchains) {
  std::unordered_map<uint64_t, fps_swap_chain_data> swap_chain_data;

  if (*num_swapchains == 0) {
    return PM_STATUS::PM_STATUS_ERROR;
  }

  auto iter = clients_.find(process_id);
  if (iter == clients_.end()) {
    LOG(INFO) << "Stream client for process " << process_id
              << " doesn't exist. Please call pmStartStream to initialize the "
                 "client.";
    return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
  }

  StreamClient* client = iter->second.get();
  auto nsm_view = client->GetNamedSharedMemView();
  auto nsm_hdr = nsm_view->GetHeader();
  if (!nsm_hdr->process_active) {
    // Server destroyed the named shared memory due to process exit. Destroy
    // the mapped view from client side.
    StopStreamProcess(process_id);
    return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
  }

  auto cache_iter = client_metric_caches_.find(process_id);
  if (cache_iter == client_metric_caches_.end()) {
    // This should never happen!
    return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
  }
  auto metric_cache = &cache_iter->second;

  LARGE_INTEGER qpc_frequency = client->GetQpcFrequency();

  uint64_t index = 0;
  PmNsmFrameData* frame_data =
      GetFrameDataStart(client, index, window_size_in_ms);
  if (frame_data == nullptr) {
    if (CopyCacheData(gfx_latency_data, num_swapchains, metric_cache->cached_latency_data_)) {
      return PM_STATUS::PM_STATUS_SUCCESS;
    } else {
      return PM_STATUS::PM_STATUS_NO_DATA;
    }
  }

  // Calculate the end qpc based on the current frame's qpc and
  // requested window size coverted to a qpc
  uint64_t end_qpc =
      frame_data->present_event.PresentStartTime -
      ContvertMsToQpcTicks(window_size_in_ms, qpc_frequency.QuadPart);

  bool data_gathering_complete = false;
  // Loop from the most recent frame data until we either run out of data or we
  // meet the window size requirements sent in by the client
  for (;;) {
    auto result = swap_chain_data.emplace(
        frame_data->present_event.SwapChainAddress, fps_swap_chain_data());
    auto swap_chain = &result.first->second;
    if (result.second) {
      swap_chain->cpu_n_time = frame_data->present_event.PresentStartTime;
      swap_chain->cpu_0_time = frame_data->present_event.PresentStartTime;
      swap_chain->display_latency_ms.clear();
      swap_chain->render_latency_ms.clear();
    } else {
      if (frame_data->present_event.PresentStartTime > end_qpc) {
        if (frame_data->present_event.FinalState == PresentResult::Presented) {
          double current_render_latency_ms = 0.0;
          double current_display_latency_ms = 0.0;

          if (frame_data->present_event.ScreenTime >
              frame_data->present_event.ReadyTime) {
            current_display_latency_ms =
                QpcDeltaToMs(frame_data->present_event.ScreenTime -
                                 frame_data->present_event.ReadyTime,
                             qpc_frequency);
            swap_chain->display_latency_sum +=
                frame_data->present_event.ScreenTime -
                frame_data->present_event.ReadyTime;
          } else {
            current_display_latency_ms =
                -1.0 * QpcDeltaToMs(frame_data->present_event.ReadyTime -
                                        frame_data->present_event.ScreenTime,
                                    qpc_frequency);
            swap_chain->display_latency_sum +=
                (-1 * (frame_data->present_event.ReadyTime -
                       frame_data->present_event.ScreenTime));
          }
          swap_chain->display_latency_ms.push_back(current_display_latency_ms);

          current_render_latency_ms =
              QpcDeltaToMs(frame_data->present_event.ScreenTime -
                               frame_data->present_event.PresentStartTime,
                           qpc_frequency);
          swap_chain->render_latency_ms.push_back(current_render_latency_ms);
          swap_chain->render_latency_sum +=
              frame_data->present_event.ScreenTime -
              frame_data->present_event.PresentStartTime;
          swap_chain->display_count++;
        }
        swap_chain->cpu_0_time = frame_data->present_event.PresentStartTime;
      } else {
        data_gathering_complete = true;
      }
    }

    if (data_gathering_complete) {
      break;
    }

    // Get the index of the next frame
    if (DecrementIndex(nsm_view, index) == false) {
      // We have run out of data to process, time to go
      break;
    }
    frame_data = client->ReadFrameByIdx(index);
    if (frame_data == nullptr) {
      break;
    }
  }

  PM_GFX_LATENCY_DATA* current_gfx_latency_data = gfx_latency_data;
  uint32_t gfx_latency_data_counter = 0;
  for (auto pair : swap_chain_data) {
    ZeroMemory(current_gfx_latency_data, sizeof(PM_GFX_LATENCY_DATA));

    // Save off the current swap chain
    current_gfx_latency_data->swap_chain = pair.first;

    auto chain = pair.second;
    if (chain.display_count >= 1) {
      current_gfx_latency_data->display_latency_ms.avg =
          QpcDeltaToMs(chain.display_latency_sum, qpc_frequency) /
          chain.display_count;
      current_gfx_latency_data->render_latency_ms.avg =
          QpcDeltaToMs(chain.render_latency_sum, qpc_frequency) /
          chain.display_count;
    }

    if (chain.display_latency_ms.size() > 1) {
      std::sort(chain.display_latency_ms.begin(),
                chain.display_latency_ms.end());

      current_gfx_latency_data->display_latency_ms.low =
          chain.display_latency_ms[0];
      current_gfx_latency_data->display_latency_ms.high =
          chain.display_latency_ms[chain.display_latency_ms.size() - 1];

      current_gfx_latency_data->display_latency_ms.percentile_90 =
          GetPercentile(chain.display_latency_ms, 0.1);
      current_gfx_latency_data->display_latency_ms.percentile_95 =
          GetPercentile(chain.display_latency_ms, 0.05);
      current_gfx_latency_data->display_latency_ms.percentile_99 =
          GetPercentile(chain.display_latency_ms, 0.01);
    } else {
        current_gfx_latency_data->display_latency_ms.low =
            current_gfx_latency_data->display_latency_ms.avg;
        current_gfx_latency_data->display_latency_ms.high =
            current_gfx_latency_data->display_latency_ms.avg;
        current_gfx_latency_data->display_latency_ms.percentile_90 =
            current_gfx_latency_data->display_latency_ms.avg;
        current_gfx_latency_data->display_latency_ms.percentile_95 =
            current_gfx_latency_data->display_latency_ms.avg;
        current_gfx_latency_data->display_latency_ms.percentile_99 =
            current_gfx_latency_data->display_latency_ms.avg;
    }

    if (chain.render_latency_ms.size() > 1) {
      std::sort(chain.render_latency_ms.begin(), chain.render_latency_ms.end());

      current_gfx_latency_data->render_latency_ms.low =
          chain.render_latency_ms[0];
      current_gfx_latency_data->render_latency_ms.high =
          chain.render_latency_ms[chain.render_latency_ms.size() - 1];

      current_gfx_latency_data->render_latency_ms.percentile_90 =
          GetPercentile(chain.render_latency_ms, 0.1);
      current_gfx_latency_data->render_latency_ms.percentile_95 =
          GetPercentile(chain.render_latency_ms, 0.05);
      current_gfx_latency_data->render_latency_ms.percentile_99 =
          GetPercentile(chain.render_latency_ms, 0.01);
    } else {
      current_gfx_latency_data->render_latency_ms.low =
          current_gfx_latency_data->render_latency_ms.avg;
      current_gfx_latency_data->render_latency_ms.high =
          current_gfx_latency_data->render_latency_ms.avg;
      current_gfx_latency_data->render_latency_ms.percentile_90 =
          current_gfx_latency_data->render_latency_ms.avg;
      current_gfx_latency_data->render_latency_ms.percentile_95 =
          current_gfx_latency_data->render_latency_ms.avg;
      current_gfx_latency_data->render_latency_ms.percentile_99 =
          current_gfx_latency_data->render_latency_ms.avg;
    }

    // There are a couple reasons where we will not be able to produce
    // latency metric data. The first is if all of the frames are dropped.
    // The second case is if the requested offset is produces a start frame
    // qpc that is earlier then the frame data present in the NSM.
    if ((chain.display_count < 1) ||
        (chain.cpu_n_time <= chain.cpu_0_time)) {
      if (gfx_latency_data_counter < metric_cache->cached_latency_data_.size()) {
        *current_gfx_latency_data =
            metric_cache->cached_latency_data_[gfx_latency_data_counter];
      }
    } else {
      if (gfx_latency_data_counter >=
          metric_cache->cached_latency_data_.size()) {
        metric_cache->cached_latency_data_.push_back(*current_gfx_latency_data);
      } else {
        metric_cache->cached_latency_data_[gfx_latency_data_counter] =
            *current_gfx_latency_data;
      }
    }

    gfx_latency_data_counter++;
    if (gfx_latency_data_counter >= *num_swapchains) {
      // Reached the amount of fp data structures passed in.
      break;
    }
    current_gfx_latency_data++;
  }

  // Update the number of swap chains available so client can adjust
  *num_swapchains = static_cast<uint32_t>(swap_chain_data.size());
  return PM_STATUS::PM_STATUS_SUCCESS;
}

PM_STATUS PresentMonClient::GetFramesPerSecondData(uint32_t process_id,
                                                   PM_FPS_DATA* fps_data,
                                                   double window_size_in_ms,
                                                   uint32_t* num_swapchains) {
  std::unordered_map<uint64_t, fps_swap_chain_data> swap_chain_data;

  if (*num_swapchains == 0) {
    return PM_STATUS::PM_STATUS_ERROR;
  }

  auto iter = clients_.find(process_id);
  if (iter == clients_.end()) {
    LOG(INFO) << "Stream client for process " << process_id
              << " doesn't exist. Please call pmStartStream to initialize the "
                 "client.";
    return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
  }

  StreamClient* client = iter->second.get();
  auto nsm_view = client->GetNamedSharedMemView();
  auto nsm_hdr = nsm_view->GetHeader();
  if (!nsm_hdr->process_active) {
    // Server destroyed the named shared memory due to process exit. Destroy the
    // mapped view from client side.
    StopStreamProcess(process_id);
    return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
  }

  auto cache_iter = client_metric_caches_.find(process_id);
  if (cache_iter == client_metric_caches_.end()) {
    // This should never happen!
    return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
  }
  auto metric_cache = &cache_iter->second;

  LARGE_INTEGER qpc_frequency = client->GetQpcFrequency();

  uint64_t index = 0;
  PmNsmFrameData* frame_data =
      GetFrameDataStart(client, index, window_size_in_ms);
  if (frame_data == nullptr) {
    if (CopyCacheData(fps_data, num_swapchains, metric_cache->cached_fps_data_)) {
      return PM_STATUS::PM_STATUS_SUCCESS;
    } else {
      return PM_STATUS::PM_STATUS_NO_DATA;
    }
  }

  // Calculate the end qpc based on the current frame's qpc and
  // requested window size coverted to a qpc
  uint64_t end_qpc =
      frame_data->present_event.PresentStartTime -
      ContvertMsToQpcTicks(window_size_in_ms, qpc_frequency.QuadPart);

  bool data_gathering_complete = false;
  // Loop from the most recent frame data until we either run out of data or
  // we meet the window size requirements sent in by the client
  for (;;) {
    auto result = swap_chain_data.emplace(
        frame_data->present_event.SwapChainAddress, fps_swap_chain_data());
    auto swap_chain = &result.first->second;
    if (result.second) {
      // Save off the QPCTime of the latest present event
      swap_chain->cpu_n_time = frame_data->present_event.PresentStartTime;
      swap_chain->cpu_0_time = frame_data->present_event.PresentStartTime;
      // Add in the inital gpu duration for the present event
      swap_chain->gpu_sum = frame_data->present_event.GPUDuration;
      // Initialize num_presents to 1 as we have just determined that the
      // present event is valid
      swap_chain->num_presents = 1;
      swap_chain->sync_interval = frame_data->present_event.SyncInterval;
      swap_chain->present_mode =
          TranslatePresentMode(frame_data->present_event.PresentMode);

      if (frame_data->present_event.FinalState == PresentResult::Presented) {
        swap_chain->display_n_screen_time =
            frame_data->present_event.ScreenTime;
        swap_chain->display_0_screen_time =
            frame_data->present_event.ScreenTime;
        swap_chain->display_count = 1;
      } else {
        swap_chain->drop_count = 1;
      }
      swap_chain->cpu_fps.clear();
      swap_chain->display_fps.clear();
    } else {
      // Calculate the amount of time passed between the current and previous
      // events and add it to our cumulative window size
      double time_in_ms = QpcDeltaToMs(
          swap_chain->cpu_0_time - frame_data->present_event.PresentStartTime,
          qpc_frequency);

      if (frame_data->present_event.PresentStartTime > end_qpc) {
        // Save off the cpu fps
        double fps_data = 1000.0f / time_in_ms;
        swap_chain->cpu_fps.push_back(fps_data);
        if (frame_data->present_event.FinalState == PresentResult::Presented) {
          if (swap_chain->display_0_screen_time != 0) {
            time_in_ms = QpcDeltaToMs(swap_chain->display_0_screen_time -
                                          frame_data->present_event.ScreenTime,
                                      qpc_frequency);
            fps_data = 1000.0f / time_in_ms;
            swap_chain->display_fps.push_back(fps_data);
          }
          if (swap_chain->display_count == 0) {
            swap_chain->display_n_screen_time =
                frame_data->present_event.ScreenTime;
          }
          swap_chain->display_0_screen_time =
              frame_data->present_event.ScreenTime;
          swap_chain->display_count++;
        } else {
          swap_chain->drop_count++;
        }
        swap_chain->cpu_0_time = frame_data->present_event.PresentStartTime;
        swap_chain->gpu_sum += frame_data->present_event.GPUDuration;
        swap_chain->sync_interval = frame_data->present_event.SyncInterval;
        swap_chain->present_mode =
            TranslatePresentMode(frame_data->present_event.PresentMode);
        swap_chain->allows_tearing =
            static_cast<int32_t>(frame_data->present_event.SupportsTearing);
        swap_chain->num_presents++;
      } else {
        data_gathering_complete = true;
      }
    }

    if (data_gathering_complete) {
      break;
    }

    // Get the index of the next frame
    if (DecrementIndex(nsm_view, index) == false) {
      // We have run out of data to process, time to go
      break;
    }
    frame_data = client->ReadFrameByIdx(index);
    if (frame_data == nullptr) {
      break;
    }
  }

  PM_FPS_DATA* current_fps_data = fps_data;
  uint32_t fps_data_counter = 0;
  for (auto pair : swap_chain_data) {
    
    ZeroMemory(current_fps_data, sizeof(PM_FPS_DATA));

    // Save off the current swap chain
    current_fps_data->swap_chain = pair.first;
    auto chain = pair.second;

    // Calculate the average display fps if any frames were displayed.
    if (chain.display_count > 1) {
      current_fps_data->display_fps.avg = QpcDeltaToMs(
          chain.display_n_screen_time - chain.display_0_screen_time,
          qpc_frequency);
      current_fps_data->display_fps.avg /= (chain.display_count - 1);
      current_fps_data->display_fps.avg =
          (current_fps_data->display_fps.avg > 0.0)
              ? 1000.0 / current_fps_data->display_fps.avg
                                      : 0.0;
    } else {
      current_fps_data->display_fps.avg = 0.0;
    }

    // Calculate the average cpu fps and average gpu fps if we have any
    // presents.
    if (chain.num_presents > 1) {
      current_fps_data->cpu_fps.avg =
          QpcDeltaToMs(chain.cpu_n_time - chain.cpu_0_time, qpc_frequency);
      current_fps_data->cpu_fps.avg =
          current_fps_data->cpu_fps.avg / (chain.num_presents - 1);
      current_fps_data->cpu_fps.avg = (current_fps_data->cpu_fps.avg > 0.0)
              ? 1000.0 / current_fps_data->cpu_fps.avg
                                          : 0.0;
      current_fps_data->gpu_fps_avg =
          QpcDeltaToMs(chain.gpu_sum, qpc_frequency) / (chain.num_presents - 1);
      current_fps_data->gpu_fps_avg = (current_fps_data->gpu_fps_avg > 0.0)
              ? 1000.0 / current_fps_data->gpu_fps_avg
                                          : 0.0;
    } else {
      current_fps_data->cpu_fps.avg = 0.0;
      current_fps_data->gpu_fps_avg = 0.0;
    }

    // Calculate the 99th, 95th and 90th percentiles for the display fps
    if (chain.display_fps.size() > 1) {
      // First sort the fps data
      std::sort(chain.display_fps.begin(), chain.display_fps.end());

      current_fps_data->display_fps.low = chain.display_fps[0];
      current_fps_data->display_fps.high =
          chain.display_fps[chain.display_fps.size() - 1];

      current_fps_data->display_fps.percentile_90 =
          GetPercentile(chain.display_fps, 0.1);
      current_fps_data->display_fps.percentile_95 =
          GetPercentile(chain.display_fps, 0.05);
      current_fps_data->display_fps.percentile_99 =
          GetPercentile(chain.display_fps, 0.01);
    } else {
      current_fps_data->display_fps.low = current_fps_data->display_fps.avg;
      current_fps_data->display_fps.high = current_fps_data->display_fps.avg;
      current_fps_data->display_fps.percentile_90 =
          current_fps_data->display_fps.avg;
      current_fps_data->display_fps.percentile_95 =
          current_fps_data->display_fps.avg;
      current_fps_data->display_fps.percentile_99 =
          current_fps_data->display_fps.avg;
    }

    // Calculate the 99th, 95th and 90th percentiles for the cpu fps
    if (chain.cpu_fps.size() > 1) {
      // First sort the fps data
      std::sort(chain.cpu_fps.begin(), chain.cpu_fps.end());

      current_fps_data->cpu_fps.low = chain.cpu_fps[0];
      current_fps_data->cpu_fps.high = chain.cpu_fps[chain.cpu_fps.size() - 1];

      current_fps_data->cpu_fps.percentile_90 =
          GetPercentile(chain.cpu_fps, 0.1);
      current_fps_data->cpu_fps.percentile_95 =
          GetPercentile(chain.cpu_fps, 0.05);
      current_fps_data->cpu_fps.percentile_99 =
          GetPercentile(chain.cpu_fps, 0.01);
    } else {
      current_fps_data->cpu_fps.low = current_fps_data->cpu_fps.avg;
      current_fps_data->cpu_fps.high = current_fps_data->cpu_fps.avg;
      current_fps_data->cpu_fps.percentile_90 = current_fps_data->cpu_fps.avg;
      current_fps_data->cpu_fps.percentile_95 = current_fps_data->cpu_fps.avg;
      current_fps_data->cpu_fps.percentile_99 = current_fps_data->cpu_fps.avg;
    }

    // We don't record each frame's sync interval, present
    // mode or tearing status. Instead we take the values from
    // the more recent frame
    current_fps_data->sync_interval = chain.sync_interval;
    current_fps_data->present_mode = chain.present_mode;
    current_fps_data->allows_tearing = chain.allows_tearing;

    current_fps_data->num_presents = chain.num_presents;
    current_fps_data->num_dropped_frames = chain.drop_count;

    // There are few reasons where we will not be able to produce
    // fps metric data. The first is if all of the frames are dropped.
    // The second is if in the requested sample window there are
    // no presents. And the final case is if the requested offset
    // is produces a start frame qpc that is earlier then the frame
    // data present in the NSM.
    if ((chain.display_count <= 1) || (chain.num_presents <= 1) ||
        (chain.cpu_n_time <= chain.cpu_0_time)) {
      if (fps_data_counter < metric_cache->cached_fps_data_.size()) {
        *current_fps_data = metric_cache->cached_fps_data_[fps_data_counter];
      }
    } else {
      if (fps_data_counter >= metric_cache->cached_fps_data_.size()) {
        metric_cache->cached_fps_data_.push_back(*current_fps_data);
      } else {
        metric_cache->cached_fps_data_[fps_data_counter] = *current_fps_data;
      }
    }
    
    fps_data_counter++;
    if (fps_data_counter >= *num_swapchains) {
      // Reached the amount of fp data structures passed in.
      break;
    }
    current_fps_data++;
  }

  // Update the number of swap chains available so client can adjust
  *num_swapchains = static_cast<uint32_t>(swap_chain_data.size());
  return PM_STATUS::PM_STATUS_SUCCESS;
}

PM_STATUS PresentMonClient::SendRequest(MemBuffer* rqst_buf) {
  DWORD bytes_written;

  BOOL success = WriteFile(
      pipe_,                                           // pipe handle
      rqst_buf->AccessMem(),                           // message
      static_cast<DWORD>(rqst_buf->GetCurrentSize()),  // message length
      &bytes_written,                                  // bytes written
      NULL);                                           // not overlapped

  if (success && rqst_buf->GetCurrentSize() == bytes_written) {
    return PM_STATUS::PM_STATUS_SUCCESS;
  } else {
    return TranslateGetLastErrorToPmStatus(GetLastError());
  }
}

PM_STATUS PresentMonClient::ReadResponse(MemBuffer* rsp_buf) {
  BOOL success;
  DWORD bytes_read;
  BYTE in_buffer[kMaxRespBufferSize];
  ZeroMemory(&in_buffer, sizeof(in_buffer));

  do {
    // Read from the pipe using a nonoverlapped read
    success = ReadFile(pipe_,              // pipe handle
                       in_buffer,          // buffer to receive reply
                       sizeof(in_buffer),  // size of buffer
                       &bytes_read,        // number of bytes read
                       NULL);              // not overlapped

    // If the call was not successful AND there was
    // no more data to read bail out
    if (!success && GetLastError() != ERROR_MORE_DATA) {
      break;
    }

    // Either the call was successful or there was more
    // data in the pipe. In both cases add the response data
    // to the memory buffer
    rsp_buf->AddItem(in_buffer, bytes_read);
  } while (!success);  // repeat loop if ERROR_MORE_DATA

  if (success) {
    return PM_STATUS::PM_STATUS_SUCCESS;
  } else {
    return TranslateGetLastErrorToPmStatus(GetLastError());
  }
}

PM_STATUS PresentMonClient::CallPmService(MemBuffer* rqst_buf,
                                          MemBuffer* rsp_buf) {
  PM_STATUS status;

  status = SendRequest(rqst_buf);
  if (status != PM_STATUS::PM_STATUS_SUCCESS) {
    return status;
  }

  status = ReadResponse(rsp_buf);
  if (status != PM_STATUS::PM_STATUS_SUCCESS) {
    return status;
  }

  return status;
}

PM_STATUS PresentMonClient::RequestStreamProcess(uint32_t process_id) {
  MemBuffer rqst_buf;
  MemBuffer rsp_buf;

  NamedPipeHelper::EncodeStartStreamingRequest(&rqst_buf, client_process_id_,
                                               process_id, nullptr);

  PM_STATUS status = CallPmService(&rqst_buf, &rsp_buf);
  if (status != PM_STATUS::PM_STATUS_SUCCESS) {
    return status;
  }

  IPMSMFileName file_name_data;

  status =
      NamedPipeHelper::DecodeStartStreamingResponse(&rsp_buf, &file_name_data);
  if (status != PM_STATUS::PM_STATUS_SUCCESS) {
    return status;
  }

  string mapfile_name(file_name_data.fileName);

  // Initialize client with returned mapfile name
  auto iter = clients_.find(process_id);
  if (iter == clients_.end()) {
    try {
      std::unique_ptr<StreamClient> client =
          std::make_unique<StreamClient>(mapfile_name, false);

      clients_.emplace(process_id, std::move(client));
    } catch (...) {
      LOG(ERROR) << "Unabled to add client.\n";
      return PM_STATUS::PM_STATUS_ERROR;
    }
  }

  if (!SetupClientCaches(process_id)) {
    LOG(ERROR) << "Unabled to setup client metric caches.\n";
    return PM_STATUS::PM_STATUS_ERROR;
  }

  return status;
}

PM_STATUS PresentMonClient::RequestStreamProcess(char const* etl_file_name) {
    MemBuffer rqst_buf;
    MemBuffer rsp_buf;

    NamedPipeHelper::EncodeStartStreamingRequest(
        &rqst_buf, client_process_id_,
        static_cast<uint32_t>(StreamPidOverride::kEtlPid), etl_file_name);

    PM_STATUS status = CallPmService(&rqst_buf, &rsp_buf);
    if (status != PM_STATUS::PM_STATUS_SUCCESS) {
        return status;
    }

    IPMSMFileName file_name_data;

    status = NamedPipeHelper::DecodeStartStreamingResponse(&rsp_buf, &file_name_data);
    if (status != PM_STATUS::PM_STATUS_SUCCESS) {
        return status;
    }

    string mapfile_name(file_name_data.fileName);

    try {
        etl_client_ = std::make_unique<StreamClient>(mapfile_name, true);
    } catch (...) {
        LOG(ERROR) << "Unabled to create stream client.\n";
        return PM_STATUS::PM_STATUS_ERROR;
    }

    return status;
}

PM_STATUS PresentMonClient::StopStreamProcess(uint32_t process_id) {
    MemBuffer rqst_buf;
    MemBuffer rsp_buf;

    NamedPipeHelper::EncodeStopStreamingRequest(&rqst_buf,
                                                client_process_id_,
                                                process_id);

    PM_STATUS status = CallPmService(&rqst_buf, &rsp_buf);
    if (status != PM_STATUS::PM_STATUS_SUCCESS) {
        return status;
    }

    status = NamedPipeHelper::DecodeStopStreamingResponse(&rsp_buf);
    if (status != PM_STATUS::PM_STATUS_SUCCESS) {
        return status;
    }

    // Remove client
    {
        auto iter = clients_.find(process_id);
        if (iter != clients_.end()) {
      clients_.erase(iter);
        }
    }

    // Remove process metric cache
    RemoveClientCaches(process_id);

    return status;
}

PM_STATUS PresentMonClient::GetGpuData(uint32_t process_id,
                                       PM_GPU_DATA* gpu_data,
                                       double window_size_in_ms) {
  std::vector<double> gpu_power;
  std::vector<double> gpu_sustained_power_limit;
  std::vector<double> gpu_voltage;
  std::vector<double> gpu_frequency;
  std::vector<double> gpu_temperature;
  std::vector<double> gpu_fan_speed[MAX_PM_FAN_COUNT];
  std::vector<double> gpu_utilization;
  std::vector<double> gpu_render_compute_utilization;
  std::vector<double> gpu_media_utilization;
  std::vector<double> vram_power;
  std::vector<double> vram_voltage;
  std::vector<double> vram_frequency;
  std::vector<double> vram_effective_frequency;
  std::vector<double> vram_read_bandwidth;
  std::vector<double> vram_write_bandwidth;
  std::vector<double> vram_temperature;
  std::vector<double> psu_power[MAX_PM_PSU_COUNT];
  std::vector<double> psu_voltage[MAX_PM_PSU_COUNT];
  std::vector<double> gpu_power_limited;
  std::vector<double> gpu_temperature_limited;
  std::vector<double> gpu_current_limited;
  std::vector<double> gpu_voltage_limited;
  std::vector<double> gpu_utilization_limited;
  std::vector<double> vram_power_limited;
  std::vector<double> vram_temperature_limited;
  std::vector<double> vram_current_limited;
  std::vector<double> vram_voltage_limited;
  std::vector<double> vram_utilization_limited;
  std::vector<double> gpu_mem_total_size;
  std::vector<double> gpu_mem_used_in_mb;
  std::vector<double> gpu_mem_utilization;
  std::vector<double> gpu_mem_max_bandwidth;
  std::vector<double> gpu_mem_read_bandwidth;
  std::vector<double> gpu_mem_write_bandwidth;
  uint64_t n_qpc = 0;
  uint64_t zero_qpc = 0;
  
  auto iter = clients_.find(process_id);

  if (iter == clients_.end()) {
    LOG(INFO) << "Stream client for process " << process_id
              << " doesn't exist. Please call pmStartStream to initialize the "
                 "client.";
    return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
  }

  StreamClient* client = iter->second.get();
  auto nsm_view = client->GetNamedSharedMemView();
  LARGE_INTEGER qpc_frequency = client->GetQpcFrequency();

  auto cache_iter = client_metric_caches_.find(process_id);
  if (cache_iter == client_metric_caches_.end()) {
    // This should never happen!
    return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
  }
  auto metric_cache = &cache_iter->second;

  uint64_t index = 0;
  PmNsmFrameData* frame_data =
      GetFrameDataStart(client, index, window_size_in_ms);
  if (frame_data == nullptr) {
    *gpu_data = metric_cache->cached_gpu_data_[0];
    return PM_STATUS::PM_STATUS_SUCCESS;
  }

  // Save off the initial qpc
  n_qpc = frame_data->present_event.PresentStartTime;

  // Calculate the end qpc based on the current frame's qpc and
  // requested window size coverted to a qpc
  uint64_t end_qpc =
      frame_data->present_event.PresentStartTime -
      ContvertMsToQpcTicks(window_size_in_ms, qpc_frequency.QuadPart);

  // Loop from the most recent frame data until we either run out of data or we
  // meet the window size requirements sent in by the client
  while (frame_data->present_event.PresentStartTime > end_qpc) {
    gpu_power.push_back(frame_data->power_telemetry.gpu_power_w);
    gpu_sustained_power_limit.push_back(
        frame_data->power_telemetry.gpu_sustained_power_limit_w);
    //gpu_voltage.push_back((1000.0 * frame_data->power_telemetry.gpu_voltage));
    gpu_voltage.push_back(frame_data->power_telemetry.gpu_voltage_v);
    gpu_frequency.push_back(frame_data->power_telemetry.gpu_frequency_mhz);
    gpu_utilization.push_back(frame_data->power_telemetry.gpu_utilization);
    gpu_render_compute_utilization.push_back(
        frame_data->power_telemetry.gpu_render_compute_utilization);
    gpu_media_utilization.push_back(
        frame_data->power_telemetry.gpu_media_utilization);
    gpu_temperature.push_back(frame_data->power_telemetry.gpu_temperature_c);
    // Iterate through all fans
    for (int i = 0; i < MAX_PM_FAN_COUNT; i++) {
      gpu_fan_speed[i].push_back(frame_data->power_telemetry.fan_speed_rpm[i]);
    }
    // Iterate through all power supplies
    for (int i = 0; i < MAX_PM_PSU_COUNT; i++) {
      psu_power[i].push_back(frame_data->power_telemetry.psu[i].psu_power);
      psu_voltage[i].push_back(frame_data->power_telemetry.psu[i].psu_voltage);
      gpu_data->psu_data[i].psu_type =
          TranslatePsuType(frame_data->power_telemetry.psu[i].psu_type);
    }

    vram_power.push_back(frame_data->power_telemetry.vram_power_w);
    vram_voltage.push_back(frame_data->power_telemetry.vram_voltage_v);
    vram_frequency.push_back(frame_data->power_telemetry.vram_frequency_mhz);
    vram_effective_frequency.push_back(
        frame_data->power_telemetry.vram_effective_frequency_gbps);
    vram_read_bandwidth.push_back(
        frame_data->power_telemetry.vram_read_bandwidth_bps);
    vram_write_bandwidth.push_back(
        frame_data->power_telemetry.vram_write_bandwidth_bps);
    vram_temperature.push_back(frame_data->power_telemetry.vram_temperature_c);

    gpu_mem_total_size.push_back(
        static_cast<double>(frame_data->power_telemetry.gpu_mem_total_size_b));
    gpu_mem_used_in_mb.push_back(
        static_cast<double>(frame_data->power_telemetry.gpu_mem_used_b));
    // Using the total gpu memory size and the amount currently
    // used calculate the percent utilization.
    gpu_mem_utilization.push_back(
        100. *
        (static_cast<double>(frame_data->power_telemetry.gpu_mem_used_b) /
         frame_data->power_telemetry.gpu_mem_total_size_b));
    gpu_mem_max_bandwidth.push_back(
        static_cast<double>(frame_data->power_telemetry.gpu_mem_max_bandwidth_bps));
    gpu_mem_read_bandwidth.push_back(
        frame_data->power_telemetry.gpu_mem_read_bandwidth_bps);
    gpu_mem_write_bandwidth.push_back(
        frame_data->power_telemetry.gpu_mem_write_bandwidth_bps);

    gpu_power_limited.push_back(
        static_cast<double>(frame_data->power_telemetry.gpu_power_limited));
    gpu_temperature_limited.push_back(static_cast<double>(
        frame_data->power_telemetry.gpu_temperature_limited));
    gpu_current_limited.push_back(
        static_cast<double>(frame_data->power_telemetry.gpu_current_limited));
    gpu_voltage_limited.push_back(
        static_cast<double>(frame_data->power_telemetry.gpu_voltage_limited));
    gpu_utilization_limited.push_back(static_cast<double>(
        frame_data->power_telemetry.gpu_utilization_limited));

    vram_power_limited.push_back(
        static_cast<double>(frame_data->power_telemetry.vram_power_limited));
    vram_temperature_limited.push_back(static_cast<double>(
        frame_data->power_telemetry.vram_temperature_limited));
    vram_current_limited.push_back(
        static_cast<double>(frame_data->power_telemetry.vram_current_limited));
    vram_voltage_limited.push_back(
        static_cast<double>(frame_data->power_telemetry.vram_voltage_limited));
    vram_utilization_limited.push_back(static_cast<double>(
        frame_data->power_telemetry.vram_utilization_limited));

    // Get the index of the next frame
    if (DecrementIndex(nsm_view, index) == false) {
      // We have run out of data to process, time to go
      break;
    }
    frame_data = client->ReadFrameByIdx(index);
    if (frame_data == nullptr) {
      break;
    } else {
      zero_qpc = frame_data->present_event.PresentStartTime;
    }
  }

  CalculateMetricDoubleData(gpu_power, gpu_data->gpu_power_w);
  CalculateMetricDoubleData(gpu_sustained_power_limit,
                            gpu_data->gpu_sustained_power_limit_w);
  CalculateMetricDoubleData(gpu_voltage, gpu_data->gpu_voltage_v);
  CalculateMetricDoubleData(gpu_frequency, gpu_data->gpu_frequency_mhz);
  CalculateMetricDoubleData(gpu_utilization, gpu_data->gpu_utilization);
  CalculateMetricDoubleData(gpu_render_compute_utilization,
      gpu_data->gpu_render_compute_utilization);
  CalculateMetricDoubleData(gpu_media_utilization,
      gpu_data->gpu_media_utilization);
  CalculateMetricDoubleData(gpu_temperature, gpu_data->gpu_temperature_c);
  // Calculate metrics for all fans
  for (int i = 0; i < MAX_PM_FAN_COUNT; i++) {
    CalculateMetricDoubleData(gpu_fan_speed[i], gpu_data->gpu_fan_speed_rpm[i]);
  }

  CalculateMetricDoubleData(vram_power, gpu_data->vram_power_w);
  CalculateMetricDoubleData(vram_voltage, gpu_data->vram_voltage_v);
  CalculateMetricDoubleData(vram_frequency, gpu_data->vram_frequency_mhz);
  CalculateMetricDoubleData(vram_effective_frequency,
      gpu_data->vram_effective_frequency_gbps);
  CalculateMetricDoubleData(vram_read_bandwidth,
                            gpu_data->vram_read_bandwidth_bps);
  CalculateMetricDoubleData(vram_write_bandwidth,
      gpu_data->vram_write_bandwidth_bps);
  CalculateMetricDoubleData(vram_temperature, gpu_data->vram_temperature_c);

  CalculateMetricDoubleData(gpu_mem_total_size,
                            gpu_data->gpu_mem_total_size_b);
  CalculateMetricDoubleData(gpu_mem_used_in_mb,
                            gpu_data->gpu_mem_used_b);
  CalculateMetricDoubleData(gpu_mem_utilization,
                            gpu_data->gpu_mem_utilization);
  CalculateMetricDoubleData(gpu_mem_max_bandwidth,
                            gpu_data->gpu_mem_max_bandwidth_bps);
  CalculateMetricDoubleData(gpu_mem_read_bandwidth,
                            gpu_data->gpu_mem_read_bandwidth_bps);
  CalculateMetricDoubleData(gpu_mem_write_bandwidth,
                            gpu_data->gpu_mem_write_bandwidth_bps);

  // Calculate metrics for all PSUs
  for (int i = 0; i < MAX_PM_PSU_COUNT; i++) {
    CalculateMetricDoubleData(psu_power[i], gpu_data->psu_data[i].psu_power);
    CalculateMetricDoubleData(psu_voltage[i], gpu_data->psu_data[i].psu_voltage);
  }

  CalculateMetricDoubleData(gpu_power_limited, gpu_data->gpu_power_limited);
  CalculateMetricDoubleData(gpu_temperature_limited,
                            gpu_data->gpu_temperature_limited);
  CalculateMetricDoubleData(gpu_current_limited,
                            gpu_data->gpu_current_limited);
  CalculateMetricDoubleData(gpu_voltage_limited, gpu_data->gpu_voltage_limited);
  CalculateMetricDoubleData(gpu_utilization_limited,
                            gpu_data->gpu_utilization_limited);

  CalculateMetricDoubleData(vram_power_limited, gpu_data->vram_power_limited);
  CalculateMetricDoubleData(vram_temperature_limited,
                            gpu_data->vram_temperature_limited);
  CalculateMetricDoubleData(vram_current_limited, gpu_data->vram_current_limited);
  CalculateMetricDoubleData(vram_voltage_limited, gpu_data->vram_voltage_limited);
  CalculateMetricDoubleData(vram_utilization_limited,
                            gpu_data->vram_utilization_limited);

  // There are two reasons where we will not be able to produce
  // gpu metric data. The first is if there is not enough gpu data
  // to calculate the metrics. The second case is if the requested offset
  // is produces a start frame qpc that is earlier then the frame
  // data present in the NSM.
  if ((gpu_power.size() <= 1) || (n_qpc <= zero_qpc)) {
    *gpu_data = metric_cache->cached_gpu_data_[0];
  } else {
    metric_cache->cached_gpu_data_[0] = *gpu_data;
  }

  return PM_STATUS::PM_STATUS_SUCCESS;

}

PM_STATUS PresentMonClient::GetCpuData(uint32_t process_id,
                                       PM_CPU_DATA* cpu_data,
                                       double window_size_in_ms) {
  std::vector<double> cpu_utilization;
  std::vector<double> cpu_power;
  std::vector<double> cpu_power_limit;
  std::vector<double> cpu_temperature;
  std::vector<double> cpu_frequency;
  uint64_t n_qpc = 0;
  uint64_t zero_qpc = 0;

  auto iter = clients_.find(process_id);

  if (iter == clients_.end()) {
    LOG(INFO) << "Stream client for process " << process_id
              << " doesn't exist. Please call pmStartStream to initialize the "
                 "client.";
    return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
  }

  StreamClient* client = iter->second.get();
  auto nsm_view = client->GetNamedSharedMemView();
  LARGE_INTEGER qpc_frequency = client->GetQpcFrequency();

  auto cache_iter = client_metric_caches_.find(process_id);
  if (cache_iter == client_metric_caches_.end()) {
    // This should never happen!
    return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
  }
  auto metric_cache = &cache_iter->second;

  uint64_t index = 0;
  PmNsmFrameData* frame_data =
      GetFrameDataStart(client, index, window_size_in_ms);
  if (frame_data == nullptr) {
    *cpu_data = metric_cache->cached_cpu_data_[0];
    return PM_STATUS::PM_STATUS_SUCCESS;
  }
  // Save off the initial qpc
  n_qpc = frame_data->present_event.PresentStartTime;

  // Calculate the end qpc based on the current frame's qpc and
  // requested window size coverted to a qpc
  uint64_t end_qpc =
      frame_data->present_event.PresentStartTime -
      ContvertMsToQpcTicks(window_size_in_ms, qpc_frequency.QuadPart);

  // Loop from the most recent frame data until we either run out of data or we
  // meet the window size requirements sent in by the client
  while (frame_data->present_event.PresentStartTime > end_qpc) {
    cpu_utilization.push_back(frame_data->cpu_telemetry.cpu_utilization);
    cpu_power.push_back(frame_data->cpu_telemetry.cpu_power_w);
    cpu_power_limit.push_back(frame_data->cpu_telemetry.cpu_power_limit_w);
    cpu_temperature.push_back(frame_data->cpu_telemetry.cpu_temperature);
    cpu_frequency.push_back(frame_data->cpu_telemetry.cpu_frequency);

    // Get the index of the next frame
    if (DecrementIndex(nsm_view, index) == false) {
      // We have run out of data to process, time to go
      break;
    }
    frame_data = client->ReadFrameByIdx(index);
    if (frame_data == nullptr) {
      break;
    } else {
      zero_qpc = frame_data->present_event.PresentStartTime;
    }
  }

  CalculateMetricDoubleData(cpu_power, cpu_data->cpu_power_w);
  CalculateMetricDoubleData(cpu_power_limit,
                            cpu_data->cpu_power_limit_w);
  CalculateMetricDoubleData(cpu_temperature, cpu_data->cpu_temperature_c);
  CalculateMetricDoubleData(cpu_utilization, cpu_data->cpu_utilization);
  CalculateMetricDoubleData(cpu_frequency, cpu_data->cpu_frequency);

  // There are two reasons where we will not be able to produce
  // gpu metric data. The first is if there is not enough gpu data
  // to calculate the metrics. The second case is if the requested offset
  // is produces a start frame qpc that is earlier then the frame
  // data present in the NSM.
  if ((cpu_power.size() <= 1) || (n_qpc <= zero_qpc)) {
    *cpu_data = metric_cache->cached_cpu_data_[0];
  } else {
    metric_cache->cached_cpu_data_[0] = *cpu_data;
  }

  return PM_STATUS::PM_STATUS_SUCCESS;
}

void PresentMonClient::CalculateMetricDoubleData(
    std::vector<double>& in_data, PM_METRIC_DOUBLE_DATA& metric_double_data)
{
  metric_double_data.avg = 0.0;
  if (in_data.size() > 1) {
    std::sort(in_data.begin(), in_data.end());
    metric_double_data.low = in_data[0];
    metric_double_data.high = in_data[in_data.size() - 1];
    metric_double_data.percentile_99 = GetPercentile(in_data, 0.01);
    metric_double_data.percentile_95 = GetPercentile(in_data, 0.05);
    metric_double_data.percentile_90 = GetPercentile(in_data, 0.1);
    for (auto& element : in_data) {
      metric_double_data.avg += element;
    }
    metric_double_data.avg /= in_data.size();
  } else if (in_data.size() == 1) {
    metric_double_data.low = in_data[0];
    metric_double_data.high = in_data[0];
    metric_double_data.percentile_99 = in_data[0];
    metric_double_data.percentile_95 = in_data[0];
    metric_double_data.percentile_90 = in_data[0];
    metric_double_data.avg = in_data[0];
  } else {
    metric_double_data.low = 0.;
    metric_double_data.high = 0.;
    metric_double_data.percentile_99 = 0.;
    metric_double_data.percentile_95 = 0.;
    metric_double_data.percentile_90 = 0.;
    metric_double_data.avg = 0.;
  }

}

// in_out_num_frames: input value indicates number of frames the out_buf can hold.
// out value indicate numbers of FrameData returned. 
PM_STATUS PresentMonClient::GetFrameData(uint32_t process_id,
                                         bool is_etl,
                                         uint32_t* in_out_num_frames,
                                         PM_FRAME_DATA* out_buf) {
  PM_STATUS status = PM_STATUS::PM_STATUS_SUCCESS;

  if (in_out_num_frames == nullptr) {
    return PM_STATUS::PM_STATUS_ERROR;
  }

  uint32_t frames_to_copy = *in_out_num_frames;
  // We have saved off the number of frames to copy, now set
  // to zero in case we error out along the way BEFORE we
  // copy frames into the buffer. If a successful copy occurs
  // we'll set to actual number copied.
  *in_out_num_frames = 0;
  uint32_t frames_copied = 0;
  *in_out_num_frames = 0;

  StreamClient* client = nullptr;
  if (is_etl) {
    if (etl_client_) {
      client = etl_client_.get();
    } else {
      LOG(INFO)
          << "Stream client for process "
          << " doesn't exist. Please call pmStartStream to initialize the "
             "client.";
      return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
    }
  } else {
    auto iter = clients_.find(process_id);

    if (iter == clients_.end()) {
      try {
        LOG(INFO)
            << "Stream client for process " << process_id
            << " doesn't exist. Please call pmStartStream to initialize the "
               "client.";
      } catch (...) {
        LOG(INFO)
            << "Stream client for process "
            << " doesn't exist. Please call pmStartStream to initialize the "
               "client.";
      }
      return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
    }

    client = iter->second.get();
  }

  auto nsm_view = client->GetNamedSharedMemView();
  auto nsm_hdr = nsm_view->GetHeader();
  if (!nsm_hdr->process_active) {
    // Service destroyed the named shared memory due to process exit. Destroy
    // the mapped view from client side.
    if (is_etl == false) {
      StopStreamProcess(process_id);
    }
    return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
  }

  uint64_t last_frame_idx = client->GetLatestFrameIndex();
  if (last_frame_idx == UINT_MAX) {
    // There are no frames available
    return PM_STATUS::PM_STATUS_NO_DATA;
  }

  PM_FRAME_DATA* dst_frame = out_buf;

  for (uint32_t i = 0; i < frames_to_copy; i++) {
    if (is_etl) {
      status = client->DequeueFrame(&dst_frame);
    } else {
      status = client->RecordFrame(&dst_frame);
    }
    if (status != PM_STATUS::PM_STATUS_SUCCESS) {
      break;
    }

    dst_frame++;
    frames_copied++;
  }

  if ((status == PM_STATUS::PM_STATUS_NO_DATA) && (frames_copied > 0)) {
    // There are no more frames available in the NSM but frames have been
    // processed. Update status to PM_SUCCESS
    status = PM_STATUS::PM_STATUS_SUCCESS;
  }
  // Set to the actual number of frames copied
  *in_out_num_frames = frames_copied;

  return status;
}

PM_STATUS PresentMonClient::SetMetricsOffset(double offset_in_ms) {
  //(TODO)megalvan: think about removing storing qpc frequency from the
  // stream client as we should not need the process id for setting the
  // metric offset
  LARGE_INTEGER qpc_frequency;
  QueryPerformanceFrequency(&qpc_frequency);
  set_metric_offset_in_qpc_ticks_ =
      ContvertMsToQpcTicks(offset_in_ms, qpc_frequency.QuadPart);
  return PM_STATUS::PM_STATUS_SUCCESS;
}

uint64_t PresentMonClient::GetAdjustedQpc(uint64_t current_qpc,
                                          uint64_t frame_data_qpc,
                                          LARGE_INTEGER frequency) {
  // Calculate how far behind the frame data qpc is compared
  // to the client qpc
  uint64_t current_qpc_delta = current_qpc - frame_data_qpc;
  if (client_to_frame_data_delta_ == 0) {
    client_to_frame_data_delta_ = current_qpc_delta;
  } else {
    if (_abs64(client_to_frame_data_delta_ - current_qpc_delta) >
        kClientFrameDeltaQPCThreshold) {
      client_to_frame_data_delta_ = current_qpc_delta;
    }
  }

  // Add in the client set metric offset in qpc ticks
  return current_qpc -
         (client_to_frame_data_delta_ + set_metric_offset_in_qpc_ticks_);
}

PmNsmFrameData* PresentMonClient::GetFrameDataStart(
    StreamClient* client, uint64_t& index, double& window_sample_size_in_ms) {

  PmNsmFrameData* frame_data = nullptr;
  index = 0;
  if (client == nullptr) {
    return nullptr;
  }

  auto nsm_view = client->GetNamedSharedMemView();
  auto nsm_hdr = nsm_view->GetHeader();
  if (!nsm_hdr->process_active) {
    return nullptr;
  }

  index = client->GetLatestFrameIndex();
  frame_data = client->ReadFrameByIdx(index);
  if (frame_data == nullptr) {
    index = 0;
    return nullptr;
  }
  
  if (set_metric_offset_in_qpc_ticks_ == 0) {
    // Client has not specified a metric offset. Return back the most
    // most recent frame data
    return frame_data;
  }

  LARGE_INTEGER client_qpc = {};
  QueryPerformanceCounter(&client_qpc);
  uint64_t adjusted_qpc = GetAdjustedQpc(
      client_qpc.QuadPart, frame_data->present_event.PresentStartTime,
                     client->GetQpcFrequency());

  if (adjusted_qpc > frame_data->present_event.PresentStartTime) {
    // Need to adjust the size of the window sample size
    double ms_adjustment =
        QpcDeltaToMs(adjusted_qpc - frame_data->present_event.PresentStartTime,
                     client->GetQpcFrequency());
    window_sample_size_in_ms = window_sample_size_in_ms - ms_adjustment;
    if (window_sample_size_in_ms <= 0.0) {
      return nullptr;
    }
  } else {
    // Find the frame with the appropriate time based on the adjusted
    // qpc
    for (;;) {
      
      if (DecrementIndex(nsm_view, index) == false) {
        break;
      }
      frame_data = client->ReadFrameByIdx(index);
      if (frame_data == nullptr) {
        return nullptr;
      }
      if (adjusted_qpc >= frame_data->present_event.PresentStartTime) {
        break;
      }
    }
  }

  return frame_data;
}

bool PresentMonClient::DecrementIndex(NamedSharedMem* nsm_view,
    uint64_t& index) {

  if (nsm_view == nullptr) {
    return false;
  }

  auto nsm_hdr = nsm_view->GetHeader();
  if (!nsm_hdr->process_active) {
    return false;
  }

  uint64_t current_max_entries =
      (nsm_view->IsFull()) ? nsm_hdr->max_entries - 1 : nsm_hdr->tail_idx;
  index = (index == 0) ? current_max_entries : index - 1;
  if (index == nsm_hdr->head_idx) {
    return false;
  }

  return true;
}

uint64_t PresentMonClient::ContvertMsToQpcTicks(double time_in_ms,
                                                uint64_t frequency) {
  return static_cast<uint64_t>((time_in_ms / 1000.0) * frequency);
}

PM_STATUS PresentMonClient::EnumerateAdapters(
    PM_ADAPTER_INFO* adapter_info_buffer, uint32_t* adapter_count) {
  MemBuffer rqst_buf;
  MemBuffer rsp_buf;
  
  NamedPipeHelper::EncodeEnumerateAdaptersRequest(&rqst_buf);

  PM_STATUS status = CallPmService(&rqst_buf, &rsp_buf);
  if (status != PM_STATUS::PM_STATUS_SUCCESS) {
    return status;
  }
  
  IPMAdapterInfo adapter_info{};
  status =
      NamedPipeHelper::DecodeEnumerateAdaptersResponse(&rsp_buf, &adapter_info);
  if (status != PM_STATUS::PM_STATUS_SUCCESS) {
    return status;
  }

  if (!adapter_info_buffer) {
    *adapter_count = adapter_info.num_adapters;
    // if buffer size too small, signal required size with error
  } else if (*adapter_count < adapter_info.num_adapters) {
    *adapter_count = adapter_info.num_adapters;
    return PM_STATUS::PM_STATUS_INSUFFICIENT_BUFFER;
    // buffer exists and is large enough, fill it and signal number of entries
    // filled
  } else {
    *adapter_count = adapter_info.num_adapters;
    std::ranges::copy(
        std::span{adapter_info.adapters, adapter_info.num_adapters},
        adapter_info_buffer);
  }
  return PM_STATUS::PM_STATUS_SUCCESS;
}

PM_STATUS PresentMonClient::SetActiveAdapter(uint32_t adapter_id) {
  MemBuffer rqst_buf;
  MemBuffer rsp_buf;
  
  NamedPipeHelper::EncodeGeneralSetActionRequest(PM_ACTION::SELECT_ADAPTER,
                                                 &rqst_buf, adapter_id);
  
  PM_STATUS status = CallPmService(&rqst_buf, &rsp_buf);
  if (status != PM_STATUS::PM_STATUS_SUCCESS) {
    return status;
  }

  status = NamedPipeHelper::DecodeGeneralSetActionResponse(
      PM_ACTION::SELECT_ADAPTER, &rsp_buf);

  return status;
}

PM_STATUS PresentMonClient::SetGPUTelemetryPeriod(uint32_t period_ms) {
  MemBuffer rqst_buf;
  MemBuffer rsp_buf;

  NamedPipeHelper::EncodeGeneralSetActionRequest(
      PM_ACTION::SET_GPU_TELEMETRY_PERIOD, &rqst_buf, period_ms);

  PM_STATUS status = CallPmService(&rqst_buf, &rsp_buf);
  if (status != PM_STATUS::PM_STATUS_SUCCESS) {
    return status;
  }

  status = NamedPipeHelper::DecodeGeneralSetActionResponse(
      PM_ACTION::SET_GPU_TELEMETRY_PERIOD, &rsp_buf);
  return status;
}